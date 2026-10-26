#pragma once

// 15 Werks-Presets für Braincut. Reihenfolge der Felder siehe struct Preset.

namespace bcpresets
{

struct Preset
{
    const char* name;
    // Slicer
    int mode, phrase, subdiv; float repeat; int seed; bool lock;
    // Stretch
    bool strOn; float strAmt, strMix;
    // Repeat
    bool repOn; float repChance; int repRate; float repGate, repDecay;
    // Glitch
    bool gliOn; float gliAmt, gliMix; bool bit, rev, stop, frz, pit;
    // Ausgang
    float mix, outDb;
};

// phrase: 0 = 1 Takt, 1 = 2 Takte, 2 = 4 Takte
// subdiv: 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32
// repRate: 0 = 1/8, 1 = 1/8T, 2 = 1/16, 3 = 1/16T, 4 = 1/32
inline const Preset kPresets[15] =
{
    //  Name              mode ph sub  rep  seed  lock | strOn amt  mix | repOn chance rate gate decay | gliOn amt  mix  bit    rev    stop   frz    pit   | mix  out
    { "Init Slice",        0, 0, 2, 0.35f,    1, false, false, 0.50f, 1.0f, false, 0.30f, 2, 0.70f, 0.30f, false, 0.30f, 1.0f, true,  true,  true,  true,  true,  1.0f, 0.0f },
    { "Break Chop",        0, 0, 2, 0.55f,   12, true,  false, 0.50f, 1.0f, true,  0.15f, 2, 0.80f, 0.20f, false, 0.30f, 1.0f, true,  true,  true,  true,  true,  1.0f, 0.0f },
    { "Jungle Roll",       0, 1, 2, 0.70f,   77, false, true,  0.25f, 0.8f, true,  0.35f, 4, 0.90f, 0.35f, false, 0.30f, 1.0f, true,  true,  true,  true,  true,  1.0f, 0.0f },
    { "Warp Drift",        1, 1, 2, 0.50f,    5, false, true,  0.60f, 1.0f, false, 0.30f, 2, 0.70f, 0.30f, false, 0.30f, 1.0f, true,  true,  true,  true,  true,  1.0f, 0.0f },
    { "Stutter Fill",      0, 0, 2, 0.30f,   21, true,  false, 0.50f, 1.0f, true,  0.60f, 3, 0.60f, 0.50f, false, 0.30f, 1.0f, true,  true,  true,  true,  true,  1.0f, 0.0f },
    { "Rewind Rave",       0, 0, 1, 0.50f,   33, false, false, 0.50f, 1.0f, false, 0.30f, 2, 0.70f, 0.30f, true,  0.50f, 1.0f, false, true,  false, false, false, 1.0f, 0.0f },
    { "Tape Crash",        1, 1, 2, 0.40f,    9, false, true,  0.30f, 1.0f, false, 0.30f, 2, 0.70f, 0.30f, true,  0.55f, 1.0f, true,  false, true,  false, false, 1.0f, 0.0f },
    { "Frozen Hits",       0, 0, 2, 0.45f,   64, false, false, 0.50f, 1.0f, true,  0.25f, 0, 1.00f, 0.10f, true,  0.50f, 0.9f, false, false, false, true,  false, 1.0f, 0.0f },
    { "Pitch Hopper",      1, 0, 2, 0.45f,   18, true,  false, 0.50f, 1.0f, false, 0.30f, 2, 0.70f, 0.30f, true,  0.60f, 1.0f, false, false, false, false, true,  1.0f, 0.0f },
    { "Crusher Cut",       0, 0, 3, 0.60f,    3, false, false, 0.50f, 1.0f, true,  0.30f, 4, 0.50f, 0.00f, true,  0.70f, 0.8f, true,  false, false, false, false, 1.0f, -1.0f },
    { "Slow Melt",         1, 2, 1, 0.30f,   40, false, true,  1.00f, 1.0f, false, 0.30f, 2, 0.70f, 0.30f, false, 0.30f, 1.0f, true,  true,  true,  true,  true,  1.0f, 0.0f },
    { "Breakcore Storm",   1, 0, 3, 0.90f,  666, false, true,  0.70f, 1.0f, true,  0.70f, 4, 0.60f, 0.20f, true,  0.70f, 1.0f, true,  true,  true,  true,  true,  1.0f, -2.0f },
    { "Subtle Shuffle",    0, 1, 1, 0.20f,    7, true,  false, 0.50f, 1.0f, false, 0.30f, 2, 0.70f, 0.30f, false, 0.30f, 1.0f, true,  true,  true,  true,  true,  1.0f, 0.0f },
    { "Hitech Stutter",    0, 0, 2, 0.40f,  150, true,  false, 0.50f, 1.0f, true,  0.50f, 2, 0.45f, 0.15f, true,  0.30f, 1.0f, true,  false, false, false, true,  1.0f, 0.0f },
    { "Full Chaos",        1, 1, 2, 1.00f, 1337, false, true,  1.00f, 1.0f, true,  0.80f, 3, 0.70f, 0.30f, true,  0.90f, 1.0f, true,  true,  true,  true,  true,  1.0f, -3.0f },
};

inline constexpr int kNumPresets = 15;

} // namespace bcpresets
