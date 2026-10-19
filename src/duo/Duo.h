// Scratch Cat Duo: the plugin (audio processor). Engine in src/core, editor in DuoEditor.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "../core/DuoEngine.h"
#include <array>
#include <atomic>
#include <memory>

namespace duoui
{
enum CmdType { kPlay = 1, kCueDown, kCueUp, kHot, kHotClear, kLoopIn, kLoopOut, kLoopExit, kBeatLoop, kBrake, kBackspin, kMoveStart, kMoveEnd, kJog, kBend, kGoLive, kSeek };
struct Cmd { int type = 0, deck = 0, i = 0; double a = 0.0; };

// Things you can map to a MIDI control (note or CC). Deck actions are offset by 100 for deck B.
enum Action {
    aPlay = 0, aCue, aSync, aKeyLock, aBrake, aBackspin, aLoopIn, aLoopOut, aLoopExit, aHot1 = 10, /* ..17 */ aJog = 30, aJogTouch, aPitch, aFader, aTrim, aHi, aMid, aLow, aColour, aCueMon,
    aCrossfader = 200, aMaster, aFx1, aFx2, aFx3, aCount
};
juce::String actionName(int action);
juce::Array<int> allActions();
}

class DuoProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    DuoProcessor();
    ~DuoProcessor() override;

    // ---- editor side ----
    void post(const duoui::Cmd& c);
    void loadFile(int deck, const juce::File& f);
    std::shared_ptr<const duo::Track> trackFor(int deck) const;
    float get(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }
    void setParam(const juce::String& id, float realValue);
    juce::String statusLine(int deck) const;
    bool tipsOn() const { return (bool) apvts.state.getProperty("tips", true); }
    void setTipsOn(bool b) { apvts.state.setProperty("tips", b, nullptr); }
    int getProp(const juce::String& k, int def) const { return (int) apvts.state.getProperty(k, def); }
    void setProp(const juce::String& k, int v) { apvts.state.setProperty(k, v, nullptr); }
    void clearMidiMap();
    int mappedCount() const;
    juce::String learnStatus() const;
    std::atomic<int> learnAction { -1 };
    std::atomic<bool> learnedNew { false };
    std::atomic<float> jogTicksPerTurn { 128.f };

    // read by the editor (written on the audio thread)
    struct DeckView {
        std::atomic<double> pos { 0.0 }, len { 0.0 }, bpm { 0.0 }, cue { 0.0 }, speed { 0.0 }, loopA { -1.0 }, loopB { -1.0 };
        std::atomic<float> phase { -1.f };
        std::atomic<bool> playing { false }, live { false }, loopOn { false }, touching { false };
        std::array<std::atomic<double>, 8> hot;
        DeckView() { for (auto& h : hot) h.store(-1.0); }
    } view[2];

    juce::AudioProcessorValueTreeState apvts;
    duo::Engine engine;

    // ---- AudioProcessor ----
    void prepareToPlay(double sr, int bs) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Scratch Cat Duo"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void handleAsyncUpdate() override;
    void applyCmd(const duoui::Cmd&);
    void handleMidi(const juce::MidiBuffer&, int numSamples);
    void doAction(int action, int deck, bool down, float value01);
    void queueParam(const juce::String& id, float norm);
    void finishLoad(int deck, std::shared_ptr<duo::Track> t, const juce::File& f);
    duo::MixParams readParams() const;

    juce::AbstractFifo cmdFifo { 512 };
    std::array<duoui::Cmd, 512> cmdBuf;
    struct PendingParam { int index = 0; float norm = 0.f; };
    juce::AbstractFifo parFifo { 128 };
    std::array<PendingParam, 128> parBuf;
    std::vector<juce::String> paramIds;     // for the async parameter queue
    std::array<std::atomic<int>, 3 * 16 * 128> midiMap;   // message type (0 note, 1 cc) x channel x number -> action (+1000 per deck B), -1 none
    std::atomic<bool> jogTouch[2] { false, false };
    float jogTicks[2] = { 0.f, 0.f };
    bool midiJogOn[2] = { false, false };
    double restoreHot[2][8];
    bool haveRestore[2] = { false, false };
    double fs = 44100.0; int maxBlock = 512;
    std::vector<float> in[4];
    std::vector<float> dummy[2];
    juce::SpinLock trackLock;
    std::shared_ptr<duo::Track> pending[2];
    std::shared_ptr<const duo::Track> shown[2];       // what the editor draws (message thread side)
    std::shared_ptr<const duo::Track> retired[2];
    juce::File pendingFile[2];
    juce::String fileName[2], analysis[2];
    bool wasLive[2] = { false, false };
    juce::AudioFormatManager formats;
    juce::ThreadPool pool { 1 };
    int lastKeyLock[2] = { -1, -1 };
    juce::SpinLock statusLock;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuoProcessor)
};
