#pragma once

#include "PluginProcessor.h"
#include "BrainBugzLook.h"

// Look mit Schriftgroesse, die sich an die Buttonbreite anpasst
class OrchLook : public bb::LookAndFeel
{
public:
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto accent = b.getProperties().getWithDefault ("accent", "tox").toString() == "brain" ? bb::col::brain : bb::col::tox;
        const auto text = b.getButtonText();
        float size = std::min ((float) b.getHeight() * 0.42f, 15.5f);
        auto font = bb::fonts::display (size, true);
        while (size > 9.0f && juce::GlyphArrangement::getStringWidth (font, text) > (float) b.getWidth() - 12.0f)
            font = bb::fonts::display (size -= 0.5f, true);
        g.setFont (font);
        g.setColour (b.getToggleState() ? accent : bb::col::ink);
        g.drawText (text, b.getLocalBounds(), juce::Justification::centred, false);
    }
};

// Engine-Kachel in der linken Leiste
class EngineTile : public juce::Button
{
public:
    EngineTile (int m, const char* title, const char* sub) : juce::Button (title), mode (m), subtitle (sub) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    int mode; juce::String subtitle; float glow = 0;
};

// Tastatur mit farbigen Zonen (Keyswitches, Spielbereich, Drum-Belegung)
class ZoneKeyboard : public juce::MidiKeyboardComponent
{
public:
    ZoneKeyboard (juce::MidiKeyboardState& s, OrchestraProcessor& p)
        : juce::MidiKeyboardComponent (s, horizontalKeyboard), proc (p) {}
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
    juce::Colour zoneColour (int note) const;
    OrchestraProcessor& proc;
};

class OrchestraEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OrchestraEditor (OrchestraProcessor&);
    ~OrchestraEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Panel { juce::Rectangle<int> r; juce::String title; juce::Colour accent; };

    void timerCallback() override;
    void setupKnob (Knob& k, const char* id, const char* text, const char* accent = "tox");
    void placeKnob (Knob& k, juce::Rectangle<int> cell, bool small);
    void updateModeUi();
    void showPresetMenu();
    void savePresetDialog();
    void selectFactory (int idx);

    void drawStage (juce::Graphics&, juce::Rectangle<float>);
    void drawStrings (juce::Graphics&, juce::Rectangle<float>);
    void drawPads (juce::Graphics&, juce::Rectangle<float>);
    void drawDrums (juce::Graphics&, juce::Rectangle<float>);
    void drawSteps (juce::Graphics&, juce::Rectangle<float>);
    std::vector<std::pair<int, juce::String>> currentKitPads() const;
    juce::Rectangle<float> padRect (int index, int count, juce::Rectangle<float> area) const;

    OrchestraProcessor& proc;
    OrchLook look;
    juce::Image logo, brain;

    EngineTile tiles[4] { { 0, "STRINGS", "6 ARTICULATIONS" }, { 1, "CELLOS", "CELLOS + BASSES" },
                          { 2, "ATMOS PADS", "10 SOURCES / MORPH" }, { 3, "DRUMS", "5 KITS / 160+ SOUNDS" } };
    juce::TextButton articBtn[orch::A_NUM], kitBtn[orch::K_NUM], rhyBtn[3], legatoBtn { "LEGATO" };
    juce::TextButton presetBtn, prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" };
    juce::ComboBox padABox, padBBox, patternBox, rateBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> padAAtt, padBAtt, patternAtt, rateAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> legatoAtt;
    juce::Slider morphSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAtt;
    ZoneKeyboard keyboard;

    Knob kAttack, kRelease, kTone, kTune, kDrive, kReverb, kSpace, kWidth, kVolume,
         kEnsemble, kHumanize, kBasses, kGlide, kEvolve, kAir, kShimmer, kMotion,
         kSub, kStack, kPunch, kDecay, kSwing, kDepth;

    std::vector<Panel> panels;
    juce::Rectangle<int> stageArea, stepArea, morphLabelArea;
    int shownMode = -1, shownArtic = -1, shownKit = -1, shownRhy = -1;
    float phase = 0, pulse = 0, levelSm = 0;
    std::array<float, 64> padGlow {};
    struct Ripple { juce::Point<float> p; float age; };
    std::vector<Ripple> ripples;
    juce::uint32 lastHitSeen = 0;
    struct Particle { float x, y, vx, vy, size, hue; };
    std::vector<Particle> particles;
    std::array<float, 128> stringEnergy {};
    std::unique_ptr<juce::AlertWindow> saveWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchestraEditor)
};
