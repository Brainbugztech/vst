#pragma once

// Braincut Engine: Slicer (+ Stretcher) -> Repeater -> Glitcher, danach Dry/Wet und Ausgangspegel.
// Läuft nur, solange der Host spielt. Steht der Transport, wird das Signal unverändert durchgereicht.
// Die Dichte (DECENT/MEDIUM/NUTS) legt fest, an welchen Stellen die Effekte arbeiten dürfen.
//
// Für die Anzeige zeichnet die Engine ein Raster aus 4 Takten auf: pro Schritt (Subdivision) ein Feld
// mit Quelle im Loop und Merkern (Zone, umgestellt, rückwärts, gestreckt, Repeat, Glitch).

#include <array>
#include <atomic>
#include <cmath>
#include "Params.h"
#include "Slicer.h"
#include "Repeater.h"
#include "Glitcher.h"
#include "Density.h"

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
        curCell = -1;
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
            vizPlaying.store (false, std::memory_order_relaxed);
            curCell = -1;
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

        const int dens = std::min ((int) kNuts, std::max ((int) kDecent, p.density));
        const bool sliceOk = slicer.beginBlock (tr, p.slicer, p.stretch, dens);
        const double spb = 60.0 * sr / std::min (400.0, std::max (20.0, tr.bpm));
        const double beatsPerSample = 1.0 / spb;
        const uint64_t seedKey = hashCombine ((uint64_t) p.slicer.seed + 0x5EEDull, 0x77ull);
        const double bpb = std::min (16.0, std::max (1.0, tr.beatsPerBar));
        const int sb = kSubdivSteps[std::min (3, std::max (0, p.slicer.subdivIdx))];
        const int cols = std::max (1, std::min (kVizCells / kVizRows, (int) std::lround (bpb * (double) sb)));
        const int winSteps = cols * kVizRows;

        for (int i = 0; i < n; ++i)
        {
            const float dl = L[i], dr = R[i];
            const double beat = tr.ppq + (double) i * beatsPerSample;
            const Zone zone = densityZone (dens, beat + 1e-7, bpb, p.slicer.seed, p.slicer.lock);

            float wl = dl, wr = dr;
            if (sliceOk)
                slicer.process (dl, dr, beat, wl, wr, zone.on);
            else
                slicer.info = Slicer::UnitInfo {};

            repeater.process (wl, wr, beat, spb, p.repeat, seedKey, zone, dens);
            glitcher.process (wl, wr, beat, spb, p.glitch, seedKey, zone, dens);
            recordCell (beat + 1e-7, sb, cols, winSteps, zone.on);

            mixS += (p.mix - mixS) * smoothK;
            outS += (outTarget - outS) * smoothK;
            wetRamp = std::min (1.0f, wetRamp + rampStep);
            const float m = mixS * wetRamp;

            L[i] = (dl + (wl - dl) * m) * outS;
            R[i] = (dr + (wr - dr) * m) * outS;
        }

        vizRepeat.store (repeater.isRepeating(), std::memory_order_relaxed);
        vizGlitch.store (glitcher.isGlitching(), std::memory_order_relaxed);
        vizPlaying.store (true, std::memory_order_relaxed);
        const double endBeat = tr.ppq + (double) n * beatsPerSample;
        vizCellPos.store ((float) std::fmod (std::max (0.0, endBeat * (double) sb), (double) winSteps), std::memory_order_relaxed);
        vizWindowStart.store (std::floor (endBeat / (bpb * (double) kVizRows)) * bpb * (double) kVizRows, std::memory_order_relaxed);
        vizCols.store (cols, std::memory_order_relaxed);
        vizSteps.store (sb, std::memory_order_relaxed);
        vizBpb.store ((float) bpb, std::memory_order_relaxed);
        vizLoopSteps.store (slicer.info.loopSteps, std::memory_order_relaxed);
    }

    Slicer slicer;

    // --- Anzeige (Audio-Thread schreibt, Oberfläche liest) ---
    static constexpr int kVizRows = 4, kVizCells = 512;
    enum CellFlags : uint8_t { cfZone = 1, cfMoved = 2, cfRev = 4, cfStretch = 8, cfFast = 16, cfRepeat = 32, cfGlitch = 64, cfPlayed = 128 };
    std::array<std::atomic<int16_t>, kVizCells> vizCellSrc {};
    std::array<std::atomic<uint8_t>, kVizCells> vizCellFlags {};
    std::atomic<float>  vizCellPos { 0.0f };      // Abspielposition im Raster (in Feldern)
    std::atomic<double> vizWindowStart { 0.0 };   // Beat, an dem das aktuelle 4-Takt-Raster beginnt
    std::atomic<int>    vizCols { 16 }, vizSteps { 4 }, vizLoopSteps { 16 };
    std::atomic<float>  vizBpb { 4.0f };
    std::atomic<bool>   vizRepeat { false }, vizGlitch { false }, vizPlaying { false };

private:
    // Feld des Rasters aktualisieren (beim Betreten neu setzen, danach Repeat/Glitch dazu-odern)
    void recordCell (double beat, int sb, int cols, int winSteps, bool zoneOn)
    {
        const long long abs = (long long) std::floor (beat * (double) sb);
        const int cell = (int) (((abs % winSteps) + winSteps) % winSteps);
        const auto& in = slicer.info;
        uint8_t f = cfPlayed;
        if (zoneOn)       f |= cfZone;
        if (in.moved)     f |= cfMoved;
        if (in.rev)       f |= cfRev;
        if (in.stretched) f |= in.fast ? (uint8_t) (cfStretch | cfFast) : (uint8_t) cfStretch;
        if (repeater.isRepeating()) f |= cfRepeat;
        if (glitcher.isGlitching()) f |= cfGlitch;
        (void) cols;

        if (cell != curCell)
        {
            curCell = cell;
            cellFlags = f;
            vizCellSrc[(size_t) cell].store ((int16_t) in.src, std::memory_order_relaxed);
            vizCellFlags[(size_t) cell].store (f, std::memory_order_relaxed);
        }
        else if ((cellFlags | f) != cellFlags)
        {
            cellFlags |= f;
            vizCellFlags[(size_t) cell].store (cellFlags, std::memory_order_relaxed);
        }
    }

    int curCell = -1;
    uint8_t cellFlags = 0;
    double sr = 48000.0;
    Repeater repeater;
    Glitcher glitcher;
    bool wasPlaying = false;
    float wetRamp = 0.0f, mixS = -1.0f, outS = 1.0f, smoothK = 0.002f, rampStep = 0.005f;
};

} // namespace bc
