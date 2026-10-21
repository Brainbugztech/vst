#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AlignView.h"
#include "BrainBugzLook.h"

// Layout wie BugFuzz: Kopfzeile, Anzeige oben, Regler und Taster unten
class BugzAlignEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BugzAlignEditor (BugzAlignProcessor&);
    ~BugzAlignEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 900, baseH = 600;

private:
    void timerCallback() override;
    juce::Rectangle<float> R (float x, float y, float w, float h) const { return { x * u, y * u, w * u, h * u }; }

    BugzAlignProcessor& proc;
    bb::LookAndFeel lnf;
    AlignView view;
    juce::Image logo;
    juce::TooltipWindow tips { this, 600 };

    juce::Slider amount, split, window, glide, sens, offset;
    juce::TextButton holdBtn { "HOLD" }, abBtn { "A/B VORHER" }, subBtn { "SUB CHECK" };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SA>> sAtt;
    std::vector<std::unique_ptr<BA>> bAtt;

    float u = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BugzAlignEditor)
};
