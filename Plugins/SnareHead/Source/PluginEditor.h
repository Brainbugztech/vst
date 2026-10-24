#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"

namespace sh
{
namespace col
{
    inline const juce::Colour bg      { 0xff07030d };
    inline const juce::Colour panel   { 0xff120a1e };
    inline const juce::Colour panel2  { 0xff1c0f2e };
    inline const juce::Colour line    { 0xff311a4d };
    inline const juce::Colour purple  { 0xffa14dff };
    inline const juce::Colour magenta { 0xffff3df2 };
    inline const juce::Colour lilac   { 0xffd7b4ff };
    inline const juce::Colour ink     { 0xfff3eaff };
    inline const juce::Colour inkDim  { 0xffa391bf };
    inline const juce::Colour deep    { 0xff3b0f6e };
}

juce::Font dripFont (float height);   // Creepster (Fallback: Chakra Petch Bold)

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
};

// Slider, der im Note-Modus auf Halbtoene rastet
class NoteSlider : public juce::Slider
{
public:
    std::function<bool()> noteMode;
    double snapValue (double attemptedValue, DragMode) override;
};

// Regler mit Name und Wert (Wert anklicken = eintippen, optional Hz/Note-Umschalter)
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& label, bool big = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    void enableNoteMode (SnareHeadProcessor& p, const juce::String& stateKey);
    bool isNoteMode() const { return proc != nullptr && proc->getNoteMode (noteKey); }
    void refreshValue();
    NoteSlider slider;
    bool dimmed = false;

private:
    juce::RangedAudioParameter* param;
    juce::String name;
    bool isBig;
    juce::Label valueLabel;
    juce::TextButton noteBtn;
    SnareHeadProcessor* proc = nullptr;
    juce::String noteKey;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Wellenform des eigenen Samples: Start ziehen, Klick (leer) = laden
class SampleView : public juce::Component
{
public:
    explicit SampleView (SnareHeadProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onLoadRequest;
    float sinceHitMs = 1e9f;
    bool dragOver = false;
    juce::String status;          // kurze Meldung (z. B. Analyse-Ergebnis)
    double statusUntil = 0.0;
private:
    SnareHeadProcessor& proc;
    std::vector<std::pair<float, float>> peaks;
    int shownVersion = -1, shownWidth = -1;
    double lengthMs = 0.0;
    void rebuildPeaks();
    void setStartFromX (float x);
};

// Res-EQ Anzeige: Glocke + Ring-Teiltoene + Note
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
    float xToHz (float x) const;
    float hzToX (float hz) const;
    void setFromMouse (juce::Point<float>);
};

// Huellkurve + Reverb-Fahne + Gate
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
    void setGateFromX (float x);
};

// Animiertes Logo: pulsiert beim Schlag, wackelt mit Xtreme Noisy, tropft. Klick = anspielen.
class LogoView : public juce::Component
{
public:
    explicit LogoView (SnareHeadProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick (bool hit, float level);
private:
    SnareHeadProcessor& proc;
    juce::Image logo;
    float pulse = 0.0f, glow = 0.0f, shakeX = 0.0f, shakeY = 0.0f, rot = 0.0f;
    struct Drop { float x, y, vy, r, life; };
    std::vector<Drop> drops;
    juce::Random rng;
};

} // namespace sh

class SnareHeadEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit SnareHeadEditor (SnareHeadProcessor&);
    ~SnareHeadEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { sampleView.dragOver = true; sampleView.repaint(); }
    void fileDragExit (const juce::StringArray&) override { sampleView.dragOver = false; sampleView.repaint(); }

private:
    void timerCallback() override;
    void openSampleChooser();
    void loadSample (const juce::File&);
    void runAnalyze();
    void refreshPresetBox();
    void savePreset (bool askName);
    void showPresetMenu();
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, bool hot = false);

    SnareHeadProcessor& proc;
    sh::LookAndFeel lnf;

    sh::LogoView logo;
    sh::EqView eqView;
    sh::GateView envView, gateView;

    sh::Knob tune, spread, metal, punch, ring;
    sh::Knob noise, noiseTone;
    sh::Knob hold, release;
    sh::Knob eqFreq, eqGain, eqQ;
    sh::Knob verbSize, verbMix, verbGate, verbCut;
    sh::Knob noisy, width, volume;
    sh::Knob smpLevel, smpPitch, smpStart;
    sh::SampleView sampleView;
    juce::TextButton loadBtn { "LOAD" }, clearBtn { "X" }, revBtn { "REV" }, shapeBtn { "SHAPE" }, analyzeBtn { "ANALYZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> revAtt, shapeAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::TextButton linkBtn { "LINK" }, keyBtn { "KEY TRACK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAtt, keyAtt;
    juce::ComboBox syncBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAtt;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, moreBtn { "..." };
    juce::String shownPresetName;
    bool shownModified = false;
    int shownPresetCount = -1;

    juce::Rectangle<int> rRing, rNoise, rShape, rEq, rVerb, rNoisy, rOut, rLogo, rSticker, rSample;
    int lastHits = 0;
    float stickerPulse = 0.0f, meter = 0.0f;
    double lastHitTime = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareHeadEditor)
};
