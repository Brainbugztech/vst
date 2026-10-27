#pragma once

#include "PluginProcessor.h"
#include "WormholeLook.h"
#include "Tunnel.h"

namespace whl
{
// Einfache Text-Schaltfläche im Wormhole-Stil
class PillButton : public juce::Button
{
public:
    enum class Style { primary, outline, link };
    PillButton (const juce::String& name, Style s) : juce::Button (name), style (s) {}
    float progress = -1.0f;    // >= 0: Fortschrittsanzeige (Render)
    std::function<void()> onDragOut;   // Ziehen aus der Schaltfläche heraus

    void paintButton (juce::Graphics&, bool hover, bool down) override;
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (onDragOut && isEnabled() && e.getDistanceFromDragStart() > 6 && ! dragStarted)
        {
            dragStarted = true;
            setState (juce::Button::buttonNormal);
            onDragOut();
            return;
        }
        juce::Button::mouseDrag (e);
    }
    void mouseDown (const juce::MouseEvent& e) override { dragStarted = false; juce::Button::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragStarted) { dragStarted = false; setState (juce::Button::buttonNormal); return; }
        juce::Button::mouseUp (e);
    }

private:
    Style style;
    bool dragStarted = false;
};

// Live / Render Umschalter
class ModeSwitch : public juce::Component
{
public:
    std::function<void (bool render)> onChange;
    bool renderMode = false;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool r = e.position.x > getWidth() * 0.5f;
        if (r != renderMode && onChange) onChange (r);
    }
};

// BPM-Eingabefeld
class BpmField : public juce::Component
{
public:
    BpmField();
    void paint (juce::Graphics&) override;
    void resized() override { label.setBounds (getLocalBounds()); }
    void setValue (double bpm);
    std::function<void (double)> onEdit;
    bool accentOutline = false;
    juce::Label label;
};

// Clip-Anzeige: Original grau, Ergebnis im Verlauf, Attacks markiert
class WaveView : public juce::Component
{
public:
    explicit WaveView (WormholeProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override { dragDone = false; }

    void update();   // vom Editor-Timer
    std::function<void()> onDragOut;
    std::function<void()> onLoad, onCapture;
    bool canDragOut = false;

    PillButton loadBtn { "LADEN", PillButton::Style::link };
    PillButton captureBtn { "AUFNEHMEN", PillButton::Style::link };

private:
    WormholeProcessor& proc;
    std::shared_ptr<const wh::Clip> clip;
    std::shared_ptr<const wh::Result> result;
    bool stale = false, live = false, capturing = false, dragDone = false;
    double predictedRatio = 1.0;
    float progress = -1.0f;
};

// Gesamte Oberfläche in Design-Koordinaten 960 x 600 (wird vom Editor skaliert)
class Panel : public juce::Component,
              public juce::FileDragAndDropTarget,
              private juce::Timer
{
public:
    explicit Panel (WormholeProcessor& p);
    ~Panel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void dragResultOut();
    void chooseFile();
    void toggleCapture();
    void startOrCancelRender();
    void refreshTexts();
    void setMode (bool render);

    WormholeProcessor& proc;
    LookAndFeel lnf;
    juce::TooltipWindow tips { this, 700 };
    Tunnel tunnel;
    Logo logo;
    ModeSwitch modeSwitch;

    juce::Slider tempo, pitch, fine, formant;
    juce::ToggleButton formantLock { "FORMANT LOCK" }, attackLock { "ATTACK LOCK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aTempo, aPitch, aFine, aFormant;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aFormantLock, aAttackLock;

    BpmField srcBpm, dstBpm;
    PillButton hostBtn { "PROJEKT", PillButton::Style::link };
    WaveView wave;
    PillButton dragBtn { "IN ABLETON ZIEHEN", PillButton::Style::outline };
    PillButton renderBtn { "RENDER", PillButton::Style::primary };

    std::unique_ptr<juce::FileChooser> chooser;
    int lastGeneration = -1;
    bool userLoadPending = false, dropHover = false, live = true;
    juce::String metaText, bpmNote;
};
} // namespace whl

class WormholeEditor : public juce::AudioProcessorEditor
{
public:
    explicit WormholeEditor (WormholeProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (whl::col::bgEdge); }

private:
    whl::Panel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WormholeEditor)
};
