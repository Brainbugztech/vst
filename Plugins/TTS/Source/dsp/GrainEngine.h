#pragma once

// Grain Engine (24 kHz, Mono rein, Stereo raus), angelehnt an Abletons Warp-Modus "Texture":
// die Stimme läuft in einen Verlauf-Puffer, Körner werden daraus gelesen.
//  Size   = Korngröße
//  Flux   = Zufall (Größe, Zeitpunkt, Position, etwas Tonhöhe)
//  Density= wie viele Körner sich überlappen
//  Spray  = wie weit zurück in die Vergangenheit gestreut wird
//  Pitch  = Tonhöhen-Streuung in Halbtönen, Width = Stereo-Breite, Reverse = Anteil rückwärts
//  Freeze = Verlauf einfrieren (Körner spielen weiter auf dem eingefrorenen Stück)

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
    float width = 0.5f;     // 0..1
    float reverse = 0.0f;   // 0..1
    bool freeze = false;
};

// für die Anzeige
struct GrainVis
{
    float age = 0;     // 0..1 (Position im Verlauf, 0 = jetzt)
    float pitch = 0;   // Halbtöne
    float size = 0;    // 0..1
    float amp = 0;     // Hüllkurve 0..1
    float pan = 0;     // -1..1
    bool reverse = false;
};

class GrainEngine
{
public:
    static constexpr int maxGrains = 96;
    static constexpr int historySeconds = 4;

    void prepare (int sampleRate)
    {
        sr = sampleRate;
        hist.assign ((size_t) (sr * historySeconds), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (hist.begin(), hist.end(), 0.0f);
        write = 0;
        recorded = 0;
        for (auto& g : grains) g.active = false;
        nextOnset = 0.0;
        silentFor = 0;
    }

    bool isBusy() const { return busy; }

    // in: Stimme (Mono), outL/outR: Körner (werden überschrieben)
    void process (const float* in, float* outL, float* outR, int num, const GrainParams& p)
    {
        const int H = (int) hist.size();
        const double sizeS = juce::jlimit (10.0, 500.0, (double) p.sizeMs) * 0.001 * sr;
        const double density = juce::jlimit (1.0, 16.0, (double) p.density);
        const double interval = sizeS / density;
        bool any = false;
        for (int i = 0; i < num; ++i)
        {
            // Verlauf aufnehmen
            if (! p.freeze)
            {
                hist[(size_t) write] = in[i];
                write = (write + 1) % H;
                recorded = juce::jmin ((juce::int64) H, recorded + 1);
                if (std::abs (in[i]) > 1e-5f) silentFor = 0;
                else ++silentFor;
            }
            // neue Körner starten
            nextOnset -= 1.0;
            if (nextOnset <= 0.0)
            {
                // ohne Material (Stille seit > Verlauf) keine Körner
                if (recorded > (juce::int64) sizeS && (p.freeze || silentFor < (juce::int64) H))
                    spawn (p, sizeS);
                const double jit = 1.0 + (uni (rng) * 2.0 - 1.0) * 0.9 * p.flux;
                nextOnset += juce::jmax (1.0, interval * jit);
            }
            float l = 0.0f, r = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                any = true;
                const double ph = g.pos / g.len;           // 0..1
                const float w = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * ph));
                double rp = g.readPos;
                while (rp < 0) rp += H;
                while (rp >= H) rp -= H;
                const int i0 = (int) rp;
                const int i1 = (i0 + 1) % H;
                const float fr = (float) (rp - i0);
                const float s = (hist[(size_t) i0] + fr * (hist[(size_t) i1] - hist[(size_t) i0])) * w * g.gain;
                l += s * g.gl;
                r += s * g.gr;
                g.readPos += g.rate;
                g.pos += 1.0;
                if (g.pos >= g.len) g.active = false;
            }
            outL[i] = l;
            outR[i] = r;
        }
        busy = any;
    }

    // Kopie der aktiven Körner für die Anzeige (aus dem Audio-Thread geschrieben)
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
        const int H = (int) hist.size();
        visCount = 0;
        for (auto& g : grains)
        {
            if (! g.active || visCount >= maxGrains) continue;
            auto& v = vis[(size_t) visCount++];
            double age = (double) write - g.readPos;
            while (age < 0) age += H;
            while (age >= H) age -= H;
            v.age = (float) (age / (sr * 2.0));
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

    void spawn (const GrainParams& p, double sizeS)
    {
        Grain* g = nullptr;
        for (auto& c : grains) if (! c.active) { g = &c; break; }
        if (g == nullptr) return;
        const int H = (int) hist.size();
        const double flux = juce::jlimit (0.0, 1.0, (double) p.flux);
        double len = sizeS * (1.0 + (uni (rng) * 2.0 - 1.0) * 0.6 * flux);
        len = juce::jlimit (0.004 * sr, 0.6 * sr, len);
        float semis = (float) ((uni (rng) * 2.0 - 1.0) * p.pitchSpread);
        semis += (float) ((uni (rng) * 2.0 - 1.0) * 0.35 * flux); // Flux: leichtes Schwimmen
        const double rate = std::pow (2.0, semis / 12.0);
        const bool rev = uni (rng) < p.reverse;
        // wie weit zurück: Spray + Zufall durch Flux, mindestens so weit, dass das Korn komplett in der Vergangenheit liegt
        const double spray = juce::jlimit (0.0, 1000.0, (double) p.sprayMs) * 0.001 * sr;
        double back = uni (rng) * spray + uni (rng) * flux * 0.5 * len;
        const double need = rev ? 2.0 : len * rate + 2.0;
        const double avail = (double) juce::jmin ((juce::int64) H - 2, recorded) - (rev ? len * rate : 0.0);
        back = juce::jlimit (need, juce::jmax (need, avail), back + need);
        double start = (double) write - back;
        g->active = true;
        g->readPos = start;
        g->rate = rev ? -rate : rate;
        g->pos = 0;
        g->len = len;
        g->semis = semis;
        // Lautstärke: bei vielen überlappenden Körnern zurücknehmen
        g->gain = (float) (1.0 / std::sqrt (juce::jmax (1.0, (double) p.density * 0.5)));
        const float pan = (float) ((uni (rng) * 2.0 - 1.0) * p.width);
        g->pan = pan;
        const float a = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        g->gl = std::cos (a) * 1.41421356f;
        g->gr = std::sin (a) * 1.41421356f;
    }

    int sr = 24000;
    std::vector<float> hist;
    int write = 0;
    juce::int64 recorded = 0, silentFor = 0;
    std::array<Grain, maxGrains> grains;
    double nextOnset = 0;
    bool busy = false;
    std::minstd_rand rng { 777 };
    std::uniform_real_distribution<double> uni { 0.0, 1.0 };
    mutable juce::SpinLock visLock;
    std::array<GrainVis, maxGrains> vis;
    int visCount = 0;
};
} // namespace tts::dsp
