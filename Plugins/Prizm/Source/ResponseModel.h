#pragma once

#include <cmath>
#include <vector>

#include "Scales.h"

namespace prizm
{
// Gemeinsamer Filter-Kern beider Modi: eine Bank aus Bandpässen (TPT-SVF, Spitze = 1),
// ein Filter pro Note der Tonart. Zero Latency rechnet die Filter direkt, High Quality
// benutzt die exakt gleiche Betragskurve als spektrale Maske. So klingen beide Modi gleich.
class ResponseModel
{
public:
    static constexpr int kLowestNote = 24;   // C1, ca. 33 Hz
    static constexpr int kHighestNote = 127;

    // Color 0..2 -> Filtergüte: 0 % = Q 8 (weich), 100 % = Q ~110, 200 % = Q 1500 (langer Tail)
    static float qFromColor (float color)
    {
        const float c = color < 0.0f ? 0.0f : (color > 2.0f ? 2.0f : color);
        return 8.0f * std::pow (187.5f, c * 0.5f);
    }

    static double noteHz (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        notes.clear();
        prewarped.clear();
        for (int n = kLowestNote; n <= kHighestNote; ++n)
        {
            const double f = noteHz (n);
            if (f > 0.45 * fs)
                break;
            notes.push_back (n);
            prewarped.push_back (std::tan (pi * f / fs));
        }
        table.assign ((size_t) kTableSize, 0.0f);
        lastMask = 0xFFFF;
        lastQ = -1.0f;
    }

    const std::vector<int>& getNotes() const noexcept { return notes; }
    const std::vector<double>& getPrewarped() const noexcept { return prewarped; }

    // Baut die Nachschlagetabelle neu, wenn sich Tonart oder Güte merklich geändert haben.
    void update (PitchMask mask, float q)
    {
        if (mask == lastMask && lastQ > 0.0f && std::abs (q - lastQ) < lastQ * 0.004f)
            return;
        lastMask = mask;
        lastQ = q;

        const double k = 1.0 / q;
        for (int t = 0; t < kTableSize; ++t)
        {
            const double midi = kTableLowMidi + (double) t / kStepsPerSemitone;
            const double hz = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
            table[(size_t) t] = (float) evaluate (mask, k, hz, midi);
        }
    }

    // Betrag der Filterbank bei einer Frequenz (aus der Tabelle, interpoliert).
    float magnitudeAt (float hz) const noexcept
    {
        if (hz <= 1.0f)
            return 0.0f;
        const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const float pos = (midi - (float) kTableLowMidi) * (float) kStepsPerSemitone;
        if (pos <= 0.0f)
            return table.front();
        if (pos >= (float) (kTableSize - 1))
            return table.back();
        const int i = (int) pos;
        const float frac = pos - (float) i;
        return table[(size_t) i] + (table[(size_t) i + 1] - table[(size_t) i]) * frac;
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    static constexpr int kTableLowMidi = 12;
    static constexpr int kStepsPerSemitone = 40;
    static constexpr int kTableSize = (140 - kTableLowMidi) * kStepsPerSemitone;

    // Exakte digitale Antwort: Summe der Bandpässe H(s) = k s / (s^2 + k s + 1), s = j tan(w/2) / tan(w0/2)
    double evaluate (PitchMask mask, double k, double hz, double midi) const
    {
        if (hz >= 0.5 * fs)
            return 0.0;
        const double t = std::tan (pi * hz / fs);
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            if (std::abs ((double) notes[i] - midi) > 30.0 || ! isAllowed (mask, notes[i]))
                continue;
            const double w = t / prewarped[i];
            const double a = 1.0 - w * w, b = k * w;
            const double d = a * a + b * b;
            re += (b * b) / d;
            im += (b * a) / d;
        }
        return std::sqrt (re * re + im * im);
    }

    double fs = 48000.0;
    std::vector<int> notes;
    std::vector<double> prewarped;
    std::vector<float> table;
    PitchMask lastMask = 0xFFFF;
    float lastQ = -1.0f;
};
} // namespace prizm
