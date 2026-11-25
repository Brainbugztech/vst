#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BoothLook.h"

// Frequenzgang, wird nur bei Parameteränderung neu gezeichnet
class BoothEQCurve : public juce::Component
{
public:
    BoothEQCurve (BoothEQProcessor& p, booth::Accent a) : proc (p), accent (a) {}
    void paint (juce::Graphics&) override;

private:
    BoothEQProcessor& proc;
    booth::Accent accent;
};

class BoothEQEditor : public juce::AudioProcessorEditor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::AsyncUpdater
{
public:
    explicit BoothEQEditor (BoothEQProcessor&);
    ~BoothEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;

    BoothEQProcessor& proc;
    booth::Accent accent = booth::Accent::violet();
    booth::Look look { accent };
    booth::Strip strip;

    struct Band
    {
        juce::Slider knob;
        juce::Label value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        const char* id = nullptr;
    };
    std::array<Band, 3> bands;

    BoothEQCurve curve;
    std::unique_ptr<booth::HoldPad> killLow, killMid;
    juce::TextButton isoButton { "ISO" }, eqButton { "EQ" };
    juce::Slider xLow, xHigh;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xLowAtt, xHighAtt;
    std::unique_ptr<booth::HelpToggle> help;

    juce::Path logoOutline, logoLetters;
    juce::Image background;
    float bgScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoothEQEditor)
};
