#pragma once

#include <cmath>

#include "Scales.h"

namespace prizm
{
// Reglerwerte, die die Engines brauchen, einmal pro Audioblock gelesen.
// Strength, Mix, Bereich und Pegelausgleich rechnet der Processor für beide Modi gleich.
struct EngineParams
{
    PitchMask mask = kAllPitches; // erlaubte Tonklassen (absolut)
    float color = 0.6f;           // 0..2   Filtergüte, über 1 langer Tail
    float clear = 0.3f;           // 0..1   alte Resonanzen bei neuen Anschlägen dämpfen
    float gate = 0.0f;            // 0..1   leise Noten relativ zur lautesten ausblenden
    float shift = 0.0f;           // 0..1   nur High Quality: schiefe Teiltöne auf die Tonart verschieben
};

// Gate-Kennlinie, für beide Modi gleich: Schwelle relativ zur lautesten Note.
inline float gateGain (float noteLevel, float loudest, float gate)
{
    if (gate <= 0.001f || loudest <= 0.0f)
        return 1.0f;
    const float thresholdDb = -60.0f + 50.0f * gate; // 0 % = aus, 100 % = -10 dB unter der lautesten Note
    const float threshold = loudest * std::pow (10.0f, thresholdDb / 20.0f);
    if (noteLevel >= threshold)
        return 1.0f;
    const float r = noteLevel / threshold;
    return r * r; // weicher Abwärts-Expander
}
} // namespace prizm
