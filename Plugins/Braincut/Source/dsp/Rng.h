#pragma once

// Kleine, schnelle Zufallsfunktionen ohne Speicher-Allokation (Audio-Thread-tauglich)
// und ohne Abhängigkeit zu JUCE.

#include <cstdint>

namespace bc
{

inline uint64_t mix64 (uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Zahl in [0, 1) aus einem Schlüssel: gleicher Schlüssel = gleiches Ergebnis
inline float hash01 (uint64_t key)
{
    return (float) (mix64 (key) >> 40) * (1.0f / 16777216.0f);
}

inline uint64_t hashCombine (uint64_t a, uint64_t b)
{
    return mix64 (a ^ (b + 0x9E3779B97F4A7C15ull + (a << 6) + (a >> 2)));
}

// Zufallsfolge (SplitMix64)
struct Rng
{
    uint64_t s;

    explicit Rng (uint64_t seed = 1) : s (mix64 (seed)) {}

    uint64_t next()
    {
        s += 0x9E3779B97F4A7C15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    float uni() { return (float) (next() >> 40) * (1.0f / 16777216.0f); }

    // ganze Zahl von lo bis hi (beide eingeschlossen)
    int range (int lo, int hi)
    {
        if (hi <= lo) return lo;
        return lo + (int) (next() % (uint64_t) (hi - lo + 1));
    }
};

} // namespace bc
