#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Anzeige oben: Kick- und Bass-Sub im Punch-Fenster übereinander (vorher/nachher),
// Phasen-Scheibe mit dem aktuellen Winkel und Korrelations-Balken
class AlignView : public juce::Component
{
public:
    explicit AlignView (BugzAlignProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void update(); // vom Editor-Timer aufgerufen

private:
    void drawScope (juce::Graphics&, juce::Rectangle<float>);
    void drawDial (juce::Graphics&, juce::Rectangle<float>);
    void drawMeters (juce::Graphics&, juce::Rectangle<float>);

    BugzAlignProcessor& proc;
    std::array<float, bb::AlignEngine::kSnapPoints> kick {}, before {}, after {};
    float corrBefore = 0, corrAfter = 0, detected = 0, applied = 0;
    int serial = -1, lastKicks = 0;
    float kickFlash = 0, freshness = 0;
    bool hasData = false;
};
