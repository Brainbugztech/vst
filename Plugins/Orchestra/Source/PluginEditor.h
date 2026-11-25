#pragma once

#include "PluginProcessor.h"
#include "BrainBugzLook.h"

class OrchestraEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OrchestraEditor (OrchestraProcessor&);
    ~OrchestraEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void timerCallback() override;
    void setupKnob (Knob& k, const char* id, const char* text, const char* accent = "tox");
    void updateModeUi();
    void mouseDown (const juce::MouseEvent&) override;
    void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r);
    void drawModeIcon (juce::Graphics& g, int mode, juce::Rectangle<float> r, juce::Colour c);
    void selectPreset (int idx);

    OrchestraProcessor& proc;
    bb::LookAndFeel look;

    juce::TextButton modeBtn[4], articBtn[4];
    juce::ComboBox presetBox;
    juce::Image logo, brain;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };

    Knob kAttack, kRelease, kTone, kTune, kEnsemble, kHumanize, kAir, kShimmer, kMotion, kPunch, kDecay,
         kReverb, kSpace, kWidth, kVolume;

    juce::Rectangle<int> displayArea, modeKnobArea;
    int shownMode = -1, shownArtic = -1, shownPreset = -1;
    float phase = 0, pulse = 0, levelSm = 0;
    std::array<float, 28> padGlow {};
    juce::uint32 lastHitSeen = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchestraEditor)
};
