#pragma once

// Brainbugz Step Delay: Engine ohne JUCE-Abhängigkeit
// Feste Taps ohne Feedback: Step k ist genau ein Echo nach k * Step-Länge,
// mit eigener Lautstärke. Alle Steps teilen sich ein Raster (z. B. 1/8 punktiert).

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace bb
{

struct StepDelayParams
{
    static constexpr int kMaxSteps = 32;

    int   numSteps    = 8;
    double stepSamples = 12000.0;              // Länge eines Steps in Samples (aus Raster oder ms)
    std::array<float, kMaxSteps> level {};     // 0..1 pro Step
    float mix     = 0.5f;                      // 0 = nur trocken, 1 = nur Echos
    float outGain = 1.0f;                      // linear
    float loCutHz = 20.0f;                     // <= 20 Hz = aus
    float hiCutHz = 20000.0f;                  // >= 20 kHz = aus
};

// 12-dB-State-Variable-Filter (TPT), liefert Hoch- oder Tiefpass
struct Svf
{
    double ic1 = 0.0, ic2 = 0.0, g = 0.0, k = 1.4142135623730951, a1 = 0.0, a2 = 0.0, a3 = 0.0;

    void set (double hz, double sr)
    {
        hz = std::clamp (hz, 10.0, sr * 0.45);
        g  = std::tan (3.141592653589793 * hz / sr);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0.0; }

    // mode 0 = Tiefpass, 1 = Hochpass
    double process (double x, int mode)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return mode == 0 ? v2 : x - k * v1 - v2;
    }
};

class StepDelayEngine
{
public:
    static constexpr int    kMaxSteps   = StepDelayParams::kMaxSteps;
    static constexpr double kMaxSeconds = 24.0;   // längstes Echo (Step 32), Rest wird stumm

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        bufLen = (int) std::ceil (kMaxSeconds * sr) + 8;
        for (auto& b : buf) b.assign ((size_t) bufLen, 0.0f);
        write = 0;
        curStep = -1.0;
        gains.fill (0.0f);
        tapPeak.fill (0.0f);
        lastLo = lastHi = -1.0f;
        for (auto& f : lo) f.reset();
        for (auto& f : hi) f.reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& f : lo) f.reset();
        for (auto& f : hi) f.reset();
        tapPeak.fill (0.0f);
    }

    // L/R werden in-place bearbeitet
    void process (float* L, float* R, int n, const StepDelayParams& p)
    {
        if (bufLen <= 0 || n <= 0) return;

        const double maxDelay = (double) (bufLen - 4);
        const double target   = std::clamp (p.stepSamples, 16.0, maxDelay);
        if (curStep < 0.0) curStep = target;

        // Step-Länge sanft nachziehen (Tempowechsel ohne Knackser, ~60 ms)
        const double glide = 1.0 - std::exp (-1.0 / (0.06 * sr));

        // Lautstärken pro Block linear rampen
        std::array<float, kMaxSteps> gTarget {}, gStep {};
        for (int k = 0; k < kMaxSteps; ++k)
        {
            gTarget[(size_t) k] = k < p.numSteps ? std::clamp (p.level[(size_t) k], 0.0f, 1.0f) : 0.0f;
            gStep[(size_t) k]   = (gTarget[(size_t) k] - gains[(size_t) k]) / (float) n;
        }

        // Filter nur neu berechnen, wenn sich etwas ändert
        const bool loOn = p.loCutHz > 20.5f, hiOn = p.hiCutHz < 19999.0f;
        if (loOn && std::abs (p.loCutHz - lastLo) > 0.01f) { for (auto& f : lo) f.set (p.loCutHz, sr); lastLo = p.loCutHz; }
        if (hiOn && std::abs (p.hiCutHz - lastHi) > 0.01f) { for (auto& f : hi) f.set (p.hiCutHz, sr); lastHi = p.hiCutHz; }

        const float m   = std::clamp (p.mix, 0.0f, 1.0f);
        const float dry = std::min (1.0f, 2.0f * (1.0f - m)) * p.outGain;
        const float wet = std::min (1.0f, 2.0f * m) * p.outGain;

        std::array<float, kMaxSteps> peak {};
        int active = 0;
        for (int k = 0; k < kMaxSteps; ++k)
            if (gTarget[(size_t) k] > 0.0f || gains[(size_t) k] > 0.0f) active = k + 1;

        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            buf[0][(size_t) write] = inL;
            buf[1][(size_t) write] = inR;

            curStep += (target - curStep) * glide;

            double wl = 0.0, wr = 0.0;
            for (int k = 0; k < active; ++k)
            {
                gains[(size_t) k] += gStep[(size_t) k];
                const float g = gains[(size_t) k];
                if (g <= 0.00001f) continue;

                const double d = curStep * (double) (k + 1);
                if (d > maxDelay) break; // weiter hinten passt nichts mehr in den Puffer

                double rp = (double) write - d;
                if (rp < 0.0) rp += (double) bufLen;
                const int   i0 = (int) rp;
                const int   i1 = i0 + 1 >= bufLen ? 0 : i0 + 1;
                const float fr = (float) (rp - (double) i0);

                const float sl = buf[0][(size_t) i0] + (buf[0][(size_t) i1] - buf[0][(size_t) i0]) * fr;
                const float sr_ = buf[1][(size_t) i0] + (buf[1][(size_t) i1] - buf[1][(size_t) i0]) * fr;
                wl += sl * g;
                wr += sr_ * g;
                peak[(size_t) k] = std::max (peak[(size_t) k], std::max (std::abs (sl), std::abs (sr_)) * g);
            }

            if (loOn) { wl = lo[0].process (wl, 1); wr = lo[1].process (wr, 1); }
            if (hiOn) { wl = hi[0].process (wl, 0); wr = hi[1].process (wr, 0); }

            L[i] = inL * dry + (float) wl * wet;
            R[i] = inR * dry + (float) wr * wet;

            if (++write >= bufLen) write = 0;
        }

        for (int k = 0; k < kMaxSteps; ++k)
        {
            gains[(size_t) k] = gTarget[(size_t) k];
            tapPeak[(size_t) k] = peak[(size_t) k];
        }
    }

    // Spitzenpegel jedes Echos im letzten Block (für die leuchtenden Balken)
    float getTapPeak (int k) const { return tapPeak[(size_t) k]; }
    double getStepSamples() const { return curStep; }

private:
    double sr = 48000.0;
    int bufLen = 0, write = 0;
    std::array<std::vector<float>, 2> buf;
    double curStep = -1.0;
    std::array<float, kMaxSteps> gains {}, tapPeak {};
    std::array<Svf, 2> lo, hi;
    float lastLo = -1.0f, lastHi = -1.0f;
};

// Raster: Notenwert in Vierteln (1/4 = 1.0) mal Modus-Faktor
namespace stepgrid
{
    inline constexpr int kNumRates = 7;
    inline const char* rateNames[kNumRates] = { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" };
    inline constexpr double rateBeats[kNumRates] = { 0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0 };

    inline constexpr int kNumModes = 3;
    inline const char* modeNames[kNumModes] = { "Gerade", "Punktiert", "Triole" };
    inline const char* modeShort[kNumModes] = { "", ".", "T" };
    inline constexpr double modeFactor[kNumModes] = { 1.0, 1.5, 2.0 / 3.0 };

    inline double beats (int rate, int mode)
    {
        rate = std::clamp (rate, 0, kNumRates - 1);
        mode = std::clamp (mode, 0, kNumModes - 1);
        return rateBeats[rate] * modeFactor[mode];
    }
}

} // namespace bb
