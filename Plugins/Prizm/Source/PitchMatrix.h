#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

#include "PrizmLookAndFeel.h"
#include "Scales.h"

// Pitch Matrix: zeigt unter den Notentasten live, wo jede eingehende Note landet.
// Oben (IN) sitzt jede Note in der Spalte ihrer Taste, unten (OUT) die Ausgangsnoten.
// Strahl geradeaus = bleibt, cyan Kurve = SHIFT, pinke Kurve = MAP, verblasst mit × = gefiltert/aus.
// Bedienung: vom Strahl zu einer Note unten ziehen = umleiten, Rechtsklick = Menü,
// Doppelklick = zurück auf normal.
class PitchMatrix : public juce::Component, public juce::TooltipClient
{
public:
    struct Route
    {
        enum Kind { pass, shift, map, mute, filtered };
        Kind kind = pass;
        int target = 0;         // Zielnote (Tonklasse)
        float targetPos = 0.0f; // Spalte, auf der der Strahl landet (bei Shift < 100 % dazwischen)
        float weight = 1.0f;    // 0..1, wie stark die Note durchkommt
        float dry = 0.0f;       // Anteil Original, der zusätzlich unverändert durchgeht
        int ghost = -1;         // Map gesetzt, wirkt aber nicht (Zero Latency): Ziel
    };

    std::function<void (int pc, int mapIndex)> onSetMap; // 0 = normal, 1..12 = Ziel C..B, 13 = aus
    std::function<void()> onResetAll;

    void setRoutes (const std::array<Route, 12>& newRoutes, bool isHighQuality, prizm::PitchMask allowedMask, prizm::PitchMask scaleMaskToUse);
    void setLevels (const std::array<float, 12>& in, const std::array<float, 12>& out, float animPhase);

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    juce::String getTooltip() override;

    // Spaltengeometrie, gleich wie die Notentasten darüber
    juce::Rectangle<float> laneBounds (int pc) const;
    float laneX (float pos) const;

private:
    int laneAt (float x) const;
    float portY() const noexcept { return 6.0f; }
    float outTop() const noexcept { return (float) getHeight() - 30.0f; }
    juce::Rectangle<float> outNode (int pc) const;
    juce::Path beamPath (float x0, float x1) const;
    void showMenu (int pc);
    juce::String describe (int pc) const;

    std::array<Route, 12> routes {};
    std::array<float, 12> inLevel {}, outLevel {};
    prizm::PitchMask allowed = 0, scaleMask = 0;
    bool hq = false;
    float phase = 0.0f;

    int hoverLane = -1;
    int dragFrom = -1, dragTarget = -1;
    juce::Point<float> dragPos;
    bool dragging = false;
};
