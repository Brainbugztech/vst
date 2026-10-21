#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"

// Die animierte Labor-Szene: Brain Bug im Wassertank, Konsolen, Schläuche.
// BugFuzz-Variante: Fell auf dem Gehirn, das mit dem Drive aufstellt, Funken aus den Kabeln,
// links die Fuzz-Kennlinie mit Oszilloskop, rechts die vier Bänder zum Ziehen.
// Gezeichnet in einem festen 1980x880-Koordinatensystem und auf die Komponentengröße skaliert.
class LabScene : public juce::Component, private juce::Timer
{
public:
    explicit LabScene (BugFuzzProcessor&);
    ~LabScene() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr float CW = 1980.0f, CH = 880.0f;

private:
    void timerCallback() override;

    // Zeichen-Helfer
    void drawConsoles (juce::Graphics&, bool act, float env);
    void drawScreen (juce::Graphics&, float x, int ch, bool act);
    void drawPlates (juce::Graphics&);
    void drawWater (juce::Graphics&, bool act, float env, juce::Colour mixC);
    void drawHoses (juce::Graphics&, bool act);
    void drawArcs (juce::Graphics&);
    void renderBugBuffer (float physScale, float env);
    void ensureCaches (float physScale);
    void buildFur();

    static void flowDash (juce::Graphics&, const juce::Path&, float dash, float gap, float offset, float width, juce::Colour);
    static void hose (juce::Graphics&, juce::Point<float> p0, juce::Point<float> c1, juce::Point<float> c2,
                      juce::Point<float> p1, float flow);

    BugFuzzProcessor& proc;
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
    float smoothF = 0.5f, burst = 0.0f, furUp = 0.3f;
    long long glitchBeat = -1;
    float bugCX = 990.0f, bugCY = 480.0f, bugTilt = 0.0f; // aktuelle Lage des Käfers (für Funken)

    struct Bubble { float x, y, r, s, w; };
    std::vector<Bubble> bubbles;
    float btnSeeds[32] {};

    // Fell: Haarwurzeln auf dem Gehirn-Umriss (Szenen-Koordinaten relativ zum Käfer-Puffer)
    struct Hair { juce::Point<float> p; float ang, len, seed; };
    std::vector<Hair> fur;

    // Funken: Zickzack-Linien von den Kabeln zum Käfer
    struct Arc { std::vector<juce::Point<float>> pts; float life; juce::Colour c; };
    std::vector<Arc> arcs;

    // Bedienfeld auf dem rechten Monitor
    class BandPanel;
    std::unique_ptr<BandPanel> bandPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabScene)
};
