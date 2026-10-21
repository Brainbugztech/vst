#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Step-Editor: 16 Spalten.
// - Oben der Punkt: Klick = Akzent an/aus (Akzent = Velocity 115, sonst 90)
// - Mitte: Tonhoehe ziehen (auch ueber mehrere Steps hinweg malen)
// - Unterer Balken: Wahrscheinlichkeit, hoch/runter ziehen
// - Rechtsklick: Step sperren (Roll laesst ihn in Ruhe)
// - Doppelklick in der Mitte: Tonhoehe auf 0
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (BleepProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { zone = Zone::none; }
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum class Zone { none, accent, pitch, prob };

    int stepAt (float x) const;
    juce::Rectangle<float> column (int i) const;
    float pitchTop() const    { return 18.0f; }
    float pitchBottom() const { return (float) getHeight() - 36.0f; }
    float probY() const       { return (float) getHeight() - 30.0f; }
    int maxSemis() const;
    void setPitchFromY (int step, float y);

    BleepProcessor& proc;
    Zone zone = Zone::none;
    int dragStep = -1, startProb = 0;
    float startY = 0.0f;
};
