#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"

// Die animierte Labor-Szene: Brain Bug im Wassertank, Konsolen, Schläuche.
// Gezeichnet in einem festen 1980x880-Koordinatensystem (wie der Browser-Prototyp)
// und auf die Komponentengröße skaliert.
class LabScene : public juce::Component, private juce::Timer
{
public:
    explicit LabScene (HaasProcessor&);
    ~LabScene() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr float CW = 1980.0f, CH = 880.0f;

private:
    void timerCallback() override;

    // Zeichen-Helfer
    void drawConsoles (juce::Graphics&, bool act, float env);
    void drawScreen (juce::Graphics&, float x, int ch, bool act);
    void drawWater (juce::Graphics&, bool act, float env, float heat, juce::Colour mixC);
    void drawHoses (juce::Graphics&, bool act);
    void renderBugBuffer (float physScale);
    void ensureCaches (float physScale);

    static void flowDash (juce::Graphics&, const juce::Path&, float dash, float gap, float offset, float width, juce::Colour);
    static void hose (juce::Graphics&, juce::Point<float> p0, juce::Point<float> c1, juce::Point<float> c2,
                      juce::Point<float> p1, float flow);

    HaasProcessor& proc;
    juce::Random rng;

    // Bilder
    juce::Image labImg, frontImg;
    juce::Image parts[4];           // body, head, legL, legR (Original)
    juce::Image partsScaled[4];
    juce::Image labCache, frontCache, bugBuf;
    float cacheScale = -1.0f;

    // Animation
    double lastMs = 0.0, tt = 0.0, dt = 0.016;
    double gp = 0.0, lastBeats = 0.0, flowPos = 0.0;
    float smoothF = 0.5f, burst = 0.0f;
    long long glitchBeat = -1;

    struct Bubble { float x, y, r, s, w; };
    std::vector<Bubble> bubbles;
    float btnSeeds[32] {};

    // Bedienelemente auf dem rechten Monitor
    class MsValue;
    std::unique_ptr<MsValue> msVal[2];
    juce::TextButton msDown[2], msUp[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabScene)
};
