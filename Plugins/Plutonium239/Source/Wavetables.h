#pragma once

// Eingebaute Wavetables: 8 Tabellen x 32 Frames x 2048 Samples, je 10 bandbegrenzte Mip-Stufen
// (pro Oktave weniger Obertoene, damit hohe Toene nicht aliasen). Alles wird beim Start berechnet.

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace pluto
{
class WavetableBank
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numFrames = 32;
    static constexpr int numMips = 10;
    static constexpr int numTables = 8;

    WavetableBank()
    {
        data.resize ((size_t) numTables * numFrames * numMips * tableSize);
        juce::dsp::FFT fft (11);
        std::vector<float> buf (tableSize * 2), spec (tableSize * 2), tmp (tableSize);

        for (int t = 0; t < numTables; ++t)
            for (int fr = 0; fr < numFrames; ++fr)
            {
                const float pos = (float) fr / (float) (numFrames - 1);
                for (int i = 0; i < tableSize; ++i)
                    tmp[(size_t) i] = generate (t, pos, (float) i / (float) tableSize);

                std::fill (spec.begin(), spec.end(), 0.0f);
                std::copy (tmp.begin(), tmp.end(), spec.begin());
                fft.performRealOnlyForwardTransform (spec.data(), true);
                spec[0] = spec[1] = 0.0f; // DC weg

                float gain = 1.0f;
                for (int m = 0; m < numMips; ++m)
                {
                    const int maxH = (tableSize / 2) >> m;
                    std::copy (spec.begin(), spec.end(), buf.begin());
                    for (int h = maxH; h <= tableSize / 2; ++h)
                        buf[(size_t) h * 2] = buf[(size_t) h * 2 + 1] = 0.0f;
                    // Rest der Haelfte (negative Frequenzen) ist bei performRealOnlyInverseTransform egal
                    fft.performRealOnlyInverseTransform (buf.data());

                    float* dst = frame (t, fr, m);
                    if (m == 0)
                    {
                        float peak = 1.0e-6f;
                        for (int i = 0; i < tableSize; ++i) peak = std::max (peak, std::abs (buf[(size_t) i]));
                        gain = 1.0f / peak;
                    }
                    for (int i = 0; i < tableSize; ++i) dst[i] = buf[(size_t) i] * gain;
                }
            }
    }

    // phase 0..1, pos 0..1, inc = Phasenschritt pro Sample (fuer die Mip-Wahl)
    float read (int table, float pos, double phase, double inc) const noexcept
    {
        const double ai = std::abs (inc);
        int mip = 0;
        if (ai > 0.0)
        {
            mip = (int) std::ceil (std::log2 (ai * (double) tableSize));
            mip = juce::jlimit (0, numMips - 1, mip);
        }
        const float fp = juce::jlimit (0.0f, 1.0f, pos) * (float) (numFrames - 1);
        const int f0 = juce::jmin ((int) fp, numFrames - 2);
        const float ff = fp - (float) f0;
        const double x = phase * tableSize;
        const int i0 = (int) x & (tableSize - 1);
        const int i1 = (i0 + 1) & (tableSize - 1);
        const float fx = (float) (x - std::floor (x));
        const int tb = juce::jlimit (0, numTables - 1, table);
        const float* a = frame (tb, f0, mip);
        const float* b = frame (tb, f0 + 1, mip);
        const float va = a[i0] + (a[i1] - a[i0]) * fx;
        const float vb = b[i0] + (b[i1] - b[i0]) * fx;
        return va + (vb - va) * ff;
    }

    // fuer die Anzeige (ohne Bandbegrenzung)
    float readDisplay (int table, float pos, float phase) const noexcept { return read (table, pos, phase, 0.0); }

private:
    std::vector<float> data;

    float* frame (int t, int fr, int m) { return data.data() + (((size_t) t * numFrames + (size_t) fr) * numMips + (size_t) m) * tableSize; }
    const float* frame (int t, int fr, int m) const { return data.data() + (((size_t) t * numFrames + (size_t) fr) * numMips + (size_t) m) * tableSize; }

    static float sinc2pi (float x) { return std::sin (juce::MathConstants<float>::twoPi * x); }

    static float naive (int shape, float x)
    {
        switch (shape)
        {
            case 0: return sinc2pi (x);
            case 1: return x < 0.25f ? 4.0f * x : (x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f);
            case 2: return 2.0f * x - 1.0f;
            default: return x < 0.5f ? 1.0f : -1.0f;
        }
    }

    static float generate (int table, float t, float x)
    {
        using juce::MathConstants;
        switch (table)
        {
            case 0: // Basic Morph: Sin -> Tri -> Saw -> Square
            {
                const float p = t * 3.0f;
                const int a = juce::jmin ((int) p, 2);
                const float f = p - (float) a;
                return naive (a, x) * (1.0f - f) + naive (a + 1, x) * f;
            }
            case 1: // PWM Sweep
            {
                const float w = 0.5f - t * 0.46f;
                return x < w ? 1.0f : -1.0f;
            }
            case 2: // Harmonic Rise: Obertoene kommen nacheinander dazu
            {
                const float n = 1.0f + t * 63.0f;
                float s = 0.0f;
                for (int h = 1; h <= 64; ++h)
                {
                    const float g = juce::jlimit (0.0f, 1.0f, n - (float) (h - 1));
                    if (g <= 0.0f) break;
                    s += g * sinc2pi ((float) h * x) / (float) h;
                }
                return s;
            }
            case 3: // Vowel Morph A-E-I-O-U
            {
                static const float F[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
                const float p = t * 4.0f;
                const int a = juce::jmin ((int) p, 3);
                const float f = p - (float) a;
                float fm[3];
                for (int k = 0; k < 3; ++k) fm[k] = F[a][k] * (1.0f - f) + F[a + 1][k] * f;
                const float f0 = 110.0f;
                float s = 0.0f;
                for (int h = 1; h <= 120; ++h)
                {
                    const float hz = f0 * (float) h;
                    float amp = 0.0f;
                    for (int k = 0; k < 3; ++k)
                    {
                        const float bw = 80.0f + 40.0f * (float) k;
                        const float d = (hz - fm[k]) / bw;
                        amp += std::exp (-0.5f * d * d) * (k == 0 ? 1.0f : (k == 1 ? 0.6f : 0.3f));
                    }
                    s += (amp + 0.02f / (float) h) * sinc2pi ((float) h * x);
                }
                return s;
            }
            case 4: // Sync Sweep: hart synchronisierter Saw
            {
                const float r = 1.0f + t * 7.0f;
                const float y = x * r;
                return 2.0f * (y - std::floor (y)) - 1.0f;
            }
            case 5: // FM Sweep
            {
                const float idx = t * 8.0f;
                return std::sin (MathConstants<float>::twoPi * x + idx * std::sin (MathConstants<float>::twoPi * x * 2.0f));
            }
            case 6: // Hollow Comb: Saw-Spektrum mit wanderndem Kamm
            {
                float s = 0.0f;
                for (int h = 1; h <= 96; ++h)
                {
                    const float c = 0.5f + 0.5f * std::cos ((float) h * MathConstants<float>::pi * (0.15f + t * 0.6f));
                    s += c * sinc2pi ((float) h * x) / (float) h;
                }
                return s;
            }
            default: // Digital Grit: gestufter Sinus mit Sample-and-Hold
            {
                const float levels = 24.0f - t * 21.0f;
                const float steps = 64.0f - t * 56.0f;
                const float xs = std::floor (x * steps) / steps;
                const float v = sinc2pi (xs) * 0.7f + sinc2pi (xs * 3.0f) * 0.3f * t;
                return std::round (v * levels) / levels;
            }
        }
    }
};
} // namespace pluto
