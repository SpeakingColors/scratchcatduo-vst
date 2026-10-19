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
    std::atomic<float> peakA { 0.f }, peakB { 0.f }, peakM { 0.f };

    void prepare(double sr, int maxBlock) {
        fs = sr; for (auto& d : deck) d.prepare(sr);
        for (auto& s : strip) s.prepare(sr);
        for (auto& f : fx) f.prepare(sr);
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
        configureFx(p);
        if (p.fxTarget == 0) runFx(dl[0], dr[0], n); else if (p.fxTarget == 1) runFx(dl[1], dr[1], n);
        float* mixL = bufs[6].data(); float* mixR = bufs[7].data();
        for (int i = 0; i < n; ++i) { mixL[i] = dl[0][i] * xS[0] + dl[1][i] * xS[1]; mixR[i] = dr[0][i] * xS[0] + dr[1][i] * xS[1]; }
        if (p.fxTarget == 2) runFx(mixL, mixR, n);
        float pa = 0, pb = 0, pm = 0;
        for (int i = 0; i < n; ++i) { mixL[i] *= masterS; mixR[i] *= masterS; }
        if (p.fxTarget == 3) runFx(mixL, mixR, n);
        for (int i = 0; i < n; ++i) {
            pa = std::max(pa, std::max(std::fabs(dl[0][i]), std::fabs(dr[0][i]))); pb = std::max(pb, std::max(std::fabs(dl[1][i]), std::fabs(dr[1][i])));
            // safety: soft clip only above 0.95 so normal levels stay bit exact
            mixL[i] = softClip(mixL[i]); mixR[i] = softClip(mixR[i]);
            pm = std::max(pm, std::max(std::fabs(mixL[i]), std::fabs(mixR[i])));
        }
        auto fall = [n, this](std::atomic<float>& a, float v) { const float old = a.load() * std::pow(0.2f, (float) n / (float) fs); a.store(std::max(old, v)); };
        fall(peakA, pa); fall(peakB, pb); fall(peakM, pm);
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
private:
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
