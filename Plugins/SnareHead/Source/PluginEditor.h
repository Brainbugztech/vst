#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"

// BrainBugz Snarehead v1.0 - Oberflaeche nach dem Design-Kit (Designflaeche 1579 x 996, stufenlos skaliert)

namespace sh
{
// Farben aus design_tokens.json
namespace col
{
    inline const juce::Colour canvas    { 0xff080512 };
    inline const juce::Colour panel     { 0xff110a1c };
    inline const juce::Colour panelDeep { 0xff07040e };
    inline const juce::Colour border    { 0xff5d4276 };
    inline const juce::Colour magenta   { 0xfff12cef };
    inline const juce::Colour purple    { 0xffb23cff };
    inline const juce::Colour violet    { 0xff8136cc };
    inline const juce::Colour cyan      { 0xff14e7f6 };
    inline const juce::Colour ink       { 0xfff4e9fc };
    inline const juce::Colour inkDim    { 0xffb89ec8 };
    inline const juce::Colour inkMuted  { 0xff88799c };
    inline const juce::Colour value     { 0xfff5f0fc };
    inline const juce::Colour graphFill { 0xff9b2feb };
    inline const juce::Colour metal     { 0xffd0b8e1 };
}

namespace fonts
{
    juce::Font heading (float h);   // Rajdhani Bold
    juce::Font label (float h);     // Rajdhani SemiBold
    juce::Font value (float h);     // Oxanium Regular
    juce::Font valueBold (float h); // Oxanium SemiBold
}

constexpr int kDesignW = 1579, kDesignH = 996;

class LookAndFeel : public bb::LookAndFeel
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
private:
    juce::Image knobSmall, knobLarge;
};

// Slider, der im Note-Modus auf Halbtoene rastet
class NoteSlider : public juce::Slider
{
public:
    std::function<bool()> noteMode;
    double snapValue (double attemptedValue, DragMode) override;
};

// Regler: Sprite-Knopf, Name darueber, Wert darunter (Wert anklicken = eintippen)
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& label, bool big = false);
    // Lage in Designkoordinaten: Mittelpunkt, Durchmesser, Abstand Name/Wert zur Mitte, Versatz des Namens
    void place (int cx, int cy, int diameter, int labelDy, int valueDy, int labelDx = 0);
    void resized() override;
    void paint (juce::Graphics&) override;
    void enableNoteMode (SnareHeadProcessor& p, const juce::String& stateKey);
    bool isNoteMode() const { return proc != nullptr && proc->getNoteMode (noteKey); }
    void refreshValue();
    void setDimmed (bool d);

    NoteSlider slider;
    juce::TextButton noteBtn;   // Hz/NOTE - wird vom Editor platziert

private:
    juce::RangedAudioParameter* param;
    juce::String name;
    bool isBig, dimmed = false;
    int diam = 96, lDy = 54, vDy = 62, lDx = 0;
    juce::Label valueLabel;
    SnareHeadProcessor* proc = nullptr;
    juce::String noteKey;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Res-EQ Anzeige: Glocke + Ring-Teiltoene + Note (ziehen = Freq/Boost, Mausrad = Q)
class EqView : public juce::Component
{
public:
    explicit EqView (SnareHeadProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    SnareHeadProcessor& proc;
    juce::Rectangle<float> plot() const;
    float xToHz (float x) const;
    float hzToX (float hz) const;
    void setFromMouse (juce::Point<float>);
};

// Huellkurve (Shape) bzw. Reverb-Fahne mit Gate (ziehen = Gate-Zeit)
class GateView : public juce::Component
{
public:
    GateView (SnareHeadProcessor& p, bool showReverb) : proc (p), reverb (showReverb) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    float sinceHitMs = 1e9f;
private:
    SnareHeadProcessor& proc;
    bool reverb;
    float spanMs = 1000.0f;
    juce::Rectangle<float> plot() const;
    void setGateFromX (float x);
};

// Maskottchen: pulsiert beim Schlag, wackelt mit Xtreme Noisy. Klick = anspielen.
class MascotView : public juce::Component
{
public:
    explicit MascotView (SnareHeadProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick (bool hit, float level);
private:
    SnareHeadProcessor& proc;
    juce::Image img;
    float pulse = 0.0f, glow = 0.0f, shakeX = 0.0f, shakeY = 0.0f, rot = 0.0f;
    juce::Random rng;
};

// "Logo = Anspielen" mit Pegelbalken
class AuditionStrip : public juce::Component
{
public:
    explicit AuditionStrip (SnareHeadProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { proc.audition(); }
    float meter = 0.0f;
private:
    SnareHeadProcessor& proc;
};

// Kleine Anzeige unten links: Version + Lizenz-/Update-Status. Klick = Center-Menue.
class CenterCorner : public juce::Component, public juce::SettableTooltipClient
{
public:
    CenterCorner();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void setState (const juce::String& info, juce::Colour dot, bool highlight);
    std::function<void()> onClick;
    float pulse = 0.0f;
private:
    juce::String version, info;
    juce::Colour dotColour;
    bool hot = false;
};

} // namespace sh

class SnareHeadEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SnareHeadEditor (SnareHeadProcessor&);
    ~SnareHeadEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Alles liegt auf einer Flaeche in Designgroesse; der Editor skaliert sie nur.
    class Canvas : public juce::Component
    {
    public:
        explicit Canvas (SnareHeadEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
    private:
        SnareHeadEditor& owner;
    };

    void timerCallback() override;
    void refreshPresetBox();
    void savePreset (bool askName);
    void showPresetMenu();
    void layout();
    void buildStaticLayer();

    SnareHeadProcessor& proc;
    sh::LookAndFeel lnf;
    Canvas canvas { *this };
    juce::Image staticLayer;

    sh::MascotView mascot;
    sh::EqView eqView;
    sh::GateView envView, gateView;
    sh::AuditionStrip audition;

    // Lizenz + Updates + VST-Ordner (Brain Bugz Center)
    sh::CenterCorner corner;
    juce::TextButton licBtn { "LIZENZ" };
   #if BUGZ_LICENSE
    std::unique_ptr<bbcenter::CenterBar> centerBar;   // unsichtbar: liefert Dialoge + Updater-Logik
   #endif
    void refreshCorner();
    void showCenterMenu();
    void showLicense();

    sh::Knob tune, spread, metal, punch, ring;
    sh::Knob noise, noiseTone;
    sh::Knob hold, release;
    sh::Knob eqFreq, eqGain, eqQ;
    sh::Knob verbSize, verbMix, verbGate, verbCut;
    sh::Knob noisy, width, volume;

    juce::TextButton linkBtn { "LINK" }, keyBtn { "KEY TRACK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAtt, keyAtt;
    juce::ComboBox syncBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAtt;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, moreBtn { "..." };
    juce::String shownPresetName;
    bool shownModified = false;
    int shownPresetCount = -1;

    int lastHits = 0;
    float noisyGlow = 0.0f, hitFlash = 0.0f;
    double lastHitTime = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareHeadEditor)
};
