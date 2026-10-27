#pragma once

// Parameter der Braincut-Engine (reine Werte, keine JUCE-Typen)

#include <cstdint>

namespace bc
{

// Schritte pro Beat für die Auswahl "Subdivision": 1/4, 1/8, 1/16, 1/32
inline constexpr int kSubdivSteps[4] { 1, 2, 4, 8 };
inline constexpr int kPhraseBars[3] { 1, 2, 4 };

// Länge einer Wiederholungs-Einheit in Beats: 1/8, 1/8 Triole, 1/16, 1/16 Triole, 1/32
inline constexpr double kRepeatUnitBeats[5] { 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

struct SlicerParams
{
    int   mode = 0;          // 0 = CUT (CutProc-Stil), 1 = WARP
    int   loopIdx = 0;       // Länge der Quelle (Loop): 0..2 -> 1, 2, 4 Takte
    int   phraseMin = 1;     // kürzeste Phrase in Takten (1..8)
    int   phraseMax = 4;     // längste Phrase in Takten (1..8)
    int   repMin = 0;        // wenigste Wiederholungen pro Block (0..16)
    int   repMax = 3;        // meiste Wiederholungen pro Block (0..16)
    int   subdivIdx = 2;     // 0..3 -> 1/4, 1/8, 1/16, 1/32
    float repeatProb = 0.4f; // 0..1: wie oft ein Beat umgebaut wird
    int   seed = 1;          // 0..9999
    bool  lock = false;      // an: jede Runde spielt dasselbe Muster
};

struct StretchParams
{
    bool  on = true;
    float amount = 0.5f;     // 0..1 -> Streckung bis Faktor 2 (und 1/2)
    float mix = 1.0f;
};

struct RepeatParams
{
    bool  on = true;
    float chance = 0.3f;     // Wahrscheinlichkeit pro Beat, dass ein Stotterer startet
    int   rateIdx = 2;       // 0..4 siehe kRepeatUnitBeats
    float gate = 0.7f;       // 0.1..1
    float decay = 0.3f;      // 0..1
};

struct GlitchParams
{
    bool  on = true;
    float amount = 0.3f;
    float mix = 1.0f;
    bool  bit = true, rev = true, stop = true, freeze = true, pitch = true;
};

struct EngineParams
{
    SlicerParams  slicer;
    StretchParams stretch;
    RepeatParams  repeat;
    GlitchParams  glitch;
    float mix = 1.0f;        // Dry/Wet gesamt
    float outDb = 0.0f;
};

// Takt-Information vom Host
struct Transport
{
    double ppq = 0.0;          // Position in Vierteln (Beat) am Blockanfang
    double bpm = 120.0;
    double beatsPerBar = 4.0;
    bool   playing = false;
};

} // namespace bc
