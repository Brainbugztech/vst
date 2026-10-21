#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"

// Farben des Bleep Designers (an das Maskottchen angelehnt)
namespace bleepcol
{
    inline const juce::Colour bg     { 0xff0a0a0c };
    inline const juce::Colour panel  { 0xff141416 };
    inline const juce::Colour line   { 0xff2c2426 };
    inline const juce::Colour red    { 0xffff2a3a };
    inline const juce::Colour gold   { 0xffe2b53e };
    inline const juce::Colour ink    { 0xffece6e7 };
    inline const juce::Colour inkDim { 0xffa89c9e };
}

class BleepEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BleepEditor (BleepProcessor&);
    ~BleepEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1000, baseH = 720;

private:
    void timerCallback() override;

    BleepProcessor& proc;
    bb::LookAndFeel lnf;

    juce::Slider output;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAtt;

    int seenNotes = 0;
    float flash = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepEditor)
};
