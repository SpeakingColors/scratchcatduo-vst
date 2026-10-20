// Pure C++ tests for the Scratch Cat Duo engine (no JUCE): sh tests/run.sh
#include "../src/core/DuoEngine.h"
#include "../src/core/DuoLibrary.h"
#include <cstdio>
#include <random>
using namespace duo;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FAIL %s:%d  ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::shared_ptr<Track> makeTrack(double sr, double secs, double bpm, double firstBeat, bool chord = false, int tonic = 9, bool minor = true) {
    auto t = std::make_shared<Track>(); t->sr = sr; const size_t n = (size_t)(sr * secs); t->l.assign(n, 0.f); t->r.assign(n, 0.f);
    std::mt19937 rng(5); std::uniform_real_distribution<float> u(-1.f, 1.f);
    const double per = 60.0 / bpm;
    for (double b = firstBeat; b < secs; b += per) {                      // kick: decaying 60 Hz sine plus a bit of noise
        const size_t s0 = (size_t)(b * sr);
        for (size_t i = 0; i < (size_t)(0.18 * sr) && s0 + i < n; ++i) { const double e = std::exp(-(double) i / (0.04 * sr)); const float v = (float)(0.8 * e * std::sin(2 * kPi * 60 * i / sr) + 0.2 * e * u(rng)); t->l[s0 + i] += v; t->r[s0 + i] += v; }
        const size_t s1 = (size_t)((b + per / 2) * sr);                   // offbeat hat
        for (size_t i = 0; i < (size_t)(0.03 * sr) && s1 + i < n; ++i) { const float v = 0.2f * std::exp(-(float) i / (0.008f * (float) sr)) * u(rng); t->l[s1 + i] += v; t->r[s1 + i] += v; }
    }
    if (chord) {   // I IV V I progression, one chord per second (a lone triad is ambiguous with its relative key)
        const int roots[4] = { 0, 5, 7, 0 };
        for (size_t seg = 0; seg * (size_t) sr < n; ++seg) {
            const int rt = roots[seg % 4]; const bool third = (seg % 4 == 2) ? false : minor;   // the V chord is major in minor keys
            const int iv[3] = { 0, third ? 3 : 4, 7 };
            for (int k = 0; k < 3; ++k) for (int oct = 0; oct < 2; ++oct) {
                const double f = 440.0 * std::pow(2.0, ((tonic + rt + iv[k] - 9) + 12 * (oct - 1)) / 12.0);
                const size_t a0 = seg * (size_t) sr, a1 = std::min(n, a0 + (size_t) sr);
                for (size_t i = a0; i < a1; ++i) { const float v = 0.12f * (float) std::sin(2 * kPi * f * (double) i / sr); t->l[i] += v; t->r[i] += v; }
            }
        }
    }
    return t;
}

static void testBpm() {
    for (double bpm : { 100.0, 128.0, 140.0, 174.0 }) {
        auto t = makeTrack(44100, 40, bpm, 0.30);
        auto r = analyseBpm(t->l.data(), t->r.data(), t->frames(), t->sr);
        CHECK(std::fabs(r.bpm - bpm) < 0.3, "bpm %.1f detected %.2f", bpm, r.bpm);
        const double per = 60.0 / bpm; double d = std::fmod(r.firstBeat - 0.30 + per * 10, per); d = std::min(d, per - d);
        CHECK(d < 0.03, "bpm %.0f first beat error %.3f s (got %.3f)", bpm, d, r.firstBeat);
    }
    auto t = makeTrack(48000, 40, 126.0, 0.1);
    auto r = analyseBpm(t->l.data(), t->r.data(), t->frames(), t->sr);
    CHECK(std::fabs(r.bpm - 126.0) < 0.3, "48k bpm %.2f", r.bpm);
}
static void testKey() {
    struct C { int tonic; bool minor; const char* camel; } cases[] = { { 9, true, "8A" }, { 0, false, "8B" }, { 7, false, "9B" }, { 4, true, "9A" }, { 2, true, "7A" } };
    for (auto& c : cases) {
        auto t = makeTrack(44100, 30, 120, 0.0, true, c.tonic, c.minor);
        auto k = analyseKey(t->l.data(), t->r.data(), t->frames(), t->sr);
        CHECK(k.tonic == c.tonic && k.minor == c.minor, "key tonic %d minor %d -> got %d %d (%s)", c.tonic, c.minor, k.tonic, k.minor, keyName(k).c_str());
        CHECK(camelot(k) == c.camel, "camelot %s expected %s", camelot(k).c_str(), c.camel);
    }
    KeyResult a{ 9, true, 1 }, b{ 4, true, 1 }, c{ 0, false, 1 }, d{ 6, true, 1 };
    CHECK(keysCompatible(a, b), "Am Em compatible"); CHECK(keysCompatible(a, c), "Am C compatible"); CHECK(!keysCompatible(a, d), "Am F#m not compatible");
}
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt(s / (double)(b - a)); }
static double zeroCrossHz(const std::vector<float>& v, size_t a, size_t b, double sr) { int z = 0; for (size_t i = a + 1; i < b; ++i) if ((v[i - 1] < 0) != (v[i] < 0)) ++z; return 0.5 * z / ((double)(b - a) / sr); }

static std::shared_ptr<Track> sineTrack(double sr, double secs, double f) { auto t = std::make_shared<Track>(); t->sr = sr; const size_t n = (size_t)(sr * secs); t->l.resize(n); t->r.resize(n); for (size_t i = 0; i < n; ++i) t->l[i] = t->r[i] = 0.5f * (float) std::sin(2 * kPi * f * i / sr); return t; }

static void render(Deck& d, int n, std::vector<float>& L, std::vector<float>& R) {
    L.resize(n); R.resize(n);
    for (int i = 0; i < n; i += 256) { const int c = std::min(256, n - i); d.process(&L[i], &R[i], c, nullptr, nullptr); }
}
static void testDeck() {
    const double sr = 48000;
    { // plain playback, pitch 0: output frequency equals source
        Deck d; d.prepare(sr); auto t = sineTrack(sr, 5, 1000); d.setTrack(t); d.setPlaying(true);
        std::vector<float> L, R; render(d, (int) sr, L, R);
        CHECK(std::fabs(zeroCrossHz(L, 24000, 48000, sr) - 1000) < 5, "play 1 kHz at %.1f", zeroCrossHz(L, 24000, 48000, sr));
    }
    { // pitch +8 percent, vinyl style: frequency rises 8 percent
        Deck d; d.prepare(sr); auto t = sineTrack(sr, 5, 1000); d.setTrack(t); d.setPitch(1.f, 0.08f); d.setPlaying(true);
        std::vector<float> L, R; render(d, (int) sr, L, R);
        CHECK(std::fabs(zeroCrossHz(L, 24000, 48000, sr) - 1080) < 8, "pitch +8 percent at %.1f", zeroCrossHz(L, 24000, 48000, sr));
    }
    { // key lock: tempo +8 percent but pitch stays at 1 kHz, and the position moves 8 percent faster
        Deck d; d.prepare(sr); auto t = sineTrack(sr, 8, 1000); d.setTrack(t); d.setPitch(1.f, 0.08f); d.setKeyLock(true); d.setPlaying(true);
        std::vector<float> L, R; render(d, (int) sr * 3, L, R);
        const double hz = zeroCrossHz(L, (size_t) sr, (size_t) sr * 3, sr);
        CHECK(std::fabs(hz - 1000) < 15, "keylock pitch %.1f", hz);
        CHECK(std::fabs(d.positionSeconds() - 3 * 1.08) < 0.12, "keylock position %.3f", d.positionSeconds());
        CHECK(rms(L, (size_t) sr, (size_t) sr * 3) > 0.25, "keylock level %.3f", rms(L, (size_t) sr, (size_t) sr * 3));
    }
    { // scratch: touching and moving the platter backwards plays reversed, at the hand speed
        Deck d; d.prepare(sr); auto t = sineTrack(sr, 5, 1000); d.setTrack(t); d.seekSeconds(2.0);
        d.setJog(true, -Deck::kPlatterRevPerSec * 2.0);   // twice normal speed backwards
        std::vector<float> L, R; render(d, (int) sr / 2, L, R);
        CHECK(std::fabs(zeroCrossHz(L, 12000, 24000, sr) - 2000) < 40, "scratch freq %.1f", zeroCrossHz(L, 12000, 24000, sr));
        CHECK(std::fabs(d.positionSeconds() - (2.0 - 1.0)) < 0.05, "scratch position %.3f", d.positionSeconds());
        d.setJog(true, 0.0); render(d, 2000, L, R);
        CHECK(rms(L, 1000, 2000) < 0.001, "held platter is silent, rms %.4f", rms(L, 1000, 2000));
    }
    { // brake stops, cue returns, backspin goes backwards then stops
        Deck d; d.prepare(sr); auto t = makeTrack(sr, 10, 120, 0.5); t->bpm = 120; t->firstBeat = 0.5; d.setTrack(t);
        d.setPlaying(true); std::vector<float> L, R; render(d, (int) sr * 2, L, R);
        const double p0 = d.positionSeconds(); CHECK(p0 > 1.5, "moved %.2f", p0);
        d.brake(); render(d, (int) sr * 3, L, R); CHECK(!d.isPlaying() && std::fabs(d.speed()) < 0.01, "brake stopped");
        d.cue(true); CHECK(std::fabs(d.positionSeconds() - 0.5) < 0.01 || d.positionSeconds() > 0, "cue pos %.3f", d.positionSeconds());
        d.setPlaying(true); render(d, (int) sr, L, R); const double p1 = d.positionSeconds(); d.backspin(); render(d, (int) sr / 4, L, R);
        CHECK(d.positionSeconds() < p1, "backspin went back %.3f -> %.3f", p1, d.positionSeconds());
        render(d, (int) sr * 4, L, R); CHECK(!d.isPlaying(), "backspin ends stopped");
    }
    { // loop: a 1 beat loop keeps the position inside the loop
        Deck d; d.prepare(sr); auto t = makeTrack(sr, 10, 120, 0.0); t->bpm = 120; t->firstBeat = 0; d.setTrack(t); d.seekSeconds(2.0); d.setPlaying(true);
        std::vector<float> L, R; render(d, 4800, L, R); d.beatLoop(1.0); render(d, (int) sr * 3, L, R);
        const double p = d.positionSeconds(); CHECK(p >= 1.9 && p <= 2.7, "loop position %.3f", p); CHECK(d.loopActive(), "loop active");
        d.loopExit(); render(d, (int) sr, L, R); CHECK(d.positionSeconds() > 3.0, "after exit %.3f", d.positionSeconds());
    }
    { // hot cue set and jump; sync
        Deck d; d.prepare(sr); auto t = makeTrack(sr, 10, 120, 0.0); t->bpm = 120; d.setTrack(t); d.seekSeconds(3.0); d.hotCue(2, true);
        CHECK(std::fabs(d.hotCuePos(2) / sr - 3.0) < 0.26, "hot cue %.3f", d.hotCuePos(2) / sr); d.seekSeconds(7.0); d.hotCue(2, true); CHECK(std::fabs(d.positionSeconds() - 3.0) < 0.26, "jump %.3f", d.positionSeconds());
        d.setSync(true, 128.0); CHECK(std::fabs(d.tempo() - 128.0 / 120.0) < 1e-9 && std::fabs(d.currentBpm() - 128.0) < 1e-6, "sync tempo %.4f", d.tempo());
        d.setSync(true, 62.0); CHECK(std::fabs(d.currentBpm() - 124.0) < 1e-6, "half time folds %.3f", d.currentBpm());
    }
    { // end of track stops
        Deck d; d.prepare(sr); auto t = sineTrack(sr, 1, 440); d.setTrack(t); d.setPlaying(true); std::vector<float> L, R; render(d, (int) sr * 2, L, R);
        CHECK(!d.isPlaying(), "stopped at end"); CHECK(rms(L, (size_t) sr + 4000, (size_t) sr * 2) < 1e-6, "silent after end");
    }
    { // live input: passes through, then scratch plays the recent past reversed
        Deck d; d.prepare(sr); d.useLiveInput(true);
        std::vector<float> in(48000), L(48000), R(48000); for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * (float) std::sin(2 * kPi * 500 * i / sr);
        for (int i = 0; i < 48000; i += 256) { const int c = std::min(256, 48000 - i); d.process(&L[i], &R[i], c, &in[i], &in[i]); }
        double err = 0; for (size_t i = 0; i < in.size(); ++i) err += std::fabs(L[i] - in[i]); CHECK(err < 1e-3, "live pass through err %.5f", err);
        d.setJog(true, -Deck::kPlatterRevPerSec); std::vector<float> o(4800), o2(4800); d.process(o.data(), o2.data(), 4800, nullptr, nullptr);
        CHECK(rms(o, 1000, 4800) > 0.2, "scratch of live audio has signal %.3f", rms(o, 1000, 4800)); CHECK(!d.inLiveMode(), "left live mode");
        d.goLive(); CHECK(d.inLiveMode(), "back live");
    }
}
static void testMixer() {
    float a, b;
    crossGains(0, 0.f, a, b); CHECK(a == 1.f && b == 0.f, "smooth left"); crossGains(0, 0.5f, a, b); CHECK(std::fabs(a - 0.5f) < 1e-6f && std::fabs(b - 0.5f) < 1e-6f, "smooth mid");
    crossGains(1, 0.5f, a, b); CHECK(a == 1.f && b == 1.f, "sharp mid both full"); crossGains(1, 1.f, a, b); CHECK(a == 0.f && b == 1.f, "sharp cuts A at the end"); crossGains(1, 0.95f, a, b); CHECK(std::fabs(a - 0.5f) < 1e-5f, "sharp fades over the last 10 percent");
    crossGains(2, 0.5f, a, b); CHECK(std::fabs(a * a + b * b - 1.f) < 1e-5f, "constant power");
    // EQ kill and colour filter
    const double sr = 48000; ChannelStrip s; s.prepare(sr);
    auto tone = [&](float f, float lo, float mid, float hi, float col) {
        s.prepare(sr); s.setParams(lo, mid, hi, col); double e = 0, e0 = 0; 
        for (int i = 0; i < 48000; ++i) { const float x = 0.5f * (float) std::sin(2 * kPi * f * i / sr); const float y = s.tick(0, x); if (i > 24000) { e += (double) y * y; e0 += (double) x * x; } }
        return 10 * std::log10(e / e0); };
    CHECK(tone(50, 0, 0, 0, 0) > -0.3 && tone(50, 0, 0, 0, 0) < 0.3, "flat");
    CHECK(tone(50, -26, 0, 0, 0) < -20, "low kill %.1f dB", tone(50, -26, 0, 0, 0));
    CHECK(tone(1000, 0, -26, 0, 0) < -15, "mid kill %.1f dB", tone(1000, 0, -26, 0, 0));
    CHECK(tone(12000, 0, 0, -26, 0) < -15, "high kill %.1f dB", tone(12000, 0, 0, -26, 0));
    CHECK(tone(50, 6, 0, 0, 0) > 4.5, "low boost %.1f", tone(50, 6, 0, 0, 0));
    CHECK(tone(8000, 0, 0, 0, -0.8f) < -20, "colour low pass cuts highs %.1f", tone(8000, 0, 0, 0, -0.8f));
    CHECK(tone(200, 0, 0, 0, 0.8f) < -15, "colour high pass cuts lows %.1f", tone(200, 0, 0, 0, 0.8f));
}
static void testEngineAndFx() {
    const double sr = 48000; Engine e; e.prepare(sr, 512);
    auto ta = sineTrack(sr, 10, 440), tb = sineTrack(sr, 10, 1320); e.deck[0].setTrack(ta); e.deck[1].setTrack(tb); e.deck[0].setPlaying(true); e.deck[1].setPlaying(true);
    MixParams p; p.cross = 0.f; p.curve = 2;
    std::vector<float> ml(512), mr(512), al(512), ar(512), bl(512), br(512), cl(512), cr(512);
    auto run = [&](int blocks, MixParams& pp, std::vector<float>& out) { out.clear(); for (int k = 0; k < blocks; ++k) { e.process(pp, 512, nullptr, nullptr, nullptr, nullptr, ml.data(), mr.data(), al.data(), ar.data(), bl.data(), br.data(), cl.data(), cr.data()); out.insert(out.end(), ml.begin(), ml.end()); } };
    std::vector<float> o; run(40, p, o);
    CHECK(std::fabs(zeroCrossHz(o, 12000, o.size(), sr) - 440) < 8, "crossfader left = deck A only, %.1f Hz", zeroCrossHz(o, 12000, o.size(), sr));
    p.cross = 1.f; run(40, p, o); CHECK(std::fabs(zeroCrossHz(o, 12000, o.size(), sr) - 1320) < 20, "crossfader right = deck B only, %.1f", zeroCrossHz(o, 12000, o.size(), sr));
    p.cross = 0.5f; p.fader[0] = 0.f; p.fader[1] = 0.f; run(40, p, o); CHECK(rms(o, 12000, o.size()) < 1e-4, "faders down = silence");
    p.fader[0] = 1.f; p.fader[1] = 1.f; p.cross = 0.f; run(40, p, o); const double r0 = rms(o, 12000, o.size());
    CHECK(r0 > 0.3 && r0 < 0.4, "unity level %.3f (0.5 sine = 0.354)", r0);
    // fx: off is bit exact with chain of three slots; on changes the sound; no NaN
    MixParams q = p; q.fxTarget = 3; std::vector<float> o1; run(20, q, o1); 
    Engine e2; e2.prepare(sr, 512); e2.deck[0].setTrack(ta); e2.deck[1].setTrack(tb); e2.deck[0].setPlaying(true); e2.deck[1].setPlaying(true);
    for (int type = 1; type < FxTypeCount; ++type) {
        MixParams z = p; z.fxTarget = 3; for (int k = 0; k < 3; ++k) { z.fxType[k] = type; z.fxOn[k] = true; z.fxP2[k] = 1.f; } z.chainMix = 1.f;
        Engine e3; e3.prepare(sr, 512); e3.deck[0].setTrack(ta); e3.deck[0].setPlaying(true);
        bool bad = false; double en = 0;
        for (int k = 0; k < 60; ++k) { e3.process(z, 512, nullptr, nullptr, nullptr, nullptr, ml.data(), mr.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr); for (float v : ml) { if (!std::isfinite(v) || std::fabs(v) > 1.01f) bad = true; en += (double) v * v; } }
        CHECK(!bad, "fx %s finite and bounded", fxName(type)); CHECK(en > 0.5, "fx %s makes sound %.3f", fxName(type), en);
    }
    { // slot with on=false is bit exact
        FxSlot s; s.prepare(sr); s.type = FxEcho; s.on = false; std::vector<float> l(512), r(512), l0, r0; for (int i = 0; i < 512; ++i) l[i] = r[i] = 0.3f * (float) std::sin(i * 0.1); l0 = l; r0 = r; s.process(l.data(), r.data(), 512);
        CHECK(l == l0 && r == r0, "fx off is bit exact");
    }
    { // cue bus carries only channels with cue on
        MixParams z; z.cue[1] = true; z.cueMix = 0.f; z.fader[0] = 0.f; z.cross = 0.f; std::vector<float> c; for (int k = 0; k < 40; ++k) { e.process(z, 512, nullptr, nullptr, nullptr, nullptr, ml.data(), mr.data(), nullptr, nullptr, nullptr, nullptr, cl.data(), cr.data()); c.insert(c.end(), cl.begin(), cl.end()); }
        CHECK(std::fabs(zeroCrossHz(c, 12000, c.size(), sr) - 1320) < 20, "cue bus plays deck B (%.1f Hz) even with crossfader on A", zeroCrossHz(c, 12000, c.size(), sr));
    }
    { // soft clip leaves normal levels untouched and bounds the rest
        CHECK(Engine::softClip(0.5f) == 0.5f && Engine::softClip(-0.9f) == -0.9f, "softclip transparent"); CHECK(std::fabs(Engine::softClip(8.f)) <= 1.0f, "softclip bounded");
    }
}

// Share of the windowed energy found at the wanted tones: 1.0 is a clean copy, lower means smeared or combed.
static double toneShare(const std::vector<float>& v, size_t a, size_t b, double sr, const std::vector<double>& freqs) {
    const double N = (double)(b - a); double tot = 0, got = 0;
    for (size_t i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos(2 * kPi * (double)(i - a) / N); tot += w * w * (double) v[i] * v[i]; }
    for (double f : freqs) { double c = 0, s = 0; for (size_t i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos(2 * kPi * (double)(i - a) / N); c += w * v[i] * std::cos(2 * kPi * f * (double) i / sr); s += w * v[i] * std::sin(2 * kPi * f * (double) i / sr); } got += 2.0 * (c * c + s * s) * (0.375 * N) / ((0.5 * N) * (0.5 * N)); }
    return got / std::max(tot, 1e-12);
}
static double maxStep(const std::vector<float>& v, size_t a, size_t b) { double m = 0; for (size_t i = a + 1; i < b; ++i) m = std::max(m, (double) std::fabs(v[i] - v[i - 1])); return m; }
static void testKeyLockQuality() {
    const double sr = 48000;
    for (double pct : { 0.08, -0.08, 0.5, -0.4 }) {
        Deck d; d.prepare(sr); auto t = std::make_shared<Track>(); t->sr = sr; const size_t n = (size_t)(sr * 12); t->l.resize(n); t->r.resize(n);
        for (size_t i = 0; i < n; ++i) { const float v = 0.3f * (float) std::sin(2 * kPi * 440.0 * i / sr) + 0.2f * (float) std::sin(2 * kPi * 1330.0 * i / sr); t->l[i] = t->r[i] = v; }
        d.setTrack(t); d.setPitch((float)(pct > 0.4 || pct < -0.3 ? pct : pct / 0.08), pct > 0.4 || pct < -0.3 ? 1.f : 0.08f); d.setKeyLock(true); d.setPlaying(true);
        std::vector<float> L, R; render(d, (int) sr * 3, L, R);
        const size_t a = (size_t) sr, b = (size_t) sr * 3;
        // the 20 ms windowed RMS should stay steady; a bad overlap makes it pulse
        double mn = 1e9, mx = 0; for (size_t i = a; i + 960 < b; i += 480) { const double r = rms(L, i, i + 960); mn = std::min(mn, r); mx = std::max(mx, r); }
        double share = 0; int nw = 0; for (size_t w = a; w + 6000 <= b; w += 6000, ++nw) share += toneShare(L, w, w + 6000, sr, { 440.0, 1330.0 }); share /= nw;
        std::printf("  tone share %.3f\n", share);
        std::printf("keylock %+.0f%%: rms swing %.3f..%.3f (ratio %.3f), max step %.3f\n", pct * 100, mn, mx, mx / std::max(mn, 1e-9), maxStep(L, a, b));
        CHECK(share > 0.9, "keylock %+.0f%% tone purity %.3f (WSOLA alignment)", pct * 100, share);
        CHECK(mx / std::max(mn, 1e-9) < 1.12, "keylock %+.0f%% output pulses: rms ratio %.3f", pct * 100, mx / std::max(mn, 1e-9));
        CHECK(maxStep(L, a, b) < 0.25, "keylock %+.0f%% output has a jump of %.3f", pct * 100, maxStep(L, a, b));
    }
}
static void testAlign() {
    const double sr = 48000;
    Deck a, b; a.prepare(sr); b.prepare(sr);
    auto ta = makeTrack(sr, 20, 120, 0.10), tb = makeTrack(sr, 20, 120, 0.30); ta->bpm = tb->bpm = 120; ta->firstBeat = 0.10; tb->firstBeat = 0.30;
    a.setTrack(ta); b.setTrack(tb);
    a.seekSeconds(4.0); b.seekSeconds(4.13);                // b is 0.13 s (0.26 beat) ahead of a
    CHECK(b.alignBeatTo(a), "align works with grids");
    double d = std::fabs(a.beatPhase() - b.beatPhase()); d = std::min(d, 1.0 - d);
    CHECK(d < 0.002, "beat phases match after align (diff %.4f)", d);
    CHECK(std::fabs(b.positionSeconds() - 4.13) < 0.26, "align moves less than half a beat (%.3f)", b.positionSeconds());
    Deck c; c.prepare(sr); c.setTrack(sineTrack(sr, 5, 440));
    CHECK(!c.alignBeatTo(a), "no grid, no align");
}
static void testLibrary() {
    namespace fs = std::filesystem; const fs::path root = fs::temp_directory_path() / "duo_lib_test"; fs::remove_all(root);
    fs::create_directories(root / "drums" / "kicks"); fs::create_directories(root / "synth" / "leads" / "deep"); fs::create_directories(root / "docs");
    auto mk = [&](const char* rel) { std::ofstream(root / rel) << "x"; };
    mk("drums/kicks/Kick.WAV"); mk("drums/loop.mp3"); mk("synth/leads/deep/lead.flac"); mk("synth/pad.aiff"); mk("docs/readme.txt"); mk("drums/cover.png");
    Library lib; const int n = lib.addFolder(root.string());
    CHECK(n == 4, "scan finds audio in all subfolders and skips other files (found %d)", n);
    CHECK(lib.addFolder(root.string()) == 0, "scanning the same folder twice adds nothing");
    CHECK(lib.search("kick").size() == 1 && lib.search("KICK")[0].ext == "wav", "search ignores case and reads upper case extensions");
    CHECK(lib.search("flac").size() == 1 && lib.search("").size() == 4, "search matches the format, empty query lists all");
    CHECK(lib.search("", (root / "synth").string()).size() == 2, "folder filter includes subfolders");
    CHECK(duo::inFolder("/music/a", "/music") && duo::inFolder("/music", "/music") && !duo::inFolder("/music2", "/music"), "inFolder respects folder boundaries");
    CHECK(lib.addFolder((root / "missing").string()) == 0, "missing folder is ignored");
    { Library other; const auto found = lib.all(); CHECK(other.merge(found) == 4 && other.merge(found) == 0, "merge adds scan results once"); }
    lib.toggleFavourite("/a/b"); lib.toggleFavourite("/c d"); Library l2; l2.favouritesFromString(lib.favouritesToString());
    CHECK(l2.isFavourite("/a/b") && l2.isFavourite("/c d") && l2.favourites().size() == 2, "favourites survive a save and load");
    lib.toggleFavourite("/a/b"); CHECK(!lib.isFavourite("/a/b"), "favourite can be removed");
    fs::remove_all(root);
}
static void testPadsAndShots() {
    const double sr = 48000;
    { // jumpBeats moves exactly by beats; trans latch chops; vinyl changes stop time
        Deck d; d.prepare(sr); auto t = makeTrack(sr, 20, 120, 0.0); t->bpm = 120; t->firstBeat = 0.0; d.setTrack(t); d.seekSeconds(4.0);
        d.jumpBeats(4); CHECK(std::fabs(d.positionSeconds() - 6.0) < 0.02, "jump +4 beats %.3f", d.positionSeconds());
        d.jumpBeats(-8); CHECK(std::fabs(d.positionSeconds() - 2.0) < 0.02, "jump -8 beats %.3f", d.positionSeconds());
        d.jumpBeats(-100); CHECK(d.positionSeconds() >= 0.0, "jump clamps at start");
        Deck s; s.prepare(sr); s.setTrack(sineTrack(sr, 5, 440)); s.seekSeconds(1.0); s.jumpBeats(4); CHECK(std::fabs(s.positionSeconds() - 1.0) < 1e-6, "no grid, no jump");
        Deck g; g.prepare(sr); auto t2 = makeTrack(sr, 20, 120, 0.0); g.setTrack(t2); g.setPlaying(true);
        std::vector<float> L, R; g.setTransLatch(true); render(g, (int) sr * 2, L, R);
        double mn = 1e9, mx = 0; for (size_t i = 24000; i + 2400 < L.size(); i += 2400) { double e = rms(L, i, i + 2400); mn = std::min(mn, e); mx = std::max(mx, e); }
        CHECK(mn < 0.25 * mx, "trans latch gates the sound (min %.4f max %.4f)", mn, mx);
        g.setTransLatch(false); render(g, (int) sr, L, R); CHECK(std::isfinite(rms(L, 0, L.size())), "trans off finite");
    }
    { // shots and pad fx in the engine
        Engine e; e.prepare(sr, 512); std::vector<float> ml(512), mr(512);
        MixParams p; auto run = [&](int blocks) { double en = 0; bool bad = false; for (int k = 0; k < blocks; ++k) { e.process(p, 512, nullptr, nullptr, nullptr, nullptr, ml.data(), mr.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr); for (float v : ml) { if (!std::isfinite(v) || std::fabs(v) > 1.5f) bad = true; en += (double) v * v; } } CHECK(!bad, "shot output finite"); return en; };
        CHECK(run(10) < 1e-9, "no shot, silence");
        for (int i = 0; i < Engine::kShots; ++i) { e.triggerShot(i); const double en = run(40); CHECK(en > 0.05, "shot %s makes sound (%.4f)", Engine::shotName(i), en); }
        CHECK(run(40) < 1e-9, "shots end");
        for (int i = 0; i < 12; ++i) e.triggerShot(i % 8); CHECK(run(10) > 0.05, "many shots at once ok"); e.triggerShot(-1); e.triggerShot(99);
        e.deck[0].setTrack(sineTrack(sr, 10, 440)); e.deck[0].setPlaying(true); run(40);
        for (int k = 0; k < 5; ++k) { e.setPadFx(0, k, true); const double en = run(60); CHECK(en > 0.5, "pad fx %d makes sound %.3f", k, en); e.setPadFx(0, k, false); }
        e.setPadFx(0, 9, true); run(5);
    }
}
int main() {
    testBpm(); testKey(); testDeck(); testMixer(); testEngineAndFx(); testKeyLockQuality(); testAlign(); testLibrary(); testPadsAndShots();
    if (fails) { std::printf("%d FAILED\n", fails); return 1; }
    std::printf("all tests passed\n"); return 0;
}
