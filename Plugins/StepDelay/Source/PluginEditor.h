#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "StepBars.h"
#include "CyberLook.h"

class StepDelayEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StepDelayEditor (StepDelayProcessor&);
    ~StepDelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Entwurf in 1000 x 500 Einheiten, Startgröße 1100 x 550
    static constexpr int designW = 1000, designH = 500;
    static constexpr int baseW = 1100, baseH = 550;

private:
    void timerCallback() override;
    void renderBackground();
    void paintHeader (juce::Graphics&);
    void sectionLabel (juce::Graphics&, float x, float y, const juce::String& text);
    void stepPreset (int dir);
    juce::Rectangle<float> R (float x, float y, float w, float h) const { return { x * u, y * u, w * u, h * u }; }

    StepDelayProcessor& proc;
    sd::LookAndFeel lnf;
    StepBars bars;
    juce::Image logo, background;

    // Pattern Control
    juce::TextButton prevBtn { juce::String::fromUTF8 ("\xe2\x97\x80") }, nextBtn { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    juce::TextButton syncBtn { "SYNC" };
    std::array<juce::TextButton, bb::stepgrid::kNumRates> rateBtns;
    std::array<juce::TextButton, bb::stepgrid::kNumModes> modeBtns;
    juce::TextButton invBtn { "INVERT" }, resetBtn { "RESET" };

    // Sound Engine
    static constexpr int kNumKnobs = 6;
    std::array<juce::Slider, kNumKnobs> knobs;
    const std::array<const char*, kNumKnobs> knobIds { ParamIDs::steps, ParamIDs::timeMs, ParamIDs::mix,
                                                       ParamIDs::loCut, ParamIDs::hiCut, ParamIDs::out };
    const std::array<const char*, kNumKnobs> knobNames { "STEPS", "TIME", "MIX", "LO CUT", "HI CUT", "OUTPUT" };
    static constexpr std::array<float, kNumKnobs> knobX { 152, 295, 435, 575, 716, 860 };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<SA>, kNumKnobs> knobAtts;

    float u = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepDelayEditor)
};
