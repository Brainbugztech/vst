#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"
#include "StepGrid.h"

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
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
};

class BleepEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BleepEditor (BleepProcessor&);
    ~BleepEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int baseW = 1000, baseH = 720;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::String label;
        juce::Slider slider;
        std::unique_ptr<SliderAtt> att;
        juce::Rectangle<float> labelArea;   // in Basis-Einheiten
    };

    struct Panel
    {
        juce::String title, tag;
        std::vector<Knob*> knobs;
        juce::Rectangle<float> area;        // in Basis-Einheiten
        float cell = 46.0f;
    };

    struct Choice
    {
        juce::String label;
        juce::ComboBox box;
        std::unique_ptr<ComboAtt> att;
        juce::Rectangle<float> labelArea;
    };

    struct Toggle
    {
        juce::TextButton button;
        std::unique_ptr<ButtonAtt> att;
    };

    struct Bolt
    {
        double start = 0.0;
        int target = 0;
        float yOffset = 0.0f;
    };

    void timerCallback() override;
    Knob* makeKnob (const juce::String& id, const juce::String& label);
    Choice* makeChoice (const juce::String& id, const juce::String& label);
    Toggle* makeToggle (const juce::String& id, const juce::String& text, const juce::String& style);
    void placeKnobs (Panel&, float x, float labelY, float sliderY, float sliderH);
    void place (juce::Component&, juce::Rectangle<float> base);
    juce::Rectangle<float> s (juce::Rectangle<float> base) const { return base * u; }
    void setSource (int index);
    void rebuildWaves();

    void paintPanel (juce::Graphics&, juce::Rectangle<float> base, const juce::String& title, bool highlight = false);
    void paintHeader (juce::Graphics&);
    void paintMech (juce::Graphics&);
    void paintSlots (juce::Graphics&);
    juce::Point<float> muzzle() const;
    juce::Point<float> slotTarget (int slot) const;

    BleepProcessor& proc;
    BleepLook look;
    float u = 1.0f;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Toggle>> toggles;

    // Generator-Reihe
    std::vector<Panel> genPanels;
    Panel generatePanel;
    juce::TextButton generateBtn { "GENERATE" }, rerenderBtn { "" };
    Toggle* pitchLink = nullptr;

    // Header
    juce::TextButton liveBtn { "LIVE" }, slotsBtn { "SLOTS" };
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, menuBtn { "..." };
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::ScopedMessageBox confirmBox;
    int presetTick = 0;
    void rebuildPresetMenu();
    void updatePresetText();
    void stepPreset (int delta);
    void savePreset (bool askName);
    void showPresetMenu();
    juce::Image logo, mech;
    juce::Typeface::Ptr graffiti;

    // Slots
    std::array<juce::Rectangle<float>, BleepProcessor::kNumSlots> slotTiles, waveAreas;
    std::array<Toggle*, BleepProcessor::kNumSlots> slotLocks {};
    std::array<Knob*, BleepProcessor::kNumSlots * 2> slotKnobs {};
    std::array<bleep::SlotBuffer::Ptr, BleepProcessor::kNumSlots> shownSlots;
    std::array<juce::Path, BleepProcessor::kNumSlots> wavePaths;
    std::array<int, BleepProcessor::kNumSlots> seenHits {};
    std::array<float, BleepProcessor::kNumSlots> hitGlow {}, laserGlow {};

    // Mech
    juce::Rectangle<float> mechArea;
    std::vector<Bolt> bolts;
    int seenAccents = 0;
    float muzzleFlash = 0.0f;

    // Sequencer
    juce::Rectangle<float> seqArea;
    StepGrid grid { proc };
    juce::TextButton rollBtn { "ROLL" };
    std::vector<Knob*> seqKnobs;
    std::vector<Choice*> seqChoices;
    Toggle* arpOn = nullptr;
    Toggle* restart = nullptr;

    // Effekte
    std::vector<Panel> fxPanels;
    std::vector<Toggle*> fxPower;
    Choice* filterType = nullptr;
    Choice* delayTime = nullptr;
    Toggle* pingPong = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepEditor)
};
