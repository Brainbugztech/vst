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
    inline const juce::Colour knob   { 0xff1e1c1e };
    inline const juce::Colour red    { 0xffff2a3a };
    inline const juce::Colour gold   { 0xffe2b53e };
    inline const juce::Colour ink    { 0xffece6e7 };
    inline const juce::Colour inkDim { 0xffa89c9e };
}

// Regler und Taster im Look des UI-Entwurfs: dunkle Kappe, roter Zeiger mit Glow, Gold fuer Schalter
class BleepLook : public juce::LookAndFeel_V4
{
public:
    BleepLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class BleepEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BleepEditor (BleepProcessor&);
    ~BleepEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1000, baseH = 720;

private:
    struct Knob
    {
        juce::String label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::Rectangle<float> labelArea;
    };

    struct Group
    {
        juce::String title, tag;
        std::vector<Knob*> knobs;
        bool hasToggle = false;
        juce::Rectangle<float> area;
    };

    void timerCallback() override;
    Knob* makeKnob (const char* id, const juce::String& label);

    BleepProcessor& proc;
    BleepLook look;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Group> groups;
    juce::TextButton pitchLinkBtn { "PITCH-LINK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pitchLinkAtt;

    juce::Rectangle<float> stageArea;
    int seenNotes = 0;
    float flash = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepEditor)
};
