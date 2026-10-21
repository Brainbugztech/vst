#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"

// Band-Anzeige oben im Plugin:
// - waagerecht die Frequenz (20 Hz – 20 kHz, logarithmisch), die vier Bänder als farbige Blöcke
// - Weichen (senkrechte Linien mit Griff) seitlich ziehen
// - im Band hoch/runter ziehen = Drive des Bandes (Höhe des Blocks)
// - Oberkante "fuzzt" mit dem Pegel des Bandes
class BandView : public juce::Component, private juce::Timer
{
public:
    explicit BandView (BugFuzzProcessor&);
    ~BandView() override;

    void paint (juce::Graphics&) override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    void timerCallback() override;

    // Ziele: 0..3 = Band, 4..6 = Weiche, -1 = nichts
    int itemAt (juce::Point<float>) const;
    juce::Rectangle<float> plot() const;
    float freqToX (float hz) const;
    float xToFreq (float x) const;
    void setBand (int b, float v);
    void setXover (int i, float hz);
    void step (int item, float dir, bool fine);

    BugFuzzProcessor& proc;
    int hover = -1, dragItem = -1, focusItem = 1;
    float startValue = 0.0f;
    std::array<float, 4> level {};
    juce::uint32 frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandView)
};
