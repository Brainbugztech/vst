#pragma once

// Grain Engine (24 kHz, Stereo raus), wie Abletons Warp-Modus "Texture":
// Körner werden direkt aus der Stimme rund um die aktuelle Abspielposition gelesen.
// Dadurch dehnt Speed die Textur (Time-Stretch), Freeze hält die Position fest.
//  Size    = Korngröße                Flux    = Zufall (Zeitpunkt, Position, Größe, etwas Tonhöhe)
//  Density = Überlappung              Spray   = Streuung um die Abspielposition
//  Pitch   = Tonhöhen-Streuung        Octave  = Anteil Körner eine Oktave höher/tiefer (Shimmer)
//  Width   = Stereo-Breite            Reverse = Anteil rückwärts
//  Drift   = langsames Wandern der Leseposition
//  LFO     = Rate/Depth: Leseposition und Korngröße schwingen (Scan vor/zurück)

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <random>
#include <vector>

namespace tts::dsp
{
struct GrainParams
{
    float sizeMs = 80.0f;   // 10..500
    float flux = 0.3f;      // 0..1
    float density = 4.0f;   // Überlappung 1..16
    float sprayMs = 60.0f;  // 0..1000
    float pitchSpread = 0;  // Halbtöne 0..24
    float octave = 0.0f;    // 0..1
    float width = 0.5f;     // 0..1
    float reverse = 0.0f;   // 0..1
    float drift = 0.0f;     // 0..1
    float lfoRate = 0.5f;   // Hz
    float lfoDepth = 0.0f;  // 0..1
    bool freeze = false;
};

struct GrainVis
{
    float pos = 0;     // 0..1 in der Stimme
    float pitch = 0;   // Halbtöne
    float size = 0;    // 0..1
    float amp = 0;     // Hüllkurve 0..1
    float pan = 0;     // -1..1
    bool reverse = false;
};

class GrainEngine
{
public:
    static constexpr int maxGrains = 128;

    void prepare (int sampleRate) { sr = sampleRate; reset(); }

    void reset()
    {
        for (auto& g : grains) g.active = false;
        nextOnset = 0.0;
        driftPos = driftTarget = 0.0;
        driftCountdown = 0;
        lfoPhase = 0.0;
        busy = false;
    }

    bool isBusy() const { return busy; }

    // src: Originalstimme, playhead: Startposition dieses Blocks (Samples), speed: Fortschritt je Sample
    // pitchRatio: globale Tonhöhe (Pitch-Regler + MIDI), spawn: neue Körner erlaubt
    void process (const std::vector<float>* src, double playhead, double speed, double pitchRatio, bool spawn,
                  float* outL, float* outR, int num, const GrainParams& p)
    {
        const int len = src != nullptr ? (int) src->size() : 0;
        const float* s = len > 0 ? src->data() : nullptr;
        const double flux = juce::jlimit (0.0, 1.0, (double) p.flux);
        const double density = juce::jlimit (1.0, 16.0, (double) p.density);
        bool any = false;
        for (int i = 0; i < num; ++i)
        {
            // LFO + Drift
            lfoPhase += juce::jlimit (0.01, 20.0, (double) p.lfoRate) / sr;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;
            const double lfo = std::sin (juce::MathConstants<double>::twoPi * lfoPhase) * p.lfoDepth;
            // Drift: alle 0,5-1,5 s ein neues Zufallsziel, weich angesteuert
            if (--driftCountdown <= 0)
            {
                driftTarget = uni (rng) * 2.0 - 1.0;
                driftCountdown = (int) (sr * (0.5 + uni (rng)));
            }
            driftPos += (driftTarget - driftPos) * (1.0 / (0.8 * sr));

            double sizeMs = juce::jlimit (10.0, 500.0, (double) p.sizeMs) * (1.0 + 0.5 * lfo);
            const double sizeS = juce::jlimit (0.004 * sr, 0.6 * sr, sizeMs * 0.001 * sr);
            nextOnset -= 1.0;
            if (nextOnset <= 0.0)
            {
                if (spawn && s != nullptr)
                {
                    const double centre = playhead + i * speed
                                        + driftPos * p.drift * 1.0 * sr
                                        + lfo * 0.5 * sr;
                    spawnGrain (s, len, centre, sizeS, pitchRatio, p);
                }
                const double interval = sizeS / density;
                const double jit = 1.0 + (uni (rng) * 2.0 - 1.0) * 0.9 * flux;
                nextOnset += juce::jmax (1.0, interval * jit);
            }
            float l = 0.0f, r = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                any = true;
                const double ph = g.pos / g.len;
                const float w = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * ph));
                const double rp = g.readPos;
                float smp = 0.0f;
                if (s != nullptr && rp >= 0.0 && rp < len - 1)
                {
                    const int i0 = (int) rp;
                    const float fr = (float) (rp - i0);
                    smp = s[i0] + fr * (s[i0 + 1] - s[i0]);
                }
                smp *= w * g.gain;
                l += smp * g.gl;
                r += smp * g.gr;
                g.readPos += g.rate;
                g.pos += 1.0;
                if (g.pos >= g.len) g.active = false;
            }
            outL[i] = l;
            outR[i] = r;
        }
        busy = any;
        lastLen = len;
    }

    int snapshot (GrainVis* out, int maxCount) const
    {
        const juce::SpinLock::ScopedTryLockType sl (visLock);
        if (! sl.isLocked()) return -1;
        const int n = juce::jmin (maxCount, visCount);
        for (int i = 0; i < n; ++i) out[i] = vis[(size_t) i];
        return n;
    }

    void updateVis()
    {
        const juce::SpinLock::ScopedTryLockType sl (visLock);
        if (! sl.isLocked()) return;
        visCount = 0;
        for (auto& g : grains)
        {
            if (! g.active || visCount >= maxGrains) continue;
            auto& v = vis[(size_t) visCount++];
            v.pos = lastLen > 0 ? (float) (g.readPos / lastLen) : 0.0f;
            v.pitch = g.semis;
            v.size = (float) (g.len / (0.5 * sr));
            const double ph = g.pos / g.len;
            v.amp = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * ph));
            v.pan = g.pan;
            v.reverse = g.rate < 0;
        }
    }

private:
    struct Grain
    {
        bool active = false;
        double readPos = 0, rate = 1, pos = 0, len = 1;
        float gain = 1, gl = 1, gr = 1, semis = 0, pan = 0;
    };

    void spawnGrain (const float*, int len, double centre, double sizeS, double pitchRatio, const GrainParams& p)
    {
        Grain* g = nullptr;
        for (auto& c : grains) if (! c.active) { g = &c; break; }
        if (g == nullptr) return;
        const double flux = juce::jlimit (0.0, 1.0, (double) p.flux);
        const double glen = juce::jlimit (0.004 * sr, 0.6 * sr, sizeS * (1.0 + (uni (rng) * 2.0 - 1.0) * 0.6 * flux));
        float semis = (float) ((uni (rng) * 2.0 - 1.0) * p.pitchSpread);
        if (uni (rng) < p.octave) semis += uni (rng) < 0.7 ? 12.0f : -12.0f;
        semis += (float) ((uni (rng) * 2.0 - 1.0) * 0.3 * flux);
        const double rate = std::pow (2.0, semis / 12.0) * juce::jlimit (0.125, 8.0, pitchRatio);
        const bool rev = uni (rng) < p.reverse;
        // Position: Spray streut um die Abspielposition, Flux legt noch etwas Zufall drauf
        const double spray = juce::jlimit (0.0, 1000.0, (double) p.sprayMs) * 0.001 * sr;
        double pos = centre + (uni (rng) * 2.0 - 1.0) * spray + (uni (rng) * 2.0 - 1.0) * flux * 0.5 * glen;
        // Korn so legen, dass seine Mitte bei pos liegt
        const double span = glen * rate;
        double start = rev ? pos + span * 0.5 : pos - span * 0.5;
        start = juce::jlimit (0.0, juce::jmax (0.0, (double) len - 2.0), start);
        g->active = true;
        g->readPos = start;
        g->rate = rev ? -rate : rate;
        g->pos = 0;
        g->len = glen;
        g->semis = semis;
        g->gain = (float) (1.6 / std::sqrt (juce::jmax (1.0, (double) p.density)));
        const float pan = (float) ((uni (rng) * 2.0 - 1.0) * p.width);
        g->pan = pan;
        const float a = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        g->gl = std::cos (a) * 1.41421356f;
        g->gr = std::sin (a) * 1.41421356f;
    }

    int sr = 24000;
    std::array<Grain, maxGrains> grains;
    double nextOnset = 0, driftPos = 0, driftTarget = 0, lfoPhase = 0;
    int driftCountdown = 0;
    int lastLen = 0;
    bool busy = false;
    std::minstd_rand rng { 777 };
    std::uniform_real_distribution<double> uni { 0.0, 1.0 };
    mutable juce::SpinLock visLock;
    std::array<GrainVis, maxGrains> vis;
    int visCount = 0;
};
} // namespace tts::dsp
