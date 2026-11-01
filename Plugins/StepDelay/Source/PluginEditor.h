#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "StepBars.h"
#include "BrainBugzLook.h"

class StepDelayEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StepDelayEditor (StepDelayProcessor&);
    ~StepDelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 980, baseH = 640;

private:
    void timerCallback() override;
    void paintHeader (juce::Graphics&);
    juce::Rectangle<float> R (float x, float y, float w, float h) const { return { x * u, y * u, w * u, h * u }; }

    StepDelayProcessor& proc;
    bb::LookAndFeel lnf;
    StepBars bars;
    juce::Image logo;

    // Raster
    juce::TextButton syncBtn { "SYNC" };
    std::array<juce::TextButton, bb::stepgrid::kNumRates> rateBtns;
    std::array<juce::TextButton, bb::stepgrid::kNumModes> modeBtns;

    // Regler
    static constexpr int kNumKnobs = 6;
    std::array<juce::Slider, kNumKnobs> knobs;
    const std::array<const char*, kNumKnobs> knobIds { ParamIDs::steps, ParamIDs::timeMs, ParamIDs::mix,
                                                       ParamIDs::loCut, ParamIDs::hiCut, ParamIDs::out };
    const std::array<const char*, kNumKnobs> knobNames { "STEPS", "ZEIT", "MIX", "LO CUT", "HI CUT", "OUTPUT" };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<SA>, kNumKnobs> knobAtts;

    // Muster-Werkzeuge
    juce::ComboBox presetBox;
    juce::TextButton randBtn { "ZUFALL" }, resetBtn { "RESET" }, invBtn { "INVERT" };
    juce::TextButton leftBtn { juce::String::fromUTF8 ("\xe2\x97\x80") }, rightBtn { juce::String::fromUTF8 ("\xe2\x96\xb6") };

    float u = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepDelayEditor)
};
