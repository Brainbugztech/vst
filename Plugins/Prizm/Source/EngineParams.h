#pragma once

#include <array>
#include <cmath>
#include <cstdint>

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
    float a4 = 440.0f;            // Stimmton in Hz

    // Noten-Map (nur High Quality): pro Tonklasse -1 = normal, 0..11 = Ziel-Tonklasse, 12 = stumm
    std::array<int8_t, 12> map { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    float formant = 1.0f;         // 0..1   Klangfarbe beim Verschieben erhalten

    // Gewichtung pro Tonklasse (0..1): wie stark eine erlaubte Note durch den Filter kommt
    std::array<float, 12> weight { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

    bool mapActive() const noexcept
    {
        for (auto m : map)
            if (m >= 0)
                return true;
        return false;
    }
};

inline constexpr int8_t kMapKeep = -1;
inline constexpr int8_t kMapMute = 12;

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
