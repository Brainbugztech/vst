#pragma once

// Braincut Engine: Slicer (+ Stretcher) -> Repeater -> Glitcher, danach Dry/Wet und Ausgangspegel.
// Läuft nur, solange der Host spielt. Steht der Transport, wird das Signal unverändert durchgereicht.

#include <atomic>
#include <cmath>
#include "Params.h"
#include "Slicer.h"
#include "Repeater.h"
#include "Glitcher.h"

namespace bc
{

class BraincutEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        slicer.prepare (sr);
        repeater.prepare (sr);
        glitcher.prepare (sr);
        smoothK = 1.0f - std::exp (-1.0f / (float) (0.01 * sr));
        rampStep = 1.0f / (float) std::max (32.0, 0.004 * sr);
        reset();
    }

    void reset()
    {
        slicer.reset();
        repeater.reset();
        glitcher.reset();
        wasPlaying = false;
        wetRamp = 0.0f;
        mixS = -1.0f;
    }

    void process (float* L, float* R, int n, const Transport& tr, const EngineParams& p)
    {
        const float outTarget = std::pow (10.0f, p.outDb / 20.0f);
        if (mixS < 0.0f) { mixS = p.mix; outS = outTarget; }

        if (! tr.playing)
        {
            // Transport steht: unverändert durch (nur Ausgangspegel)
            wasPlaying = false;
            for (int i = 0; i < n; ++i)
            {
                outS += (outTarget - outS) * smoothK;
                L[i] *= outS;
                R[i] *= outS;
            }
            vizRepeat.store (false, std::memory_order_relaxed);
            vizGlitch.store (false, std::memory_order_relaxed);
            return;
        }

        if (! wasPlaying)
        {
            slicer.discontinuity();
            repeater.reset();
            glitcher.reset();
            wetRamp = 0.0f;
            wasPlaying = true;
        }

        const bool sliceOk = slicer.beginBlock (tr, p.slicer, p.stretch);
        const double spb = 60.0 * sr / std::min (400.0, std::max (20.0, tr.bpm));
        const double beatsPerSample = 1.0 / spb;
        const uint64_t seedKey = hashCombine ((uint64_t) p.slicer.seed + 0x5EEDull, 0x77ull);

        for (int i = 0; i < n; ++i)
        {
            const float dl = L[i], dr = R[i];
            const double beat = tr.ppq + (double) i * beatsPerSample;

            float wl = dl, wr = dr;
            if (sliceOk)
                slicer.process (dl, dr, beat, wl, wr);

            repeater.process (wl, wr, beat, spb, p.repeat, seedKey);
            glitcher.process (wl, wr, beat, spb, p.glitch, seedKey);

            mixS += (p.mix - mixS) * smoothK;
            outS += (outTarget - outS) * smoothK;
            wetRamp = std::min (1.0f, wetRamp + rampStep);
            const float m = mixS * wetRamp;

            L[i] = (dl + (wl - dl) * m) * outS;
            R[i] = (dr + (wr - dr) * m) * outS;
        }

        vizPos.store (sliceOk ? slicer.vizStepPos : 0.0f, std::memory_order_relaxed);
        vizRepeat.store (repeater.isRepeating(), std::memory_order_relaxed);
        vizGlitch.store (glitcher.isGlitching(), std::memory_order_relaxed);
    }

    Slicer slicer;

    // Anzeige (Audio-Thread schreibt, Oberfläche liest)
    std::atomic<float> vizPos { 0.0f };
    std::atomic<bool>  vizRepeat { false }, vizGlitch { false };

private:
    double sr = 48000.0;
    Repeater repeater;
    Glitcher glitcher;
    bool wasPlaying = false;
    float wetRamp = 0.0f, mixS = -1.0f, outS = 1.0f, smoothK = 0.002f, rampStep = 0.005f;
};

} // namespace bc
