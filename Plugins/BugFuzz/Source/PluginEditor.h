#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LabScene.h"
#include "BrainBugzLook.h"

class BugFuzzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BugFuzzEditor (BugFuzzProcessor&);
    ~BugFuzzEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 990, baseH = 760, headerH = 70, sceneH = 440;

private:
    void timerCallback() override;
    void paintHeader (juce::Graphics&, juce::Rectangle<float>);
    void paintDeck (juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<float> deckRect (float x, float y, float w, float h) const;

    // ziehbarer Output-Wert in der Anzeige
    class OutValue;

    BugFuzzProcessor& proc;
    bb::LookAndFeel lnf;
    LabScene scene;
    juce::Image logo;

    juce::Slider drive, mix;
    juce::TextButton octaBtn { "OCTA" }, agBtn { "Auto-Gain" };
    std::unique_ptr<OutValue> outVal;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> driveAtt, mixAtt;
    std::unique_ptr<BA> octaAtt, agAtt;

    float u = 1.0f;     // Skalierung gegenüber 990 px Breite
    double tubePhase = 0.0, lastMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BugFuzzEditor)
};
