// Scratch Cat Duo: the whole engine. Two decks, mixer (trim, EQ, colour, fader, crossfader, master), 3 slot FX chain, cue bus.
#pragma once
#include "DuoDeck.h"
#include <atomic>

namespace duo {

struct MixParams {
    float trimDb[2] = { 0, 0 }, eqHi[2] = { 0, 0 }, eqMid[2] = { 0, 0 }, eqLo[2] = { 0, 0 }, colour[2] = { 0, 0 };
    float fader[2] = { 1, 1 };               // 0..1
    bool cue[2] = { false, false };
    float cross = 0.5f; int curve = 2;       // 0 smooth, 1 sharp, 2 const
    float masterDb = 0.f;
    float cueMix = 0.f;                      // 0 = cue only .. 1 = master only in the cue bus
    int fxType[3] = { 1, 2, 3 }; bool fxOn[3] = { false, false, false }; float fxP1[3] = { 0.5f, 0.45f, 0.6f }, fxP2[3] = { 0.5f, 0.4f, 0.25f };
    int fxTarget = 1;                        // 0 deck A, 1 deck B, 2 A+B, 3 master
    float chainMix = 0.6f;
    float echoBeats = 0.75f;
    double hostBpm = 0;                      // > 0 when following the DAW tempo
};

class Engine {
public:
    Deck deck[2];
    std::atomic<float> peakA { 0.f }, peakB { 0.f }, peakM { 0.f }, peakML { 0.f }, peakMR { 0.f };

    void prepare(double sr, int maxBlock) {
        fs = sr; for (auto& d : deck) d.prepare(sr);
        for (auto& s : strip) s.prepare(sr);
        for (auto& f : fx) f.prepare(sr);
        for (auto& f : padFx) { f.prepare(sr); f.on = false; f.type = 1; }
        buildShots(); for (auto& v : voices) v.on = false;
        bufs.assign(10, std::vector<float>((size_t) maxBlock + 8, 0.f));
        gS[0] = gS[1] = 1.f; xS[0] = xS[1] = 0.f; masterS = 1.f; trimS[0] = trimS[1] = 1.f;
        cueG[0] = cueG[1] = 0.f;
    }

    // inA/inB: live input for a deck (may be null). Outputs may be null (cue / deck outs).
    void process(const MixParams& p, int n, const float* inAL, const float* inAR, const float* inBL, const float* inBR,
                 float* mL, float* mR, float* aL, float* aR, float* bL, float* bR, float* cL, float* cR) {
        if ((int) bufs[0].size() < n + 8) for (auto& b : bufs) b.resize((size_t) n + 8);
        float* dl[2] = { bufs[0].data(), bufs[2].data() }; float* dr[2] = { bufs[1].data(), bufs[3].data() };
        float* pl = bufs[4].data(); float* pr = bufs[5].data();     // pfl bus
        // deck follows the leading tempo
        const double lead = p.hostBpm > 0 ? p.hostBpm : 0.0;
        for (int d = 0; d < 2; ++d) {
            if (lead > 0) deck[d].setSync(deck[d].isSyncOn(), lead);
            deck[d].process(dl[d], dr[d], n, d == 0 ? inAL : inBL, d == 0 ? inAR : inBR);
            strip[d].setParams(p.eqLo[d], p.eqMid[d], p.eqHi[d], p.colour[d]);
        }
        float gA, gB; crossGains(p.curve, p.cross, gA, gB);
        const float cg[2] = { gA, gB };
        for (int i = 0; i < n; ++i) {
            // smooth the target gains so knob moves never zipper
            const float sm = 1.f - std::exp(-1.f / (0.005f * (float) fs));
            for (int d = 0; d < 2; ++d) {
                trimS[d] += (dbToLin(p.trimDb[d]) - trimS[d]) * sm;
                gS[d] += (p.fader[d] * p.fader[d] - gS[d]) * sm;     // fader law: squared (smoother at the bottom)
                xS[d] += (cg[d] - xS[d]) * sm;
                cueG[d] += ((p.cue[d] ? 1.f : 0.f) - cueG[d]) * sm;
            }
            masterS += (dbToLin(p.masterDb) - masterS) * sm;
            float ch[2][2];
            for (int d = 0; d < 2; ++d) {
                const float l = strip[d].tick(0, dl[d][i] * trimS[d]), r = strip[d].tick(1, dr[d][i] * trimS[d]);
                pl[i] = d == 0 ? l * cueG[0] : pl[i] + l * cueG[1];
                pr[i] = d == 0 ? r * cueG[0] : pr[i] + r * cueG[1];
                ch[d][0] = l * gS[d]; ch[d][1] = r * gS[d];
            }
            dl[0][i] = ch[0][0]; dr[0][i] = ch[0][1]; dl[1][i] = ch[1][0]; dr[1][i] = ch[1][1];
        }
        // FX on a deck channel (post fader, pre crossfader)
        for (int d = 0; d < 2; ++d) { padFx[d].bpm = (float) std::max(60.0, p.hostBpm > 0 ? p.hostBpm : std::max(60.0, deck[d].currentBpm())); padFx[d].echoBeats = p.echoBeats; padFx[d].process(dl[d], dr[d], n); }
        configureFx(p);
        if (p.fxTarget == 0) runFx(dl[0], dr[0], n); else if (p.fxTarget == 1) runFx(dl[1], dr[1], n);
        float* mixL = bufs[6].data(); float* mixR = bufs[7].data();
        for (int i = 0; i < n; ++i) { mixL[i] = dl[0][i] * xS[0] + dl[1][i] * xS[1]; mixR[i] = dr[0][i] * xS[0] + dr[1][i] * xS[1]; }
        if (p.fxTarget == 2) runFx(mixL, mixR, n);
        float pa = 0, pb = 0, pm = 0, pml = 0, pmr = 0;
        for (int i = 0; i < n; ++i) { mixL[i] *= masterS; mixR[i] *= masterS; }
        if (p.fxTarget == 3) runFx(mixL, mixR, n);
        for (auto& v : voices) if (v.on) { const auto& b = shotBuf[(size_t) v.idx]; for (int i = 0; i < n && v.on; ++i) { const float x = b[v.pos] * 0.7f; mixL[i] += x; mixR[i] += x; if (++v.pos >= b.size()) v.on = false; } }
        for (int i = 0; i < n; ++i) {
            pa = std::max(pa, std::max(std::fabs(dl[0][i]), std::fabs(dr[0][i]))); pb = std::max(pb, std::max(std::fabs(dl[1][i]), std::fabs(dr[1][i])));
            // safety: soft clip only above 0.95 so normal levels stay bit exact
            mixL[i] = softClip(mixL[i]); mixR[i] = softClip(mixR[i]);
            pml = std::max(pml, std::fabs(mixL[i])); pmr = std::max(pmr, std::fabs(mixR[i]));
            pm = std::max(pm, std::max(std::fabs(mixL[i]), std::fabs(mixR[i])));
        }
        auto fall = [n, this](std::atomic<float>& a, float v) { const float old = a.load() * std::pow(0.2f, (float) n / (float) fs); a.store(std::max(old, v)); };
        fall(peakA, pa); fall(peakB, pb); fall(peakM, pm); fall(peakML, pml); fall(peakMR, pmr);
        if (mL) std::copy(mixL, mixL + n, mL);
        if (mR) std::copy(mixR, mixR + n, mR);
        if (aL) std::copy(dl[0], dl[0] + n, aL);
        if (aR) std::copy(dr[0], dr[0] + n, aR);
        if (bL) std::copy(dl[1], dl[1] + n, bL);
        if (bR) std::copy(dr[1], dr[1] + n, bR);
        if (cL && cR) for (int i = 0; i < n; ++i) { cL[i] = pl[i] * (1.f - p.cueMix) + mixL[i] * p.cueMix; cR[i] = pr[i] * (1.f - p.cueMix) + mixR[i] * p.cueMix; }
    }
    static float softClip(float x) {
        const float a = std::fabs(x); if (a <= 0.95f) return x;
        const float s = x < 0 ? -1.f : 1.f; return s * (0.95f + 0.05f * std::tanh((a - 0.95f) / 0.05f));
    }
    ChannelStrip strip[2];
    FxSlot fx[3];
    FxSlot padFx[2];                       // momentary pad effect on a deck (hold a Pad FX pad)

    // Pad FX: kind 0..4 = Filter, Echo, Reverb, Flanger, Crush on the deck channel while held.
    void setPadFx(int deckIdx, int kind, bool on) {
        auto& f = padFx[deckIdx & 1];
        if (!on || kind < 0 || kind > 4) { f.on = false; return; }
        const int t = kind + 1; if (f.type != t) { f.type = t; f.prepare(fs); }
        f.p1 = kind == 0 ? 0.18f : 0.6f; f.p2 = 0.85f; f.on = true;
    }
    // Sampler: eight built in one shots played over the master. Synthesised, nothing to load.
    static constexpr int kShots = 8;
    void triggerShot(int i) {
        if (i < 0 || i >= kShots || shotBuf[(size_t) i].empty()) return;
        for (auto& v : voices) if (!v.on) { v.on = true; v.idx = i; v.pos = 0; return; }
        voices[0] = { true, i, 0 };
    }
    static const char* shotName(int i) { static const char* n[kShots] = { "Kick", "Snare", "Clap", "Hat", "Horn", "Zap", "Tom", "Stab" }; return n[i & 7]; }
private:
    struct Voice { bool on = false; int idx = 0; size_t pos = 0; };
    std::array<Voice, 8> voices;
    std::array<std::vector<float>, 8> shotBuf;
    void buildShots() {
        uint32_t seed = 12345u; auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 8388608.f - 1.f; };
        const double twoPi = 6.283185307179586;
        for (int k = 0; k < kShots; ++k) {
            const double dur = k == 0 ? 0.40 : k == 1 ? 0.22 : k == 2 ? 0.20 : k == 3 ? 0.07 : k == 4 ? 0.45 : k == 5 ? 0.28 : k == 6 ? 0.30 : 0.35;
            std::vector<float> b((size_t) (dur * fs)); double ph = 0, ph2 = 0, hp = 0, last = 0;
            for (size_t i = 0; i < b.size(); ++i) {
                const double t = (double) i / fs; const float nz = rnd(); float y = 0.f;
                if (k == 0) { ph += twoPi * (45.0 + 110.0 * std::exp(-t * 28.0)) / fs; y = (float) (std::sin(ph) * std::exp(-t * 7.0)); }
                else if (k == 6) { ph += twoPi * (80.0 + 140.0 * std::exp(-t * 18.0)) / fs; y = (float) (std::sin(ph) * std::exp(-t * 9.0)); }
                else if (k == 1) { ph += twoPi * 190.0 / fs; hp += 0.3 * ((double) nz - hp); y = (float) (0.5 * std::sin(ph) * std::exp(-t * 20.0) + ((double) nz - hp) * std::exp(-t * 18.0)); }
                else if (k == 2) { double e = std::exp(-t * 22.0) * 0.5; for (double o : { 0.0, 0.011, 0.022 }) if (t >= o) e += std::exp(-(t - o) * 400.0) * 0.6; hp += 0.4 * ((double) nz - hp); y = (float) (((double) nz - hp) * e); }
                else if (k == 3) { const double d = (double) nz - last; last = nz; y = (float) (d * 0.5 * std::exp(-t * 70.0)); }
                else if (k == 5) { ph += twoPi * (80.0 + 2000.0 * std::exp(-t * 11.0)) / fs; y = (float) (std::sin(ph) * std::exp(-t * 9.0) * 0.8); }
                else { const double f0 = k == 4 ? 220.0 : 329.6; ph += twoPi * f0 / fs; ph2 += twoPi * f0 * 1.005 / fs; double s2 = 0; for (int h = 1; h <= 8; ++h) s2 += std::sin(ph * h) / h + std::sin(ph2 * h) / h; y = (float) (s2 * 0.22 * std::exp(-t * 6.0) * std::min(1.0, t * 400.0)); }
                b[i] = clampf(y, -1.f, 1.f);
            }
            shotBuf[(size_t) k] = std::move(b);
        }
    }
    void configureFx(const MixParams& p) {
        for (int k = 0; k < 3; ++k) {
            if (fx[k].type != p.fxType[k]) { const bool wasOn = fx[k].on; (void) wasOn; fx[k].type = p.fxType[k]; fx[k].prepare(fs); }
            fx[k].on = p.fxOn[k]; fx[k].p1 = p.fxP1[k]; fx[k].p2 = p.fxP2[k] * p.chainMix;
            fx[k].bpm = p.hostBpm > 0 ? (float) p.hostBpm : (float) std::max(60.0, std::max(deck[0].currentBpm(), deck[1].currentBpm() * (p.fxTarget == 1 ? 1.0 : 0.0)));
            if (p.fxTarget == 1 && deck[1].currentBpm() > 0 && p.hostBpm <= 0) fx[k].bpm = (float) deck[1].currentBpm();
            if (p.fxTarget == 0 && deck[0].currentBpm() > 0 && p.hostBpm <= 0) fx[k].bpm = (float) deck[0].currentBpm();
            fx[k].echoBeats = p.echoBeats;
        }
    }
    void runFx(float* l, float* r, int n) { for (auto& f : fx) f.process(l, r, n); }
    double fs = 44100;
    std::vector<std::vector<float>> bufs;
    float gS[2] = { 1, 1 }, xS[2] = { 0, 0 }, trimS[2] = { 1, 1 }, cueG[2] = { 0, 0 }, masterS = 1.f;
};
}
