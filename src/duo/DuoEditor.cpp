#include "DuoEditor.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>
#include <catui/Logo.h>

using juce::Colour; using juce::Rectangle;
namespace
{
constexpr int kBaseW = 1500, kBaseH = 940, kMinW = 900, kMaxW = 3000, kCheek = 22, kContentW = kBaseW - 2 * kCheek;
const Colour kNavy(0xff161638), kPanel(0xff1d1d46), kDark(0xff0b0b20), kLine(0xff3a3a6a), kDim(0xff9aa0c8), kText(0xffe8e6e1);
const Colour kDeckCol[2] = { Colour(0xffF6A970), Colour(0xff4ED8C8) };

// A button that reports press and release (for cue, brake, roll and the other hold moves).
struct HoldButton : juce::TextButton
{
    std::function<void()> onPress, onRelease, onRight;
    explicit HoldButton(const juce::String& t) : juce::TextButton(t) {}
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; }
        juce::TextButton::mouseDown(e);
    }
    void buttonStateChanged() override
    {
        const bool d = isDown();
        if (d && ! was) { if (onPress) onPress(); }
        if (! d && was) { if (onRelease) onRelease(); }
        was = d;
    }
    bool was = false;
};

class DuoLAF : public catui::CatLookAndFeel
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const Colour& base, bool hl, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        Colour c = b.getToggleState() ? base.brighter(0.1f) : kDark;
        if (down) c = c.brighter(0.3f); else if (hl) c = c.brighter(0.12f);
        g.setColour(c); g.fillRoundedRectangle(r, 5.f);
        g.setColour(b.getToggleState() ? base : kLine); g.drawRoundedRectangle(r, 5.f, 1.2f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
        g.setColour(b.getToggleState() ? Colour(0xff0b0b20) : kText.withAlpha(b.isEnabled() ? 1.f : 0.4f));
        g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(2, 0), juce::Justification::centred, 2);
    }
    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
        g.setColour(kDark); g.fillRoundedRectangle(r, 5.f); g.setColour(kLine); g.drawRoundedRectangle(r, 5.f, 1.2f);
        juce::Path p; const float cx = (float) w - 12.f, cy = (float) h * 0.5f; p.addTriangle(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour(kDim); g.fillPath(p);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(12.5f)); }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override
    {
        const Colour c = s.findColour(juce::Slider::thumbColourId);
        g.setColour(kDark);
        if (st == juce::Slider::LinearVertical)
        {
            const float cx = (float) x + (float) w * 0.5f; g.fillRoundedRectangle(cx - 4.f, (float) y, 8.f, (float) h, 4.f);
            g.setColour(kLine); g.drawRoundedRectangle(cx - 4.f, (float) y, 8.f, (float) h, 4.f, 1.f);
            g.setColour(kLine); for (int i = 0; i <= 8; ++i) g.drawHorizontalLine((int) ((float) y + (float) h * (float) i / 8.f), cx - 12.f, cx - 7.f);
            Rectangle<float> th(cx - 17.f, pos - 9.f, 34.f, 18.f); g.setColour(Colour(0xffc9c9d8)); g.fillRoundedRectangle(th, 3.f);
            g.setColour(c); g.fillRect(th.getX() + 3, th.getCentreY() - 1.f, th.getWidth() - 6, 2.f);
        }
        else
        {
            const float cy = (float) y + (float) h * 0.5f; g.fillRoundedRectangle((float) x, cy - 4.f, (float) w, 8.f, 4.f);
            g.setColour(kLine); g.drawRoundedRectangle((float) x, cy - 4.f, (float) w, 8.f, 4.f, 1.f);
            Rectangle<float> th(pos - 10.f, cy - 12.f, 20.f, 24.f); g.setColour(Colour(0xffc9c9d8)); g.fillRoundedRectangle(th, 3.f);
            g.setColour(c); g.fillRect(th.getCentreX() - 1.f, th.getY() + 3.f, 2.f, th.getHeight() - 6.f);
        }
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.f); const float d = juce::jmin(r.getWidth(), r.getHeight()); r = r.withSizeKeepingCentre(d, d);
        const float ang = a0 + pos * (a1 - a0); const Colour c = s.findColour(juce::Slider::rotarySliderFillColourId);
        g.setColour(kDark); g.fillEllipse(r);
        juce::Path bg; bg.addCentredArc(r.getCentreX(), r.getCentreY(), d * 0.5f - 2, d * 0.5f - 2, 0.f, a0, a1, true);
        g.setColour(kLine); g.strokePath(bg, juce::PathStrokeType(2.5f));
        juce::Path arc; arc.addCentredArc(r.getCentreX(), r.getCentreY(), d * 0.5f - 2, d * 0.5f - 2, 0.f, a0, ang, true);
        g.setColour(c); g.strokePath(arc, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path t; t.addRoundedRectangle(-1.5f, -d * 0.5f + 5.f, 3.f, d * 0.24f, 1.f); g.setColour(kText); g.fillPath(t, juce::AffineTransform::rotation(ang).translated(r.getCentre()));
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
        auto r = getLocalBounds().toFloat().reduced(4.f); const float d = std::min(r.getWidth(), r.getHeight()); r = r.withSizeKeepingCentre(d, d);
        const auto c = r.getCentre(); const Colour col = kDeckCol[deck];
        g.setColour(Colour(0xff0e0e24)); g.fillEllipse(r);
        const double rev = proc.view[deck].pos.load() * duo::Deck::kPlatterRevPerSec; const float ang = (float) (rev - std::floor(rev)) * juce::MathConstants<float>::twoPi;
        for (int i = 0; i < 60; ++i)
        {
            const float a = ang + (float) i * juce::MathConstants<float>::twoPi / 60.f; const float r1 = d * 0.5f - 6.f, r2 = r1 - (i % 5 == 0 ? 14.f : 8.f);
            g.setColour(col.withAlpha(i % 5 == 0 ? 0.95f : 0.55f));
            g.drawLine(c.x + std::sin(a) * r1, c.y - std::cos(a) * r1, c.x + std::sin(a) * r2, c.y - std::cos(a) * r2, i % 5 == 0 ? 3.f : 2.f);
        }
        g.setColour(Colour(0xff2a2a52)); for (int k = 1; k < 5; ++k) g.drawEllipse(r.reduced(d * 0.09f + (float) k * d * 0.045f), 1.f);
        const float hub = d * 0.22f; g.setColour(col); g.fillEllipse(Rectangle<float>(hub * 2, hub * 2).withCentre(c));
        g.setColour(Colour(0xff111111)); g.setFont(juce::FontOptions(14.f, juce::Font::bold)); g.drawText("DECK", Rectangle<float>(hub * 2, 20.f).withCentre(c), juce::Justification::centred);
        g.setColour(juce::Colours::white); g.drawLine(c.x, c.y, c.x + std::sin(ang) * (d * 0.5f - 14.f), c.y - std::cos(ang) * (d * 0.5f - 14.f), 3.f);
        if (proc.view[deck].touching.load()) { g.setColour(col.withAlpha(0.5f)); g.drawEllipse(r, 3.f); }
    }
    void mouseDown(const juce::MouseEvent& e) override { last = ang(e); acc = 0.f; duoui::Cmd c; c.type = duoui::kJog; c.deck = deck; c.i = 1; proc.post(c); startTimerHz(60); }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float a = ang(e); float dlt = a - last; while (dlt > juce::MathConstants<float>::pi) dlt -= juce::MathConstants<float>::twoPi; while (dlt < -juce::MathConstants<float>::pi) dlt += juce::MathConstants<float>::twoPi;
        acc += dlt / juce::MathConstants<float>::twoPi; last = a;
    }
    void mouseUp(const juce::MouseEvent&) override { stopTimer(); duoui::Cmd c; c.type = duoui::kJog; c.deck = deck; c.i = 0; proc.post(c); }
private:
    float ang(const juce::MouseEvent& e) const { const auto c = getLocalBounds().toFloat().getCentre(); return std::atan2(e.position.x - c.x, -(e.position.y - c.y)); }
    void timerCallback() override { duoui::Cmd c; c.type = duoui::kJog; c.deck = deck; c.i = 1; c.a = (double) acc * 60.0; acc = 0.f; proc.post(c); }
    DuoProcessor& proc; int deck; float last = 0.f, acc = 0.f;
};

// Two stacked waveforms on one time axis (real time, so beats line up when the tempos match).
class Waves : public juce::Component
{
public:
    explicit Waves(DuoProcessor& p) : proc(p) {}
    int bars = 8;
    std::shared_ptr<const duo::Track> trk[2];
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(kDark); g.fillRoundedRectangle(r, 7.f);
        double refBpm = std::max(proc.view[0].bpm.load(), proc.view[1].bpm.load()); if (refBpm < 40) refBpm = 120;
        const double windowSec = (double) bars * 4.0 * 60.0 / refBpm;
        const float laneH = (r.getHeight() - 6.f) / 2.f;
        for (int d = 0; d < 2; ++d)
        {
            auto lane = Rectangle<float>(r.getX() + 3, r.getY() + 3 + (float) d * laneH, r.getWidth() - 6, laneH - 2.f);
            paintLane(g, lane, d, windowSec);
        }
        g.setColour(juce::Colours::white); g.drawVerticalLine((int) r.getCentreX(), r.getY() + 3, r.getBottom() - 3);
        // beat alignment hint between the lanes
        const float pa = proc.view[0].phase.load(), pb = proc.view[1].phase.load();
        if (pa >= 0 && pb >= 0)
        {
            float dlt = pa - pb; while (dlt > 0.5f) dlt -= 1.f; while (dlt < -0.5f) dlt += 1.f;
            const bool ok = std::fabs(dlt) < 0.03f; g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
            g.setColour(ok ? catui::SodaPop::mint() : catui::SodaPop::amber()); g.drawText(ok ? "Beats aligned" : "Beat offset " + juce::String(dlt * 100.f, 0) + " %", r.withTrimmedLeft(r.getWidth() - 200).withHeight(16.f).translated(-8.f, r.getHeight() * 0.5f - 8.f), juce::Justification::right);
        }
    }
private:
    void paintLane(juce::Graphics& g, Rectangle<float> r, int d, double windowSec)
    {
        const Colour col = kDeckCol[d]; auto& v = proc.view[d]; auto t = trk[d];
        g.setColour(Colour(0xff10102a)); g.fillRect(r);
        g.setColour(col); g.setFont(juce::FontOptions(12.f, juce::Font::bold));
        g.drawText(juce::String(d == 0 ? "A  " : "B  ") + proc.statusLine(d), r.reduced(6, 2), juce::Justification::topLeft);
        if (t == nullptr || t->peaks.empty())
        {
            g.setColour(kDim); g.drawText(proc.get(d == 0 ? "src_a" : "src_b") > 0.5f ? "DAW input: live. Touch the platter to scratch the last minute." : "Load a track with the button on the deck.", r, juce::Justification::centred);
            return;
        }
        const double pos = v.pos.load(); const double tempo = t->bpm > 0 ? std::max(0.1, v.bpm.load() / t->bpm) : 1.0;
        const double srcPerPx = windowSec / (double) r.getWidth() * tempo;
        const int bins = (int) (t->peaks.size() / 2); const double binSec = 256.0 / t->sr;
        const float cy = r.getCentreY(), hh = r.getHeight() * 0.46f;
        g.setColour(col.withAlpha(0.85f));
        for (int x = 0; x < (int) r.getWidth(); ++x)
        {
            const double t0 = pos + ((double) x - (double) r.getWidth() * 0.5) * srcPerPx; if (t0 < 0) continue;
            int b0 = (int) (t0 / binSec), b1 = (int) ((t0 + srcPerPx) / binSec); if (b0 >= bins) break; b1 = std::min(bins - 1, std::max(b0, b1));
            float mn = 0, mx = 0; for (int b = b0; b <= b1; ++b) { mn = std::min(mn, t->peaks[(size_t) b * 2]); mx = std::max(mx, t->peaks[(size_t) b * 2 + 1]); }
            g.drawVerticalLine((int) r.getX() + x, cy - mx * hh * 1.8f, cy - mn * hh * 1.8f + 1.f);
        }
        auto xOf = [&](double sec) { return r.getX() + (float) ((double) r.getWidth() * 0.5 + (sec - pos) / srcPerPx); };
        if (t->bpm > 0)   // beat grid
        {
            const double per = 60.0 / t->bpm; const double left = pos - (double) r.getWidth() * 0.5 * srcPerPx, right = pos + (double) r.getWidth() * 0.5 * srcPerPx;
            long k0 = (long) std::floor((left - t->firstBeat) / per); g.setColour(juce::Colours::white.withAlpha(0.22f));
            for (long k = k0; t->firstBeat + (double) k * per < right; ++k) { const float x = xOf(t->firstBeat + (double) k * per); if (x >= r.getX() && x <= r.getRight()) g.drawVerticalLine((int) x, r.getY() + 16, r.getBottom()); }
        }
        if (v.loopOn.load() && v.loopA.load() >= 0 && v.loopB.load() > v.loopA.load()) { g.setColour(catui::SodaPop::mint().withAlpha(0.2f)); const float a = xOf(v.loopA.load()), b = xOf(v.loopB.load()); g.fillRect(Rectangle<float>(a, r.getY(), b - a, r.getHeight()).getIntersection(r)); }
        for (int i = 0; i < 8; ++i) { const double h = v.hot[(size_t) i].load(); if (h < 0) continue; const float x = xOf(h); if (x < r.getX() || x > r.getRight()) continue; g.setColour(padColour(i)); g.fillRect(x - 1.f, r.getY(), 2.f, r.getHeight()); g.drawText(juce::String(i + 1), (int) x + 3, (int) r.getY() + 2, 14, 12, juce::Justification::left); }
        const float cx = xOf(v.cue.load()); if (cx >= r.getX() && cx <= r.getRight()) { g.setColour(catui::SodaPop::lemon()); juce::Path tri; tri.addTriangle(cx - 5, r.getBottom(), cx + 5, r.getBottom(), cx, r.getBottom() - 9); g.fillPath(tri); }
    }
public:
    static Colour padColour(int i) { static const Colour c[8] = { Colour(0xffF286A8), Colour(0xffF2D060), Colour(0xff4ED8C8), Colour(0xffB69CF0), Colour(0xffFF9000), Colour(0xff7BD87B), Colour(0xff6FA8FF), Colour(0xffFF5C5C) }; return c[i % 8]; }
private:
    DuoProcessor& proc;
};
}   // namespace

struct DuoEditor::Impl : public juce::Component
{
    DuoEditor& ed; DuoProcessor& proc; DuoLAF laf; catui::RenderGate gate { *this };
    juce::TooltipWindow tips { this, 900 };
    Waves waves { proc }; std::unique_ptr<Jog> jog[2];
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cAtt;
    struct Lbl { juce::String text; Rectangle<int> r; Colour c; bool big; };
    std::vector<Lbl> labels;
    HoldButton* pads[2][8] = {}; HoldButton* playBtn[2] = {}; HoldButton* liveBtn[2] = {};
    juce::TextButton* syncBtn[2] = {};
    juce::ComboBox midiAction; juce::TextButton learnBtn { "Learn" }, clearBtn { "Clear all" }, gearBtn { "gear" }, infoBtn { "i" }, sizeBtn { "100%" };
    juce::ComboBox zoom;
    juce::Array<int> actionIds;
    bool pauseBg = true; std::unique_ptr<juce::FileChooser> chooser;

    Impl(DuoEditor& e, DuoProcessor& p) : ed(e), proc(p)
    {
        setSize(kContentW, kBaseH);
        addAndMakeVisible(waves); waves.setBounds(0, 52, kContentW, 214);
        for (int d = 0; d < 2; ++d) buildDeck(d);
        buildMixer(); buildBottom();
        for (auto* b : { &gearBtn, &infoBtn, &sizeBtn }) addAndMakeVisible(*b);
        sizeBtn.setBounds(kContentW - 62, 8, 56, 28); gearBtn.setBounds(kContentW - 126, 8, 58, 28); infoBtn.setBounds(kContentW - 164, 8, 34, 28);
        sizeBtn.onClick = [this] { ed.setSize(kBaseW, kBaseH); }; gearBtn.onClick = [this] { showMenu(); }; infoBtn.onClick = [this] { showInfo(); };
        sizeBtn.setTooltip("Back to the normal window size (100 %). You can also drag any edge or corner.");
        gearBtn.setTooltip("Settings: hover tips, jog wheel sensitivity for MIDI controllers, pause drawing in the background."); infoBtn.setTooltip("How Scratch Cat Duo works.");
        addAndMakeVisible(zoom); zoom.addItemList({ "4 bars", "8 bars", "16 bars" }, 1); zoom.setSelectedId(2, juce::dontSendNotification); zoom.setBounds(kContentW - 110, 270, 106, 24);
        zoom.onChange = [this] { waves.bars = zoom.getSelectedId() == 1 ? 4 : zoom.getSelectedId() == 2 ? 8 : 16; waves.repaint(); }; zoom.setTooltip("How much of the tracks you see around the playhead.");
        gate.onFrame = [this] { refreshPlay(); waves.trk[0] = proc.trackFor(0); waves.trk[1] = proc.trackFor(1); repaint(); };
        gate.start(30); applyTips();
    }
    ~Impl() override { gate.stop(); for (auto* b : { &gearBtn, &infoBtn, &sizeBtn }) b->setLookAndFeel(nullptr); }

    // ---- builders ----
    juce::Slider* knob(const juce::String& id, const juce::String& name, int x, int y, int sz, Colour col, const juce::String& tip)
    {
        auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, sz + 14, 14); s->setBounds(x, y + 12, sz, sz + 14); s->setColour(juce::Slider::rotarySliderFillColourId, col);
        s->setColour(juce::Slider::textBoxTextColourId, kText); s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack); s->setTooltip(tip);
        s->setDoubleClickReturnValue(true, proc.apvts.getParameter(id)->convertFrom0to1(proc.apvts.getParameter(id)->getDefaultValue()));
        addAndMakeVisible(*s); sAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, *s));
        labels.push_back({ name, { x - 8, y - 1, sz + 16, 13 }, kDim, false });
        auto* r = s.get(); owned.push_back(std::move(s)); return r;
    }
    juce::Slider* linear(const juce::String& id, Rectangle<int> r, bool vertical, Colour col, const juce::String& tip)
    {
        auto s = std::make_unique<juce::Slider>(vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        s->setBounds(r); s->setColour(juce::Slider::thumbColourId, col); s->setTooltip(tip); s->setDoubleClickReturnValue(true, proc.apvts.getParameter(id)->convertFrom0to1(proc.apvts.getParameter(id)->getDefaultValue()));
        addAndMakeVisible(*s); sAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, *s));
        auto* p = s.get(); owned.push_back(std::move(s)); return p;
    }
    juce::TextButton* toggle(const juce::String& id, const juce::String& text, Rectangle<int> r, Colour col, const juce::String& tip)
    {
        auto b = std::make_unique<juce::TextButton>(text); b->setClickingTogglesState(true); b->setBounds(r); b->setColour(juce::TextButton::buttonColourId, col); b->setColour(juce::TextButton::buttonOnColourId, col); b->setTooltip(tip);
        addAndMakeVisible(*b); bAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, id, *b));
        auto* p = b.get(); owned.push_back(std::move(b)); return p;
    }
    juce::ComboBox* choice(const juce::String& id, Rectangle<int> r, const juce::String& tip)
    {
        auto c = std::make_unique<juce::ComboBox>(); c->setBounds(r); c->setTooltip(tip);
        if (auto* pc = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) c->addItemList(pc->choices, 1);
        addAndMakeVisible(*c); cAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, id, *c));
        auto* p = c.get(); owned.push_back(std::move(c)); return p;
    }
    HoldButton* hold(const juce::String& text, Rectangle<int> r, Colour col, const juce::String& tip, std::function<void()> down, std::function<void()> up = {})
    {
        auto b = std::make_unique<HoldButton>(text); b->setBounds(r); b->setColour(juce::TextButton::buttonColourId, col); b->setTooltip(tip); b->onPress = std::move(down); b->onRelease = std::move(up);
        addAndMakeVisible(*b); auto* p = b.get(); owned.push_back(std::move(b)); return p;
    }
    void cmd(int type, int deck, int i = 0, double a = 0.0) { duoui::Cmd c; c.type = type; c.deck = deck; c.i = i; c.a = a; proc.post(c); }

    void buildDeck(int d)
    {
        const int ox = d == 0 ? 0 : 836, oy = 292; const Colour col = kDeckCol[d]; const juce::String s = d == 0 ? "a" : "b"; const juce::String D = d == 0 ? "A" : "B";
        labels.push_back({ "DECK " + D, { ox, oy, 80, 26 }, col, true });
        auto* load = hold("Load track", { ox + 96, oy, 92, 26 }, col, "Load a track on this deck (wav, aiff, flac, ogg, mp3). It is analysed for BPM, beat grid and key.", [this, d] { chooseFile(d); });
        (void) load;
        choice("src_" + s, { ox + 428, oy, 100, 26 }, "Where the deck gets its sound. File: the loaded track. DAW input: audio from the host (the plugin's input for this deck); touch the platter to scratch the last minute of it.");
        hold("Re-analyse", { ox + 534, oy, 86, 26 }, col, "Analyse the loaded track again for BPM, beat grid and key.", [this, d] { auto t = proc.trackFor(d); (void) t; reanalyse(d); });
        jog[d] = std::make_unique<Jog>(proc, d); addAndMakeVisible(*jog[d]); jog[d]->setBounds(ox + 0, oy + 40, 284, 284);
        jog[d]->setTooltip("Platter. Drag to scratch: it follows your hand forward and backward and holds when you stop.");
        linear("pitch_" + s, { ox + 292, oy + 44, 48, 268 }, true, col, "Tempo fader. Up is faster, down is slower. Double click: back to 0.");
        choice("prange_" + s, { ox + 292, oy + 318, 52, 24 }, "Range of the tempo fader.");
        labels.push_back({ "TEMPO", { ox + 288, oy + 30, 56, 12 }, kDim, false });
        const int cx = ox + 352;
        toggle("sync_" + s, "Sync", { cx, oy + 44, 86, 28 }, col, "Match this deck's tempo to the DAW tempo (or to the other deck when Tempo is set to Decks lead). Beat position stays yours: nudge or scratch to line up.");
        toggle("keylock_" + s, "Key lock", { cx + 91, oy + 44, 86, 28 }, col, "Change the tempo without changing the pitch.");
        toggle("quant_" + s, "Quant", { cx + 182, oy + 44, 86, 28 }, col, "Snap cue points and loops to the beat grid.");
        hold("Loop in", { cx, oy + 78, 86, 26 }, col, "Start of a loop at the playhead.", [this, d] { cmd(duoui::kLoopIn, d); });
        hold("Loop out", { cx + 91, oy + 78, 86, 26 }, col, "End of the loop. The loop starts playing.", [this, d] { cmd(duoui::kLoopOut, d); });
        hold("Exit", { cx + 182, oy + 78, 86, 26 }, col, "Leave the loop.", [this, d] { cmd(duoui::kLoopExit, d); });
        static const char* bl[6] = { "1/4", "1/2", "1", "2", "4", "8" }; static const double bv[6] = { 0.25, 0.5, 1, 2, 4, 8 };
        for (int i = 0; i < 6; ++i) hold(bl[i], { cx + i * 45, oy + 110, 42, 26 }, col, juce::String("Beat loop of ") + bl[i] + " beat(s) from the playhead.", [this, d, i] { cmd(duoui::kBeatLoop, d, 0, bv[i]); });
        hold("Brake", { cx, oy + 142, 132, 26 }, col, "Slow to a stop like a turntable being switched off.", [this, d] { cmd(duoui::kBrake, d); });
        hold("Backspin", { cx + 136, oy + 142, 132, 26 }, col, "Spin the record backwards and stop.", [this, d] { cmd(duoui::kBackspin, d); });
        static const char* mv[4] = { "Roll", "Censor", "Stutter", "Trans" }; static const char* mt[4] = {
            "Hold: loops a short slice (1/4 beat) while the track keeps running underneath, then jumps back in time.", "Hold: plays backward while the track keeps running underneath, then jumps back in time.",
            "Hold: retriggers from the cue point every 1/4 beat.", "Hold: chops the sound on and off every 1/8 beat." };
        static const duo::Move mm[4] = { duo::Move::Roll, duo::Move::Censor, duo::Move::Stutter, duo::Move::Transform };
        for (int i = 0; i < 4; ++i) hold(mv[i], { cx + i * 68, oy + 174, 64, 26 }, col, mt[i], [this, d, i] { cmd(duoui::kMoveStart, d, (int) mm[i], 0.25); }, [this, d] { cmd(duoui::kMoveEnd, d); });
        hold("Nudge -", { cx, oy + 206, 86, 26 }, col, "Hold: slows the deck a little to line up the beats.", [this, d] { cmd(duoui::kBend, d, 0, -0.6); }, [this, d] { cmd(duoui::kBend, d, 0, 0.0); });
        hold("Nudge +", { cx + 90, oy + 206, 86, 26 }, col, "Hold: speeds the deck up a little to line up the beats.", [this, d] { cmd(duoui::kBend, d, 0, 0.6); }, [this, d] { cmd(duoui::kBend, d, 0, 0.0); });
        hold("Align", { cx + 180, oy + 206, 88, 26 }, col, "Moves this deck by up to half a beat so its beat lands on the other deck. Tempo is not changed: use Sync for that.", [this, d] { cmd(duoui::kAlign, d); });
        hold("CUE", { cx, oy + 246, 132, 64 }, catui::SodaPop::lemon(), "Cue. Playing: jump back to the cue point and stop. Stopped: set the cue point here. Hold on the cue point to preview.", [this, d] { cmd(duoui::kCueDown, d); }, [this, d] { cmd(duoui::kCueUp, d); });
        playBtn[d] = hold("PLAY", { cx + 136, oy + 246, 132, 64 }, catui::SodaPop::mint(), "Play or pause.", [this, d] { cmd(duoui::kPlay, d, 2); });
        liveBtn[d] = hold("Go live", { ox + 100, oy + 330, 90, 26 }, col, "DAW input only: back to the live sound.", [this, d] { cmd(duoui::kGoLive, d); });
        for (int i = 0; i < 8; ++i)
        {
            auto* b = hold(juce::String(i + 1), { ox + (i % 4) * 156, oy + 372 + (i / 4) * 54, 152, 48 }, Waves::padColour(i), "Hot cue " + juce::String(i + 1) + ". Empty: sets it at the playhead. Set: jumps there. Right click: clears it.", [this, d, i] { cmd(duoui::kHot, d, i); });
            b->onRight = [this, d, i] { cmd(duoui::kHotClear, d, i); }; pads[d][i] = b;
        }
        labels.push_back({ "HOT CUES", { ox, oy + 358, 100, 12 }, kDim, false });
    }

    void buildMixer()
    {
        const int mx = 632, oy = 292;
        labels.push_back({ "MIXER", { mx, oy, 192, 22 }, kText, true });
        knob("master", "MASTER", mx + 4, oy + 24, 56, catui::SodaPop::hotPink(), "Master level.");
        auto* cv = choice("xcurve", { mx + 76, oy + 30, 110, 26 }, "Crossfader curve. Smooth: gentle linear blend. Sharp: both decks at full level, then a quick cut near each end. Const: constant power blend.");
        (void) cv; labels.push_back({ "CURVE", { mx + 76, oy + 16, 110, 12 }, kDim, false });
        for (int d = 0; d < 2; ++d)
        {
            const int x = mx + 4 + d * 94; const Colour col = kDeckCol[d]; const juce::String s = d == 0 ? "a" : "b";
            labels.push_back({ d == 0 ? "A" : "B", { x, oy + 86, 86, 14 }, col, true });
            knob("trim_" + s, "TRIM", x + 14, oy + 104, 50, col, "Input gain of this channel.");
            knob("hi_" + s, "HI", x + 14, oy + 160, 50, col, "High EQ. All the way down kills the highs.");
            knob("mid_" + s, "MID", x + 14, oy + 216, 50, col, "Mid EQ. All the way down kills the mids.");
            knob("lo_" + s, "LOW", x + 14, oy + 272, 50, col, "Low EQ. All the way down kills the lows.");
            knob("col_" + s, "COLOUR", x + 14, oy + 328, 50, col, "Colour filter. Left: low pass. Right: high pass. Centre: off.");
            toggle("cue_" + s, "CUE", { x + 8, oy + 394, 70, 24 }, catui::SodaPop::lemon(), "Send this channel to the Cue output (headphones).");
            linear("fader_" + s, { x + 18, oy + 424, 40, 64 }, true, col, "Channel fader.");
        }
        linear("xfade", { mx + 12, oy + 482, 168, 26 }, false, catui::SodaPop::hotPink(), "Crossfader. Left is deck A, right is deck B.");
    }

    void buildBottom()
    {
        const int y = 806;
        labels.push_back({ "DAW ROUTING AND MIDI", { 0, y, 300, 16 }, kText, true });
        choice("outmode", { 0, y + 38, 170, 26 }, "Outputs. Master only: one stereo output. A, B and Master: each deck also gets its own output so you can route it to its own DAW channel (enable the extra outputs in your host).");
        labels.push_back({ "OUTPUTS", { 0, y + 22, 100, 12 }, kDim, false });
        choice("tempomode", { 178, y + 38, 150, 26 }, "Tempo. Follow DAW: Sync matches the host tempo. Decks lead: Sync matches the other deck.");
        labels.push_back({ "TEMPO", { 178, y + 22, 100, 12 }, kDim, false });
        knob("cuemix", "CUE MIX", 346, y + 18, 46, catui::SodaPop::lemon(), "Cue output: cue only (left) to master mix (right).");
        labels.push_back({ "Inputs: Deck A in and Deck B in are the plugin's audio inputs (route a DAW track to them as a sidechain). Set a deck to DAW input to scratch it.", { 410, y + 22, 300, 44 }, kDim, false });
        actionIds = duoui::allActions(); int id = 1; for (int a : actionIds) midiAction.addItem(duoui::actionName(a), id++);
        midiAction.setSelectedId(1, juce::dontSendNotification); addAndMakeVisible(midiAction); midiAction.setBounds(0, y + 98, 300, 26);
        midiAction.setTooltip("Pick what you want to control, press Learn, then move or press the control on your MIDI controller.");
        addAndMakeVisible(learnBtn); learnBtn.setBounds(308, y + 98, 80, 26); learnBtn.setTooltip("Then move or press the control on your controller.");
        learnBtn.onClick = [this] { const int i = midiAction.getSelectedId() - 1; if (i >= 0 && i < actionIds.size()) proc.learnAction.store(actionIds[i]); };
        addAndMakeVisible(clearBtn); clearBtn.setBounds(394, y + 98, 90, 26); clearBtn.setTooltip("Remove every MIDI mapping."); clearBtn.onClick = [this] { proc.clearMidiMap(); };
        labels.push_back({ "MIDI LEARN", { 0, y + 80, 200, 14 }, kDim, false });
        // FX chain
        const int fx = 740; labels.push_back({ "FX CHAIN", { fx, y, 120, 16 }, kText, true });
        choice("fxtarget", { fx + 100, y - 4, 110, 24 }, "Which sound the three effects work on."); knob("chain", "CHAIN MIX", fx + 640, y - 2, 40, catui::SodaPop::periwinkle(), "Overall amount of the FX chain.");
        for (int k = 1; k <= 3; ++k)
        {
            const int x = fx + (k - 1) * 240; const juce::String n(k); const Colour c = k == 1 ? catui::SodaPop::aqua() : k == 2 ? catui::SodaPop::lilac() : catui::SodaPop::bubblegum();
            choice("fxt" + n, { x, y + 28, 130, 26 }, "Effect type for slot " + n + ".");
            toggle("fxon" + n, "On", { x + 136, y + 28, 54, 26 }, c, "Switch this effect on or off.");
            knob("fxp1_" + n, k == 1 ? "AMOUNT" : "AMOUNT", x + 4, y + 62, 46, c, "Main control of the effect: filter cutoff, echo feedback, reverb size, flanger depth, crush amount.");
            knob("fxp2_" + n, "DRY/WET", x + 70, y + 62, 46, c, "How much of the effect you hear.");
        }
        choice("echobeats", { fx + 480, y + 30, 70, 24 }, "Echo time in beats."); labels.push_back({ "ECHO TIME", { fx + 480, y + 16, 80, 12 }, kDim, false });
    }

    // ---- actions ----
    void chooseFile(int d)
    {
        chooser = std::make_unique<juce::FileChooser>("Load a track on deck " + juce::String(d == 0 ? "A" : "B"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, d](const juce::FileChooser& fc)
        { auto f = fc.getResult(); if (f.existsAsFile()) { proc.setParam(d == 0 ? "src_a" : "src_b", 0.f); proc.loadFile(d, f); } });
    }
    void reanalyse(int d)
    {
        const juce::String key = d == 0 ? "fileA" : "fileB"; juce::File f(proc.apvts.state.getProperty(key, "").toString());
        if (f.existsAsFile()) proc.loadFile(d, f);
    }
    void refreshPlay()
    {
        for (int d = 0; d < 2; ++d)
        {
            const bool pl = proc.view[d].playing.load();
            if (playBtn[d] != nullptr) { playBtn[d]->setButtonText(pl ? "PAUSE" : "PLAY"); playBtn[d]->setToggleState(pl, juce::dontSendNotification); }
            const bool live = proc.get(d == 0 ? "src_a" : "src_b") > 0.5f; if (liveBtn[d] != nullptr) { liveBtn[d]->setEnabled(live); liveBtn[d]->setToggleState(live && proc.view[d].live.load(), juce::dontSendNotification); }
            for (int i = 0; i < 8; ++i) if (pads[d][i] != nullptr)
            {
                const double h = proc.view[d].hot[(size_t) i].load();
                pads[d][i]->setToggleState(h >= 0, juce::dontSendNotification);
                const auto t = juce::String(i + 1) + (h >= 0 ? "   " + juce::String((int) h / 60) + ":" + juce::String((int) h % 60).paddedLeft('0', 2) : juce::String());
                if (pads[d][i]->getButtonText() != t) pads[d][i]->setButtonText(t);
            }
        }
        learnBtn.setToggleState(proc.learnAction.load() >= 0, juce::dontSendNotification);
        if (proc.learnedNew.exchange(false)) learnBtn.setToggleState(false, juce::dontSendNotification);
    }
    void showInfo()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Scratch Cat Duo",
            "Two decks and a mixer in one plugin.\n\nLoad a track on each deck (it is analysed for BPM, beat grid and key), or set a deck to DAW input and send a DAW track to the plugin input.\n\n"
            "Drag the platter to scratch. Sync matches the tempo to the DAW (or the other deck). Hot cues, loops, brake, backspin, roll, censor, stutter and transform are on each deck.\n\n"
            "Mixer: trim, 3 band EQ with kill, colour filter, channel fader, crossfader with three curves. FX chain: three effects on deck A, deck B, A+B or the master.\n\n"
            "MIDI: pick a control, press Learn, move the control on your controller.");
    }
    void showMenu()
    {
        juce::PopupMenu m, js;
        m.addItem(1, "Hover tips", true, proc.tipsOn());
        m.addItem(2, "Pause drawing while in the background", true, pauseBg);
        for (int t : { 64, 128, 256, 512, 1024 }) js.addItem(100 + t, juce::String(t) + " ticks per turn", true, (int) proc.jogTicksPerTurn.load() == t);
        m.addSubMenu("MIDI jog wheel sensitivity", js);
        m.addSeparator(); m.addItem(4, "Reset window size (100%)"); m.addSeparator(); m.addItem(6, "About Scratch Cat Duo");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&gearBtn), [this](int r)
        {
            if (r == 1) { proc.setTipsOn(! proc.tipsOn()); applyTips(); }
            else if (r == 2) { pauseBg = ! pauseBg; gate.setPauseInBackground(pauseBg); }
            else if (r >= 164 && r <= 1124) proc.jogTicksPerTurn.store((float) (r - 100));
            else if (r == 4) ed.setSize(kBaseW, kBaseH);
            else if (r == 6) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Scratch Cat Duo",
                "Scratch Cat Duo by Studio 87.\nFree and open source (AGPL-3.0-or-later).\nIt collects no data and never touches the network.\nhttps://github.com/SpeakingColors/scratchcatduo-vst");
        });
    }
    void applyTips()
    {
        const bool on = proc.tipsOn(); tips.setMillisecondsBeforeTipAppears(on ? 900 : 100000000);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(kNavy);
        g.setColour(kText); g.setFont(juce::FontOptions(24.f, juce::Font::bold)); g.drawText("SCRATCH CAT DUO", 58, 4, 400, 30, juce::Justification::centredLeft);
        g.setColour(kDim); g.setFont(juce::FontOptions(12.f)); g.drawText("two decks + mixer inside your DAW", 58, 30, 400, 16, juce::Justification::centredLeft);
        catui::drawCatHead(g, { 8.f, 6.f, 42.f, 38.f }, kDeckCol[0]);
        juce::String t = proc.get("tempomode") < 0.5f ? "Tempo: follows the DAW" : "Tempo: decks lead";
        g.setColour(kDim); g.setFont(juce::FontOptions(12.5f)); g.drawText(t, kContentW - 420, 12, 250, 22, juce::Justification::centredRight);
        // panels
        g.setColour(kPanel); g.fillRoundedRectangle(0.f, 284.f, 626.f, 514.f, 8.f); g.fillRoundedRectangle(828.f, 284.f, 628.f, 514.f, 8.f); g.fillRoundedRectangle(628.f, 284.f, 200.f, 514.f, 8.f);
        g.fillRoundedRectangle(0.f, 800.f, 722.f, 136.f, 8.f); g.fillRoundedRectangle(730.f, 800.f, 726.f, 136.f, 8.f);
        // deck info
        for (int d = 0; d < 2; ++d)
        {
            const int ox = d == 0 ? 0 : 836; auto& v = proc.view[d]; g.setColour(kDeckCol[d]); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
            auto tk = waves.trk[d]; juce::String s;
            if (tk) { s = juce::String(v.bpm.load(), 1) + " BPM"; if (tk->key.tonic >= 0) s += "  " + juce::String(duo::camelot(tk->key)) + " " + juce::String(duo::keyName(tk->key)); }
            g.drawText(s, ox + 196, 292, 228, 26, juce::Justification::centredLeft);
        }
        for (auto& l : labels) { g.setColour(l.c); g.setFont(juce::FontOptions(l.big ? 13.f : 10.5f, l.big ? juce::Font::bold : juce::Font::plain)); g.drawFittedText(l.text, l.r, juce::Justification::centredLeft, 3); }
        // meters
        auto bar = [&](float v, Rectangle<float> r, Colour c) { g.setColour(kDark); g.fillRect(r); const float db = juce::jlimit(-60.f, 6.f, 20.f * std::log10(std::max(1e-4f, v))); const float h = r.getHeight() * (db + 60.f) / 66.f; g.setColour(db > -1.f ? juce::Colour(0xffff4060) : c); g.fillRect(r.withTop(r.getBottom() - h)); };
        bar(proc.engine.peakA.load(), { 632.f + 4 + 70, 292.f + 424, 8.f, 64.f }, kDeckCol[0]); bar(proc.engine.peakB.load(), { 632.f + 98 + 70, 292.f + 424, 8.f, 64.f }, kDeckCol[1]);
        bar(proc.engine.peakM.load(), { 632.f + 66, 292.f + 24, 8.f, 56.f }, catui::SodaPop::hotPink());
        g.setColour(kDim); g.setFont(juce::FontOptions(11.5f)); g.drawText(proc.learnStatus(), 490, 806 + 98, 220, 26, juce::Justification::centredLeft);
    }
};

DuoEditor::DuoEditor(DuoProcessor& p) : AudioProcessorEditor(&p)
{
    impl = std::make_unique<Impl>(*this, p);
    setLookAndFeel(&impl->laf); addAndMakeVisible(*impl); impl->sendLookAndFeelChange();
    setResizable(true, true);
    setResizeLimits(kMinW, (int) std::lround((double) kMinW * kBaseH / kBaseW), kMaxW, (int) std::lround((double) kMaxW * kBaseH / kBaseW));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio((double) kBaseW / (double) kBaseH);
    const int w = juce::jlimit(kMinW, kMaxW, p.getProp("uiw", kBaseW));
    setSize(w, (int) std::lround((double) w * kBaseH / kBaseW));
}
DuoEditor::~DuoEditor() { setLookAndFeel(nullptr); }
void DuoEditor::paint(juce::Graphics& g)
{
    g.fillAll(catui::Colours::panel());
    catui::drawWoodCheeks(g, getLocalBounds(), (int) std::lround(kCheek * (double) getWidth() / kBaseW));
}
void DuoEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int ck = (int) std::lround(kCheek * (double) getWidth() / kBaseW);
    if (e.x < ck || e.x >= getWidth() - ck) setSize(kBaseW, kBaseH);
}
void DuoEditor::resized()
{
    if (impl == nullptr) return;
    const float s = (float) getWidth() / (float) kBaseW;
    impl->setBounds(0, 0, kContentW, kBaseH);
    impl->setTransform(juce::AffineTransform::scale(s).translated((float) kCheek * s, 0.f));
    if (auto* pr = dynamic_cast<DuoProcessor*>(&processor)) pr->setProp("uiw", getWidth());
}
