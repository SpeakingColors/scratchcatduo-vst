#include "Duo.h"
#include "DuoEditor.h"

using namespace duoui;

juce::String duoui::actionName(int a)
{
    const char* names[] = { "Play", "Cue", "Sync", "Key lock", "Brake", "Backspin", "Loop in", "Loop out", "Loop exit" };
    if (a >= 200)
    {
        if (a == aCrossfader) return "Crossfader";
        if (a == aMaster) return "Master level";
        if (a >= aFx1 && a <= aFx3) return "FX " + juce::String(a - aFx1 + 1) + " on/off";
        return {};
    }
    const int deck = a >= 100 ? 1 : 0; const int k = a % 100; juce::String d = deck ? "Deck B " : "Deck A ";
    if (k <= 8) return d + names[k];
    if (k >= aHot1 && k < aHot1 + 8) return d + "Hot cue " + juce::String(k - aHot1 + 1);
    switch (k) { case aJog: return d + "Jog wheel (relative)"; case aJogTouch: return d + "Jog touch";
                 case aPitch: return d + "Tempo fader"; case aFader: return d + "Channel fader"; case aTrim: return d + "Trim";
                 case aHi: return d + "EQ high"; case aMid: return d + "EQ mid"; case aLow: return d + "EQ low";
                 case aColour: return d + "Colour filter"; case aCueMon: return d + "Cue (headphones)"; default: break; }
    return {};
}
juce::Array<int> duoui::allActions()
{
    juce::Array<int> r;
    for (int d = 0; d < 2; ++d)
    {
        const int o = d * 100;
        for (int k : { aPlay, aCue, aSync, aKeyLock, aBrake, aBackspin, aLoopIn, aLoopOut, aLoopExit }) r.add(o + k);
        for (int i = 0; i < 8; ++i) r.add(o + aHot1 + i);
        for (int k : { aJog, aJogTouch, aPitch, aFader, aTrim, aHi, aMid, aLow, aColour, aCueMon }) r.add(o + k);
    }
    for (int k : { (int) aCrossfader, (int) aMaster, (int) aFx1, (int) aFx2, (int) aFx3 }) r.add(k);
    return r;
}

// ---------------------------------------------------------------- parameters
juce::AudioProcessorValueTreeState::ParameterLayout DuoProcessor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto F = [&](const juce::String& id, const juce::String& nm, float lo, float hi, float def, const juce::String& unit = {}, int dec = 1)
    {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id, 1 }, nm, juce::NormalisableRange<float>(lo, hi), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([unit, dec](float v, int) { auto t = juce::String(v, dec); if (t.containsOnly("-0.")) t = juce::String(0.f, dec); return t + unit; })
                                                 .withValueFromStringFunction([lo, hi](const juce::String& t) { return juce::jlimit(lo, hi, t.getFloatValue()); })));
    };
    auto C = [&](const juce::String& id, const juce::String& nm, const juce::StringArray& items, int def) { p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id, 1 }, nm, items, def)); };
    auto B = [&](const juce::String& id, const juce::String& nm, bool def) { p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id, 1 }, nm, def)); };
    for (int d = 0; d < 2; ++d)
    {
        const juce::String s = d == 0 ? "a" : "b", D = d == 0 ? "Deck A " : "Deck B ";
        F("trim_" + s, D + "Trim", -12.f, 12.f, 0.f, " dB"); F("hi_" + s, D + "EQ high", -26.f, 6.f, 0.f, " dB"); F("mid_" + s, D + "EQ mid", -26.f, 6.f, 0.f, " dB"); F("lo_" + s, D + "EQ low", -26.f, 6.f, 0.f, " dB");
        F("col_" + s, D + "Colour", -1.f, 1.f, 0.f, "", 2); F("fader_" + s, D + "Fader", 0.f, 1.f, 1.f, "", 2); B("cue_" + s, D + "Cue", false);
        F("pitch_" + s, D + "Tempo fader", -1.f, 1.f, 0.f, "", 3); C("prange_" + s, D + "Tempo range", { "8 %", "16 %", "50 %" }, 0);
        B("keylock_" + s, D + "Key lock", false); B("sync_" + s, D + "Sync", false); B("quant_" + s, D + "Quantize", true);
        B("master_" + s, D + "Tempo master", false); B("vinyl_" + s, D + "Vinyl", true); B("trans_" + s, D + "Transform", false);
        C("src_" + s, D + "Source", { "File", "DAW input" }, 0);
    }
    F("xfade", "Crossfader", 0.f, 1.f, 0.5f, "", 2); C("xcurve", "Crossfader curve", { "Smooth", "Sharp", "Const" }, 2);
    F("master", "Master", -60.f, 6.f, 0.f, " dB"); F("cuemix", "Cue mix", 0.f, 1.f, 0.f, "", 2);
    const int deft[3] = { 1, 2, 3 }; const float d1[3] = { 0.5f, 0.45f, 0.6f }, d2[3] = { 0.5f, 0.4f, 0.25f };
    for (int k = 1; k <= 3; ++k)
    {
        const juce::String n(k);
        C("fxt" + n, "FX " + n + " type", { "Off", "Filter", "Echo", "Reverb", "Flanger", "Crush" }, deft[k - 1]); B("fxon" + n, "FX " + n + " on", false);
        F("fxp1_" + n, "FX " + n + " amount", 0.f, 1.f, d1[k - 1], "", 2); F("fxp2_" + n, "FX " + n + " dry/wet", 0.f, 1.f, d2[k - 1], "", 2);
    }
    C("fxtarget", "FX applies to", { "Deck A", "Deck B", "A + B", "Master" }, 1); F("chain", "FX chain mix", 0.f, 1.f, 0.6f, "", 2);
    C("echobeats", "Echo time", { "1/4", "1/2", "3/4", "1", "2" }, 2);
    C("outmode", "Outputs", { "Master only", "A, B and Master" }, 1); C("tempomode", "Tempo", { "Follow DAW", "Decks lead" }, 0);
    return { p.begin(), p.end() };
}

DuoProcessor::DuoProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Deck A in", juce::AudioChannelSet::stereo(), true).withInput("Deck B in", juce::AudioChannelSet::stereo(), false)
          .withOutput("Master", juce::AudioChannelSet::stereo(), true).withOutput("Deck A out", juce::AudioChannelSet::stereo(), false)
          .withOutput("Deck B out", juce::AudioChannelSet::stereo(), false).withOutput("Cue", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "STATE", makeLayout())
{
    formats.registerBasicFormats();
    for (auto* pr : getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(pr)) paramIds.push_back(r->paramID);
    for (auto& m : midiMap) m.store(-1);
}
DuoProcessor::~DuoProcessor() { pool.removeAllJobs(true, 4000); cancelPendingUpdate(); }

bool DuoProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (auto& b : l.inputBuses) if (! b.isDisabled() && b != juce::AudioChannelSet::stereo()) return false;
    for (auto& b : l.outputBuses) if (! b.isDisabled() && b != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void DuoProcessor::prepareToPlay(double sr, int bs)
{
    fs = sr; maxBlock = std::max(bs, 64); engine.prepare(sr, maxBlock);
    for (auto& v : in) v.assign((size_t) maxBlock + 8, 0.f);
    for (auto& v : dummy) v.assign((size_t) maxBlock + 8, 0.f);
    for (int d = 0; d < 2; ++d) { lastKeyLock[d] = -1; wasLive[d] = false; }
    // a track loaded before prepare (state restore) needs to be picked up: the audio thread does it on the first block
}

duo::MixParams DuoProcessor::readParams() const
{
    duo::MixParams m;
    for (int d = 0; d < 2; ++d)
    {
        const juce::String s = d == 0 ? "a" : "b";
        m.trimDb[d] = get("trim_" + s); m.eqHi[d] = get("hi_" + s); m.eqMid[d] = get("mid_" + s); m.eqLo[d] = get("lo_" + s);
        m.colour[d] = get("col_" + s); m.fader[d] = get("fader_" + s); m.cue[d] = get("cue_" + s) > 0.5f;
    }
    m.cross = get("xfade"); m.curve = (int) std::lround(get("xcurve")); m.masterDb = get("master"); m.cueMix = get("cuemix");
    for (int k = 0; k < 3; ++k)
    {
        const juce::String n(k + 1);
        m.fxType[k] = (int) std::lround(get("fxt" + n)); m.fxOn[k] = get("fxon" + n) > 0.5f; m.fxP1[k] = get("fxp1_" + n); m.fxP2[k] = get("fxp2_" + n);
    }
    m.fxTarget = (int) std::lround(get("fxtarget")); m.chainMix = get("chain");
    static const float beats[5] = { 0.25f, 0.5f, 0.75f, 1.f, 2.f }; m.echoBeats = beats[juce::jlimit(0, 4, (int) std::lround(get("echobeats")))];
    return m;
}

// ---------------------------------------------------------------- commands from the editor and from MIDI
void DuoProcessor::post(const Cmd& c)
{
    const auto w = cmdFifo.write(1);
    if (w.blockSize1 > 0) cmdBuf[(size_t) w.startIndex1] = c; else if (w.blockSize2 > 0) cmdBuf[(size_t) w.startIndex2] = c; else return;
}
void DuoProcessor::applyCmd(const Cmd& c)
{
    auto& d = engine.deck[juce::jlimit(0, 1, c.deck)];
    switch (c.type)
    {
        case kPlay: if (c.i == 2) d.togglePlay(); else d.setPlaying(c.i != 0); break;
        case kCueDown: d.cue(true); break; case kCueUp: d.cue(false); break;
        case kHot: d.hotCue(c.i, true); break; case kHotClear: d.clearHotCue(c.i); break;
        case kLoopIn: d.loopIn(); break; case kLoopOut: d.loopOut(); break; case kLoopExit: d.loopExit(); break;
        case kBeatLoop: d.beatLoop(c.a); break; case kBrake: d.brake(); break; case kBackspin: d.backspin(); break;
        case kMoveStart: d.startMove((duo::Move) c.i, c.a); break; case kMoveEnd: d.endMove(); break;
        case kJog: d.setJog(c.i != 0, c.a); break; case kBend: d.setBend((float) c.a); break;
        case kGoLive: d.goLive(); break; case kSeek: d.seekSeconds(c.a); break;
        case kJumpBeats: d.jumpBeats(c.a); break; case kShot: engine.triggerShot(c.i); break;
        case kPadFx: engine.setPadFx(c.deck, c.i, c.a > 0.5); break;
        case kSetHot: if (const auto t = d.track()) d.setHotCuePos(c.i, std::max(0.0, c.a) * t->sr); break;
        case kAlign: d.alignBeatTo(engine.deck[1 - juce::jlimit(0, 1, c.deck)]); break; default: break;
    }
}

void DuoProcessor::queueParam(const juce::String& id, float norm)
{
    const int idx = (int) (std::find(paramIds.begin(), paramIds.end(), id) - paramIds.begin());
    if (idx >= (int) paramIds.size()) return;
    const auto w = parFifo.write(1);
    if (w.blockSize1 > 0) parBuf[(size_t) w.startIndex1] = { idx, norm }; else if (w.blockSize2 > 0) parBuf[(size_t) w.startIndex2] = { idx, norm }; else return;
    triggerAsyncUpdate();
}
void DuoProcessor::handleAsyncUpdate()
{
    int s1, n1, s2, n2; parFifo.prepareToRead(parFifo.getNumReady(), s1, n1, s2, n2);
    auto apply = [this](int a, int n) { for (int i = 0; i < n; ++i) { auto pp = parBuf[(size_t) (a + i)]; if (auto* r = apvts.getParameter(paramIds[(size_t) pp.index])) r->setValueNotifyingHost(pp.norm); } };
    apply(s1, n1); apply(s2, n2); parFifo.finishedRead(n1 + n2);
    // retire tracks that were replaced
    const juce::SpinLock::ScopedLockType l(trackLock); retired[0].reset(); retired[1].reset();
}
void DuoProcessor::setParam(const juce::String& id, float real)
{
    if (auto* p = apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(real)); p->endChangeGesture(); }
}

void DuoProcessor::doAction(int action, int deck, bool down, float v)
{
    const juce::String s = deck == 0 ? "a" : "b";
    auto toggle = [&](const juce::String& id) { if (down) queueParam(id, get(id) > 0.5f ? 0.f : 1.f); };
    auto cmd = [&](int type, int i = 0, double a = 0) { Cmd c; c.type = type; c.deck = deck; c.i = i; c.a = a; applyCmd(c); };
    if (action >= aHot1 && action < aHot1 + 8) { if (down) cmd(kHot, action - aHot1); return; }
    switch (action)
    {
        case aPlay: if (down) cmd(kPlay, 2); break;
        case aCue: cmd(down ? kCueDown : kCueUp); break;
        case aSync: toggle("sync_" + s); break; case aKeyLock: toggle("keylock_" + s); break;
        case aBrake: if (down) cmd(kBrake); break; case aBackspin: if (down) cmd(kBackspin); break;
        case aLoopIn: if (down) cmd(kLoopIn); break; case aLoopOut: if (down) cmd(kLoopOut); break; case aLoopExit: if (down) cmd(kLoopExit); break;
        case aJogTouch: jogTouch[deck].store(down); break;
        case aPitch: queueParam("pitch_" + s, v); break; case aFader: queueParam("fader_" + s, v); break;
        case aTrim: queueParam("trim_" + s, v); break; case aHi: queueParam("hi_" + s, v); break; case aMid: queueParam("mid_" + s, v); break;
        case aLow: queueParam("lo_" + s, v); break; case aColour: queueParam("col_" + s, v); break; case aCueMon: toggle("cue_" + s); break;
        case aCrossfader: queueParam("xfade", v); break; case aMaster: queueParam("master", v); break;
        case aFx1: toggle("fxon1"); break; case aFx2: toggle("fxon2"); break; case aFx3: toggle("fxon3"); break;
        default: break;
    }
}

void DuoProcessor::handleMidi(const juce::MidiBuffer& mb, int)
{
    for (const auto meta : mb)
    {
        const auto m = meta.getMessage(); int type = -1, num = 0; float val = 0.f; bool down = false;
        if (m.isNoteOn()) { type = 0; num = m.getNoteNumber(); down = true; val = 1.f; }
        else if (m.isNoteOff()) { type = 0; num = m.getNoteNumber(); down = false; }
        else if (m.isController()) { type = 1; num = m.getControllerNumber(); val = (float) m.getControllerValue() / 127.f; down = m.getControllerValue() >= 64; }
        if (type < 0) continue;
        const int idx = type * 2048 + (m.getChannel() - 1) * 128 + num;
        const int la = learnAction.load();
        if (la >= 0 && (down || type == 1)) { midiMap[(size_t) idx].store(la); learnAction.store(-1); learnedNew.store(true); continue; }
        int act = midiMap[(size_t) idx].load(); if (act < 0) continue;
        const int deck = act >= 200 ? 0 : (act >= 100 ? 1 : 0), a = act >= 200 ? act : act % 100;
        if (a == aJog && type == 1) { const int v = m.getControllerValue(); jogTicks[deck] += (float) (v < 64 ? v : v - 128); continue; }
        doAction(a, deck, down, val);
    }
}

// ---------------------------------------------------------------- audio
void DuoProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    if (n > maxBlock) { maxBlock = n; for (auto& v : in) v.resize((size_t) n + 8); for (auto& v : dummy) v.resize((size_t) n + 8); engine.prepare(fs, n); }

    // 1. copy the inputs first (input and output buses share channels in the host buffer)
    const float* inp[4] = { nullptr, nullptr, nullptr, nullptr };
    for (int b = 0; b < 2; ++b)
    {
        auto* bus = getBus(true, b);
        if (bus != nullptr && bus->isEnabled())
        {
            auto v = getBusBuffer(buf, true, b);
            if (v.getNumChannels() >= 1)
            {
                std::copy(v.getReadPointer(0), v.getReadPointer(0) + n, in[b * 2].begin());
                std::copy(v.getReadPointer(v.getNumChannels() > 1 ? 1 : 0), v.getReadPointer(v.getNumChannels() > 1 ? 1 : 0) + n, in[b * 2 + 1].begin());
                inp[b * 2] = in[b * 2].data(); inp[b * 2 + 1] = in[b * 2 + 1].data();
            }
        }
    }

    // 2. commands, MIDI
    for (int d = 0; d < 2; ++d) jogTicks[d] = 0.f;
    handleMidi(midi, n);
    {
        int s1, n1, s2, n2; cmdFifo.prepareToRead(cmdFifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) applyCmd(cmdBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) applyCmd(cmdBuf[(size_t) (s2 + i)]);
        cmdFifo.finishedRead(n1 + n2);
    }
    const float tpt = std::max(8.f, jogTicksPerTurn.load());
    for (int d = 0; d < 2; ++d)
    {
        if (jogTouch[d].load()) { engine.deck[d].setJog(true, (double) jogTicks[d] / tpt / ((double) n / fs)); midiJogOn[d] = true; }
        else if (midiJogOn[d]) { engine.deck[d].setJog(false, 0.0); midiJogOn[d] = false; }
        else if (std::fabs(jogTicks[d]) > 0.f) engine.deck[d].setBend(juce::jlimit(-1.f, 1.f, jogTicks[d] / 6.f));
    }

    // 3. new tracks and source changes
    for (int d = 0; d < 2; ++d)
    {
        { const juce::SpinLock::ScopedTryLockType l(trackLock);
          if (l.isLocked() && pending[d] != nullptr)
          {
              retired[d] = engine.deck[d].setTrack(pending[d]);
              if (haveRestore[d]) { for (int i = 0; i < 8; ++i) if (restoreHot[d][i] >= 0) engine.deck[d].setHotCuePos(i, restoreHot[d][i] * pending[d]->sr); haveRestore[d] = false; }
              pending[d].reset(); triggerAsyncUpdate();
          } }
        const bool wantLive = get(d == 0 ? "src_a" : "src_b") > 0.5f;
        if (wantLive != wasLive[d]) { engine.deck[d].useLiveInput(wantLive); wasLive[d] = wantLive; }
    }

    // 4. parameters to the decks
    auto mp = readParams();
    double host = 0.0, hostSeen = 0.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) hostSeen = *b;
    hostBpmView.store(hostSeen);
    if (get("tempomode") < 0.5f) host = hostSeen;
    mp.hostBpm = host;
    static const float ranges[3] = { 0.08f, 0.16f, 0.5f };
    for (int d = 0; d < 2; ++d)
    {
        const juce::String s = d == 0 ? "a" : "b";
        engine.deck[d].setPitch(get("pitch_" + s), ranges[juce::jlimit(0, 2, (int) std::lround(get("prange_" + s)))]);
        engine.deck[d].setKeyLock(get("keylock_" + s) > 0.5f); engine.deck[d].setQuant(get("quant_" + s) > 0.5f);
        engine.deck[d].setVinyl(get("vinyl_" + s) > 0.5f); engine.deck[d].setTransLatch(get("trans_" + s) > 0.5f);
    }
    {   // sync: follow the DAW tempo, or the other deck (deck A leads when both are synced)
        const bool sa = get("sync_a") > 0.5f, sb = get("sync_b") > 0.5f;
        auto& A = engine.deck[0]; auto& B = engine.deck[1];
        if (host > 0.0) { A.setSync(sa, host); B.setSync(sb, host); }
        else if (sa && sb) {   // the deck with Master on leads; deck A when neither or both are set
            const bool bLeads = get("master_b") > 0.5f && get("master_a") < 0.5f;
            if (bLeads) { B.setSync(false, 0.0); A.setSync(true, B.currentBpm()); } else { A.setSync(false, 0.0); B.setSync(true, A.currentBpm()); } }
        else { A.setSync(sa, B.currentBpm()); B.setSync(sb, A.currentBpm()); }
    }

    // 5. render
    auto master = getBusBuffer(buf, false, 0);
    float* out[8] = { master.getWritePointer(0), master.getWritePointer(master.getNumChannels() > 1 ? 1 : 0), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    const bool multi = get("outmode") > 0.5f;
    for (int b = 1; b < 4; ++b)
    {
        auto* bus = getBus(false, b);
        if (bus == nullptr || !bus->isEnabled()) continue;
        auto v = getBusBuffer(buf, false, b);
        if (v.getNumChannels() < 2) continue;
        if (b == 3 || multi) { out[b * 2] = v.getWritePointer(0); out[b * 2 + 1] = v.getWritePointer(1); }
        else v.clear();
    }
    engine.process(mp, n, inp[0], inp[1], inp[2], inp[3], out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7]);

    // 6. tell the editor
    for (int d = 0; d < 2; ++d)
    {
        auto& dk = engine.deck[d]; auto& v = view[d];
        v.pos.store(dk.positionSeconds()); v.len.store(dk.lengthSeconds()); v.bpm.store(dk.currentBpm()); v.cue.store(dk.cueSeconds()); v.speed.store(dk.speed());
        v.loopA.store(dk.loopStartSec()); v.loopB.store(dk.loopEndSec()); v.phase.store((float) dk.beatPhase());
        v.playing.store(dk.isPlaying()); v.live.store(dk.inLiveMode()); v.loopOn.store(dk.loopActive()); v.touching.store(dk.touching());
        for (int i = 0; i < 8; ++i) { const double p = dk.hotCuePos(i); v.hot[(size_t) i].store(p < 0 || !dk.track() ? -1.0 : p / dk.track()->sr); }
    }
}

// ---------------------------------------------------------------- files and analysis
std::shared_ptr<const duo::Track> DuoProcessor::trackFor(int deck) const
{
    const juce::SpinLock::ScopedLockType l(const_cast<juce::SpinLock&>(trackLock)); return shown[deck];
}
juce::String DuoProcessor::statusLine(int deck) const
{
    const juce::SpinLock::ScopedLockType l(const_cast<juce::SpinLock&>(statusLock)); return analysis[deck];
}

void DuoProcessor::loadFile(int deck, const juce::File& f)
{
    { const juce::SpinLock::ScopedLockType l(statusLock); analysis[deck] = "Loading " + f.getFileName() + " ..."; fileName[deck] = f.getFileName(); }
    apvts.state.setProperty(deck == 0 ? "fileA" : "fileB", f.getFullPathName(), nullptr);
    pool.addJob([this, deck, f]
    {
        std::unique_ptr<juce::AudioFormatReader> rd(formats.createReaderFor(f));
        if (rd == nullptr || rd->lengthInSamples < 1024)
        { const juce::SpinLock::ScopedLockType l(statusLock); analysis[deck] = "Could not read " + f.getFileName(); return; }
        const juce::int64 maxFrames = (juce::int64) (rd->sampleRate * 60.0 * 20.0);    // 20 minutes at most
        const int n = (int) std::min<juce::int64>(rd->lengthInSamples, maxFrames);
        juce::AudioBuffer<float> b((int) std::max<unsigned>(2, rd->numChannels), n);
        rd->read(&b, 0, n, 0, true, true);
        auto t = std::make_shared<duo::Track>();
        t->sr = rd->sampleRate; t->name = f.getFileNameWithoutExtension().toStdString();
        t->l.assign(b.getReadPointer(0), b.getReadPointer(0) + n);
        t->r.assign(b.getReadPointer(rd->numChannels > 1 ? 1 : 0), b.getReadPointer(rd->numChannels > 1 ? 1 : 0) + n);
        { const juce::SpinLock::ScopedLockType l(statusLock); analysis[deck] = "Analysing " + f.getFileName() + " ..."; }
        const auto bp = duo::analyseBpm(t->l.data(), t->r.data(), t->frames(), t->sr);
        t->bpm = bp.bpm; t->firstBeat = bp.firstBeat;
        t->key = duo::analyseKey(t->l.data(), t->r.data(), t->frames(), t->sr);
        t->peaks = duo::makePeaks(t->l.data(), t->r.data(), t->frames(), (int) std::max<size_t>(1, t->frames() / 256));
        juce::MessageManager::callAsync([this, deck, t, f] { finishLoad(deck, t, f); });
    });
}
void DuoProcessor::finishLoad(int deck, std::shared_ptr<duo::Track> t, const juce::File& f)
{
    juce::String info = f.getFileNameWithoutExtension();
    if (t->bpm > 0) info += "   " + juce::String(t->bpm, 1) + " BPM";
    if (t->key.tonic >= 0) info += "   " + juce::String(duo::camelot(t->key)) + "  " + juce::String(duo::keyName(t->key));
    { const juce::SpinLock::ScopedLockType l(statusLock); analysis[deck] = info; }
    { const juce::SpinLock::ScopedLockType l(trackLock); shown[deck] = t; pending[deck] = t; }
    triggerAsyncUpdate();
}

// ---------------------------------------------------------------- MIDI map bookkeeping
void DuoProcessor::clearMidiMap() { for (auto& m : midiMap) m.store(-1); }
int DuoProcessor::mappedCount() const { int c = 0; for (auto& m : midiMap) if (m.load() >= 0) ++c; return c; }
juce::String DuoProcessor::learnStatus() const
{
    const int a = learnAction.load();
    if (a >= 0) return "Move or press a control on your controller to map: " + actionName(a);
    return juce::String(mappedCount()) + " controls mapped";
}

// ---------------------------------------------------------------- state
void DuoProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    for (int d = 0; d < 2; ++d)
    {
        juce::StringArray h; for (int i = 0; i < 8; ++i) h.add(juce::String(view[d].hot[(size_t) i].load(), 4));
        st.setProperty(d == 0 ? "hotA" : "hotB", h.joinIntoString(","), nullptr);
    }
    st.removeChild(st.getChildWithName("MIDIMAP"), nullptr);
    juce::ValueTree mm("MIDIMAP");
    for (size_t i = 0; i < midiMap.size(); ++i) { const int a = midiMap[i].load(); if (a >= 0) { juce::ValueTree e("MAP"); e.setProperty("k", (int) i, nullptr); e.setProperty("a", a, nullptr); mm.appendChild(e, nullptr); } }
    st.appendChild(mm, nullptr);
    st.setProperty("jogtpt", jogTicksPerTurn.load(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
void DuoProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml == nullptr || ! xml->hasTagName(apvts.state.getType())) return;
    auto st = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(st);
    clearMidiMap();
    if (auto mm = st.getChildWithName("MIDIMAP"); mm.isValid())
        for (auto e : mm) { const int k = (int) e.getProperty("k", -1); if (k >= 0 && k < (int) midiMap.size()) midiMap[(size_t) k].store((int) e.getProperty("a", -1)); }
    jogTicksPerTurn.store((float) (double) st.getProperty("jogtpt", 128.0));
    for (int d = 0; d < 2; ++d)
    {
        const juce::File f(st.getProperty(d == 0 ? "fileA" : "fileB", "").toString());
        auto hs = juce::StringArray::fromTokens(st.getProperty(d == 0 ? "hotA" : "hotB", "").toString(), ",", "");
        { const juce::SpinLock::ScopedLockType l(trackLock); for (int i = 0; i < 8; ++i) restoreHot[d][i] = i < hs.size() ? hs[i].getDoubleValue() : -1.0; haveRestore[d] = hs.size() > 0; }
        if (f.existsAsFile()) loadFile(d, f);
    }
}

juce::AudioProcessorEditor* DuoProcessor::createEditor() { return new DuoEditor(*this); }
