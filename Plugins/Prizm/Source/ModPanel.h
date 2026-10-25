#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <deque>
#include <functional>

#include "PluginProcessor.h"
#include "PrizmLookAndFeel.h"

namespace prizm
{
// Farben der Quellen, überall gleich (Reiter, Kurve, Ringe an den Reglern, Routing-Liste)
inline juce::Colour modSourceColour (int source)
{
    switch (source)
    {
        case srcLfo1: return Col::cyan;
        case srcLfo2: return Col::violet;
        case srcEnv:  return Col::amber;
        default:      return Col::muted;
    }
}

inline juce::String modSourceName (int source)
{
    switch (source)
    {
        case srcLfo1: return "LFO 1";
        case srcLfo2: return "LFO 2";
        case srcEnv:  return "ENV";
        default:      return {};
    }
}
} // namespace prizm

// Kleiner Umschalter in Holo-Optik, direkt an einen Parameter gebunden.
// Segmente nebeneinander (z. B. FREE / RETRIG / ENV) oder als Taste, die bei jedem Klick weiterschaltet.
class SegmentSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentSwitch (juce::RangedAudioParameter& param, juce::StringArray segmentLabels, bool cycles, juce::String prefixText = {});
    int getValue() const noexcept { return value; }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    int segmentAt (int x) const;
    juce::StringArray labels;
    bool cycle;
    juce::String prefix;
    int value = 0;
    juce::ParameterAttachment attachment;
};

// Reiter einer Quelle: Klick = anzeigen, Ziehen auf einen Regler = zuweisen
class ModSourceTab : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModSourceTab (int sourceIndex) : source (sourceIndex) {}
    std::function<void (int)> onSelect;
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void setLevel (float v) { level = v; repaint(); }
    void flash() { flashAmount = 1.0f; }      // Retrig/Envelope wurde ausgelöst
    void decayFlash() { flashAmount *= 0.8f; if (flashAmount < 0.02f) flashAmount = 0.0f; }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

private:
    int source;
    bool selected = false;
    float level = 0.0f; // LFO -1..1, ENV 0..1
    float flashAmount = 0.0f;
};

// Großer Editor: zeigt die LFO-Kurve (bearbeitbar) oder den Verlauf des Envelope Followers
class CurveEditor : public juce::Component, public juce::TooltipClient
{
public:
    explicit CurveEditor (PrizmAudioProcessor& p) : processor (p) {}

    void setSource (int sourceIndex) { source = sourceIndex; selectedPoint = -1; repaint(); }
    void tick();

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override { hoverPoint = hoverSegment = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    juce::String getTooltip() override;

private:
    using Point = prizm::LfoCurve::Point;
    int lfoIndex() const noexcept { return source - prizm::srcLfo1; }
    bool isLfo() const noexcept { return source == prizm::srcLfo1 || source == prizm::srcLfo2; }
    int currentShape() const;
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> toCurve (juce::Point<float> screen) const;
    std::vector<Point> editablePoints(); // wandelt eine Standardform beim ersten Bearbeiten in Punkte um
    std::vector<Point> shownPoints() const; // Griffe: Custom-Kurve oder Punkte der Standardform (leer bei S&H/Random)
    void commit (std::vector<Point> pts);
    int pointAt (juce::Point<float> pos) const;
    int segmentAt (juce::Point<float> pos) const;
    float valueAt (float x) const; // aktuelle Form (Standard oder Kurve)
    void showMenu();

    PrizmAudioProcessor& processor;
    int source = prizm::srcLfo1;
    std::vector<Point> dragPoints;
    int selectedPoint = -1, hoverPoint = -1, hoverSegment = -1, dragSegment = -1;
    float dragStartCurve = 0.0f;
    std::deque<float> envHistory;
};

// Routing-Liste: alle belegten Slots, Tiefe per Ziehen, × entfernt
class ModSlotList : public juce::Component, public juce::TooltipClient
{
public:
    explicit ModSlotList (PrizmAudioProcessor& p) : processor (p) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override { hoverRow = rowAt (e.y); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hoverRow = -1; repaint(); }
    juce::String getTooltip() override;

private:
    std::vector<int> activeSlots() const;
    int rowAt (int y) const;
    juce::Rectangle<float> rowBounds (int row) const;
    juce::Rectangle<float> barBounds (int row) const;
    juce::Rectangle<float> removeBounds (int row) const;

    PrizmAudioProcessor& processor;
    int dragSlot = -1, hoverRow = -1;
    float dragStartAmt = 0.0f;
};

// Der ganze Modulations-Bereich
class ModPanel : public juce::Component
{
public:
    explicit ModPanel (PrizmAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void tick(); // vom Editor-Timer
    int getSelectedSource() const noexcept { return selected; }

private:
    void select (int source);
    void updateRateVisibility();

    PrizmAudioProcessor& processor;
    int selected = prizm::srcLfo1;

    std::array<std::unique_ptr<ModSourceTab>, 3> tabs;
    CurveEditor editor;
    ModSlotList slots;

    // LFO-Bedienung (je LFO ein Satz, nur der gewählte ist sichtbar)
    struct LfoControls
    {
        juce::ComboBox shape;
        juce::TextButton sync { "SYNC" };
        juce::Slider rate, division;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAtt, divAtt;
    };
    std::array<LfoControls, 2> lfo;
    std::array<std::unique_ptr<SegmentSwitch>, 2> modeSwitch; // FREE / RETRIG / ENV je LFO
    std::unique_ptr<SegmentSwitch> trigSwitch;                 // gilt für beide LFOs
    std::array<uint32_t, 2> lastTrig {};

    juce::Slider envAttack, envRelease, envSens;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envAttackAtt, envReleaseAtt, envSensAtt;

    juce::Rectangle<int> controlsRow;
};
