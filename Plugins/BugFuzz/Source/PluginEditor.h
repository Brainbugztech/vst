#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BandView.h"
#include "BrainBugzLook.h"

// Schlichtes Multiband-Layout: Kopfzeile, Band-Anzeige oben, Regler unten
class BugFuzzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BugFuzzEditor (BugFuzzProcessor&);
    ~BugFuzzEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 900, baseH = 520;

private:
    void timerCallback() override;
    juce::Rectangle<float> R (float x, float y, float w, float h) const { return { x * u, y * u, w * u, h * u }; }

    BugFuzzProcessor& proc;
    bb::LookAndFeel lnf;
    BandView bands;
    juce::Image logo;

    juce::Slider drive, saturation, mix, output;
    juce::TextButton octaBtn { "OCTA" }, agBtn { "AUTO-GAIN" };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> driveAtt, satAtt, mixAtt, outAtt;
    std::unique_ptr<BA> octaAtt, agAtt;

    float u = 1.0f; // Skalierung gegenüber 900 px Breite

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BugFuzzEditor)
};
