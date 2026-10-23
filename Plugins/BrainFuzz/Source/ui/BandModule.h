#pragma once

#include "Widgets.h"

namespace bfz::ui
{

// Stereo-Bild eines Bandes (Halbkreis-Goniometer) mit Korrelation
class StereoScope : public juce::Component
{
public:
    StereoScope (BrainfuzzProcessor& p, int band) : proc (p), b (band) {}
    void update() { repaint(); }
    void paint (juce::Graphics&) override;
private:
    BrainfuzzProcessor& proc;
    int b;
    std::array<float, 512> bufL {}, bufR {};
    float corr = 1.0f;
};

// Reiter fuer einen Effekt: Name + LED (an/aus). Klick = anzeigen, Doppelklick = an/aus
class FxTab : public juce::Button
{
public:
    FxTab (BrainfuzzProcessor& p, int band, int onParam, const juce::String& name, juce::Colour accent)
        : juce::Button (name), proc (p), b (band), onIndex (onParam), colour (accent) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        proc.undo.beginNewTransaction();
        proc.setValue (onIndex, proc.value (onIndex) > 0.5f ? 0.0f : 1.0f);
    }
    bool shownOn = false;
private:
    BrainfuzzProcessor& proc;
    int b, onIndex;
    juce::Colour colour;
};

// Effekt-Seite: An/Aus, Pre/Post, Ziel (Band / Width / Both), Regler
class FxPage : public juce::Component
{
public:
    FxPage (BrainfuzzProcessor& p, int band, int fx);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
private:
    BrainfuzzProcessor& proc;
    int b, fx;
    juce::Colour accent;
    std::unique_ptr<Toggle> power;
    std::unique_ptr<ChoiceButtons> pos, target, voices;
    juce::OwnedArray<Knob> knobs;
    bool lastOn = true;
};

class BandModule : public juce::Component
{
public:
    BandModule (BrainfuzzProcessor& p, int band);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update();
private:
    void showTab (int t);

    BrainfuzzProcessor& proc;
    int b;
    juce::Colour accent;
    Toggle solo, mute, bypass;
    Knob drive, level, width, spread;
    std::array<std::unique_ptr<FxTab>, 3> tabs;
    std::array<std::unique_ptr<FxPage>, 3> pages;
    StereoScope scope;
    int tab = 0;
    float shownLevel = 0.0f;
};

} // namespace bfz::ui
