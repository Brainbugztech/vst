#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LabScene.h"
#include "BrainBugzLook.h"

class HaasEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HaasEditor (HaasProcessor&);
    ~HaasEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 990, baseH = 760, headerH = 70, sceneH = 440;

private:
    void timerCallback() override;
    void paintHeader (juce::Graphics&, juce::Rectangle<float>);
    void paintDeck (juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<float> deckRect (float x, float y, float w, float h) const;

    HaasProcessor& proc;
    bb::LookAndFeel lnf;
    LabScene scene;
    juce::Image logo;

    juce::Slider width, monoBass;
    juce::TextButton flipBtn { juce::String::fromUTF8 ("L \xe2\x87\x84 R") }, monoBtn { "Mono-Check" };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> widthAtt, bassAtt;
    std::unique_ptr<BA> flipAtt, monoAtt;

    float u = 1.0f;     // Skalierung gegenüber 990 px Breite
    double tubePhase = 0.0, lastMs = 0.0;
    juce::String lastReadout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaasEditor)
};
