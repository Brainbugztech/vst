#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

#include "Scales.h"

namespace prizm
{
// "Learn": hört eine Weile zu und schätzt Tonart (Dur/Moll, Krumhansl-Profile) und
// Stimmton (A4 in Hz). Läuft nur während des Lernens, sonst kostet es nichts.
// Sammelt Teiltöne in einem Cent-Histogramm (1200 Fächer pro Oktave).
class KeyAnalyzer
{
public:
    static constexpr double kLearnSeconds = 8.0; // so viel hörbares Material wird ausgewertet

    struct Result
    {
        int key = 0;          // 0 = C ... 11 = B
        bool minor = false;
        float a4 = 440.0f;    // erkannter Stimmton
        float confidence = 0; // 0..1, Abstand der besten zur zweitbesten Tonart
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        order = sampleRate > 50000.0 ? 14 : 13; // ~5,9 Hz Auflösung
        size = 1 << order;
        hop = size / 4;
        fft = std::make_unique<juce::dsp::FFT> (order);
        window.resize ((size_t) size);
        for (int i = 0; i < size; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
        input.assign ((size_t) size, 0.0f);
        frame.assign ((size_t) size * 2, 0.0f);
        cancel();
    }

    void start()
    {
        histogram.fill (0.0);
        std::fill (input.begin(), input.end(), 0.0f);
        writePos = hopCounter = 0;
        heardSamples = 0;
        active = true;
    }

    void cancel() { active = false; }
    bool isActive() const noexcept { return active; }
    float getProgress() const noexcept { return active ? (float) juce::jmin (1.0, heardSamples / (kLearnSeconds * fs)) : 0.0f; }

    // Mono-Eingang. Gibt true zurück, sobald ein Ergebnis fertig ist.
    bool process (const float* const* channels, int numChannels, int numSamples)
    {
        if (! active)
            return false;

        for (int n = 0; n < numSamples; ++n)
        {
            float x = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                x += channels[ch][n];
            x /= (float) juce::jmax (1, numChannels);

            input[(size_t) writePos] = x;
            writePos = (writePos + 1) % size;
            if (++hopCounter >= hop)
            {
                hopCounter = 0;
                analyseFrame();
            }
        }

        if (heardSamples >= kLearnSeconds * fs)
        {
            result = evaluate();
            active = false;
            return true;
        }
        return false;
    }

    Result getResult() const noexcept { return result; }

    // Für Tests: Ergebnis direkt aus einem Histogramm berechnen.
    Result evaluate() const
    {
        // 1) Stimmton: Kreismittel der Abweichung von der 440-Hz-Notenreihe
        double c = 0.0, s = 0.0;
        for (int i = 0; i < 1200; ++i)
        {
            const double a = juce::MathConstants<double>::twoPi * ((double) (i % 100) + 0.5) / 100.0;
            c += histogram[(size_t) i] * std::cos (a);
            s += histogram[(size_t) i] * std::sin (a);
        }
        double offsetCents = std::atan2 (s, c) / juce::MathConstants<double>::twoPi * 100.0; // -50..50
        Result r;
        r.a4 = (float) (440.0 * std::pow (2.0, offsetCents / 1200.0));

        // 2) Tonklassen-Profil mit korrigiertem Stimmton
        std::array<double, 12> chroma {};
        for (int i = 0; i < 1200; ++i)
        {
            const double cents = (double) i + 0.5 - offsetCents;
            const int pc = ((int) std::lround (cents / 100.0) % 12 + 12) % 12;
            chroma[(size_t) pc] += histogram[(size_t) i];
        }

        // 3) Krumhansl-Kessler-Profile, Korrelation für alle 24 Tonarten
        static constexpr std::array<double, 12> majorProfile { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static constexpr std::array<double, 12> minorProfile { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        double best = -2.0, second = -2.0;
        for (int key = 0; key < 12; ++key)
            for (int minor = 0; minor < 2; ++minor)
            {
                const auto& prof = minor ? minorProfile : majorProfile;
                std::array<double, 12> rotated {};
                for (int i = 0; i < 12; ++i)
                    rotated[(size_t) ((i + key) % 12)] = prof[(size_t) i];
                const double corr = correlation (chroma, rotated);
                if (corr > best)
                {
                    second = best;
                    best = corr;
                    r.key = key;
                    r.minor = minor == 1;
                }
                else if (corr > second)
                {
                    second = corr;
                }
            }
        r.confidence = (float) juce::jlimit (0.0, 1.0, (best - second) * 5.0);
        return r;
    }

private:
    static double correlation (const std::array<double, 12>& a, const std::array<double, 12>& b)
    {
        double ma = 0.0, mb = 0.0;
        for (size_t i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
        ma /= 12.0; mb /= 12.0;
        double num = 0.0, da = 0.0, db = 0.0;
        for (size_t i = 0; i < 12; ++i)
        {
            num += (a[i] - ma) * (b[i] - mb);
            da += (a[i] - ma) * (a[i] - ma);
            db += (b[i] - mb) * (b[i] - mb);
        }
        return num / (std::sqrt (da * db) + 1e-30);
    }

    void analyseFrame()
    {
        float energy = 0.0f;
        for (int i = 0; i < size; ++i)
        {
            const float v = input[(size_t) ((writePos + i) % size)];
            energy += v * v;
            frame[(size_t) i] = v * window[(size_t) i];
        }
        std::fill (frame.begin() + size, frame.end(), 0.0f);

        // nur hörbares Material zählt (ca. -50 dBFS RMS)
        if (energy / (float) size < 1.0e-5f)
            return;
        heardSamples += hop;

        fft->performFrequencyOnlyForwardTransform (frame.data(), true);
        const int bins = size / 2;
        float maxMag = 0.0f;
        for (int k = 1; k < bins; ++k)
            maxMag = juce::jmax (maxMag, frame[(size_t) k]);
        const float floorMag = maxMag * 0.01f; // -40 dB unter dem stärksten Teilton
        const double binHz = fs / size;

        for (int k = 2; k < bins - 1; ++k)
        {
            const float m = frame[(size_t) k];
            if (m < floorMag || m <= frame[(size_t) k - 1] || m < frame[(size_t) k + 1])
                continue;
            // parabolische Interpolation auf dem log-Betrag für die genaue Frequenz
            const double a = std::log (frame[(size_t) k - 1] + 1e-12), b = std::log (m + 1e-12), cc = std::log (frame[(size_t) k + 1] + 1e-12);
            const double denom = a - 2.0 * b + cc;
            const double delta = std::abs (denom) > 1e-12 ? 0.5 * (a - cc) / denom : 0.0;
            const double hz = ((double) k + juce::jlimit (-0.5, 0.5, delta)) * binHz;
            if (hz < 50.0 || hz > 5000.0)
                continue;
            const double cents = 1200.0 * std::log2 (hz / 440.0) + 900.0; // C = 0
            const int idx = ((int) std::floor (cents) % 1200 + 1200) % 1200;
            histogram[(size_t) idx] += std::sqrt ((double) m); // gedämpfte Gewichtung, laute Töne dominieren nicht alles
        }
    }

    double fs = 48000.0;
    int order = 13, size = 8192, hop = 2048;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, input, frame;
    std::array<double, 1200> histogram {};
    int writePos = 0, hopCounter = 0;
    double heardSamples = 0.0;
    bool active = false;
    Result result;
};
} // namespace prizm
