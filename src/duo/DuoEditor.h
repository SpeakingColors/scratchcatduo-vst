#pragma once
#include "Duo.h"

// Scratch Cat Duo editor: stacked waveforms, two decks, mixer, DAW routing and MIDI learn, FX chain.
// Designed on a 1500 x 940 canvas (wood cheeks included) and scaled with the window.
class DuoEditor : public juce::AudioProcessorEditor
{
public:
    explicit DuoEditor(DuoProcessor&);
    ~DuoEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;   // on a wood cheek: back to 100%
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuoEditor)
};
