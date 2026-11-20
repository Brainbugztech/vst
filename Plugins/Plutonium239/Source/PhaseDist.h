#pragma once

// Phase Distortion im Stil der Casio-CZ-Synths: die Phase (0..1) wird verbogen, bevor die Wellenform gelesen wird.
// d = Amount 0..1 (0 = unveraendert). Reso-Modi erzeugen selbst die Wellenform (Fenster x schneller Sinus).

#include <juce_core/juce_core.h>
#include <cmath>

namespace pluto
{
inline const juce::StringArray& pdModeNames()
{
    static const juce::StringArray a { "Off", "Bend", "Mirror", "Square", "Pulse", "SawPulse", "Sync", "Fold", "Reso Saw", "Reso Tri", "Reso Trap" };
    return a;
}
enum PdMode { pdOff, pdBend, pdMirror, pdSquare, pdPulse, pdSawPulse, pdSync, pdFold, pdResoSaw, pdResoTri, pdResoTrap };

inline bool pdIsReso (int mode) { return mode >= pdResoSaw; }

inline double pdBendFn (double p, double d)
{
    const double k = 0.5 - d * 0.49;
    return p < k ? p * (0.5 / k) : 0.5 + (p - k) * (0.5 / (1.0 - k));
}

inline double pdWarp (int mode, double d, double p)
{
    if (d <= 0.0) return p;
    switch (mode)
    {
        case pdBend:   return pdBendFn (p, d);
        case pdMirror: return p < 0.5 ? pdBendFn (2.0 * p, d) * 0.5 : 1.0 - pdBendFn (2.0 - 2.0 * p, d) * 0.5;
        case pdSquare:
        {
            const double h = p < 0.5 ? 0.0 : 1.0;
            const double q = p * 2.0 - h;
            return (h + pdBendFn (q, d)) * 0.5;
        }
        case pdPulse:
        {
            const double w = 1.0 - d * 0.92;
            return p < w ? p / w : 1.0;
        }
        case pdSawPulse: return p < 0.5 ? pdBendFn (p * 2.0, d) * 0.5 : 0.5 + (p - 0.5) * (1.0 - d * 0.9);
        case pdSync:
        {
            const double x = p * (1.0 + d * 7.0);
            return x - std::floor (x);
        }
        case pdFold:
        {
            const double x = p * (1.0 + d * 3.0);
            const double fl = std::floor (x);
            const double fr = x - fl;
            return ((int) fl & 1) ? 1.0 - fr : fr;
        }
        default: return p;
    }
}

// Reso-Modi: Fenster * Sinus mit (1 + d*15)-facher Frequenz
inline float pdReso (int mode, double d, double p)
{
    const double ratio = 1.0 + d * 15.0;
    double win;
    switch (mode)
    {
        case pdResoSaw: win = 1.0 - p; break;
        case pdResoTri: win = 1.0 - std::abs (2.0 * p - 1.0); break;
        default:        win = p < 0.5 ? 1.0 : 2.0 * (1.0 - p); break;
    }
    return (float) (win * std::sin (juce::MathConstants<double>::twoPi * p * ratio));
}
} // namespace pluto
