#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace prizm
{
// A pitch-class mask: bit i set = pitch class i (C = 0 ... B = 11) is allowed.
using PitchMask = uint16_t;

constexpr PitchMask kAllPitches = 0x0FFF;

template <typename... Steps>
constexpr PitchMask bits (Steps... steps)
{
    return (PitchMask) (((1u << steps) | ...));
}

struct ScaleDef
{
    const char* name;
    PitchMask mask; // relative to the root note
};

// The last entry ("Custom") uses the free mask the user clicks on the key strip.
inline constexpr std::array<ScaleDef, 15> kScales {{
    { "Major",            bits (0, 2, 4, 5, 7, 9, 11) },
    { "Minor",            bits (0, 2, 3, 5, 7, 8, 10) },
    { "Harmonic Minor",   bits (0, 2, 3, 5, 7, 8, 11) },
    { "Melodic Minor",    bits (0, 2, 3, 5, 7, 9, 11) },
    { "Dorian",           bits (0, 2, 3, 5, 7, 9, 10) },
    { "Phrygian",         bits (0, 1, 3, 5, 7, 8, 10) },
    { "Lydian",           bits (0, 2, 4, 6, 7, 9, 11) },
    { "Mixolydian",       bits (0, 2, 4, 5, 7, 9, 10) },
    { "Major Pentatonic", bits (0, 2, 4, 7, 9) },
    { "Minor Pentatonic", bits (0, 3, 5, 7, 10) },
    { "Blues",            bits (0, 3, 5, 6, 7, 10) },
    { "Major Chord",      bits (0, 4, 7) },
    { "Minor Chord",      bits (0, 3, 7) },
    { "Chromatic",        kAllPitches },
    { "Custom",           bits (0, 2, 4, 5, 7, 9, 11) },
}};

inline constexpr int kCustomScaleIndex = (int) kScales.size() - 1;

inline constexpr std::array<const char*, 12> kNoteNames {{
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
}};

// Moves a root-relative mask so that bit 0 lands on the chosen key.
inline PitchMask rotateToKey (PitchMask relative, int key)
{
    PitchMask out = 0;
    for (int i = 0; i < 12; ++i)
        if (relative & (1u << i))
            out |= (PitchMask) (1u << ((i + key) % 12));
    return out;
}

inline bool isAllowed (PitchMask mask, int midiNote)
{
    const int pc = ((midiNote % 12) + 12) % 12;
    return (mask >> pc) & 1u;
}

// Nearest allowed MIDI note (integer) to a fractional MIDI pitch.
inline int nearestAllowedNote (PitchMask mask, float midiPitch)
{
    if (mask == 0)
        return (int) std::lround (midiPitch);

    const int base = (int) std::floor (midiPitch);
    int best = base;
    float bestDist = 1.0e9f;

    for (int n = base - 6; n <= base + 7; ++n)
    {
        if (! isAllowed (mask, n))
            continue;

        const float d = std::abs (midiPitch - (float) n);
        if (d < bestDist)
        {
            bestDist = d;
            best = n;
        }
    }
    return best;
}

inline float midiToHz (float midi)  { return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f); }
inline float hzToMidi (float hz)    { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
} // namespace prizm
