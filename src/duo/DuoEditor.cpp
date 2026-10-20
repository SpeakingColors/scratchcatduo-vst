// Scratch Cat Duo editor. A port of the approved mockup (docs/mockup/scratchcatduo.html), laid out on its 2000 x 1790 design and scaled with the window.
#include "DuoEditor.h"
#include "DuoUi.h"
#include "DuoWaves.h"
#include "DuoBrowser.h"
#include "DuoPresets.h"
#include <catui/RenderGate.h>
#include <catui/Logo.h>

using namespace duoui::ui;

namespace
{
constexpr int kBaseW = 2000, kBaseH = 1790, kCheek = 30, kContentW = kBaseW - 2 * kCheek, kMinW = 800, kMaxW = 3200;
constexpr double kDefaultScale = 0.75;
int defaultWidth()
{
    int w = (int) (kBaseW * kDefaultScale);
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const double maxH = (double) d->userArea.getHeight() * 0.92, maxW = (double) d->userArea.getWidth() * 0.95;
        w = (int) std::min<double>(w, std::min(maxW, maxH * kBaseW / kBaseH));
    }
    return juce::jlimit(kMinW, kMaxW, w);
}
int heightFor(int w) { return (int) std::lround((double) w * kBaseH / kBaseW); }

// Pad: a hot cue, loop, jump, sampler or effect pad. It draws itself from a colour, a text and an on state.
struct PadButton : HoldButton
{
    Colour fill = kPanel; bool filled = false, dimmed = false; juce::String text;
    void paintButton(juce::Graphics& g, bool hl, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.f);
        Colour c = filled ? fill : kPanel; if (dimmed) c = c.withMultipliedAlpha(0.45f); if (down) c = c.brighter(0.3f); else if (hl) c = c.brighter(0.1f);
        g.setColour(c); g.fillRoundedRectangle(r, 8.f); g.setColour(filled ? c : kLine); g.drawRoundedRectangle(r, 8.f, 1.5f);
        g.setColour(filled && ! dimmed ? Colour(0xff05070a) : kInk); g.setFont(f(22.f, true)); g.drawFittedText(text, getLocalBounds().reduced(6, 0), juce::Justification::centred, 1);
    }
};

// The big round CUE and PLAY buttons.
struct RoundButton : HoldButton
{
    Colour ring = kYellow; bool isPlay = false, playing = false; juce::String text;
    void paintButton(juce::Graphics& g, bool hl, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.f); const auto c = r.getCentre();
        g.setColour(down ? Colour(0xff2c2f36) : hl ? Colour(0xff25282e) : kPanel); g.fillEllipse(r);
        g.setColour(playing ? ring.brighter(0.2f) : ring); g.drawEllipse(r, 5.f);
        g.setColour(ring);
        if (isPlay)
        {
            juce::Path p; p.addTriangle(c.x - 28.f, c.y - 24.f, c.x - 28.f, c.y + 24.f, c.x + 8.f, c.y); g.fillPath(p);
            g.fillRoundedRectangle(c.x + 14.f, c.y - 24.f, 13.f, 48.f, 2.f);
        }
        else { g.setFont(f(36.f, true)); g.drawText(text, getLocalBounds(), juce::Justification::centred); }
    }
};

// A read only text box that shows the loaded track. Click to load another.
struct NameField : juce::Component, juce::SettableTooltipClient
{
    juce::String text; std::function<void()> onClick;
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(0.5f); g.setColour(kField); g.fillRoundedRectangle(r, 4.f); g.setColour(kLine); g.drawRoundedRectangle(r, 4.f, 1.f);
        g.setColour(kInk); g.setFont(f(19.f)); g.drawText(text, 14, 0, getWidth() - 28, getHeight(), juce::Justification::centredLeft, true);
    }
    void mouseUp(const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    void setTextIf(const juce::String& t) { if (t != text) { text = t; repaint(); } }
};

// The dark card shown over the plugin for Guide, Settings, About and MIDI learn.
struct Overlay : juce::Component
{
    std::unique_ptr<juce::Component> card;
    void show(std::unique_ptr<juce::Component> c, int w, int h)
    {
        card = std::move(c); addAndMakeVisible(*card); card->setBounds(getLocalBounds().withSizeKeepingCentre(w, h)); setVisible(true); toFront(false); repaint();
    }
    void close() { setVisible(false); card.reset(); }
    void paint(juce::Graphics& g) override { g.fillAll(Colour(0xb0000000)); if (card) { auto b = card->getBounds().toFloat(); g.setColour(kPanel2); g.fillRoundedRectangle(b, 8.f); g.setColour(kLine); g.drawRoundedRectangle(b, 8.f, 1.5f); } }
    void mouseDown(const juce::MouseEvent& e) override { if (card && ! card->getBounds().contains(e.getPosition())) close(); }
};

// A card with a title, wrapped text and a row of buttons.
struct TextCard : juce::Component
{
    juce::String title; juce::AttributedString body; juce::OwnedArray<juce::TextButton> buttons; std::function<void()> onClose;
    TextCard(const juce::String& t, const juce::String& text) : title(t)
    {
        body.setJustification(juce::Justification::topLeft); body.append(text, f(17.f), kInk);
        auto* c = buttons.add(new juce::TextButton("Close")); addAndMakeVisible(c); c->onClick = [this] { if (onClose) onClose(); };
    }
    juce::TextButton* add(const juce::String& label, std::function<void()> fn) { auto* b = buttons.insert(buttons.size() - 1, new juce::TextButton(label)); addAndMakeVisible(b); b->onClick = std::move(fn); return b; }
    void resized() override { int x = getWidth() - 24; for (int i = buttons.size() - 1; i >= 0; --i) { const int w = i == buttons.size() - 1 ? 90 : 250; buttons[i]->setBounds(x - w, getHeight() - 56, w, 36); x -= w + 10; } }
    void paint(juce::Graphics& g) override
    {
        g.setColour(kInk); g.setFont(caps(22.f, 0.08f)); g.drawText(title, 24, 18, getWidth() - 48, 30, juce::Justification::centredLeft);
        body.draw(g, juce::Rectangle<float>(24.f, 64.f, (float) getWidth() - 48.f, (float) getHeight() - 130.f));
    }
};

juce::String timeText(double s) { s = std::max(0.0, s); const int m = (int) s / 60; return juce::String(m) + ":" + juce::String((int) std::fmod(s, 60.0)).paddedLeft('0', 2) + "." + juce::String((int) (std::fmod(s, 1.0) * 10.0)); }
juce::String dot() { return juce::String::charToString(0xB7); }
}   // namespace

struct DuoEditor::Impl : public juce::Component, public juce::DragAndDropContainer, public juce::FileDragAndDropTarget
{
    DuoEditor& ed; DuoProcessor& proc; DuoLAF laf; catui::RenderGate gate { *this }; juce::TooltipWindow tips { this, 1000 };
    duoui::ui::HotMeta meta; duoui::PresetBank bank { proc }; WaveStack waves { proc, meta }; TrackBrowser browser { proc }; Overlay overlay;
    std::unique_ptr<Jog> jog[2];
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cAtt;
    std::vector<KnobBox*> knobs; std::vector<Meter*> meters;
    struct SegBind { Seg* s; std::vector<juce::String> toggles; juce::String choice; };
    std::vector<SegBind> segBinds;
    PadButton* pads[2][8] = {}; RoundButton* playBtn[2] = {}; RoundButton* cueBtn[2] = {}; juce::TextButton* delBtn[2] = {}; Seg* padMode[2] = {}; Seg* srcSeg[2] = {};
    NameField* nameField[2] = {}; Label* deckInfo[2] = {}; Label* padHint[2] = {}; Label* tempoVal[2] = {}; Label* faderVal[2] = {};
    juce::TextButton* killBtn[2][3] = {}; float killStore[2][3] = {};
    Seg* loopSeg[2] = {}; Seg* stutterSeg[2] = {};
    Label* beatInfo = nullptr; Label* keyInfo = nullptr; Label* anaInfo = nullptr; Label* mixPos = nullptr; Label* fxInfo = nullptr; Label* dawInfo = nullptr; Label* browserStatus = nullptr;
    Seg* zoomSeg = nullptr; juce::Slider* tempoSl[2] = {}; Label* sizeLabel = nullptr; KnobBox* hostKnob = nullptr;
    int mode[2] = { 0, 0 }; bool deleteMode[2] = { false, false }; double lastLoopBeats[2] = { 0, 0 };
    std::unique_ptr<juce::FileChooser> chooser; bool eco = false;
    struct PresetName : juce::Component, juce::SettableTooltipClient
    {
        juce::String text; std::function<void()> onClick;
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(0.5f); g.setColour(kField); g.fillRoundedRectangle(r, 4.f); g.setColour(kLine); g.drawRoundedRectangle(r, 4.f, 1.f);
            g.setColour(kInk); g.setFont(mono(16.f).boldened()); g.drawText(text, 14, 0, getWidth() - 40, getHeight(), juce::Justification::centredLeft, true);
            juce::Path t; const float cx = (float) getWidth() - 16.f, cy = (float) getHeight() * 0.5f; t.addTriangle(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3); g.setColour(kAccent); g.fillPath(t);
        }
        void mouseUp(const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
        void setTextIf(const juce::String& t) { if (t != text) { text = t; repaint(); } }
    };
    PresetName* presetName = nullptr;

    // ---------------------------------------------------------------- helpers
    static Rectangle<int> R(int ax, int ay, int w, int h) { return { ax - kCheek, ay, w, h }; }
    template <class T> T* own(std::unique_ptr<T> c) { auto* p = c.get(); addAndMakeVisible(*p); owned.push_back(std::move(c)); return p; }
    Label* text(const juce::String& t, Rectangle<int> r, float h, Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false, bool monoFont = false)
    { auto* l = own(std::make_unique<Label>()); l->text = t; l->font = monoFont ? mono(h) : f(h, bold); l->col = c; l->just = j; l->setBounds(r); return l; }
    HoldButton* hold(const juce::String& t, Rectangle<int> r, const juce::String& tip, std::function<void()> down, std::function<void()> up = {})
    { auto* b = own(std::make_unique<HoldButton>(t)); b->setBounds(r); b->setTooltip(tip); b->onPress = std::move(down); b->onRelease = std::move(up); return b; }
    Seg* seg(Rectangle<int> r, std::vector<Seg::Opt> o, std::vector<float> w = {}, float fh = 15.f)
    { auto* s = own(std::make_unique<Seg>()); s->opts = std::move(o); s->weights = std::move(w); s->fontH = fh; s->setBounds(r); return s; }
    void cmd(int type, int deck, int i = 0, double a = 0.0) { duoui::Cmd c; c.type = type; c.deck = deck; c.i = i; c.a = a; proc.post(c); }
    void toggleParam(const juce::String& id) { proc.setParam(id, proc.get(id) > 0.5f ? 0.f : 1.f); }
    void bindToggles(Seg& s, std::vector<juce::String> ids)
    { segBinds.push_back({ &s, ids, {} }); s.onClick = [this, ids](int i) { if (i >= 0 && i < (int) ids.size() && ids[(size_t) i].isNotEmpty()) toggleParam(ids[(size_t) i]); }; }
    void bindChoice(Seg& s, const juce::String& id)
    { segBinds.push_back({ &s, {}, id }); s.onClick = [this, id](int i) { proc.setParam(id, (float) i); }; }
    KnobBox* knob(const juce::String& id, const juce::String& label, Rectangle<int> r, float px, Colour arc, const juce::String& tip, std::function<juce::String()> vt = {})
    {
        auto* k = own(std::make_unique<KnobBox>()); k->label = label; k->knobPx = px; k->setBounds(r); k->setTooltip(tip); k->s.setTooltip(tip);
        k->s.setColour(juce::Slider::rotarySliderFillColourId, arc); k->valueText = std::move(vt); k->resized();
        if (id.isNotEmpty())
        {
            auto* par = proc.apvts.getParameter(id); k->s.setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
            sAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, k->s));
            if (! k->valueText) { auto* s = &k->s; k->valueText = [s] { return s->getTextFromValue(s->getValue()); }; }
        }
        knobs.push_back(k); return k;
    }
    juce::Slider* linear(const juce::String& id, Rectangle<int> r, bool vertical, Colour col, const juce::String& tip)
    {
        auto s = std::make_unique<juce::Slider>(vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        s->setBounds(r); s->setColour(juce::Slider::thumbColourId, col); s->setTooltip(tip); s->setScrollWheelEnabled(true);
        auto* par = proc.apvts.getParameter(id); s->setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
        auto* p = own(std::move(s)); sAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, *p)); return p;
    }
    juce::ComboBox* choice(const juce::String& id, Rectangle<int> r, const juce::String& tip, const juce::StringArray* itemsOverride = nullptr)
    {
        auto* c = own(std::make_unique<juce::ComboBox>()); c->setBounds(r); c->setTooltip(tip);
        if (itemsOverride != nullptr) c->addItemList(*itemsOverride, 1); else if (auto* pc = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) c->addItemList(pc->choices, 1);
        cAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, id, *c)); return c;
    }
    Meter* meter(Rectangle<int> r, std::function<float()> pk, std::vector<int> marks, bool twin = false, std::function<float()> pk2 = {}, float bot = -48.f)
    { auto* m = own(std::make_unique<Meter>()); m->setBounds(r); m->peak = std::move(pk); m->peak2 = std::move(pk2); m->scale = true; m->twin = twin; m->marks = std::move(marks); m->botDb = bot; meters.push_back(m); return m; }

    // ---------------------------------------------------------------- build
    Impl(DuoEditor& e, DuoProcessor& p) : ed(e), proc(p)
    {
        setSize(kContentW, kBaseH);
        for (int d = 0; d < 2; ++d) meta.fromString(d, proc.apvts.state.getProperty(d == 0 ? "hotMetaA" : "hotMetaB", "").toString());
        buildTop(); buildWaves();
        for (int d = 0; d < 2; ++d) buildDeck(d);
        buildMixer(); buildRouting(); buildFx(); buildBrowser();
        sizeLabel = text("WINDOW 100 %", R(1760, 1764, 196, 14), 12.f, kDim, juce::Justification::centredRight, false, true);
        addChildComponent(overlay); overlay.setBounds(getLocalBounds());
        eco = (bool) proc.apvts.state.getProperty("eco", false); applyTips();
        gate.onFrame = [this] { frame(); }; gate.start(eco ? 15 : 30);
    }
    ~Impl() override { gate.stop(); }

    void buildTop()
    {
        hold(juce::String::charToString(0x2039), R(426, 17, 29, 31), "Previous preset.", [this] { stepPreset(-1); });
        hold(juce::String::charToString(0x203A), R(1453, 17, 29, 31), "Next preset.", [this] { stepPreset(1); });
        auto* pn = own(std::make_unique<PresetName>()); pn->setBounds(R(462, 16, 985, 32)); pn->setTooltip("Open the preset list: factory presets and your own."); pn->text = bank.list[(size_t) bank.current].name;
        pn->onClick = [this] { presetMenu(); }; presetName = pn;
        hold("SAVE", R(1488, 17, 63, 31), "Save the current settings as your own preset (stays on this computer).", [this] { savePresetDialog(); });
        hold(juce::String::charToString(0x22EF), R(1557, 17, 38, 31), "Delete, export or import your presets.", [this] { moreMenu(); });
        hold("GUIDE", R(1611, 17, 70, 31), "How Scratch Cat Duo works.", [this] { showGuide(); });
        hold("SETTINGS", R(1687, 17, 97, 31), "Info bubbles, eco mode, MIDI jog sensitivity.", [this] { showSettings(); });
        hold("ABOUT & SUPPORT", R(1791, 17, 165, 31), "About Scratch Cat Duo, and how to support Studio 87.", [this] { showAbout(); });
    }

    void buildWaves()
    {
        addAndMakeVisible(waves); waves.setBounds(R(56, 87, 1888, 246)); waves.onLoad = [this](int d, const juce::File& f) { loadTo(d, f); };
        anaInfo = text("", R(55, 340, 360, 30), 13.f, kDim, juce::Justification::centredLeft, false, true);
        beatInfo = text("", R(420, 340, 300, 30), 13.f, kDim, juce::Justification::centredLeft, false, true);
        keyInfo = text("", R(725, 340, 420, 30), 13.f, kAccent, juce::Justification::centredLeft, false, true);
        hold(juce::String::charToString(0x25B6) + " BOTH", R(1567, 338, 83, 32), "Start both decks together.", [this] { cmd(duoui::kPlay, 0, 1); cmd(duoui::kPlay, 1, 1); });
        hold("RESTART BOTH", R(1662, 339, 137, 31), "Both decks back to the start, playing together.", [this] { for (int d = 0; d < 2; ++d) { cmd(duoui::kSeek, d, 0, 0.0); cmd(duoui::kPlay, d, 1); } });
        zoomSeg = seg(R(1811, 339, 134, 31), { { "4 bars", "Show 4 bars around the playhead." }, { "8", "Show 8 bars around the playhead." }, { "16", "Show 16 bars around the playhead." } }, { 62, 31, 38 }, 14.f);
        zoomSeg->setOnly(1); zoomSeg->onClick = [this](int i) { waves.bars = i == 0 ? 4 : i == 1 ? 8 : 16; zoomSeg->setOnly(i); waves.repaint(); };
    }

    void buildDeck(int d)
    {
        const int dx = d * 1136; const juce::String s = d == 0 ? "a" : "b"; const juce::String D = d == 0 ? "A" : "B"; const Colour col = deckCol(d);
        auto X = [dx](int ax, int ay, int w, int h) { return R(ax + dx, ay, w, h); };
        text("DECK " + D, X(56, 398, 90, 32), 28.f, col, juce::Justification::centredLeft, true);
        deckInfo[d] = text("", X(150, 400, 420, 28), 16.f, kInk, juce::Justification::centredLeft, false, true);
        hold("Analyze key & BPM", X(671, 400, 140, 27), "Analyse the loaded track again for BPM, beat grid and key.", [this, d] { reanalyse(d); });
        srcSeg[d] = seg(X(56, 434, 88, 35), { { "File", "File: play the loaded track." }, { "DAW", "DAW: scratch the audio your project sends to this deck's input. Touch the platter to hold the last minute." } }, {}, 15.f);
        srcSeg[d]->onCol = col; bindChoice(*srcSeg[d], "src_" + s);
        nameField[d] = own(std::make_unique<NameField>()); nameField[d]->setBounds(X(156, 434, 655, 36));
        nameField[d]->setTooltip("The track on this deck. Click to pick a file, or drag one from the browser below."); nameField[d]->onClick = [this, d] { chooseFile(d); };
        jog[d] = std::make_unique<Jog>(proc, d); addAndMakeVisible(*jog[d]); jog[d]->setBounds(X(55, 518, 344, 344)); jog[d]->setTooltip("Platter. Drag to scratch: it follows your hand forward and backward and holds when you stop.");
        auto* sw = seg(X(414, 518, 298, 31), { { "Sync", "Match this deck's tempo to the DAW tempo (or the other deck when Tempo is set to Decks lead)." }, { "Master", "Make this deck the tempo leader when both decks are synced and the tempo follows the decks." },
                       { "Key lock", "Change the tempo without changing the pitch." }, { "Quant", "Snap cue points and loops to the beat grid." } }, { 65, 75, 87, 71 }, 14.f);
        bindToggles(*sw, { "sync_" + s, "master_" + s, "keylock_" + s, "quant_" + s });
        static const juce::String bl[6] = { juce::String::charToString(0xBC), juce::String::charToString(0xBD), "1", "2", "4", "8" }; static const double bv[6] = { 0.25, 0.5, 1, 2, 4, 8 };
        std::vector<Seg::Opt> lo; for (int i = 0; i < 6; ++i) lo.push_back({ bl[i], "Beat loop of " + bl[i] + " beat(s) from the playhead. Press again to leave it." });
        loopSeg[d] = seg(X(414, 559, 298, 31), lo, { 52, 52, 48, 48, 48, 48 }, 15.f);
        loopSeg[d]->onClick = [this, d](int i) { if (proc.view[d].loopOn.load() && lastLoopBeats[d] == bv[i]) cmd(duoui::kLoopExit, d); else { lastLoopBeats[d] = bv[i]; cmd(duoui::kBeatLoop, d, 0, bv[i]); } };
        hold("Loop in", X(413, 599, 95, 27), "Start of a loop at the playhead.", [this, d] { cmd(duoui::kLoopIn, d); });
        hold("Loop out", X(516, 599, 95, 27), "End of the loop. The loop starts playing.", [this, d] { cmd(duoui::kLoopOut, d); });
        hold("Exit", X(618, 599, 95, 27), "Leave the loop.", [this, d] { cmd(duoui::kLoopExit, d); });
        stutterSeg[d] = seg(X(414, 635, 298, 31), { { "Vinyl", "On: pause and stop slow the record down like a turntable. Off: it stops at once." }, { "Trans.", "Transform: chops the sound on and off every 1/8 beat until you switch it off." },
                            { "Stutter", "Hold: retriggers from the cue point every 1/4 beat." } }, { 93, 101, 104 }, 14.f);
        stutterSeg[d]->onCol = col; stutterSeg[d]->holdStyle = true;
        stutterSeg[d]->onHold = [this, d, s](int i, bool down)
        {
            if (i == 0 && down) toggleParam("vinyl_" + s); else if (i == 1 && down) toggleParam("trans_" + s);
            else if (i == 2) { if (down) cmd(duoui::kMoveStart, d, (int) duo::Move::Stutter, 0.25); else cmd(duoui::kMoveEnd, d); }
        };
        hold("Brake", X(413, 675, 146, 27), "Slow to a stop like a turntable being switched off.", [this, d] { cmd(duoui::kBrake, d); });
        hold("Backspin", X(567, 675, 146, 27), "Spin the record backwards and stop.", [this, d] { cmd(duoui::kBackspin, d); });
        hold("Roll", X(413, 710, 146, 27), "Hold: loops a short slice while the track keeps running underneath, then jumps back in time.", [this, d] { cmd(duoui::kMoveStart, d, (int) duo::Move::Roll, 0.25); }, [this, d] { cmd(duoui::kMoveEnd, d); });
        hold("Censor", X(567, 710, 146, 27), "Hold: plays backward while the track keeps running underneath, then jumps back in time.", [this, d] { cmd(duoui::kMoveStart, d, (int) duo::Move::Censor, 0.25); }, [this, d] { cmd(duoui::kMoveEnd, d); });
        auto* cue = own(std::make_unique<RoundButton>()); cue->setBounds(X(443, 751, 112, 112)); cue->ring = kYellow; cue->text = "CUE"; cueBtn[d] = cue;
        cue->setTooltip("Cue. Playing: back to the cue point and stop. Stopped: set the cue point here. Hold on the cue point to preview."); cue->onPress = [this, d] { cmd(duoui::kCueDown, d); }; cue->onRelease = [this, d] { cmd(duoui::kCueUp, d); };
        auto* pl = own(std::make_unique<RoundButton>()); pl->setBounds(X(571, 751, 112, 112)); pl->ring = Colour(0xff8be08b); pl->isPlay = true; playBtn[d] = pl;
        pl->setTooltip("Play or pause."); pl->onPress = [this, d] { cmd(duoui::kPlay, d, 2); };
        text("TEMPO", X(740, 478, 80, 16), 12.f, kDim, juce::Justification::centred, true);
        tempoSl[d] = linear("pitch_" + s, X(738, 496, 84, 320), true, kAccent, "Tempo fader. Up is faster, down is slower. Double click: back to 0.");
        tempoSl[d]->setSliderStyle(juce::Slider::LinearVertical);
        tempoVal[d] = text("0.0%", X(738, 818, 84, 18), 14.f, kInk, juce::Justification::centred, false, true);
        auto* rg = seg(X(729, 845, 90, 25), { { "8", "Tempo fader range: 8 percent." }, { "16", "Tempo fader range: 16 percent." }, { "50", "Tempo fader range: 50 percent." } }, { 25, 32, 32 }, 13.f); bindChoice(*rg, "prange_" + s);
        hold("Reset", X(747, 877, 55, 27), "Put the tempo fader back to zero.", [this, s] { proc.setParam("pitch_" + s, 0.f); });
        padMode[d] = seg(X(56, 911, 302, 25), { { "Hot Cue", "The eight pads set and jump to hot cues." }, { "Loop", "The eight pads start loops of 1/8 to 16 beats." }, { "Jump", "The eight pads jump back or forward by beats." },
                           { "Sampler", "The eight pads play built in sounds over the mix." }, { "Pad FX", "Hold a pad to apply an effect or move to this deck." } }, { 68, 50, 53, 69, 62 }, 13.f);
        padMode[d]->onCol = col; padMode[d]->setOnly(0); padMode[d]->onClick = [this, d](int i) { mode[d] = i; padMode[d]->setOnly(i); refreshPads(d, true); };
        auto* del = hold("Delete", X(55, 943, 62, 26), "Delete switch: while on, clicking a set hot cue removes it. Shift click does the same.", [this, d] { deleteMode[d] = ! deleteMode[d]; });
        del->setColour(juce::TextButton::buttonOnColourId, kYellow); delBtn[d] = del;
        padHint[d] = text("Click to set or jump. Right-click to edit.", X(146, 943, 660, 26), 14.f, kYellow, juce::Justification::centredLeft, false, true);
        for (int i = 0; i < 8; ++i)
        {
            auto* b = own(std::make_unique<PadButton>()); b->setBounds(X(55 + (i % 4) * 191, 975 + (i / 4) * 76, 183, 68)); pads[d][i] = b;
            b->onPress = [this, d, i] { padPress(d, i, true); }; b->onRelease = [this, d, i] { padPress(d, i, false); };
            b->onRight = [this, d, i] { if (mode[d] == 0) editHot(d, i); };
        }
        refreshPads(d, true);
    }

    void buildMixer()
    {
        const Colour none(0x00000000);
        knob("master", "MASTER", R(958, 418, 84, 74), 44.f, none, "Master level.");
        for (int d = 0; d < 2; ++d)
        {
            const int dx = d * 181; const juce::String s = d == 0 ? "a" : "b"; const Colour col = deckCol(d);
            text(d == 0 ? "A" : "B", R(843 + dx, 500, 133, 16), 15.f, col, juce::Justification::centred, true);
            knob("trim_" + s, "TRIM", R(868 + dx, 518, 84, 66), 35.f, none, "Input gain of this channel.");
            knob("hi_" + s, "HI", R(868 + dx, 586, 84, 66), 35.f, none, "High EQ. All the way down kills the highs.");
            knob("mid_" + s, "MID", R(868 + dx, 654, 84, 66), 35.f, none, "Mid EQ. All the way down kills the mids.");
            knob("lo_" + s, "LOW", R(868 + dx, 722, 84, 66), 35.f, none, "Low EQ. All the way down kills the lows.");
            knob("col_" + s, "COLOR", R(868 + dx, 790, 84, 66), 35.f, none, "Colour filter. Left: low pass. Right: high pass. Centre: off.",
                 [this, s] { const float v = proc.get("col_" + s); return std::fabs(v) < 0.03f ? juce::String("off") : v < 0 ? "LP " + juce::String((int) std::lround(-v * 100)) + "%" : "HP " + juce::String((int) std::lround(v * 100)) + "%"; });
            static const char* bn[3] = { "H", "M", "L" }; static const char* id[3] = { "hi_", "mid_", "lo_" }; static const char* nm[3] = { "highs", "mids", "lows" };
            for (int k = 0; k < 3; ++k)
            {
                auto* b = hold(bn[k], R(869 + dx + k * 28, 858, k == 1 ? 26 : 25, 20), juce::String("Kill the ") + nm[k] + " of this channel. Press again to bring them back.", [this, d, k, s] { killBand(d, k, juce::String(id[k]) + s); });
                b->setColour(juce::TextButton::buttonOnColourId, Colour(0xffff4060)); killBtn[d][k] = b;
            }
            auto* cb = own(std::make_unique<juce::TextButton>("CUE")); cb->setBounds(R(882 + dx, 880, 54, 24)); cb->setClickingTogglesState(true); cb->setColour(juce::TextButton::buttonOnColourId, kYellow);
            cb->setTooltip("Send this channel to the Cue output (headphones)."); bAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "cue_" + s, *cb));
            linear("fader_" + s, R(878 + dx, 912, 40, 126), true, col, "Channel fader.");
            faderVal[d] = text("0.0", R(868 + dx, 1040, 60, 18), 13.f, col, juce::Justification::centred, false, true);
            meter(R(921 + dx, 911, 24, 148), [this, d] { return d == 0 ? proc.engine.peakA.load() : proc.engine.peakB.load(); }, { 0, -6, -12, -24, -36, -48 });
        }
        text("MST", R(987, 505, 26, 12), 10.f, kDim, juce::Justification::centred, false, true);
        meter(R(987, 517, 26, 542), [this] { return proc.engine.peakML.load(); }, { 0, -6, -12, -18, -24, -30, -36, -42, -48, -54 }, true, [this] { return proc.engine.peakMR.load(); }, -54.f);
        text("CROSSFADER", R(843, 1062, 314, 14), 12.f, kDim, juce::Justification::centred, true);
        linear("xfade", R(922, 1072, 156, 28), false, kPink, "Crossfader. Left is deck A, right is deck B.");
        mixPos = text("mid", R(1096, 1072, 50, 28), 13.f, kAccent, juce::Justification::centredLeft, false, true);
        auto* cv = seg(R(903, 1106, 194, 29), { { "Smooth", "Smooth: gentle blend." }, { "Sharp", "Sharp: both decks at full level, then a quick cut near each end." }, { "Const", "Const: constant power blend." } }, { 72, 61, 61 }, 14.f); bindChoice(*cv, "xcurve");
    }

    void buildRouting()
    {
        text("DAW ROUTING", R(57, 1167, 300, 16), 13.f, kDim, juce::Justification::centredLeft, true);
        text("HOW SCRATCH CAT DUO TALKS TO YOUR PROJECT", R(459, 1167, 500, 16), 13.f, kDim, juce::Justification::centredRight, true);
        const juce::StringArray items { "File (the loaded track)", "DAW input" };
        text("DECK A INPUT", R(57, 1182, 300, 14), 11.f, kDim, juce::Justification::centredLeft, true); text("DECK B INPUT", R(516, 1182, 300, 14), 11.f, kDim, juce::Justification::centredLeft, true);
        choice("src_a", R(57, 1204, 443, 31), "Where deck A gets its sound. File: the loaded track. DAW input: the audio your project sends to the Deck A input.", &items);
        choice("src_b", R(516, 1204, 443, 31), "Where deck B gets its sound. File: the loaded track. DAW input: the audio your project sends to the Deck B input.", &items);
        text("OUTPUTS", R(57, 1243, 200, 14), 11.f, kDim, juce::Justification::centredLeft, true); text("TEMPO", R(517, 1243, 200, 14), 11.f, kDim, juce::Justification::centredLeft, true);
        auto* o = seg(R(58, 1261, 441, 29), { { "Master only", "One stereo output with the whole mix." }, { "A " + dot() + " B " + dot() + " Master", "The master mix plus a separate output for each deck, so you can route them to their own DAW channels. Enable the extra outputs in your host." } }, { 215, 226 }, 14.f);
        o->onCol = Colour(0xfff5a06a); bindChoice(*o, "outmode");
        auto* t = seg(R(517, 1261, 441, 29), { { "Follow DAW", "Sync matches the tempo of your project." }, { "Decks lead", "Sync matches the other deck, not the project." } }, { 224, 217 }, 14.f); bindChoice(*t, "tempomode");
        hostKnob = knob("", "HOST BPM", R(57, 1301, 84, 74), 44.f, kAccent, "The tempo your project reports.", [this] { const double b = proc.hostBpmView.load(); return b > 0 ? juce::String(b, 1) : juce::String("none"); });
        hostKnob->s.setRange(60.0, 200.0); hostKnob->s.setInterceptsMouseClicks(false, false);
        text("CONTROLLER", R(173, 1305, 200, 14), 11.f, kDim, juce::Justification::centredLeft, true);
        hold("MIDI learn...", R(153, 1330, 156, 31), "Map the controls of your MIDI controller to decks, mixer and effects.", [this] { showMidi(); });
        dawInfo = text("", R(57, 1391, 902, 54), 13.f, kDim, juce::Justification::topLeft, false, true); dawInfo->lines = 3;
    }

    void buildFx()
    {
        text("FX CHAIN", R(995, 1167, 200, 31), 13.f, kDim, juce::Justification::centredLeft, true);
        auto* tg = seg(R(1683, 1168, 260, 29), { { "Deck A", "The three effects work on deck A." }, { "Deck B", "The three effects work on deck B." }, { "A + B", "The three effects work on both decks together." }, { "Master", "The three effects work on the master mix." } }, { 68, 68, 57, 66 }, 14.f);
        bindChoice(*tg, "fxtarget");
        const juce::String amountTip = "Main control of the effect: filter cutoff, echo feedback, reverb size, flanger depth, crush amount.";
        for (int k = 1; k <= 3; ++k)
        {
            const int x0 = 995 + (k - 1) * 320; const juce::String n(k);
            text("FX" + n, R(x0 + 9, 1216, 30, 31), 12.f, kDim, juce::Justification::centredLeft, false, true);
            choice("fxt" + n, R(x0 + 37, 1216, 233, 31), "Effect type for slot " + n + ".");
            auto* on = own(std::make_unique<juce::TextButton>()); on->setBounds(R(x0 + 276, 1219, 26, 22)); on->setClickingTogglesState(true); on->setColour(juce::TextButton::buttonOnColourId, kAccent);
            on->setTooltip("Switch this effect on or off."); bAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "fxon" + n, *on));
            knob("fxp1_" + n, "AMOUNT", R(x0 + 40, 1251, 84, 66), 35.f, kAccent, amountTip, [this, n] { return fxAmountText(n); });
            knob("fxp2_" + n, "DRY/WET", R(x0 + 186, 1251, 84, 66), 35.f, kAccent, "How much of the effect you hear.", [this, n] { return juce::String((int) std::lround(proc.get("fxp2_" + n) * 100.f)) + " %"; });
        }
        knob("chain", "CHAIN MIX", R(995, 1332, 84, 74), 44.f, kAccent, "Overall amount of the whole FX chain.", [this] { return juce::String((int) std::lround(proc.get("chain") * 100.f)) + " %"; });
        fxInfo = text("", R(1093, 1352, 520, 24), 16.f, kInk, juce::Justification::centredLeft);
        text("ECHO TIME (BEATS)", R(1640, 1335, 300, 14), 11.f, kDim, juce::Justification::centredLeft, true);
        auto* eb = seg(R(1640, 1352, 300, 28), { { "1/4", "Echo repeats every quarter beat." }, { "1/2", "Echo repeats every half beat." }, { "3/4", "Echo repeats every three quarter beat." }, { "1", "Echo repeats every beat." }, { "2", "Echo repeats every two beats." } }, {}, 14.f);
        bindChoice(*eb, "echobeats");
    }
    juce::String fxAmountText(const juce::String& n)
    {
        const int type = (int) std::lround(proc.get("fxt" + n)); const float v = proc.get("fxp1_" + n);
        if (type == 0) return "-";
        if (type == 1) { const double hz = 80.0 * std::pow(12000.0 / 80.0, (double) v); return hz >= 1000 ? juce::String(hz / 1000.0, 1) + " kHz" : juce::String((int) std::lround(hz)) + " Hz"; }
        return juce::String((int) std::lround(v * 100.f)) + "%";
    }

    void buildBrowser()
    {
        text("TRACK BROWSER", R(55, 1479, 400, 14), 13.f, kDim, juce::Justification::centredLeft, true);
        browserStatus = text("", R(1645, 1479, 300, 14), 12.f, kYellow, juce::Justification::centredRight, true, true);
        addAndMakeVisible(browser); browser.setBounds(R(55, 1499, 1890, 235));
        browser.onLoad = [this](int d, const juce::File& f) { loadTo(d, f); };
        browser.freeDeck = [this] { return proc.view[0].playing.load() && ! proc.view[1].playing.load() ? 1 : 0; };
        browserStatus->setText(browser.statusText());
    }

    // ---------------------------------------------------------------- actions
    void loadTo(int d, const juce::File& f) { if (! f.existsAsFile()) return; proc.setParam(d == 0 ? "src_a" : "src_b", 0.f); proc.loadFile(d, f); }
    void chooseFile(int d)
    {
        chooser = std::make_unique<juce::FileChooser>("Load a track on deck " + juce::String(d == 0 ? "A" : "B"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, d](const juce::FileChooser& fc) { loadTo(d, fc.getResult()); });
    }
    void reanalyse(int d) { const juce::File f(proc.apvts.state.getProperty(d == 0 ? "fileA" : "fileB", "").toString()); if (f.existsAsFile()) proc.loadFile(d, f); }
    void killBand(int d, int band, const juce::String& id)
    {
        const float v = proc.get(id);
        if (v <= -25.5f) proc.setParam(id, killStore[d][band]); else { killStore[d][band] = v; proc.setParam(id, -26.f); }
    }
    void padPress(int d, int i, bool down)
    {
        static const double loops[8] = { 0.125, 0.25, 0.5, 1, 2, 4, 8, 16 }, jumps[8] = { -16, -8, -4, -1, 1, 4, 8, 16 };
        if (mode[d] == 0)
        {
            if (! down) return;
            const bool set = proc.view[d].hot[(size_t) i].load() >= 0;
            if (set && (deleteMode[d] || juce::ModifierKeys::currentModifiers.isShiftDown())) { cmd(duoui::kHotClear, d, i); meta.name[d][i] = {}; meta.on[d][i] = true; saveMeta(); return; }
            if (set && ! meta.on[d][i]) return;
            cmd(duoui::kHot, d, i); return;
        }
        if (mode[d] == 1 && down) { if (proc.view[d].loopOn.load() && lastLoopBeats[d] == loops[i]) cmd(duoui::kLoopExit, d); else { lastLoopBeats[d] = loops[i]; cmd(duoui::kBeatLoop, d, 0, loops[i]); } }
        else if (mode[d] == 2 && down) cmd(duoui::kJumpBeats, d, 0, jumps[i]);
        else if (mode[d] == 3 && down) cmd(duoui::kShot, d, i);
        else if (mode[d] == 4)
        {
            if (i < 5) cmd(duoui::kPadFx, d, i, down ? 1.0 : 0.0);
            else if (i == 5) { if (down) cmd(duoui::kBrake, d); }
            else { if (down) cmd(duoui::kMoveStart, d, (int) (i == 6 ? duo::Move::Roll : duo::Move::Stutter), 0.25); else cmd(duoui::kMoveEnd, d); }
        }
    }
    void saveMeta() { for (int d = 0; d < 2; ++d) proc.apvts.state.setProperty(d == 0 ? "hotMetaA" : "hotMetaB", meta.toString(d), nullptr); }
    void refreshPads(int d, bool force)
    {
        static const char* loopT[8] = { "1/8", "1/4", "1/2", "1", "2", "4", "8", "16" }; static const char* jumpT[8] = { "-16", "-8", "-4", "-1", "+1", "+4", "+8", "+16" };
        static const char* fxT[8] = { "Filter", "Echo", "Verb", "Flange", "Crush", "Brake", "Roll", "Stutter" };
        for (int i = 0; i < 8; ++i)
        {
            auto* p = pads[d][i]; Colour fc = kPanel; bool filled = false, dim = false; juce::String t;
            if (mode[d] == 0)
            {
                const bool set = proc.view[d].hot[(size_t) i].load() >= 0;
                filled = set; fc = meta.col(d, i); dim = set && ! meta.on[d][i]; t = set && meta.name[d][i].isNotEmpty() ? meta.name[d][i] : juce::String(i + 1);
            }
            else t = mode[d] == 1 ? loopT[i] : mode[d] == 2 ? jumpT[i] : mode[d] == 3 ? juce::String(duo::Engine::shotName(i)) : juce::String(fxT[i]);
            if (force || p->fill != fc || p->filled != filled || p->dimmed != dim || p->text != t) { p->fill = fc; p->filled = filled; p->dimmed = dim; p->text = t; p->repaint(); }
        }
        delBtn[d]->setVisible(mode[d] == 0);
        static const char* hint[5] = { "Click to set or jump. Right-click to edit.", "Loop of that many beats from the playhead. Press again to leave it.", "Jump by that many beats.", "Built in sounds played over the mix.", "Hold a pad to apply the effect or move to this deck." };
        padHint[d]->setBounds(R(mode[d] == 0 ? 146 + d * 1136 : 56 + d * 1136, 943, 700, 26)); padHint[d]->setText(hint[mode[d]]);
    }
    void editHot(int d, int i)
    {
        const double h = proc.view[d].hot[(size_t) i].load(); if (h < 0) return;
        auto* w = new juce::AlertWindow("Hot cue " + juce::String(i + 1), "Name, colour, position and on/off.", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("name", meta.name[d][i], "Name"); w->addTextEditor("pos", juce::String(h, 3), "Position in seconds");
        const juce::StringArray cols { "Pink", "Yellow", "Teal", "Lilac", "Orange", "Green", "Blue", "Red" }; w->addComboBox("col", cols, "Colour"); w->getComboBoxComponent("col")->setSelectedItemIndex(meta.colour[d][i]);
        w->addComboBox("on", juce::StringArray { "On", "Off (greyed out, does not jump)" }, "State"); w->getComboBoxComponent("on")->setSelectedItemIndex(meta.on[d][i] ? 0 : 1);
        w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Delete cue", 2); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, d, i, w](int r)
        {
            if (r == 1)
            {
                meta.name[d][i] = w->getTextEditorContents("name").trim().substring(0, 12); meta.colour[d][i] = w->getComboBoxComponent("col")->getSelectedItemIndex(); meta.on[d][i] = w->getComboBoxComponent("on")->getSelectedItemIndex() == 0;
                const double sec = w->getTextEditorContents("pos").getDoubleValue(); const double len = proc.view[d].len.load(); if (len > 0) cmd(duoui::kSetHot, d, i, juce::jlimit(0.0, len - 0.05, sec)); saveMeta();
            }
            else if (r == 2) { cmd(duoui::kHotClear, d, i); meta.name[d][i] = {}; meta.on[d][i] = true; saveMeta(); }
        }), true);
    }

    // ---------------------------------------------------------------- presets
    void stepPreset(int dir) { bank.reload(); if (bank.list.empty()) return; bank.apply((bank.current + dir + (int) bank.list.size()) % (int) bank.list.size()); }
    void presetMenu()
    {
        bank.reload(); juce::PopupMenu m; m.addSectionHeader("Factory");
        for (size_t i = 0; i < bank.list.size(); ++i) { if (! bank.list[i].factory) continue; m.addItem(1000 + (int) i, bank.list[i].name, true, (int) i == bank.current); }
        m.addSeparator(); m.addSectionHeader("Yours"); bool any = false;
        for (size_t i = 0; i < bank.list.size(); ++i) { if (bank.list[i].factory) continue; any = true; m.addItem(1000 + (int) i, bank.list[i].name, true, (int) i == bank.current); }
        if (! any) m.addItem(-1, "No saved presets yet. Use Save.", false);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetName), [this](int r) { if (r >= 1000) bank.apply(r - 1000); });
    }
    void savePresetDialog()
    {
        auto* w = new juce::AlertWindow("Save preset", "Name for your preset. It stays on this computer.", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("n", bank.list[(size_t) bank.current].factory ? juce::String("My preset") : bank.list[(size_t) bank.current].name, "Name");
        w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int r)
        {
            if (r != 1) return; const auto name = w->getTextEditorContents("n").trim();
            if (name.isEmpty()) return;
            if (bank.isFactory(name)) { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save preset", "That name belongs to a factory preset. Pick another name."); return; }
            if (bank.exists(name)) juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Replace preset?", "A preset called \"" + name + "\" already exists. Replace it?", "Replace", "Cancel", nullptr,
                juce::ModalCallbackFunction::create([this, name](int ok) { if (ok == 1) doSave(name); }));
            else doSave(name);
        }), true);
    }
    void doSave(const juce::String& name) { if (! bank.saveUser(name)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save preset", "The preset could not be saved."); }
    void moreMenu()
    {
        bank.reload(); juce::PopupMenu m, del; int idx = 0;
        for (size_t i = 0; i < bank.list.size(); ++i) if (! bank.list[i].factory) { del.addItem(2000 + (int) i, bank.list[i].name); ++idx; }
        if (idx == 0) del.addItem(-1, "No saved presets", false);
        m.addSubMenu("Delete preset", del, idx > 0);
        m.addItem(3, "Export the current preset...", ! bank.list[(size_t) bank.current].factory); m.addItem(4, "Import a preset file...");
        m.addSeparator(); m.addItem(5, "Show the presets folder");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetName), [this](int r)
        {
            if (r >= 2000)
            {
                const auto n = bank.list[(size_t) (r - 2000)].name;
                juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Delete preset?", "Delete \"" + n + "\"? This cannot be undone.", "Delete", "Cancel", nullptr, juce::ModalCallbackFunction::create([this, n](int ok) { if (ok == 1) bank.deleteUser(n); }));
            }
            else if (r == 3)
            {
                const auto n = bank.list[(size_t) bank.current].name;
                chooser = std::make_unique<juce::FileChooser>("Export preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(juce::File::createLegalFileName(n) + ".duopreset"), "*.duopreset");
                chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, n](const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) bank.exportTo(n, fc.getResult()); });
            }
            else if (r == 4)
            {
                chooser = std::make_unique<juce::FileChooser>("Import a preset", juce::File(), "*.duopreset");
                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc)
                { if (fc.getResult().existsAsFile() && ! bank.importFrom(fc.getResult())) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Import preset", "That file is not a Scratch Cat Duo preset, or its name is taken by a factory preset."); });
            }
            else if (r == 5) { duoui::PresetBank::folder().createDirectory(); duoui::PresetBank::folder().revealToUser(); }
        });
    }

    // ---------------------------------------------------------------- cards
    void showGuide()
    {
        auto c = std::make_unique<TextCard>("Guide",
            "Scratch Cat Duo is two decks, a mixer and an effect chain in one plugin: scratch, mix and transition between two tracks, or run it on your project's audio.\n\n"
            "Load a track on each deck (click the name box, use the browser, or drop a file), then press Play. Sync matches a deck's tempo to your project (or to the other deck). Key lock changes tempo without changing pitch.\n\n"
            "Drag a platter or a waveform to scratch. Hold Roll, Censor or Stutter for performance moves. The pads change with the tabs: hot cues, loops, jumps, sampler or effects.\n\n"
            "The mixer has trim, three band EQ with kill buttons, a colour filter, channel faders and a crossfader with three curves. The FX chain can work on deck A, deck B, both or the master.\n\n"
            "In the browser, add a folder (subfolders are included), star folders to keep them, search, then load with A or B or drag a row onto a deck. Nothing leaves your computer.");
        c->onClose = [this] { overlay.close(); }; overlay.show(std::move(c), 1000, 560);
    }
    void showAbout()
    {
        auto c = std::make_unique<TextCard>("About & Support",
            "Scratch Cat Duo is a free and open source plugin by Studio 87, AGPL-3.0-or-later. It collects no data and never uses the network.\n\nVersion " + juce::String(JucePlugin_VersionString) + ".\n\nIf it helps your music, a coffee on Ko-fi keeps new plugins coming.");
        c->add("Support Studio 87 on Ko-fi", [] { juce::URL("https://ko-fi.com/studio87").launchInDefaultBrowser(); });
        c->add("GitHub", [] { juce::URL("https://github.com/SpeakingColors/scratchcatduo-vst").launchInDefaultBrowser(); });
        c->onClose = [this] { overlay.close(); }; overlay.show(std::move(c), 900, 330);
    }
    struct SettingsCard : juce::Component
    {
        Impl& im; juce::TextButton bubbles, ecoB, close { "Close" }; juce::ComboBox jogTicks;
        explicit SettingsCard(Impl& i) : im(i)
        {
            for (auto* b : { &bubbles, &ecoB, &close }) addAndMakeVisible(*b);
            addAndMakeVisible(jogTicks); for (int t : { 64, 128, 256, 512, 1024 }) jogTicks.addItem(juce::String(t) + " ticks per turn", t);
            jogTicks.setSelectedId((int) im.proc.jogTicksPerTurn.load(), juce::dontSendNotification);
            jogTicks.onChange = [this] { im.proc.jogTicksPerTurn.store((float) jogTicks.getSelectedId()); };
            bubbles.onClick = [this] { im.proc.setTipsOn(! im.proc.tipsOn()); im.applyTips(); sync(); }; ecoB.onClick = [this] { im.setEco(! im.eco); sync(); }; close.onClick = [this] { im.overlay.close(); };
            bubbles.setColour(juce::TextButton::buttonOnColourId, kAccent); ecoB.setColour(juce::TextButton::buttonOnColourId, kAccent); sync();
        }
        void sync() { bubbles.setButtonText(im.proc.tipsOn() ? "On" : "Off"); bubbles.setToggleState(im.proc.tipsOn(), juce::dontSendNotification); ecoB.setButtonText(im.eco ? "On" : "Off"); ecoB.setToggleState(im.eco, juce::dontSendNotification); }
        void resized() override { bubbles.setBounds(getWidth() - 110, 70, 86, 34); ecoB.setBounds(getWidth() - 110, 124, 86, 34); jogTicks.setBounds(getWidth() - 250, 178, 226, 34); close.setBounds(getWidth() - 114, getHeight() - 56, 90, 36); }
        void paint(juce::Graphics& g) override
        {
            g.setColour(kInk); g.setFont(caps(22.f, 0.08f)); g.drawText("Settings", 24, 18, 400, 30, juce::Justification::centredLeft); g.setFont(f(17.f));
            g.drawText("Info bubbles after 1 s of hovering", 24, 70, 600, 34, juce::Justification::centredLeft); g.drawText("Eco mode (fewer screen updates, less CPU; audio is not affected)", 24, 124, 700, 34, juce::Justification::centredLeft);
            g.drawText("MIDI jog wheel sensitivity", 24, 178, 400, 34, juce::Justification::centredLeft);
            g.setColour(kDim); g.setFont(f(15.f)); g.drawFittedText("Drawing stops by itself when the window is not in front. Audio, automation and MIDI keep running.", { 24, 232, getWidth() - 48, 50 }, juce::Justification::topLeft, 2);
        }
    };
    struct MidiCard : juce::Component, juce::Timer
    {
        Impl& im; juce::ComboBox act; juce::TextButton learn { "Learn" }, clear { "Clear all" }, close { "Close" }; juce::Array<int> ids;
        explicit MidiCard(Impl& i) : im(i)
        {
            ids = duoui::allActions(); int n = 1; for (int a : ids) act.addItem(duoui::actionName(a), n++); act.setSelectedId(1, juce::dontSendNotification);
            for (auto* c : std::initializer_list<juce::Component*> { &act, &learn, &clear, &close }) addAndMakeVisible(*c);
            learn.setColour(juce::TextButton::buttonOnColourId, kAccent);
            learn.onClick = [this] { const int k = act.getSelectedId() - 1; if (k >= 0 && k < ids.size()) im.proc.learnAction.store(ids[k]); };
            clear.onClick = [this] { im.proc.clearMidiMap(); }; close.onClick = [this] { im.proc.learnAction.store(-1); im.overlay.close(); };
            act.setTooltip("Pick what you want to control, press Learn, then move or press the control on your controller."); startTimerHz(10);
        }
        ~MidiCard() override { stopTimer(); }
        void timerCallback() override { learn.setToggleState(im.proc.learnAction.load() >= 0, juce::dontSendNotification); if (im.proc.learnedNew.exchange(false)) learn.setToggleState(false, juce::dontSendNotification); repaint(); }
        void resized() override { act.setBounds(24, 80, 460, 34); learn.setBounds(496, 80, 100, 34); clear.setBounds(606, 80, 110, 34); close.setBounds(getWidth() - 114, getHeight() - 56, 90, 36); }
        void paint(juce::Graphics& g) override
        {
            g.setColour(kInk); g.setFont(caps(22.f, 0.08f)); g.drawText("MIDI learn", 24, 18, 400, 30, juce::Justification::centredLeft);
            g.setColour(kDim); g.setFont(f(16.f)); g.drawFittedText("Pick a control, press Learn, then move or press the control on your controller.", { 24, 128, getWidth() - 48, 24 }, juce::Justification::topLeft, 1);
            g.setColour(kAccent); g.setFont(mono(14.f)); g.drawFittedText(im.proc.learnStatus(), { 24, 162, getWidth() - 48, 40 }, juce::Justification::topLeft, 2);
        }
    };
    void showSettings() { overlay.show(std::make_unique<SettingsCard>(*this), 860, 360); }
    void showMidi() { overlay.show(std::make_unique<MidiCard>(*this), 760, 300); }
    void applyTips() { tips.setMillisecondsBeforeTipAppears(proc.tipsOn() ? 1000 : 100000000); }
    void setEco(bool b) { eco = b; proc.apvts.state.setProperty("eco", b, nullptr); gate.start(b ? 15 : 30); }

    // ---------------------------------------------------------------- frame
    void frame()
    {
        waves.trk[0] = proc.trackFor(0); waves.trk[1] = proc.trackFor(1); waves.repaint();
        for (auto* k : knobs) k->tick();
        for (auto* m : meters) m->tick();
        for (int d = 0; d < 2; ++d)
        {
            auto& v = proc.view[d]; const juce::String s = d == 0 ? "a" : "b"; auto t = waves.trk[d];
            playBtn[d]->playing = v.playing.load(); playBtn[d]->repaint(); jog[d]->repaint();
            juce::String info = juce::String(v.bpm.load(), 1);
            if (t && t->key.tonic >= 0) info += "  " + dot() + "  " + juce::String(duo::keyName(t->key)) + " (" + juce::String(duo::camelot(t->key)) + ")";
            info += "  " + dot() + "  " + timeText(v.pos.load()); deckInfo[d]->setText(info);
            const bool live = proc.get("src_" + s) > 0.5f; const juce::File fl(proc.apvts.state.getProperty(d == 0 ? "fileA" : "fileB", "").toString());
            nameField[d]->setTextIf(live ? juce::String("DAW input (Deck ") + (d == 0 ? "A" : "B") + " in)" : fl.getFileName().isNotEmpty() ? (t ? fl.getFileNameWithoutExtension() : proc.statusLine(d)) : juce::String("Click to load a track, or drop one here"));
            const float rg = proc.get("prange_" + s); const float tp = proc.get("pitch_" + s) * (rg < 0.5f ? 8.f : rg < 1.5f ? 16.f : 50.f);
            tempoVal[d]->setText((tp > 0 ? "+" : "") + juce::String(tp, 1) + "%");
            const float fv = proc.get("fader_" + s); faderVal[d]->setText(fv < 0.001f ? "-inf" : juce::String(20.f * std::log10(fv), 1));
            stutterSeg[d]->set(0, proc.get("vinyl_" + s) > 0.5f); stutterSeg[d]->set(1, proc.get("trans_" + s) > 0.5f);
            loopSeg[d]->setOnly(-1);
            if (v.loopOn.load()) for (int i = 0; i < 6; ++i) { static const double bv[6] = { 0.25, 0.5, 1, 2, 4, 8 }; if (bv[i] == lastLoopBeats[d]) loopSeg[d]->set(i, true); }
            static const char* ids[3] = { "hi_", "mid_", "lo_" };
            for (int k = 0; k < 3; ++k) killBtn[d][k]->setToggleState(proc.get(juce::String(ids[k]) + s) <= -25.5f, juce::dontSendNotification);
            if (mode[d] == 0) refreshPads(d, false);
            delBtn[d]->setToggleState(deleteMode[d], juce::dontSendNotification);
        }
        for (auto& b : segBinds)
        {
            if (b.choice.isNotEmpty()) b.s->setOnly((int) std::lround(proc.get(b.choice)));
            else for (size_t i = 0; i < b.toggles.size(); ++i) b.s->set((int) i, proc.get(b.toggles[i]) > 0.5f);
        }
        const double ba = proc.view[0].bpm.load(), bb = proc.view[1].bpm.load(); auto ta = waves.trk[0], tb = waves.trk[1];
        auto kt = [](const std::shared_ptr<const duo::Track>& t) { return t && t->key.tonic >= 0 ? juce::String(duo::keyName(t->key)) + " (" + juce::String(duo::camelot(t->key)) + ")" : juce::String(); };
        anaInfo->setText("Deck A " + juce::String(ba, 1) + " BPM " + kt(ta) + "  " + dot() + "  Deck B " + juce::String(bb, 1) + " BPM " + kt(tb));
        if (ta && tb && ba > 0 && bb > 0) { const bool same = std::fabs(ba - bb) < 0.3; beatInfo->col = same ? kAccent : kYellow; beatInfo->setText(same ? "Beat grids match" : "Beat grids differ: press Sync"); } else beatInfo->setText("");
        if (ta && tb && ta->key.tonic >= 0 && tb->key.tonic >= 0)
        {
            const bool ok = duo::keysCompatible(ta->key, tb->key); keyInfo->col = ok ? kAccent : Colour(0xffff7a7a);
            keyInfo->setText(juce::String(ok ? "Keys compatible (" : "Keys clash (") + juce::String(duo::camelot(ta->key)) + " + " + juce::String(duo::camelot(tb->key)) + ")");
        }
        else keyInfo->setText("");
        const float xf = proc.get("xfade"); mixPos->setText(xf < 0.04f ? "A" : xf > 0.96f ? "B" : std::fabs(xf - 0.5f) < 0.03f ? "mid" : juce::String((int) std::lround(xf * 100)) + "%");
        {
            static const char* tn[6] = { "off", "filter", "echo", "reverb", "flanger", "crush" }; static const char* tg[4] = { "deck A", "deck B", "decks A and B", "the master" };
            juce::StringArray on; bool echo = false;
            for (int k = 1; k <= 3; ++k) { const int t = (int) std::lround(proc.get("fxt" + juce::String(k))); if (proc.get("fxon" + juce::String(k)) > 0.5f && t > 0) { on.add(tn[juce::jlimit(0, 5, t)]); echo = echo || t == 2; } }
            fxInfo->setText(on.isEmpty() ? juce::String("No effect is on.") : on.joinIntoString(" + ") + " on " + tg[juce::jlimit(0, 3, (int) std::lround(proc.get("fxtarget")))] + (echo ? ", tempo synced" : ""));
        }
        const double hb = proc.hostBpmView.load(); if (hostKnob != nullptr) hostKnob->s.setValue(juce::jlimit(60.0, 200.0, hb > 0 ? hb : 60.0), juce::dontSendNotification);
        dawInfo->setText((hb > 0 ? "Host tempo " + juce::String(hb, 1) + " BPM" : juce::String("Host tempo: not reported")) + "\nDeck A " + juce::String(ba, 1) + " BPM " + dot() + " Deck B " + juce::String(bb, 1) + " BPM\nOutputs: " + (proc.get("outmode") > 0.5f ? "3 stereo (Master, Deck A, Deck B)" : "1 stereo (Master)"));
        browserStatus->setText(browser.statusText());
        presetName->setTextIf(bank.list[(size_t) juce::jlimit(0, (int) bank.list.size() - 1, bank.current)].name);
        const int pct = (int) std::lround(100.0 * ed.getWidth() / (double) defaultWidth()); sizeLabel->setText("WINDOW " + juce::String(pct) + " %");
    }

    // ---------------------------------------------------------------- drops from the file manager
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int x, int y) override
    {
        juce::StringArray loose; bool loaded = false;
        for (auto& p : files)
        {
            juce::File f(p);
            if (f.isDirectory()) browser.addRoot(f.getFullPathName());
            else if (y < 1468 && ! loaded && f.existsAsFile()) { loadTo(x < getWidth() / 2 ? 0 : 1, f); loaded = true; }
            else loose.add(p);
        }
        if (! loose.isEmpty()) browser.addLoose(loose);
    }

    // ---------------------------------------------------------------- paint
    void panel(juce::Graphics& g, Rectangle<int> r, Colour c, float round = 4.f)
    { auto b = R(r.getX(), r.getY(), r.getWidth(), r.getHeight()).toFloat(); g.setColour(c); g.fillRoundedRectangle(b, round); g.setColour(kLine); g.drawRoundedRectangle(b.reduced(0.5f), round, 1.f); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(kGround);
        catui::drawCatHead(g, { 36.f, 20.f, 46.f, 40.f }, Colour(0xfff4f5f7));
        g.setColour(kInk); g.setFont(caps(36.f, 0.1f)); g.drawText("SCRATCH CAT DUO", 90, 8, 370, 44, juce::Justification::centredLeft);
        g.setColour(Colour(0xffe0a020)); g.setFont(caps(13.f, 0.3f)); g.drawText("STUDIO 87", 92, 46, 200, 18, juce::Justification::centredLeft);
        panel(g, { 44, 75, 1912, 306 }, kPanel);
        for (int d = 0; d < 2; ++d)
        {
            auto r = R(44 + d * 1136, 391, 776, 758); g.setColour(kPanel2); g.fillRoundedRectangle(r.toFloat(), 4.f); g.setColour(kLine); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 4.f, 1.f);
            g.setColour(deckCol(d)); g.fillRect(r.getX(), r.getY() + 2, 3, r.getHeight() - 4);
        }
        panel(g, { 835, 398, 330, 740 }, Colour(0xff2b2e34), 6.f);
        g.setColour(kDim); g.setFont(caps(13.f, 0.22f)); g.drawText("MIXER", R(843, 404, 314, 14), juce::Justification::centred);
        panel(g, { 44, 1154, 928, 304 }, kPanel); panel(g, { 982, 1154, 974, 304 }, kPanel); panel(g, { 44, 1468, 1912, 277 }, kPanel);
        for (int k = 0; k < 3; ++k) panel(g, { 995 + k * 320, 1206, 308, 118 }, kPanel2);
        g.setColour(kDim); g.setFont(caps(12.f, 0.14f)); g.drawText("STUDIO 87  " + dot() + "  SCRATCH CAT DUO V" + juce::String(JucePlugin_VersionString), R(44, 1762, 800, 18), juce::Justification::centredLeft);
    }
};

DuoEditor::DuoEditor(DuoProcessor& p) : AudioProcessorEditor(&p)
{
    impl = std::make_unique<Impl>(*this, p);
    setLookAndFeel(&impl->laf); addAndMakeVisible(*impl); impl->sendLookAndFeelChange();
    setResizable(true, true);
    setResizeLimits(kMinW, heightFor(kMinW), kMaxW, heightFor(kMaxW));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio((double) kBaseW / (double) kBaseH);
    const int w = juce::jlimit(kMinW, kMaxW, p.getProp("uiw", defaultWidth()));
    setSize(w, heightFor(w));
}
DuoEditor::~DuoEditor() { setLookAndFeel(nullptr); }
void DuoEditor::paint(juce::Graphics& g)
{
    g.fillAll(kGround);
    catui::drawWoodCheeks(g, getLocalBounds(), (int) std::lround(kCheek * (double) getWidth() / kBaseW));
}
void DuoEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int ck = (int) std::lround(kCheek * (double) getWidth() / kBaseW);
    if (e.x < ck || e.x >= getWidth() - ck) setSize(defaultWidth(), heightFor(defaultWidth()));
}
void DuoEditor::resized()
{
    if (impl == nullptr) return;
    const float s = (float) getWidth() / (float) kBaseW;
    impl->setBounds(0, 0, kContentW, kBaseH);
    impl->setTransform(juce::AffineTransform::scale(s).translated((float) kCheek * s, 0.f));
    if (auto* pr = dynamic_cast<DuoProcessor*>(&processor)) pr->setProp("uiw", getWidth());
}
