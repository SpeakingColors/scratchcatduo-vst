// Scratch Cat Duo: the waveform section (two overviews and two zoomed lanes on one time axis) and the platter.
#pragma once
#include "Duo.h"
#include "DuoUi.h"
#include <catui/SodaPop.h>

namespace duoui::ui
{
// Name, colour and on/off for each hot cue. Kept by the editor, saved in the plugin state.
struct HotMeta
{
    juce::String name[2][8]; int colour[2][8]; bool on[2][8];
    HotMeta() { for (int d = 0; d < 2; ++d) for (int i = 0; i < 8; ++i) { colour[d][i] = i; on[d][i] = true; } }
    Colour col(int d, int i) const { return padColour(colour[d][i]); }
    juce::String toString(int d) const
    {
        juce::StringArray a; for (int i = 0; i < 8; ++i) a.add(name[d][i].replaceCharacters("|;", "  ") + "|" + juce::String(colour[d][i]) + "|" + (on[d][i] ? "1" : "0"));
        return a.joinIntoString(";");
    }
    void fromString(int d, const juce::String& s)
    {
        auto a = juce::StringArray::fromTokens(s, ";", "");
        for (int i = 0; i < 8 && i < a.size(); ++i)
        {
            auto p = juce::StringArray::fromTokens(a[i], "|", "");
            name[d][i] = p.size() > 0 ? p[0] : juce::String(); colour[d][i] = juce::jlimit(0, 7, p.size() > 1 ? p[1].getIntValue() : i); on[d][i] = p.size() > 2 ? p[2] != "0" : true;
        }
    }
};

// The platter. Drag it to scratch: it plays at the speed of your hand, forward or backward, and holds when you stop.
class Jog : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Jog(DuoProcessor& p, int d) : proc(p), deck(d) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    ~Jog() override { stopTimer(); }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(2.f); const float d = std::min(r.getWidth(), r.getHeight()); r = r.withSizeKeepingCentre(d, d);
        const auto c = r.getCentre(); const Colour col = deckCol(deck);
        g.setColour(Colour(0xff15161a)); g.fillEllipse(r); g.setColour(Colour(0xff3b3e47)); g.drawEllipse(r, 2.f);
        const double rev = proc.view[deck].pos.load() * duo::Deck::kPlatterRevPerSec; const float ang = (float) (rev - std::floor(rev)) * juce::MathConstants<float>::twoPi;
        for (int i = 0; i < 72; ++i)
        {
            const float a = ang + (float) i * juce::MathConstants<float>::twoPi / 72.f; const bool big = i % 6 == 0;
            const float r1 = d * 0.5f - 8.f, r2 = r1 - (big ? 24.f : 16.f);
            g.setColour(col.withAlpha(big ? 1.f : 0.75f));
            g.drawLine(c.x + std::sin(a) * r1, c.y - std::cos(a) * r1, c.x + std::sin(a) * r2, c.y - std::cos(a) * r2, big ? 3.5f : 2.5f);
        }
        g.setColour(Colour(0xff26282f)); for (int k = 0; k < 4; ++k) g.drawEllipse(r.reduced(d * 0.2f + (float) k * d * 0.035f), 1.f);
        const float hub = d * 0.125f; g.setColour(col); g.fillEllipse(Rectangle<float>(hub * 2, hub * 2).withCentre(c));
        g.setColour(Colour(0xff111111)); g.setFont(f(15.f, true)); g.drawText("DECK", Rectangle<float>(hub * 2, 22.f).withCentre(c), juce::Justification::centred);
        g.setColour(Colour(0xfff4f5f7)); g.drawLine(c.x + std::sin(ang) * hub * 0.9f, c.y - std::cos(ang) * hub * 0.9f, c.x + std::sin(ang) * (d * 0.5f - 10.f), c.y - std::cos(ang) * (d * 0.5f - 10.f), 3.f);
        if (proc.view[deck].touching.load()) { g.setColour(col.withAlpha(0.4f)); g.drawEllipse(r, 4.f); }
    }
    void mouseDown(const juce::MouseEvent& e) override { last = ang(e); acc = 0.f; post(true, 0.0); startTimerHz(60); }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float a = ang(e); float dlt = a - last; while (dlt > juce::MathConstants<float>::pi) dlt -= juce::MathConstants<float>::twoPi; while (dlt < -juce::MathConstants<float>::pi) dlt += juce::MathConstants<float>::twoPi;
        acc += dlt / juce::MathConstants<float>::twoPi; last = a;
    }
    void mouseUp(const juce::MouseEvent&) override { stopTimer(); post(false, 0.0); }
private:
    void post(bool touch, double turnsPerSec) { duoui::Cmd c; c.type = duoui::kJog; c.deck = deck; c.i = touch ? 1 : 0; c.a = turnsPerSec; proc.post(c); }
    float ang(const juce::MouseEvent& e) const { const auto c = getLocalBounds().toFloat().getCentre(); return std::atan2(e.position.x - c.x, -(e.position.y - c.y)); }
    void timerCallback() override { post(true, (double) acc * 60.0); acc = 0.f; }
    DuoProcessor& proc; int deck; float last = 0.f, acc = 0.f;
};

// Two overview strips and two zoomed lanes. Click an overview to jump; drag a lane to scratch; drop a track on a deck's lane or strip to load it.
class WaveStack : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
{
public:
    WaveStack(DuoProcessor& p, const HotMeta& m) : proc(p), meta(m) {}
    ~WaveStack() override { stopTimer(); }
    int bars = 8;
    std::shared_ptr<const duo::Track> trk[2];
    std::function<void(int deck, const juce::File&)> onLoad;

    static constexpr int kOv = 24, kLane = 90, kGap = 6;
    Rectangle<int> ov(int d) const { return d == 0 ? Rectangle<int>(0, 0, getWidth(), kOv) : Rectangle<int>(0, kOv + kGap * 2 + kLane * 2, getWidth(), kOv); }
    Rectangle<int> lane(int d) const { return Rectangle<int>(0, kOv + kGap + d * (kLane + kGap), getWidth(), kLane); }

    void paint(juce::Graphics& g) override
    {
        for (int d = 0; d < 2; ++d) paintOverview(g, d);
        double ref = std::max(proc.view[0].bpm.load(), proc.view[1].bpm.load()); if (ref < 40) ref = 120;
        const double windowSec = (double) bars * 4.0 * 60.0 / ref;
        for (int d = 0; d < 2; ++d) paintLane(g, lane(d).toFloat(), d, windowSec);
        if (dragOver >= 0) { g.setColour(deckCol(dragOver).withAlpha(0.25f)); g.fillRect(dragOver == 0 ? ov(0).getUnion(lane(0)) : lane(1).getUnion(ov(1))); }
    }
    // mouse
    void mouseDown(const juce::MouseEvent& e) override
    {
        region = -1; const auto p = e.getPosition();
        for (int d = 0; d < 2; ++d) { if (ov(d).contains(p)) { region = 10 + d; seekTo(d, p.x); return; } if (lane(d).contains(p)) { region = d; scrubD = d; acc = 0.0; post(d, true, 0.0); startTimerHz(60); last = e.position.x; return; } }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (region >= 10) { seekTo(region - 10, e.getPosition().x); return; }
        if (region >= 0) { const float dx = e.position.x - last; last = e.position.x; acc += -(double) dx * srcPerPx(region); }
    }
    void mouseUp(const juce::MouseEvent&) override { if (region >= 0 && region < 10) { stopTimer(); post(region, false, 0.0); } region = -1; }
    // drag and drop of tracks from the browser
    bool isInterestedInDragSource(const SourceDetails& s) override { return s.description.toString().startsWith("duofile:"); }
    void dragOperationEnded(const juce::DragAndDropTarget::SourceDetails&) {}
    void itemDragEnter(const SourceDetails& s) override { dragOver = deckAt(s.localPosition); repaint(); }
    void itemDragMove(const SourceDetails& s) override { const int d = deckAt(s.localPosition); if (d != dragOver) { dragOver = d; repaint(); } }
    void itemDragExit(const SourceDetails&) override { dragOver = -1; repaint(); }
    void itemDropped(const SourceDetails& s) override { dragOver = -1; repaint(); if (onLoad) onLoad(deckAt(s.localPosition), juce::File(s.description.toString().fromFirstOccurrenceOf("duofile:", false, false))); }
    static Colour waveCol(int d) { return d == 0 ? Colour(0xffd9722a) : Colour(0xff35bfb2); }

private:
    int deckAt(juce::Point<int> p) const { return p.y < getHeight() / 2 ? 0 : 1; }
    double srcPerPx(int d) const
    {
        double ref = std::max(proc.view[0].bpm.load(), proc.view[1].bpm.load()); if (ref < 40) ref = 120;
        const double windowSec = (double) bars * 4.0 * 60.0 / ref; auto t = trk[d];
        const double tempo = t && t->bpm > 0 ? std::max(0.1, proc.view[d].bpm.load() / t->bpm) : 1.0;
        return windowSec / std::max(1, getWidth()) * tempo;
    }
    void post(int d, bool touch, double turnsPerSec) { duoui::Cmd c; c.type = duoui::kJog; c.deck = d; c.i = touch ? 1 : 0; c.a = turnsPerSec; proc.post(c); }
    void timerCallback() override { post(scrubD, true, acc * 60.0 * duo::Deck::kPlatterRevPerSec); acc = 0.0; }
    void seekTo(int d, int x)
    {
        const double len = proc.view[d].len.load(); if (len <= 0) return;
        duoui::Cmd c; c.type = duoui::kSeek; c.deck = d; c.a = juce::jlimit(0.0, 1.0, (double) x / std::max(1, getWidth())) * len; proc.post(c);
    }
    void paintOverview(juce::Graphics& g, int d)
    {
        auto r = ov(d); g.setColour(kDisp); g.fillRect(r);
        auto t = trk[d]; auto& v = proc.view[d];
        if (t == nullptr || t->peaks.empty()) { g.setColour(kLine); g.drawHorizontalLine(r.getCentreY(), (float) r.getX(), (float) r.getRight()); return; }
        if (cache[d].track != t.get() || cache[d].img.getWidth() != r.getWidth()) rebuild(d, t, r.getWidth(), r.getHeight());
        g.drawImageAt(cache[d].img, r.getX(), r.getY());
        const double len = std::max(1.0, v.len.load()); const int px = r.getX() + (int) (v.pos.load() / len * r.getWidth());
        g.setColour(juce::Colours::white); g.fillRect(px, r.getY(), 2, r.getHeight());
    }
    void rebuild(int d, const std::shared_ptr<const duo::Track>& t, int w, int h)
    {
        cache[d].track = t.get(); cache[d].img = juce::Image(juce::Image::ARGB, w, h, true); juce::Graphics g(cache[d].img);
        const int bins = (int) (t->peaks.size() / 2); const float cy = (float) h * 0.5f; g.setColour(waveCol(d));
        for (int x = 0; x < w; ++x)
        {
            const int b0 = (int) ((double) x / w * bins), b1 = std::min(bins - 1, std::max(b0, (int) ((double) (x + 1) / w * bins) - 1));
            float mn = 0, mx = 0; for (int b = b0; b <= b1; ++b) { mn = std::min(mn, t->peaks[(size_t) b * 2]); mx = std::max(mx, t->peaks[(size_t) b * 2 + 1]); }
            g.drawVerticalLine(x, cy - std::min(1.f, mx * 1.8f) * cy, cy - std::max(-1.f, mn * 1.8f) * cy + 1.f);
        }
    }
    void paintLane(juce::Graphics& g, Rectangle<float> r, int d, double windowSec)
    {
        const Colour col = waveCol(d); auto& v = proc.view[d]; auto t = trk[d];
        g.setColour(kDisp); g.fillRect(r);
        g.setColour(deckCol(d)); g.setFont(mono(13.f).boldened());
        juce::String head = juce::String(d == 0 ? "A" : "B");
        if (t) { head += "  " + juce::String(v.bpm.load(), 1) + " BPM"; if (t->key.tonic >= 0) head += "  " + juce::String(duo::keyName(t->key)); }
        else head += "  " + proc.statusLine(d);
        g.drawText(head, r.reduced(8, 3), juce::Justification::topLeft);
        if (t == nullptr || t->peaks.empty())
        {
            g.setColour(kDim); g.setFont(f(15.f));
            g.drawText(proc.get(d == 0 ? "src_a" : "src_b") > 0.5f ? "DAW input: live. Touch the platter to scratch the last minute." : "Load a track with the browser below, or drop a file here.", r, juce::Justification::centred); return;
        }
        const double pos = v.pos.load(); const double sp = windowSec / (double) r.getWidth() * (t->bpm > 0 ? std::max(0.1, v.bpm.load() / t->bpm) : 1.0);
        const int bins = (int) (t->peaks.size() / 2); const double binSec = 256.0 / t->sr; const float cy = r.getCentreY(), hh = r.getHeight() * 0.47f;
        g.setColour(col);
        for (int x = 0; x < (int) r.getWidth(); ++x)
        {
            const double t0 = pos + ((double) x - (double) r.getWidth() * 0.5) * sp; if (t0 < 0) continue;
            int b0 = (int) (t0 / binSec), b1 = (int) ((t0 + sp) / binSec); if (b0 >= bins) break; b1 = std::min(bins - 1, std::max(b0, b1));
            float mn = 0, mx = 0; for (int b = b0; b <= b1; ++b) { mn = std::min(mn, t->peaks[(size_t) b * 2]); mx = std::max(mx, t->peaks[(size_t) b * 2 + 1]); }
            g.drawVerticalLine((int) r.getX() + x, cy - std::min(1.f, mx * 1.8f) * hh, cy - std::max(-1.f, mn * 1.8f) * hh + 1.f);
        }
        auto xOf = [&](double sec) { return r.getX() + (float) ((double) r.getWidth() * 0.5 + (sec - pos) / sp); };
        if (t->bpm > 0)   // beat grid and beat numbers
        {
            const double per = 60.0 / t->bpm; const double left = pos - (double) r.getWidth() * 0.5 * sp, right = pos + (double) r.getWidth() * 0.5 * sp;
            long k0 = (long) std::floor((left - t->firstBeat) / per); g.setFont(mono(11.f));
            for (long k = k0; t->firstBeat + (double) k * per < right; ++k)
            {
                const float x = xOf(t->firstBeat + (double) k * per); if (x < r.getX() || x > r.getRight()) continue;
                const int beat = (int) (((k % 4) + 4) % 4) + 1;
                g.setColour(Colour(beat == 1 ? 0x55ffffff : 0x26ffffff)); g.drawVerticalLine((int) x, r.getY(), r.getBottom());
                g.setColour(Colour(0x99a9adb8)); g.drawText(juce::String(beat), (int) x + 3, (int) r.getBottom() - 15, 18, 13, juce::Justification::left);
            }
        }
        if (v.loopOn.load() && v.loopA.load() >= 0 && v.loopB.load() > v.loopA.load()) { g.setColour(catui::SodaPop::mint().withAlpha(0.2f)); const float a = xOf(v.loopA.load()), b = xOf(v.loopB.load()); g.fillRect(Rectangle<float>(a, r.getY(), b - a, r.getHeight()).getIntersection(r)); }
        for (int i = 0; i < 8; ++i)
        {
            const double h = v.hot[(size_t) i].load(); if (h < 0) continue; const float x = xOf(h); if (x < r.getX() || x > r.getRight()) continue;
            const Colour pc = meta.col(d, i).withAlpha(meta.on[d][i] ? 1.f : 0.4f); g.setColour(pc); g.fillRect(x - 1.f, r.getY(), 2.f, r.getHeight());
            g.fillRect(x, r.getY() + 2.f, 14.f, 14.f); g.setColour(Colour(0xff05070a)); g.setFont(f(11.f, true)); g.drawText(juce::String(i + 1), (int) x, (int) r.getY() + 2, 14, 14, juce::Justification::centred);
        }
        const float cx = xOf(v.cue.load()); if (cx >= r.getX() && cx <= r.getRight()) { g.setColour(catui::SodaPop::lemon()); juce::Path tri; tri.addTriangle(cx - 6, r.getBottom(), cx + 6, r.getBottom(), cx, r.getBottom() - 11); g.fillPath(tri); }
        g.setColour(juce::Colours::white); g.fillRect(r.getCentreX() - 1.f, r.getY(), 2.f, r.getHeight());
    }
    struct Cache { const duo::Track* track = nullptr; juce::Image img; } cache[2];
    DuoProcessor& proc; const HotMeta& meta; int region = -1, scrubD = 0, dragOver = -1; double acc = 0.0; float last = 0.f;
};
}   // namespace duoui::ui
