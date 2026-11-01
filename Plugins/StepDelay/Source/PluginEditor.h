#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "StepBars.h"
#include "CyberLook.h"

// Oberfläche nach Benetts Vorlage: Hintergrund ist die bereinigte Vorlage (Assets/stepdelay_bg.jpg),
// alle beweglichen Teile liegen exakt an den Stellen aus dem Entwurf.
class StepDelayEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StepDelayEditor (StepDelayProcessor&);
    ~StepDelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Koordinaten = Pixel der Vorlage
    static constexpr int designW = 1774, designH = 887;
    static constexpr int baseW = 1240, baseH = 620;

private:
    void timerCallback() override;
    void stepPreset (int dir);
    juce::Rectangle<float> R (float x, float y, float w, float h) const { return { x * u, y * u, w * u, h * u }; }
    juce::Rectangle<int> scannerArea (float pos) const;

    StepDelayProcessor& proc;
    sd::LookAndFeel lnf;
    StepBars bars;
    juce::Image bgSource, background;

    // Pattern Control
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton syncBtn { "SYNC" };
    std::array<juce::TextButton, bb::stepgrid::kNumRates> rateBtns;
    std::array<juce::TextButton, bb::stepgrid::kNumModes> modeBtns;
    juce::TextButton invBtn { "INVERT" }, resetBtn { "RESET" };

    // Sound Engine
    static constexpr int kNumKnobs = 6;
    std::array<juce::Slider, kNumKnobs> knobs;
    const std::array<const char*, kNumKnobs> knobIds { ParamIDs::steps, ParamIDs::timeMs, ParamIDs::mix,
                                                       ParamIDs::loCut, ParamIDs::hiCut, ParamIDs::out };
    static constexpr std::array<float, kNumKnobs> knobX { 268, 523, 772, 1020, 1270, 1525 };
    static constexpr float knobY = 712;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<SA>, kNumKnobs> knobAtts;

    // Scanner-Effekt: Lichtbalken, der durch den Hintergrund läuft
    double scanPos = 0.0, lastMs = 0.0;
    float u = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepDelayEditor)
};
