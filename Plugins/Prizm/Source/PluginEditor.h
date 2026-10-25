#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>

#include "PluginProcessor.h"
#include "PrizmLookAndFeel.h"
#include "PitchMatrix.h"

// Small animated prism: a white beam enters, a spectrum fans out.
class PrismGlyph : public juce::Component
{
public:
    void setPhase (float newPhase) { phase = newPhase; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    float phase = 0.0f;
};

// Pill-shaped two-way switch with a sliding thumb.
class ModeSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int)> onChange;

    void setMode (int newMode);
    bool advance(); // animates the thumb, returns true while moving
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    int mode = 0;
    float thumb = 0.0f; // 0 = left, 1 = right
};

// Die 12 Noten: grün = in der Tonart, Leuchten = was gerade reinkommt (Eingang der Pitch Matrix).
class KeyStrip : public juce::Component, public juce::TooltipClient
{
public:
    std::function<void (int)> onToggle;
    std::function<void (int, float, bool, bool)> onWeight; // Note, 0..1, Geste Start, Geste Ende
    std::function<float (int)> getWeight;

    void setState (prizm::PitchMask newMask, int newRoot, int newSource); // Quelle: 0 = Tasten, 1 = MIDI, 2 = Sidechain
    void setEnergy (const std::array<float, 12>& e) { energy = e; repaint(); }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    juce::String getTooltip() override;

private:
    int cellAt (int x) const;
    juce::Rectangle<float> cellBounds (int pc) const;

    prizm::PitchMask mask = 0;
    int root = -1;
    int hover = -1;
    int source = 0;
    int dragNote = -1;
    float dragStartWeight = 1.0f;
    bool dragging = false;
    std::array<float, 12> energy {};
};

class PrizmAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PrizmAudioProcessorEditor (PrizmAudioProcessor&);
    ~PrizmAudioProcessorEditor() override;
#if PRIZM_HEADLESS_TEST
    void tickForTest (int ticks) { for (int i = 0; i < ticks; ++i) timerCallback(); }
#endif

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMode (int mode);
    void updateEnergy();

    struct Knob
    {
        juce::Slider slider;
        juce::String title, hint;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    PrizmAudioProcessor& processor;
    prizm::PrizmLookAndFeel lnf;
    juce::Image logo, texture;
    juce::Image staticLayer; // texture + panels + logo + title, rendered once per size
    void renderStaticLayer (float scale);
    void drawHoloPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& tag, const juce::String& code);
    juce::Rectangle<int> scanBand (juce::Rectangle<int> panel, float phase) const;

    PrismGlyph prism;
    ModeSwitch modeSwitch;
    juce::TextButton midiButton { "MIDI KEY" };
    juce::TextButton learnButton { "LEARN" };
    juce::TextButton scButton { "SIDECHAIN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scAttachment;
    juce::ComboBox presetBox, stereoBox;
    juce::TextButton savePresetButton { "SAVE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoAttachment;
    void refreshPresetBox();
    void askPresetName();
    int shownPreset = -1;
    PitchMatrix pitchMatrix;
    void updateMatrix (int mode);
    std::array<float, 12> shownInput {};
    float inputRef = 0.0f;
    juce::TextButton majToMinButton { "MAJ > MIN" }, minToMajButton { "MIN > MAJ" }, resetMapButton { "RESET MAP" };
    juce::Rectangle<int> legendArea;
    juce::String learnMessage;
    int learnMessageTicks = 0;
    juce::ComboBox keyBox, scaleBox;
    KeyStrip keyStrip;
    std::array<Knob, 11> knobs; // 0-4 SIGNAL, 5-10 RANGE + SPECTRAL
    static constexpr size_t kFormantKnob = 9;
    static constexpr size_t kShiftKnob = 8;
    juce::TooltipWindow tooltips { this, 700 };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiAttachment;

    juce::String latencyText;
    int sourceShown = 0;
    float animPhase = 0.0f;
    std::array<float, 12> shownEnergy {};
    float energyRef = 0.0f;
    uint32_t lastCounter = 0;
    int idleTicks = 0;

    juce::Rectangle<int> logoArea, titleArea, keyPanel, keyLabelArea, scaleLabelArea, stripLabelArea, readoutArea, knobPanel, extraPanel, footerArea;
    juce::Rectangle<int> lastScanA, lastScanB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrizmAudioProcessorEditor)
};
