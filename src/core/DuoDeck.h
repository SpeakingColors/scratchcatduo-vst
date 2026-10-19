// Scratch Cat Duo: one deck. Plays a loaded track (or the last minute of the DAW input) with vinyl-style speed
// changes, scratching, cue, hot cues, loops, key lock and the scratch moves from the mockup. Pure C++.
#pragma once
#include "DuoAnalysis.h"
#include <memory>
#include <array>
#include <string>

namespace duo {

struct Track {
    std::vector<float> l, r;
    double sr = 44100;
    double bpm = 0, firstBeat = 0;       // firstBeat in seconds
    KeyResult key;
    std::string name;
    std::vector<float> peaks;            // min,max pairs for the overview
    size_t frames() const { return l.size(); }
};

enum class Move { None, Roll, Censor, Stutter, Transform };

class Deck {
public:
    static constexpr double kPlatterRevPerSec = 100.0 / 3.0 / 60.0;   // 33 1/3 rpm is normal speed

    void prepare(double hostSr) {
        fs = hostSr; ringSize = (size_t)(hostSr * 60.0); ringL.assign(ringSize, 0.f); ringR.assign(ringSize, 0.f); writeIdx = 0;
        vel = 0; for (auto& g : grains) g.on = false; hopCnt = 0; gateG = 1.f;
    }
    // Returns the track that was loaded before, so the caller can free it off the audio thread.
    std::shared_ptr<const Track> setTrack(std::shared_ptr<const Track> t) {
        auto old = std::move(trk); trk = std::move(t); pos = 0; vel = 0; playing = false; cuePoint = 0; loopOn = false; hot.fill(-1.0); mode = Mode::Normal; move = Move::None; live = false;
        if (trk) { src = Source::File; if (trk->bpm > 0) { /* first cue at the first beat */ cuePoint = trk->firstBeat * trk->sr; pos = cuePoint; } }
        return old;
    }
    void useLiveInput(bool b) { if (b) { src = Source::Live; live = true; playing = false; vel = 0; pos = (double) writeIdx; } else { src = Source::File; live = false; } }
    bool isLiveInput() const { return src == Source::Live; }
    bool inLiveMode() const { return src == Source::Live && live; }
    void goLive() { if (src == Source::Live) { live = true; playing = false; vel = 0; pos = (double) writeIdx; } }

    // ---- controls (audio thread, or tests) ----
    void setPlaying(bool p) { if (!hasAudio()) return; if (src == Source::Live && live && p) return; playing = p; if (p) mode = Mode::Normal; cueHeld = false; }
    void togglePlay() { setPlaying(!playing); }
    bool isPlaying() const { return playing; }
    void setPitch(float fader, float range) { pitch = fader; pitchRange = range; }     // fader -1..1
    void setKeyLock(bool b) { keylock = b; }
    void setQuant(bool b) { quant = b; }
    void setSync(bool on, double masterBpm) { sync = on; masterBpmV = masterBpm; }
    bool isSyncOn() const { return sync; }
    void setBend(float b) { bend = clampf(b, -1.f, 1.f); }
    void setVinylTime(float sec) { vinylTc = std::max(0.005f, sec); }
    // jog: while touching, speed follows the hand. velocity in platter turns per second (positive = forward).
    void setJog(bool touching, double turnsPerSec) {
        if (touching && !jogTouch && src == Source::Live && live) { live = false; pos = (double) writeIdx - 1.0; playing = false; }
        jogTouch = touching; jogVel = turnsPerSec / kPlatterRevPerSec; if (touching) mode = Mode::Normal;
    }
    bool touching() const { return jogTouch; }
    // tempo factor actually in use (1 = original)
    double tempo() const {
        double t = 1.0 + (double) pitch * (double) pitchRange;
        if (sync && trk && trk->bpm > 0 && masterBpmV > 0) { t = masterBpmV / trk->bpm; if (t > 1.5) t *= 0.5; else if (t < 0.67) t *= 2.0; t = std::min(1.5, std::max(0.5, t)); }
        return t;
    }
    double currentBpm() const { return trk && trk->bpm > 0 ? trk->bpm * tempo() : 0.0; }
    double beatLenSrc() const { return trk && trk->bpm > 0 ? trk->sr * 60.0 / trk->bpm : 0.0; }
    double snap(double p) const {
        const double bl = beatLenSrc(); if (!quant || bl <= 0) return p;
        const double g0 = trk->firstBeat * trk->sr; return g0 + std::round((p - g0) / bl) * bl;
    }
    void cue(bool down) {
        if (!hasAudio() || src == Source::Live) return;
        if (down) {
            if (playing) { pos = cuePoint; playing = false; vel = 0; }
            else if (std::fabs(pos - cuePoint) < 2.0 * trk->sr / 1000.0 * 5.0) { playing = true; cueHeld = true; }    // at the cue point: preview while held
            else { cuePoint = snap(pos); pos = cuePoint; }
        } else if (cueHeld) { cueHeld = false; playing = false; pos = cuePoint; vel = 0; }
    }
    void hotCue(int i, bool down) {
        if (!down || i < 0 || i >= 8 || !hasAudio() || src == Source::Live) return;
        if (hot[i] < 0) hot[i] = snap(pos); else { pos = hot[i]; if (!playing) setPlaying(true); }
    }
    void setHotCuePos(int i, double p) { if (i >= 0 && i < 8) hot[i] = p; }
    void clearHotCue(int i) { if (i >= 0 && i < 8) hot[i] = -1.0; }
    double hotCuePos(int i) const { return hot[i]; }
    void loopIn() { loopA = snap(pos); loopB = -1; loopOn = false; }
    void loopOut() { if (loopA >= 0 && pos > loopA + 64) { loopB = snap(pos); if (loopB <= loopA) loopB = pos; loopOn = true; } }
    void loopExit() { loopOn = false; }
    void beatLoop(double beats) {
        const double bl = beatLenSrc(); if (bl <= 0) return;
        loopA = snap(pos); loopB = loopA + bl * beats; loopOn = true;
    }
    void brake() { if (playing && !jogTouch) mode = Mode::Brake; }
    void backspin() { if (!jogTouch && hasAudio()) { mode = Mode::Backspin; spinV = -9.0; playing = true; } }
    // Moves held with a pad: roll length in beats applies to Roll and Stutter.
    void startMove(Move m, double beats = 0.25) {
        if (!hasAudio() || src == Source::Live) return;
        endMove(); move = m; moveBeats = beats; shadow = pos;
        if (m == Move::Roll) { const double bl = std::max(beatLenSrc(), trk->sr * 0.25); loopA = pos; loopB = pos + bl * beats; savedLoopOn = loopOn; loopOn = true; }
        if (m == Move::Stutter) { stutCnt = 0; if (!playing) setPlaying(true); }
    }
    void endMove() {
        if (move == Move::Roll) { loopOn = savedLoopOn; pos = shadow; }
        else if (move == Move::Censor) { pos = shadow; }
        move = Move::None; gateG = 1.f;
    }

    // ---- state for the UI ----
    double positionSeconds() const { return src == Source::File && trk ? pos / trk->sr : (double) pos / fs; }
    double lengthSeconds() const { return trk ? (double) trk->frames() / trk->sr : 0.0; }
    double speed() const { return vel; }
    const std::shared_ptr<const Track>& track() const { return trk; }
    double cueSeconds() const { return trk ? cuePoint / trk->sr : 0.0; }
    bool loopActive() const { return loopOn; }
    double loopStartSec() const { return trk && loopA >= 0 ? loopA / trk->sr : -1; }
    double loopEndSec() const { return trk && loopB >= 0 ? loopB / trk->sr : -1; }
    double beatPhase() const {            // 0..1 position inside the current beat, -1 if there is no grid
        const double bl = beatLenSrc(); if (bl <= 0 || src != Source::File) return -1;
        double p = (pos - trk->firstBeat * trk->sr) / bl; return p - std::floor(p);
    }
    void seekSeconds(double s) { if (trk && src == Source::File) pos = std::max(0.0, std::min((double) trk->frames() - 1.0, s * trk->sr)); }

    // Render n frames. in may be null. Output is added into nothing: it overwrites outL/outR.
    void process(float* outL, float* outR, int n, const float* inL, const float* inR) {
        const double srcRatio = (src == Source::File && trk) ? trk->sr / fs : 1.0;
        // keep the live input in the ring
        if (src == Source::Live) for (int i = 0; i < n; ++i) { ringL[writeIdx % ringSize] = inL ? inL[i] : 0.f; ringR[writeIdx % ringSize] = inR ? inR[i] : (inL ? inL[i] : 0.f); ++writeIdx; }
        if (src == Source::Live && live) {
            for (int i = 0; i < n; ++i) { outL[i] = inL ? inL[i] : 0.f; outR[i] = inR ? inR[i] : (inL ? inL[i] : 0.f); }
            pos = (double) writeIdx; return;
        }
        if (!hasAudio()) { std::fill(outL, outL + n, 0.f); std::fill(outR, outR + n, 0.f); return; }
        const double t = tempo();
        const double bl = beatLenSrc();
        const float cFast = 1.f - std::exp(-1.f / (0.002f * (float) fs)), cVinyl = 1.f - std::exp(-1.f / (vinylTc * (float) fs));
        const bool useGrains = keylock && src == Source::File && std::fabs(t - 1.0) > 0.002;
        const int glen = 2048, ghop = glen / 4;
        for (int i = 0; i < n; ++i) {
            double target; float c = cVinyl;
            if (jogTouch) { target = jogVel; c = cFast; }
            else if (mode == Mode::Brake) { target = 0; c = 1.f - std::exp(-1.f / (0.35f * (float) fs)); if (std::fabs(vel) < 0.01) { playing = false; mode = Mode::Normal; vel = 0; } }
            else if (mode == Mode::Backspin) { spinV *= (1.0 - 2.2 / fs * 1.0); target = spinV; c = cFast * 4.f; if (spinV > -0.05) { mode = Mode::Normal; playing = false; vel = 0; spinV = 0; } }
            else if (move == Move::Censor) { target = -t; c = cFast; }
            else if (playing) { target = t * (1.0 + 0.08 * (double) bend); }
            else target = 0;
            vel += (target - vel) * c;
            if (!playing && !jogTouch && mode == Mode::Normal && move != Move::Censor && std::fabs(vel) < 1e-4) vel = 0;
            // sample
            float l = 0, r = 0;
            if (vel != 0.0 || jogTouch) {
                if (useGrains && vel > 0) {
                    if (hopCnt-- <= 0) { hopCnt = ghop; for (auto& g : grains) if (!g.on) { g.on = true; g.p = pos; g.age = 0; break; } }
                    float al = 0, ar = 0;
                    for (auto& g : grains) if (g.on) {
                        const float w = 0.5f - 0.5f * std::cos(2.f * (float) kPi * (float) g.age / (float) glen);
                        float a, b; readAt(g.p, a, b); al += a * w; ar += b * w;
                        g.p += srcRatio; if (++g.age >= glen) g.on = false;
                    }
                    l = al * 0.5f; r = ar * 0.5f;
                } else readAt(pos, l, r);
            }
            // advance
            pos += vel * srcRatio;
            if (move == Move::Roll || move == Move::Censor) shadow += (move == Move::Roll ? t : t) * srcRatio * (playing ? 1.0 : 0.0);
            if (loopOn && loopB > loopA) { const double len = loopB - loopA; if (vel > 0 && pos >= loopB) pos -= len * std::max(1.0, std::floor((pos - loopA) / len)); else if (vel < 0 && pos < loopA) pos += len; }
            if (move == Move::Stutter && bl > 0 && ++stutCnt >= (int)(bl * moveBeats / srcRatio / std::max(0.1, t))) { stutCnt = 0; pos = cuePoint; }
            if (move == Move::Transform && bl > 0) { const double ph = std::fmod((pos - trk->firstBeat * trk->sr) / (bl * 0.25), 2.0); const float g = ph < 1.0 ? 1.f : 0.f; gateG += (g - gateG) * 0.01f; l *= gateG; r *= gateG; }
            if (src == Source::File) {
                if (pos >= (double) trk->frames() - 1) { pos = (double) trk->frames() - 1; playing = false; vel = 0; }
                if (pos < 0) { pos = 0; if (vel < 0) vel = 0; }
            } else {
                if (pos > (double) writeIdx - 2) pos = (double) writeIdx - 2;
                const double oldest = (double) writeIdx - (double) ringSize + 4.0; if (pos < oldest) pos = oldest;
            }
            outL[i] = l; outR[i] = r;
        }
    }

private:
    enum class Source { File, Live };
    enum class Mode { Normal, Brake, Backspin };
    struct Grain { bool on = false; double p = 0; int age = 0; };
    bool hasAudio() const { return src == Source::Live || (trk && trk->frames() > 4); }
    // 4 point Hermite, out of range reads as silence
    void readAt(double p, float& l, float& r) const {
        const long long i = (long long) std::floor(p); const float f = (float)(p - (double) i);
        float a[4], b[4];
        for (int k = 0; k < 4; ++k) {
            const long long j = i - 1 + k;
            if (src == Source::File) { if (j < 0 || j >= (long long) trk->frames()) { a[k] = b[k] = 0; } else { a[k] = trk->l[(size_t) j]; b[k] = trk->r.empty() ? a[k] : trk->r[(size_t) j]; } }
            else { if (j < 0 || j >= (long long) writeIdx || (long long) writeIdx - j > (long long) ringSize) { a[k] = b[k] = 0; } else { a[k] = ringL[(size_t) j % ringSize]; b[k] = ringR[(size_t) j % ringSize]; } }
        }
        auto h = [f](const float* y) { const float c0 = y[1], c1 = 0.5f * (y[2] - y[0]), c2 = y[0] - 2.5f * y[1] + 2.f * y[2] - 0.5f * y[3], c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]); return ((c3 * f + c2) * f + c1) * f + c0; };
        l = h(a); r = h(b);
    }

    double fs = 44100;
    std::shared_ptr<const Track> trk;
    Source src = Source::File; bool live = false;
    std::vector<float> ringL, ringR; size_t ringSize = 0; unsigned long long writeIdx = 0;
    double pos = 0, vel = 0;
    bool playing = false, cueHeld = false, keylock = false, quant = true, sync = false;
    float pitch = 0.f, pitchRange = 0.08f, bend = 0.f, vinylTc = 0.06f;
    double masterBpmV = 0;
    bool jogTouch = false; double jogVel = 0;
    double cuePoint = 0; std::array<double, 8> hot = { -1, -1, -1, -1, -1, -1, -1, -1 };
    double loopA = -1, loopB = -1; bool loopOn = false, savedLoopOn = false;
    Mode mode = Mode::Normal; double spinV = 0;
    Move move = Move::None; double moveBeats = 0.25, shadow = 0; int stutCnt = 0; float gateG = 1.f;
    std::array<Grain, 6> grains; int hopCnt = 0;
};
}
