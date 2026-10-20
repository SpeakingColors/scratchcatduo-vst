// Scratch Cat Duo: look and small widgets for the editor. Colours and shapes follow the approved mockup (docs/mockup/scratchcatduo.html).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <catui/CatLookAndFeel.h>
#include <functional>

namespace duoui::ui
{
using juce::Colour; using juce::Rectangle;
inline const Colour kInk(0xfff4f5f7), kDim(0xffa9adb8), kGround(0xff0b0b0d), kPanel(0xff1f2125), kPanel2(0xff1c1d22), kLine(0xff2b2d34), kKnob(0xff23252c),
                    kField(0xff0b0c0f), kBtn(0xff35373e), kAccent(0xff4fd1c5), kOrange(0xffff8a3d), kDisp(0xff05070a), kYellow(0xffffd100), kGreen(0xff7bd87b), kPink(0xffff1383), kViolet(0xffa43dff);
inline Colour deckCol(int d) { return d == 0 ? kOrange : kAccent; }
inline Colour padColour(int i) { static const Colour c[8] = { Colour(0xfff28ba8), Colour(0xfff2d060), Colour(0xff4fd1c5), Colour(0xffb69cf0), Colour(0xffff9000), Colour(0xff7bd87b), Colour(0xff6fa8ff), Colour(0xffff5c5c) }; return c[((i % 8) + 8) % 8]; }
inline juce::Font f(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }
inline juce::Font mono(float h) { return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain)); }
inline juce::Font caps(float h, float kern = 0.12f) { auto x = f(h, true); x.setExtraKerningFactor(kern); return x; }

class DuoLAF : public catui::CatLookAndFeel
{
public:
    DuoLAF()
    {
        setColour(juce::PopupMenu::backgroundColourId, kPanel2); setColour(juce::PopupMenu::textColourId, kInk);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent); setColour(juce::PopupMenu::highlightedTextColourId, Colour(0xff05070a));
        setColour(juce::ComboBox::textColourId, kInk); setColour(juce::TextEditor::backgroundColourId, kField); setColour(juce::TextEditor::textColourId, kInk);
        setColour(juce::TextEditor::outlineColourId, kLine); setColour(juce::TextEditor::focusedOutlineColourId, kAccent);
        setColour(juce::AlertWindow::backgroundColourId, kPanel2); setColour(juce::AlertWindow::textColourId, kInk); setColour(juce::AlertWindow::outlineColourId, kLine);
        setColour(juce::TooltipWindow::backgroundColourId, Colour(0xff05070a)); setColour(juce::TooltipWindow::textColourId, kInk); setColour(juce::TooltipWindow::outlineColourId, kAccent);
        setColour(juce::ScrollBar::thumbColourId, Colour(0xff4a4d57));
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const Colour& base, bool hl, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        const bool on = b.getToggleState();
        Colour c = on ? b.findColour(juce::TextButton::buttonOnColourId) : kBtn;
        if (! b.isEnabled()) c = c.withMultipliedAlpha(0.5f);
        if (down) c = c.brighter(0.25f); else if (hl) c = c.brighter(0.1f);
        (void) base; g.setColour(c); g.fillRoundedRectangle(r, 4.f);
        g.setColour(on ? c.brighter(0.2f) : kLine); g.drawRoundedRectangle(r, 4.f, 1.f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont(f(juce::jmin(15.f, (float) b.getHeight() * 0.5f), true));
        g.setColour((b.getToggleState() ? Colour(0xff05070a) : kInk).withAlpha(b.isEnabled() ? 1.f : 0.4f));
        g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(2, 0), juce::Justification::centred, 1);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int h) override { return f(juce::jmin(15.f, (float) h * 0.5f), true); }
    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
        g.setColour(kField); g.fillRoundedRectangle(r, 4.f); g.setColour(kLine); g.drawRoundedRectangle(r, 4.f, 1.f);
        juce::Path p; const float cx = (float) w - 14.f, cy = (float) h * 0.5f; p.addTriangle(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour(kDim); g.fillPath(p);
    }
    void positionComboBoxText(juce::ComboBox& c, juce::Label& l) override { l.setBounds(8, 1, c.getWidth() - 28, c.getHeight() - 2); l.setFont(f(juce::jmin(15.f, (float) c.getHeight() * 0.5f))); }
    juce::Font getPopupMenuFont() override { return f(15.f); }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override
    {
        const Colour c = s.findColour(juce::Slider::thumbColourId);
        if (st == juce::Slider::LinearVertical)
        {
            const float cx = (float) x + (float) w * 0.5f;
            g.setColour(kField); g.fillRoundedRectangle(cx - 4.f, (float) y, 8.f, (float) h, 4.f); g.setColour(kLine); g.drawRoundedRectangle(cx - 4.f, (float) y, 8.f, (float) h, 4.f, 1.f);
            g.setColour(Colour(0xff3b3e47)); for (int i = 0; i <= 10; ++i) g.drawHorizontalLine((int) ((float) y + (float) h * (float) i / 10.f), cx - 14.f, cx - 8.f);
            Rectangle<float> th(cx - 19.f, pos - 13.f, 38.f, 26.f); g.setColour(Colour(0xffd8d6d1)); g.fillRoundedRectangle(th, 3.f);
            g.setColour(c); g.fillRect(th.getX() + 4, th.getCentreY() - 1.5f, th.getWidth() - 8, 3.f);
        }
        else
        {
            const float cy = (float) y + (float) h * 0.5f; g.setColour(kField); g.fillRoundedRectangle((float) x, cy - 4.f, (float) w, 8.f, 4.f);
            g.setColour(kLine); g.drawRoundedRectangle((float) x, cy - 4.f, (float) w, 8.f, 4.f, 1.f);
            Rectangle<float> th(pos - 12.f, cy - 14.f, 24.f, 28.f); g.setColour(Colour(0xffd8d6d1)); g.fillRoundedRectangle(th, 3.f);
            g.setColour(c); g.fillRect(th.getCentreX() - 1.5f, th.getY() + 4.f, 3.f, th.getHeight() - 8.f);
        }
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(2.f); const float d = juce::jmin(r.getWidth(), r.getHeight()); r = r.withSizeKeepingCentre(d, d);
        const float ang = a0 + pos * (a1 - a0); const Colour arcCol = s.findColour(juce::Slider::rotarySliderFillColourId);
        g.setColour(kKnob); g.fillEllipse(r);
        g.setColour(Colour(0xff3b3e47)); g.drawEllipse(r, 1.5f);
        if (arcCol.getAlpha() > 0)
        {
            juce::Path arc; arc.addCentredArc(r.getCentreX(), r.getCentreY(), d * 0.5f, d * 0.5f, 0.f, a0, ang, true);
            g.setColour(arcCol); g.strokePath(arc, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        juce::Path t; t.addRoundedRectangle(-1.4f, -d * 0.5f + 3.f, 2.8f, d * 0.32f, 1.f);
        g.setColour(kInk); g.fillPath(t, juce::AffineTransform::rotation(ang).translated(r.getCentre()));
    }
};

// A button that reports press and release (cue, brake, roll and the other hold moves), and a right click.
struct HoldButton : juce::TextButton
{
    std::function<void()> onPress, onRelease, onRight;
    explicit HoldButton(const juce::String& t = {}) : juce::TextButton(t) {}
    void mouseDown(const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; } juce::TextButton::mouseDown(e); }
    void buttonStateChanged() override
    {
        const bool d = isDown();
        if (d && ! was) { if (onPress) onPress(); }
        if (! d && was) { if (onRelease) onRelease(); }
        was = d;
    }
    bool was = false;
};

// Segmented control: a row of options in one rounded box. Each option can be a toggle or a plain click.
class Seg : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct Opt { juce::String text; juce::String tip; bool on = false; };
    std::vector<Opt> opts; Colour onCol = kAccent;
    std::function<void(int)> onClick;           // option index
    std::function<void(int, bool)> onHold;      // press (true) and release (false) for hold style buttons
    bool holdStyle = false; float fontH = 15.f; std::vector<float> weights;
    void set(int i, bool on) { if (i >= 0 && i < (int) opts.size() && opts[(size_t) i].on != on) { opts[(size_t) i].on = on; repaint(); } }
    void setOnly(int i) { for (int k = 0; k < (int) opts.size(); ++k) set(k, k == i); }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(kField); g.fillRoundedRectangle(r, 4.f); g.setColour(kLine); g.drawRoundedRectangle(r, 4.f, 1.f);
        for (int i = 0; i < (int) opts.size(); ++i)
        {
            auto b = cell(i).toFloat();
            if (opts[(size_t) i].on) { g.setColour(onCol); g.fillRoundedRectangle(b.reduced(1.5f), 3.f); }
            else if (i == hot || i == down) { g.setColour(Colour(0xff2a2c33)); g.fillRoundedRectangle(b.reduced(1.5f), 3.f); }
            if (i > 0 && ! opts[(size_t) i].on && ! opts[(size_t) i - 1].on) { g.setColour(kLine); g.drawVerticalLine((int) b.getX(), b.getY() + 6, b.getBottom() - 6); }
            g.setFont(f(fontH, true)); g.setColour(opts[(size_t) i].on ? Colour(0xff05070a) : kInk); g.drawFittedText(opts[(size_t) i].text, b.toNearestInt().reduced(2, 0), juce::Justification::centred, 1);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override { const int i = at(e.getPosition()); if (i != hot) { hot = i; setTooltip(i >= 0 ? opts[(size_t) i].tip : juce::String()); repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hot = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override { down = at(e.getPosition()); if (down >= 0 && holdStyle && onHold) onHold(down, true); repaint(); }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const int i = down; down = -1; repaint(); if (i < 0) return;
        if (holdStyle) { if (onHold) onHold(i, false); return; }
        if (at(e.getPosition()) == i && onClick) onClick(i);
    }
    Rectangle<int> cell(int i) const
    {
        float tot = 0; for (size_t k = 0; k < opts.size(); ++k) tot += k < weights.size() ? weights[k] : 1.f;
        float x0 = 0; for (int k = 0; k < i; ++k) x0 += (size_t) k < weights.size() ? weights[(size_t) k] : 1.f;
        const float wi = (size_t) i < weights.size() ? weights[(size_t) i] : 1.f; const float W = (float) getWidth();
        const int a = (int) std::lround(x0 / tot * W), b = (int) std::lround((x0 + wi) / tot * W);
        return { a, 0, b - a, getHeight() };
    }
    int at(juce::Point<int> p) const { for (int i = 0; i < (int) opts.size(); ++i) if (cell(i).contains(p)) return i; return -1; }
private:
    int hot = -1, down = -1;
};

// A knob with its name and value written under it, like the mockup.
class KnobBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String label; std::function<juce::String()> valueText; float knobPx = 35.f; Colour labelCol = kDim, valueCol = kAccent;
    KnobBox() { addAndMakeVisible(s); s.onValueChange = [this] { repaint(); }; }
    void tick() { if (valueText) { auto t = valueText(); if (t != last) { last = t; repaint(); } } }
    void resized() override { s.setBounds(Rectangle<int>((int) knobPx, (int) knobPx).withCentre({ getWidth() / 2, (int) knobPx / 2 })); }
    void paint(juce::Graphics& g) override
    {
        const int y0 = (int) knobPx + 1;
        g.setColour(labelCol); g.setFont(caps(12.f, 0.14f)); g.drawText(label, 0, y0, getWidth(), 14, juce::Justification::centred);
        g.setColour(valueCol); g.setFont(mono(13.f)); g.drawText(valueText ? valueText() : s.getTextFromValue(s.getValue()), 0, y0 + 14, getWidth(), 16, juce::Justification::centred);
    }
private:
    juce::String last;
};

// Level meter: dark bar with a bright fill. Values are linear peaks; the scale is in dB.
class Meter : public juce::Component
{
public:
    std::function<float()> peak, peak2; float topDb = 0.f, botDb = -48.f; bool scale = false, twin = false; Colour col = kAccent; std::vector<int> marks;
    float v1 = 0.f, v2 = 0.f;
    void tick()
    {
        const float a = peak ? peak() : 0.f, b = peak2 ? peak2() : a;
        if (std::fabs(a - v1) > 1e-4f || std::fabs(b - v2) > 1e-4f) { v1 = a; v2 = b; repaint(); }
    }
    float yFor(float db) const { return (float) getHeight() * (topDb - juce::jlimit(botDb, topDb, db)) / (topDb - botDb); }
    void paint(juce::Graphics& g) override
    {
        const float barW = twin ? (float) getWidth() * 0.5f - 1.f : (float) getWidth();
        auto bar = [&](float x, float w, float v)
        {
            g.setColour(kDisp); g.fillRect(x, 0.f, w, (float) getHeight());
            const float db = 20.f * std::log10(std::max(1e-5f, v)); const float y = yFor(db);
            g.setColour(col); g.fillRect(x, y, w, (float) getHeight() - y);
            if (db > -3.f) { g.setColour(Colour(0xffff4060)); g.fillRect(x, y, w, std::max(0.f, yFor(-3.f) - y)); }
        };
        if (twin) { bar(0.f, barW, v1); bar(barW + 2.f, barW, v2); } else bar(0.f, barW, v1);
        if (scale)
        {
            g.setFont(mono(9.f));
            for (int db : marks)
            {
                const float y = yFor((float) db); g.setColour(Colour(0x44ffffff)); g.drawHorizontalLine((int) y, 0.f, (float) getWidth());
                g.setColour(Colour(0xaaa9adb8)); g.drawText(juce::String(db), twin ? Rectangle<float>(0.f, y - 11.f, (float) getWidth(), 11.f) : Rectangle<float>(0.f, y - 11.f, (float) getWidth() - 1.f, 11.f), twin ? juce::Justification::centred : juce::Justification::centredRight);
            }
        }
    }
};

// Plain styled text, positioned in design pixels. Does not take clicks.
struct Label : juce::Component
{
    juce::String text; juce::Font font = f(13.f); Colour col = kDim; juce::Justification just = juce::Justification::centredLeft; int lines = 1;
    Label() { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override { g.setColour(col); g.setFont(font); g.drawFittedText(text, getLocalBounds(), just, lines, 1.f); }
    void setText(const juce::String& t) { if (t != text) { text = t; repaint(); } }
};
}   // namespace duoui::ui
