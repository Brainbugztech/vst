#pragma once

// Schneller Sinc-Resampler (Kaiser-Fenster, 32 Taps, Tabelle mit 512 Phasen) für 24 kHz -> Host-Samplerate

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace tts::dsp
{
class SincResampler
{
public:
    static constexpr int half = 16;          // Taps je Seite
    static constexpr int taps = half * 2;
    static constexpr int phases = 512;

    void prepare (double inRate, double outRate)
    {
        step = inRate / outRate;
        const double cutoff = 0.46 * juce::jmin (1.0, 1.0 / step); // relativ zur Eingangs-Samplerate
        table.assign ((size_t) (phases + 1) * taps, 0.0f);
        const double beta = 7.0;
        auto bessel0 = [] (double x)
        {
            double s = 1.0, t = 1.0;
            for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; }
            return s;
        };
        const double i0b = bessel0 (beta);
        for (int p = 0; p <= phases; ++p)
        {
            const double frac = (double) p / phases;
            double sum = 0.0;
            for (int k = 0; k < taps; ++k)
            {
                const double x = (double) (k - half + 1) - frac; // Abstand zum Ausgabezeitpunkt
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * 2.0 * cutoff * x)
                                                                 / (juce::MathConstants<double>::pi * 2.0 * cutoff * x);
                const double r = x / (half + 0.0);
                const double w = std::abs (r) >= 1.0 ? 0.0 : bessel0 (beta * std::sqrt (1.0 - r * r)) / i0b;
                const double v = 2.0 * cutoff * sinc * w;
                table[(size_t) p * taps + (size_t) k] = (float) v;
                sum += v;
            }
            // auf Gleichanteil 1 normieren
            for (int k = 0; k < taps; ++k) table[(size_t) p * taps + (size_t) k] = (float) (table[(size_t) p * taps + (size_t) k] / sum);
        }
        reset();
    }

    void reset() { pos = (double) half; }

    // Wie viele Eingangs-Samples (inkl. Vorlauf) werden für num Ausgaben gebraucht
    int inputNeeded (int num) const { return (int) std::floor (pos + step * num) + half + 1; }

    // in: Puffer, der bei Index 0 beginnt (enthält 'half' Samples Vorlauf). Rückgabe: wie viele Samples vorne verworfen werden können
    int process (const float* in, float* out, int num)
    {
        for (int i = 0; i < num; ++i)
        {
            const int ip = (int) pos;
            const double fr = pos - ip;
            const double ph = fr * phases;
            const int p0 = (int) ph;
            const float t = (float) (ph - p0);
            const float* c0 = &table[(size_t) p0 * taps];
            const float* c1 = c0 + taps;
            const float* x = in + ip - half + 1;
            float a = 0.0f, b = 0.0f;
            for (int k = 0; k < taps; ++k)
            {
                a += c0[k] * x[k];
                b += c1[k] * x[k];
            }
            out[i] = a + (b - a) * t;
            pos += step;
        }
        const int drop = juce::jmax (0, (int) pos - half);
        pos -= drop;
        return drop;
    }

private:
    double step = 0.5, pos = half;
    std::vector<float> table;
};
} // namespace tts::dsp
