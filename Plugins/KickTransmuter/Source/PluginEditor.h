#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"
#include "KickTransmuterAssets.h"

// Regler im Look von Benetts Bild: Kappe und Skala stammen aus dem Hintergrund, Zeiger und Leuchtbogen zeichnet der Regler selbst
class NeonKnob : public juce::Slider
{
public:
    NeonKnob (juce::Colour arcFrom, juce::Colour arcTo, juce::Colour pointer, float ringRadius);
    void paint (juce::Graphics&) override;
    float ring;

private:
    juce::Colour from, to, ptr;
};

class AutoGainButton : public juce::Button
{
public:
    AutoGainButton() : juce::Button ("AUTO GAIN") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class KickTransmuterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KickTransmuterEditor (KickTransmuterProcessor&);
    ~KickTransmuterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Alle Maße in Pixeln des Originalbilds
    static constexpr int artW = 1672, artH = 941;
    static constexpr int baseW = 1115, baseH = 628;

private:
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);

    KickTransmuterProcessor& proc;
    Content content;
    juce::Image bg, brain;

    NeonKnob attack, sustain, punch, sat, low, mix, output;
    AutoGainButton autoGainBtn;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> attAttack, attSustain, attPunch, attSat, attLow, attMix, attOutput;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attAuto;

    // Anzeige
    float pulse = 0.0f, transientDb = 0.0f, outLevel = 0.0f, shownAg = 0.0f;
    bool  shownAgOn = false;
    int   lastHits = 0;
    double lastMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickTransmuterEditor)
};
