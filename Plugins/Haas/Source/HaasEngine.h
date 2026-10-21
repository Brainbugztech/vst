#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// Brainbugz Haas DSP
// - Frequenzweiche (2x One-Pole, komplementär): Tiefen bleiben mono und unverzögert
// - Höhen links/rechts unterschiedlich verzögert (Haas-Effekt)
// - "Krass"-Bereich (Width > 100 %): wabernde Delays, Allpass-Dekorrelation, Side-Boost (pegelkompensiert)

namespace bb
{

struct HaasParams
{
    float width    = 0.8f;   // 0..2   (1 = 100 %, darüber Krass-Bereich)
    float lBase    = 6.0f;   // ms bei 100 % Width
    float rBase    = 30.0f;  // ms bei 100 % Width
    float monoBass = 120.0f; // Hz, < 30 = aus
    bool  flip     = false;  // Seiten tauschen
    bool  mono     = false;  // Mono-Check
};

class HaasEngine
{
public:
    static constexpr float kMaxDelayMs = 40.0f;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int needed = (int) std::ceil (fs * (kMaxDelayMs + 3.0) / 1000.0) + 4;
        size = 1;
        while (size < needed) size <<= 1;
        mask = size - 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& l : lp) l = { 0.0, 0.0 };
        for (auto& a : ap) a.fill (0.0);
        writePos = 0;
        modPhase = { 0.0, 1.7 };
        eSmooth = 0.0;
        initialised = false;
    }

    static void delaysMs (const HaasParams& p, float& left, float& right)
    {
        const float w1 = std::min (1.0f, p.width);
        const float a = std::min (kMaxDelayMs, w1 * p.lBase);
        const float b = std::min (kMaxDelayMs, w1 * p.rBase);
        left  = p.flip ? b : a;
        right = p.flip ? a : b;
    }

    // left/right: Ein- und Ausgabe (in place)
    void process (float* left, float* right, int numSamples, const HaasParams& p)
    {
        float dlMs, drMs;
        delaysMs (p, dlMs, drMs);
        const double target[2] { dlMs * fs / 1000.0, drMs * fs / 1000.0 };

        if (! initialised)
        {
            dSmooth[0] = target[0];
            dSmooth[1] = target[1];
            initialised = true;
        }

        const double e      = std::clamp ((double) p.width - 1.0, 0.0, 1.0);
        const double modAmp = e * (0.25 + e * 1.1) * fs / 1000.0;
        const double kDelay = 1.0 - std::exp (-1.0 / (fs * 0.026));
        const double kE     = 1.0 - std::exp (-1.0 / (fs * 0.03));
        const bool   useLow = p.monoBass >= 30.0f;
        const double kLow   = 1.0 - std::exp (-2.0 * pi * std::max (20.0, (double) p.monoBass) / fs);
        const double modInc[2] { 2.0 * pi * 0.29 / fs, 2.0 * pi * 0.37 / fs };

        for (int i = 0; i < numSamples; ++i)
        {
            const double x[2] { left[i], right[i] };
            double lo[2] { 0.0, 0.0 }, hi[2] { x[0], x[1] };

            if (useLow)
            {
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = lp[(size_t) c];
                    s[0] += (x[c] - s[0]) * kLow;
                    s[1] += (s[0] - s[1]) * kLow;
                    lo[c] = s[1];
                    hi[c] = x[c] - s[1];
                }
            }

            const double loMono = useLow ? (lo[0] + lo[1]) * 0.5 : 0.0;
            eSmooth += (e - eSmooth) * kE;
            const double es = eSmooth;
            double d[2];

            for (int c = 0; c < 2; ++c)
            {
                auto& b = buf[(size_t) c];
                b[(size_t) writePos] = (float) hi[c];
                dSmooth[c] += (target[c] - dSmooth[c]) * kDelay;

                modPhase[(size_t) c] += modInc[c];
                if (modPhase[(size_t) c] > 2.0 * pi) modPhase[(size_t) c] -= 2.0 * pi;
                const double mod = std::sin (modPhase[(size_t) c]) * modAmp * (c ? 1.0 : -1.0);

                const double rd = writePos - std::max (0.0, dSmooth[c] + mod);
                const double ri = std::floor (rd);
                const double fr = rd - ri;
                const int i0 = ((int) ri) & mask;
                const int i1 = (i0 + 1) & mask;
                double v = b[(size_t) i0] * (1.0 - fr) + b[(size_t) i1] * fr;

                if (es > 0.001)
                {
                    auto& z = ap[(size_t) c];
                    double u = v;
                    for (size_t q = 0; q < 4; ++q)
                    {
                        const double co = apCoef[(size_t) c][q];
                        const double y = co * u + z[q];
                        z[q] = u - co * y;
                        u = y;
                    }
                    v += (u - v) * es;
                }
                d[c] = v;
            }

            writePos = (writePos + 1) & mask;

            if (es > 0.001)
            {
                const double mg = 1.0 - es * 0.12, sg = 1.0 + es * 0.95;
                const double nrm = 1.0 / std::sqrt ((mg * mg + sg * sg) * 0.5);
                const double mid  = (d[0] + d[1]) * 0.5 * mg * nrm;
                const double side = (d[0] - d[1]) * 0.5 * sg * nrm;
                d[0] = mid + side;
                d[1] = mid - side;
            }

            double o[2];
            for (int c = 0; c < 2; ++c)
            {
                double v = loMono + d[c];
                const double av = std::abs (v);
                if (av > 0.92) v = std::copysign (0.92 + 0.08 * std::tanh ((av - 0.92) / 0.08), v);
                o[c] = v;
            }

            if (p.mono)
            {
                const double m = (o[0] + o[1]) * 0.5;
                o[0] = o[1] = m;
            }

            left[i]  = (float) o[0];
            right[i] = (float) o[1];
        }
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    static constexpr double apCoef[2][4] { { 0.62, -0.41, 0.33, -0.57 }, { -0.49, 0.46, -0.36, 0.6 } };

    double fs = 48000.0;
    std::array<std::vector<float>, 2> buf;
    int size = 0, mask = 0, writePos = 0;
    std::array<std::array<double, 2>, 2> lp {};
    std::array<std::array<double, 4>, 2> ap {};
    std::array<double, 2> modPhase { 0.0, 1.7 };
    double dSmooth[2] { 0.0, 0.0 };
    double eSmooth = 0.0;
    bool initialised = false;
};

} // namespace bb
