#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ceq::ui
{
namespace col
{
    inline const juce::Colour bgTop    { 0xff26282d };
    inline const juce::Colour bgBottom { 0xff1a1b1f };
    inline const juce::Colour well     { 0xff141518 };
    inline const juce::Colour line     { 0xff34373e };
    inline const juce::Colour lineSoft { 0xff2b2d33 };
    inline const juce::Colour purple   { 0xffa46bff };
    inline const juce::Colour purpleHi { 0xffcfadff };
    inline const juce::Colour purpleLo { 0xff5b3a96 };
    inline const juce::Colour ink      { 0xffe9e6f0 };
    inline const juce::Colour inkDim   { 0xff8e8b98 };
    inline const juce::Colour inkFaint { 0xff5a5862 };
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;

private:
    void drawCrossover (juce::Graphics&, juce::Rectangle<float>, juce::Slider&);
};

// Frequenzgang, wird nur bei Änderung neu gezeichnet
class CurveView : public juce::Component
{
public:
    explicit CurveView (ClubEQProcessor& p) : proc (p) { setOpaque (false); }
    void paint (juce::Graphics&) override;

private:
    ClubEQProcessor& proc;
};
} // namespace ceq::ui

class ClubEQEditor : public juce::AudioProcessorEditor,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::AsyncUpdater
{
public:
    explicit ClubEQEditor (ClubEQProcessor&);
    ~ClubEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;
    void renderBackground();

    ClubEQProcessor& proc;
    ceq::ui::Look look;

    struct Band
    {
        juce::Slider knob;
        juce::Label value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        const char* id = nullptr;
        juce::String name, freq;
    };
    std::array<Band, 3> bands;

    ceq::ui::CurveView curve;
    juce::TextButton eqButton { "EQ" }, isoButton { "ISO" };
    juce::Slider xLow, xHigh;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xLowAtt, xHighAtt;

    juce::Path logoOutline, logoLetters;
    juce::Image background;
    float bgScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClubEQEditor)
};
