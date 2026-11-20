#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PlutoLook.h"
#include "Widgets.h"

// Kleine Wellenform-Vorschau neben den Oszillator-Reglern (zeigt auch die Wavetable-Position)
class WavePreview : public juce::Component
{
public:
    WavePreview (juce::AudioProcessorValueTreeState& s, int osc, const pluto::WavetableBank& b) : bank (b)
    {
        auto rp = [this] { repaint(); };
        wave.attach (s, pluto::oid (osc, "wave"), rp);
        wt.attach (s, pluto::oid (osc, "wt"), rp);
        pwm.attach (s, pluto::oid (osc, "pwm"), rp);
        setInterceptsMouseClicks (false, false);
    }
    void paint (juce::Graphics& g) override;

private:
    const pluto::WavetableBank& bank;
    pluto::ParamLink wave, wt, pwm;
};

class PlutoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int baseW = 1180, baseH = 1034;

    explicit PlutoEditor (PlutoProcessor&);
    ~PlutoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct TextItem { juce::Rectangle<int> r; juce::String t; juce::Justification j; };
    struct BoxItem  { juce::Rectangle<int> r; juce::String t; bool section; };

    class Content : public juce::Component
    {
    public:
        std::vector<BoxItem> boxes;
        std::vector<TextItem> texts;
        std::vector<juce::Rectangle<int>> nameBoxes;
        juce::StringArray nameTexts;
        void paint (juce::Graphics&) override;
    };

    PlutoProcessor& proc;
    pluto::PlutoLook look;
    Content content;
    juce::TooltipWindow tooltips { this, 700 };

    juce::OwnedArray<juce::Component> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;

    std::vector<pluto::LfoDisplay*> lfoDisplays;
    juce::ComboBox* presetBox = nullptr;
    int lastPresetCounter = -1;

    // Bausteine
    juce::Slider* knob (const juce::String& id, juce::Rectangle<int> r, const juce::String& label, bool textBox);
    juce::Slider* vSlider (const juce::String& id, juce::Rectangle<int> r, bool textBox);
    juce::Slider* hSlider (const juce::String& id, juce::Rectangle<int> r, int textBoxW);
    juce::ComboBox* combo (const juce::String& id, juce::Rectangle<int> r);
    juce::ToggleButton* toggle (const juce::String& id, juce::Rectangle<int> r, const juce::String& text);
    void text (juce::Rectangle<int> r, const juce::String& t, juce::Justification j = juce::Justification::centred);
    template <typename T> T* own (T* c) { owned.add (c); content.addAndMakeVisible (c); return c; }

    void buildTopBar();
    void buildOsc (int o, int x, int y);
    void buildFilter (int f, int x, int y);
    void buildMixer (int x, int y);
    void buildSync (int x, int y);
    void buildMaster (int x, int y);
    void buildLfo (int l, int x, int y);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlutoEditor)
};
