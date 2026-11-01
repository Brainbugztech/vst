#pragma once

// Offline-Bearbeitung der gerenderten Stimme (Mono, 24 kHz):
//  - Formant: Resampling + PSOLA-Korrektur der Tonhöhe (Klangfarbe ändert sich, Tonhöhe nicht)
//  - Pitch:   TD-PSOLA (Tonhöhe ändert sich, Formanten bleiben -> klingt nicht nach Micky Maus)
//  - Speed:   0.5..2 macht Kokoro selbst, 2..4 und 0.125..0.5 per PSOLA, darunter Paulstretch (Drone)

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <functional>
#include <random>
#include <vector>

namespace tts::fx
{
using Buffer = std::vector<float>;

struct VoiceParams
{
    float pitchSemis = 0.0f; // -12..+12
    float formant = 1.0f;    // 0.5..2
    float speed = 1.0f;      // 0.001..4
};

// Wie schnell Kokoro selbst sprechen soll (der Rest passiert hier)
inline float kokoroSpeedFor (float speed) { return juce::jlimit (0.5f, 2.0f, speed); }

// Maximale Länge des Ergebnisses (Speicher): 15 Minuten
inline constexpr double maxSeconds = 15.0 * 60.0;

// ---------------------------------------------------------------- Resampling (Lanczos)

inline Buffer resample (const Buffer& in, double ratio) // ratio > 1: schneller lesen (kürzer, höher)
{
    if (in.empty() || ratio <= 0.0) return in;
    const size_t outLen = (size_t) std::max (1.0, std::floor ((double) in.size() / ratio));
    Buffer out (outLen);
    const int a = 8;
    const double scale = std::min (1.0, 1.0 / ratio); // Tiefpass beim Herunterrechnen
    const int width = (int) std::ceil (a / scale);
    const int n = (int) in.size();
    for (size_t i = 0; i < outLen; ++i)
    {
        const double t = (double) i * ratio;
        const int c = (int) std::floor (t);
        double acc = 0.0, wsum = 0.0;
        for (int k = c - width + 1; k <= c + width; ++k)
        {
            if (k < 0 || k >= n) continue;
            const double x = (t - k) * scale;
            double w;
            if (std::abs (x) < 1e-9) w = 1.0;
            else if (std::abs (x) >= a) w = 0.0;
            else
            {
                const double px = juce::MathConstants<double>::pi * x;
                w = a * std::sin (px) * std::sin (px / a) / (px * px);
            }
            acc += in[(size_t) k] * w;
            wsum += w;
        }
        out[i] = (float) (wsum > 1e-9 ? acc / wsum : 0.0);
    }
    return out;
}

// ---------------------------------------------------------------- Tonhöhe erkennen + Pitch-Marken

struct PitchMarks
{
    std::vector<int> pos;      // Sample-Position jeder Marke
    std::vector<bool> voiced;  // stimmhaft?
};

inline PitchMarks findPitchMarks (const Buffer& x, int sr)
{
    PitchMarks pm;
    const int n = (int) x.size();
    if (n == 0) return pm;
    const int hop = sr / 100;               // 10 ms
    const int win = sr / 25;                // 40 ms
    const int minLag = sr / 450, maxLag = sr / 65;
    const int frames = n / hop + 1;
    std::vector<float> f0 ((size_t) frames, 0.0f);
    std::vector<float> d ((size_t) maxLag + 2);

    for (int f = 0; f < frames; ++f)
    {
        const int start = f * hop - win / 2;
        double energy = 0.0;
        for (int i = 0; i < win; ++i)
        {
            const int k = start + i;
            const float v = (k >= 0 && k < n) ? x[(size_t) k] : 0.0f;
            energy += v * v;
        }
        if (energy / win < 1e-6) continue; // Stille
        // YIN-Differenzfunktion
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            double s = 0.0;
            for (int i = 0; i < win - maxLag; ++i)
            {
                const int k = start + i;
                const float a = (k >= 0 && k < n) ? x[(size_t) k] : 0.0f;
                const float b = (k + lag >= 0 && k + lag < n) ? x[(size_t) (k + lag)] : 0.0f;
                s += (a - b) * (a - b);
            }
            d[(size_t) lag] = (float) s;
        }
        // kumulativ normalisieren
        double run = 0.0;
        int best = -1;
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            run += d[(size_t) lag];
            const float c = run > 0 ? (float) (d[(size_t) lag] * lag / run) : 1.0f;
            d[(size_t) lag] = c;
        }
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            if (d[(size_t) lag] < 0.22f)
            {
                while (lag + 1 <= maxLag && d[(size_t) lag + 1] < d[(size_t) lag]) ++lag;
                best = lag;
                break;
            }
        }
        if (best > 0) f0[(size_t) f] = (float) sr / (float) best;
    }
    // kleine Ausreißer glätten (Median aus 3)
    for (int f = 1; f + 1 < frames; ++f)
    {
        float a = f0[(size_t) f - 1], b = f0[(size_t) f], c = f0[(size_t) f + 1];
        if (b > 0 && a <= 0 && c <= 0) f0[(size_t) f] = 0;
    }

    const int unvoicedPeriod = sr / 200;
    int t = 0;
    while (t < n)
    {
        const int f = juce::jlimit (0, frames - 1, t / hop);
        const float freq = f0[(size_t) f];
        if (freq > 0)
        {
            const int period = (int) ((float) sr / freq);
            // auf nahes Maximum einrasten
            int bestK = t;
            float bestV = -1.0f;
            const int r = period / 3;
            for (int k = std::max (0, t - r); k <= std::min (n - 1, t + r); ++k)
                if (std::abs (x[(size_t) k]) > bestV) { bestV = std::abs (x[(size_t) k]); bestK = k; }
            if (! pm.pos.empty() && bestK <= pm.pos.back() + period / 2) bestK = pm.pos.back() + period;
            if (bestK >= n) break;
            pm.pos.push_back (bestK);
            pm.voiced.push_back (true);
            t = bestK + period;
        }
        else
        {
            pm.pos.push_back (t);
            pm.voiced.push_back (false);
            t += unvoicedPeriod;
        }
    }
    return pm;
}

// ---------------------------------------------------------------- TD-PSOLA

inline Buffer psola (const Buffer& x, int sr, double pitchFactor, double timeFactor, const std::atomic<bool>* cancel = nullptr)
{
    if (x.empty()) return x;
    if (std::abs (pitchFactor - 1.0) < 1e-4 && std::abs (timeFactor - 1.0) < 1e-4) return x;
    const auto pm = findPitchMarks (x, sr);
    const int nm = (int) pm.pos.size();
    if (nm < 2) return x;
    const int n = (int) x.size();
    const size_t outLen = (size_t) ((double) n * timeFactor) + 1;
    Buffer out (outLen, 0.0f), wsum (outLen, 0.0f);

    auto periodAt = [&] (int k)
    {
        if (k + 1 < nm) return std::max (16, pm.pos[(size_t) k + 1] - pm.pos[(size_t) k]);
        return std::max (16, pm.pos[(size_t) k] - pm.pos[(size_t) k - 1]);
    };

    double tout = pm.pos[0] * timeFactor;
    int k = 0;
    int guard = 0;
    while (tout < (double) outLen)
    {
        if (cancel != nullptr && (++guard & 1023) == 0 && cancel->load()) return {};
        const double tin = tout / timeFactor;
        while (k + 1 < nm && std::abs (pm.pos[(size_t) k + 1] - tin) <= std::abs (pm.pos[(size_t) k] - tin)) ++k;
        while (k > 0 && pm.pos[(size_t) k - 1] > tin) --k;
        const int P = periodAt (k);
        const bool v = pm.voiced[(size_t) k];
        const int centre = pm.pos[(size_t) k];
        const int oc = (int) tout;
        for (int i = -P; i < P; ++i)
        {
            const int src = centre + i, dst = oc + i;
            if (src < 0 || src >= n || dst < 0 || dst >= (int) outLen) continue;
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (i + P) / (float) P);
            out[(size_t) dst] += x[(size_t) src] * w;
            wsum[(size_t) dst] += w;
        }
        const double hop = v ? (double) P / pitchFactor : (double) P;
        tout += std::max (8.0, hop);
    }
    for (size_t i = 0; i < outLen; ++i) out[i] /= std::max (wsum[i], 0.65f);
    return out;
}

// ---------------------------------------------------------------- Paulstretch (extrem langsam)

inline Buffer paulstretch (const Buffer& x, int sr, double stretch, std::function<void (float)> progress = {},
                           const std::atomic<bool>* cancel = nullptr)
{
    if (x.empty() || stretch <= 1.0) return x;
    const int order = sr >= 32000 ? 14 : 13; // ~0.34 s Fenster
    const int N = 1 << order;
    const int half = N / 2;
    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const float t = (float) (2 * i - N) / (float) N;
        win[(size_t) i] = std::pow (std::max (0.0f, 1.0f - t * t), 1.25f);
    }
    const double maxLen = maxSeconds * sr;
    const size_t outLen = (size_t) std::min ((double) x.size() * stretch + N, maxLen);
    Buffer out (outLen + (size_t) N, 0.0f);
    std::vector<float> buf ((size_t) N * 2);
    std::vector<juce::dsp::Complex<float>> spec ((size_t) N), time ((size_t) N);
    std::mt19937 rng (12345);
    std::uniform_real_distribution<float> uni (0.0f, juce::MathConstants<float>::twoPi);

    const double step = (double) half / stretch;
    double pos = -half;
    size_t o = 0;
    int frame = 0;
    const size_t totalFrames = outLen / (size_t) half + 1;
    while (o < outLen)
    {
        if (cancel != nullptr && cancel->load()) return {};
        std::fill (buf.begin(), buf.end(), 0.0f);
        const int ip = (int) pos;
        for (int i = 0; i < N; ++i)
        {
            const int k = ip + i;
            buf[(size_t) i] = (k >= 0 && k < (int) x.size()) ? x[(size_t) k] * win[(size_t) i] : 0.0f;
        }
        // Spektrum: Beträge behalten, Phasen würfeln
        for (int i = 0; i < N; ++i) time[(size_t) i] = { buf[(size_t) i], 0.0f };
        fft.perform (time.data(), spec.data(), false);
        for (int i = 0; i < N; ++i)
        {
            const float mag = std::abs (spec[(size_t) i]);
            const float ph = uni (rng);
            spec[(size_t) i] = std::polar (mag, ph);
        }
        fft.perform (spec.data(), time.data(), true);
        for (int i = 0; i < N; ++i)
            out[o + (size_t) i] += time[(size_t) i].real() * win[(size_t) i];
        o += (size_t) half;
        pos += step;
        if (pos > (double) x.size() + half) break;
        if (progress && (++frame & 63) == 0) progress ((float) frame / (float) totalFrames);
    }
    out.resize (std::min (o + (size_t) half, outLen));
    return out;
}

// ---------------------------------------------------------------- alles zusammen

inline void normalise (Buffer& b, float target = 0.89f)
{
    float peak = 0.0f;
    for (auto v : b) peak = std::max (peak, std::abs (v));
    if (peak < 1e-6f) return;
    const float g = target / peak;
    for (auto& v : b) v *= g;
}

inline Buffer processVoice (const Buffer& raw, int sr, const VoiceParams& p, std::function<void (float)> progress = {},
                            const std::atomic<bool>* cancel = nullptr)
{
    Buffer x = raw;
    const double pitch = std::pow (2.0, p.pitchSemis / 12.0);
    const double formant = juce::jlimit (0.5, 2.0, (double) p.formant);
    const double speed = juce::jlimit (0.001, 4.0, (double) p.speed);

    // Zeitfaktor, den PSOLA übernimmt (Kokoro macht schon 0.5..2)
    double timeFactor = 1.0;
    if (speed > 2.0) timeFactor = 2.0 / speed;
    double paul = 1.0;
    if (speed < 0.5)
    {
        const double extra = 0.5 / speed;             // gesamt noch zu dehnen
        timeFactor = std::min (extra, 4.0);           // bis 4x per PSOLA (noch verständlich)
        paul = extra / timeFactor;                    // Rest per Paulstretch
    }

    if (std::abs (formant - 1.0) > 1e-3)
    {
        x = resample (x, formant);                    // Formanten + Tonhöhe * formant, Länge / formant
        if (cancel != nullptr && cancel->load()) return {};
        x = psola (x, sr, pitch / formant, formant * timeFactor, cancel);
    }
    else
        x = psola (x, sr, pitch, timeFactor, cancel);
    if (progress) progress (paul > 1.0 ? 0.2f : 0.9f);

    if (paul > 1.0001)
        x = paulstretch (x, sr, paul, [&] (float f) { if (progress) progress (0.2f + 0.8f * f); }, cancel);

    normalise (x);
    if (progress) progress (1.0f);
    return x;
}
} // namespace tts::fx
