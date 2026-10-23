#pragma once

// Brainfuzz – sieben Saettigungs-Kennlinien, von hartem Splatter-Fuzz bis weicher Tape-Waerme
// Jede Kennlinie: f(0) = 0, Steigung bei 0 ungefaehr 1 (kleine Signale bleiben gleich laut),
// begrenzt auf etwa +-1. "Bite" (0..1) formt innerhalb des Typs, wie hart er zubeisst.

#include <cmath>
#include <algorithm>

namespace bfz
{

enum class SatType { splat = 0, fuzz, fold, diode, crunch, tube, tape, count };

inline const char* satTypeName (int t)
{
    static const char* names[] { "Splat", "Fuzz", "Fold", "Diode", "Crunch", "Tube", "Tape" };
    return names[std::clamp (t, 0, 6)];
}

inline const char* satTypeInfo (int t)
{
    static const char* info[] {
        "Harte, splattrige Klippung mit Gate-Kratzen",
        "Klassischer Transistor-Fuzz, asymmetrisch",
        "Wavefolder: faltet Spitzen zurueck, metallisch",
        "Dioden-Clipper, ungleich harte Seiten",
        "Crunch: weiches Knie bis harter Rand",
        "Roehre: warme, gerade Obertoene",
        "Tape: weiche Kompression, sehr warm" };
    return info[std::clamp (t, 0, 6)];
}

inline float fastTanh (float x)
{
    x = std::clamp (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return std::clamp (num / den, -1.0f, 1.0f);
}

// Vorberechnete Kennlinie fuer Typ + Bite
struct Shaper
{
    SatType type = SatType::fuzz;
    float bite = 0.5f;
    // abgeleitete Werte
    float k = 1.0f, bias = 0.0f, rest = 0.0f, norm = 1.0f, ceilP = 1.0f, ceilN = 1.0f, mixA = 0.0f;

    void set (SatType t, float b)
    {
        type = t;
        bite = std::clamp (b, 0.0f, 1.0f);
        rest = 0.0f; norm = 1.0f; bias = 0.0f; k = 1.0f; ceilP = 1.0f; ceilN = 1.0f; mixA = 0.0f;
        switch (type)
        {
            case SatType::splat:
                ceilP = 0.78f - 0.2f * bite;   // positive Seite tiefer gedeckelt
                ceilN = 1.0f;
                mixA  = 0.004f + 0.05f * bite; // Gate-Schwelle (Kratzen bei leisen Stellen)
                break;
            case SatType::fuzz:
            {
                const float h = 0.25f + 0.7f * bite;
                bias = 0.02f + 0.16f * h;
                k = 1.0f + 0.7f * h;           // positive Steilheit
                ceilP = 1.0f - 0.18f * h;
                mixA = std::max (0.0f, h - 0.45f) * 1.4f; // Richtung Rechteck
                rest = 0.0f;
                rest = raw (0.0f);
                const float e = 1e-3f;
                norm = e / std::max (1e-6f, raw (e) - rest);
                break;
            }
            case SatType::fold:
                k = 1.0f + 2.5f * bite;        // Faltdichte
                break;
            case SatType::diode:
                k = 1.0f + 1.5f * bite;        // Asymmetrie
                break;
            case SatType::crunch:
                mixA = bite;                   // Knie: weich -> hart
                break;
            case SatType::tube:
                bias = 0.12f + 0.45f * bite;
                rest = std::tanh (bias);
                break;
            case SatType::tape:
                k = 0.6f + 0.8f * bite;        // Saettigungsgrad
                break;
            case SatType::count: break;
        }
    }

    inline float raw (float a) const
    {
        // Fuzz: asymmetrischer Transistor, mit mehr Bite Richtung Rechteck
        const float v = a + bias;
        float y = v > 0.0f ? fastTanh (v * k) * ceilP : fastTanh (v);
        if (mixA > 0.0f)
        {
            const float hard = fastTanh (v * 4.0f) * (v > 0.0f ? ceilP : 1.0f);
            y += (hard - y) * std::min (1.0f, mixA);
        }
        return y;
    }

    inline float operator() (float x) const
    {
        switch (type)
        {
            case SatType::splat:
            {
                const float y = std::clamp (x * (1.0f + 1.5f * bite), -ceilN, ceilP);
                const float ax = std::abs (x);
                return y * ax / (ax + mixA);      // leise Stellen zerhackt
            }
            case SatType::fuzz:
                return (raw (x) - rest) * norm;
            case SatType::fold:
            {
                const float s = std::sqrt (k);
                return 0.9f * std::sin (x * k / 0.9f) / s;
            }
            case SatType::diode:
                if (x >= 0.0f) return 1.0f - std::exp (-x);
                return -(1.0f - std::exp (x * k)) / k;
            case SatType::crunch:
            {
                const float b = std::clamp (x / 1.5f, -1.0f, 1.0f);
                const float soft = 1.5f * (b - b * b * b / 3.0f);
                const float hard = std::clamp (x, -1.0f, 1.0f);
                return soft + (hard - soft) * mixA;
            }
            case SatType::tube:
                return std::tanh (x + bias) - rest;
            case SatType::tape:
            {
                const float a = x * k;
                const float y = 0.63662f * std::atan (1.5708f * a);
                return (y + (fastTanh (a) - y) * bite) / k;
            }
            case SatType::count: break;
        }
        return x;
    }

    // Kleinsignal-Verstaerkung (numerisch, fuer den Tight-Ausgleich), bei +-0.05 gemessen,
    // damit das Gate von Splat die Messung nicht verfaelscht
    float slope0() const
    {
        const float e = 0.05f;
        return ((*this) (e) - (*this) (-e)) / (2.0f * e);
    }
};

} // namespace bfz
