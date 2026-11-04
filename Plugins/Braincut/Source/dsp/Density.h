#pragma once

// Dichte: bestimmt, an welchen Stellen die Effekte arbeiten dürfen ("Zonen").
// Außerhalb einer Zone läuft das Signal unverändert durch.
//   DECENT - alle 4 Takte, nur auf dem letzten Viertel-Takt (Fill am Ende, in 4/4 der letzte Beat)
//   MEDIUM - alle 2 Takte auf dem letzten Viertel-Takt, dazu einzelne kurze Shots (ein halber Beat) mittendrin
//   NUTS   - immer (jeder Schlag kann umgebaut werden)
// Die Shots hängen nur vom Seed ab; mit Lock liegen sie in jeder Runde an derselben Stelle.
// In einer Zone werden die Wahrscheinlichkeiten angehoben (boost), damit dort sicher etwas passiert.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Params.h"
#include "Rng.h"

namespace bc
{

struct Zone
{
    bool   on = true;
    double left = 1.0e9;   // Beats bis zum Ende der Zone
};

inline constexpr float kShotChance = 0.17f;   // MEDIUM: Wahrscheinlichkeit pro Beat für einen Shot
inline constexpr double kShotBeats = 0.5;     // Länge eines Shots in Beats

inline double cycleBars (int density) { return density == kDecent ? 4.0 : density == kMedium ? 2.0 : 1.0; }

inline Zone densityZone (int density, double beat, double beatsPerBar, int seed, bool lock)
{
    Zone z;
    if (density >= kNuts || density < 0)
        return z;

    const double bpb = std::min (16.0, std::max (1.0, beatsPerBar));
    const double cycle = cycleBars (density) * bpb;
    const double fill = bpb * 0.25;
    const double cIdx = std::floor (beat / cycle);
    const double pos = beat - cIdx * cycle;

    if (pos >= cycle - fill)
    {
        z.on = true;
        z.left = cycle - pos;
        return z;
    }

    if (density == kMedium)
    {
        // Shots: ab dem zweiten Beat, nur wenn der Shot vor dem Fill endet
        const double b = std::floor (pos);
        if (b >= 1.0 && b + kShotBeats <= cycle - fill && pos < b + kShotBeats)
        {
            const uint64_t where = lock ? (uint64_t) b : (uint64_t) ((long long) cIdx * 64 + (long long) b);
            if (hash01 (hashCombine ((uint64_t) seed + 0xD3517ull, where)) < kShotChance)
            {
                z.on = true;
                z.left = b + kShotBeats - pos;
                return z;
            }
        }
    }

    z.on = false;
    z.left = 0.0;
    return z;
}

// Wahrscheinlichkeit in einer Zone anheben (0 bleibt 0, 1 bleibt 1)
inline float zoneBoost (int density, float p)
{
    p = std::min (1.0f, std::max (0.0f, p));
    if (density >= kNuts) return p;
    const float q = 1.0f - p;
    return 1.0f - q * q * q;
}

} // namespace bc
