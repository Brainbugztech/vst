#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Breite Verlaufs-Anzeige: Sub von Kick (grün), Bass vorher (grau) und nachher (pink) so, wie sie
// im Ausgang ankommen, darunter die Phasen-Spur mit Kick-Markern, ganz unten die Info-Leiste.
// Mausrad / +/- = Zoom (10 ms bis 8 s), PAUSE hält an, dann ziehen = zurückscrollen,
// Doppelklick = zurück zu LIVE.
class AlignView : public juce::Component
{
public:
    explicit AlignView (BugzAlignProcessor& p);

    void paint (juce::Graphics&) override;
    void resized() override;
    void update(); // vom Editor-Timer aufgerufen

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr float baseW = 860.0f, baseH = 348.0f;

private:
    juce::Rectangle<float> R (float x, float y, float w, float h) const { return { x * k, y * k, w * k, h * k }; }
    juce::Rectangle<float> waveArea() const  { return R (12, 32, 836, 206); }
    juce::Rectangle<float> phaseArea() const { return R (12, 248, 836, 44); }

    void setPaused (bool shouldPause);
    void zoomBy (float factor, float anchorX);
    void fetchVisible();
    void drawWave (juce::Graphics&);
    void drawPhase (juce::Graphics&);
    void drawInfo (juce::Graphics&);

    BugzAlignProcessor& proc;
    juce::TextButton zoomIn { "+" }, zoomOut { "-" }, pauseBtn { "PAUSE" }, beforeBtn { "VORHER" };

    // Ansicht
    float k = 1.0f;
    double spanSec = 1.0;      // sichtbare Zeit
    double endOffset = 0.0;    // Punkte vor dem neuesten Punkt (nur in Pause)
    bool paused = false;
    float dragStartX = 0.0f;
    double dragStartOffset = 0.0;

    // Pausen-Kopie
    std::vector<float> pKick, pBefore, pAfter, pTheta, pMark;
    long long pausedWritten = 0;

    // sichtbarer Ausschnitt (pro Frame)
    std::vector<float> vKick, vBefore, vAfter, vTheta, vMark;
    double rate = 12000.0;
    float shownPeak = 1.0e-3f;

    // Info
    float applied = 0, detected = 0, corrBefore = 0, corrAfter = 0;
    int lastKicks = 0, lastAnalyses = 0;
    float kickFlash = 0;
    bool hasData = false;
};
