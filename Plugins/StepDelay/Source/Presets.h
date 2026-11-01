#pragma once

// Muster-Presets: setzen Step-Anzahl, Raster und die Lautstärken.
// Mix, Filter und Output bleiben, wie sie sind.

#include <cmath>
#include <vector>

namespace bb::stepdelay
{
struct Preset
{
    const char* name;
    int steps;
    int rate;   // Index in stepgrid::rateNames (0 = 1/64 … 6 = 1/1)
    int mode;   // 0 = gerade, 1 = punktiert, 2 = Triole
    std::vector<float> levels;
};

inline std::vector<float> fade (int n, float start, float factor)
{
    std::vector<float> v;
    float x = start;
    for (int i = 0; i < n; ++i) { v.push_back (x); x *= factor; }
    return v;
}

inline std::vector<float> onSteps (int n, std::initializer_list<int> hits, float start, float factor)
{
    std::vector<float> v ((size_t) n, 0.0f);
    float x = start;
    for (int h : hits)
        if (h >= 1 && h <= n) { v[(size_t) h - 1] = x; x *= factor; }
    return v;
}

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = []
    {
        std::vector<Preset> p;
        p.push_back ({ "Init: 1/8 ausklingen",  8, 3, 0, fade (8, 0.8f, 0.78f) });
        p.push_back ({ "Punktierte Achtel",     6, 3, 1, { 0.9f, 0.7f, 0.5f, 0.35f, 0.24f, 0.15f } });
        p.push_back ({ "Psy Galopp",           16, 2, 0, { 0, 0.9f, 0.7f, 0, 0, 0.6f, 0.45f, 0, 0, 0.35f, 0.26f, 0, 0, 0.18f, 0.12f, 0 } });
        p.push_back ({ "Off-Beat Pumpe",       16, 2, 0, onSteps (16, { 2, 6, 10, 14 }, 0.85f, 0.7f) });
        p.push_back ({ "Dub 3-3-2",            16, 2, 0, onSteps (16, { 3, 6, 8, 11, 14, 16 }, 0.85f, 0.78f) });
        p.push_back ({ "Triolen-Treppe",       12, 3, 2, { 0.3f, 0.45f, 0.6f, 0.75f, 0.9f, 0.75f, 0.6f, 0.45f, 0.32f, 0.22f, 0.14f, 0.08f } });
        p.push_back ({ "Ghost Swell",          16, 2, 0, [] { std::vector<float> v; for (int i = 0; i < 16; ++i) v.push_back (0.05f + 0.85f * std::pow (i / 15.0f, 1.6f)); return v; }() });
        p.push_back ({ "Stotter-Roll",         16, 1, 0, std::vector<float> (16, 0.85f) });
        p.push_back ({ "Weite Viertel punktiert", 4, 4, 1, { 0.85f, 0.55f, 0.32f, 0.18f } });
        p.push_back ({ "Splitter 1/32T",       24, 1, 2, { 0.9f, 0, 0.5f, 0.2f, 0, 0.7f, 0, 0, 0.4f, 0.6f, 0, 0.15f,
                                                           0, 0.45f, 0.3f, 0, 0, 0.25f, 0, 0.35f, 0.1f, 0, 0.2f, 0.05f } });
        p.push_back ({ "32 Steps Kometenschweif", 32, 2, 0, fade (32, 0.9f, 0.91f) });
        return p;
    }();
    return list;
}
} // namespace bb::stepdelay
