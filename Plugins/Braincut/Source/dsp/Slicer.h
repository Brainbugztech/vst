#pragma once

// Slicer + Stretcher
//
// Der Slicer nimmt das eingehende Audio laufend in einen Loop-Puffer auf (Position im Puffer =
// Position im Loop, vom Host-Takt abgeleitet) und spielt es nach dem Schnitt-Muster wieder ab.
// Stellen, die noch nie aufgenommen wurden (erster Durchlauf), liefern das trockene Signal.
//
// Der Stretcher streckt/staucht einzelne Einheiten zufällig, ohne die Tonhöhe zu ändern: zwei
// überlappende Hann-Fenster (Granular-Verfahren) lesen die Quelle mit 1x Geschwindigkeit, ihre
// Startpunkte wandern aber mit dem Faktor v durch die Quelle.

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include "Params.h"
#include "Rng.h"
#include "Pattern.h"

namespace bc
{

class Slicer
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        capacity = (int) (sr * 30.0);
        buf[0].assign ((size_t) capacity + 4, 0.0f);
        buf[1].assign ((size_t) capacity + 4, 0.0f);
        filled.assign ((size_t) (capacity >> kShift) + 2, 0);
        grainLen = std::max (64, (int) std::lround (0.030 * sr)) & ~1;
        hop = grainLen / 2;
        xfade = std::max (16, (int) std::lround (0.0025 * sr));
        reset();
    }

    void reset()
    {
        curSlot = std::numeric_limits<long long>::min();
        std::fill (filled.begin(), filled.end(), (uint8_t) 0);
        discontinuity();
        patternSig = ~0ull;
        curPhrase = std::numeric_limits<long long>::min();
    }

    // Transport wurde neu gestartet oder gesprungen
    void discontinuity()
    {
        haveCur = havePrev = haveW = false;
        sinceSwitch = 1 << 30;
    }

    // Vor jedem Block aufrufen. Liefert false, wenn der Slicer nicht arbeiten kann
    // (z. B. Phrase länger als der Puffer): dann wird das Signal durchgereicht.
    bool beginBlock (const Transport& tr, const SlicerParams& sp, const StretchParams& st)
    {
        stretch = st;
        slicer = sp;

        const double bpm = std::min (400.0, std::max (20.0, tr.bpm));
        spb = 60.0 * sr / bpm;
        beatsPerSample = bpm / (60.0 * sr);

        bpb = std::min (16.0, std::max (1.0, tr.beatsPerBar));
        sb = kSubdivSteps[std::min (3, std::max (0, sp.subdivIdx))];

        // Loop = aufgenommene Quelle
        const int loopBars = kPhraseBars[std::min (2, std::max (0, sp.loopIdx))];
        M = std::max (1, (int) std::lround ((double) loopBars * bpb * sb));
        loopBeats = (double) M / sb;
        stepBeats = 1.0 / sb;
        stepSamples = spb / sb;
        loopLen = (int) (loopBeats * spb);

        // Phrasen: Länge je Phrase zufällig zwischen Min und Max Takten
        pMin = std::min (8, std::max (1, std::min (sp.phraseMin, sp.phraseMax)));
        pMax = std::min (8, std::max (1, std::max (sp.phraseMin, sp.phraseMax)));
        maxPhraseBeats = std::floor ((double) Pattern::kMax / sb);
        slotBeats = (double) pMax * bpb;

        active = loopLen >= 64 && loopLen <= capacity;
        if (! active)
            return false;

        // Tempo oder Länge geändert: bisher Aufgenommenes passt nicht mehr zum Raster
        if (std::abs (spb - lastSpb) > lastSpb * 0.002 || loopLen != lastLen)
        {
            std::fill (filled.begin(), filled.end(), (uint8_t) 0);
            lastSpb = spb;
            lastLen = loopLen;
        }

        const uint64_t sig = hashCombine (hashCombine ((uint64_t) sp.mode + 1, (uint64_t) M * 16 + (uint64_t) sb),
                                          hashCombine ((uint64_t) std::lround (sp.repeatProb * 100.0f),
                                                       hashCombine ((uint64_t) sp.seed * 8 + (sp.lock ? 7 : 3),
                                                                    hashCombine ((uint64_t) pMin * 64 + (uint64_t) pMax,
                                                                                 (uint64_t) sp.repMin * 64 + (uint64_t) sp.repMax + (uint64_t) std::lround (bpb * 4.0) * 4096))));
        if (sig != patternSig)
        {
            patternSig = sig;
            curSlot = curPhrase = std::numeric_limits<long long>::min(); // neu erzeugen
        }
        return true;
    }

    // Ein Sample. "beat" ist die Position in Beats. Liefert das zerschnittene Signal.
    void process (float inL, float inR, double beat, float& outL, float& outR)
    {
        if (! active)
        {
            outL = inL;
            outR = inR;
            return;
        }

        // Winziger Versatz (1e-7 Beat, ca. 0,005 Samples), damit Rundungsrauschen der Host-Position
        // an Raster-Grenzen nicht von Phrase zu Phrase um ein Sample hin- und herspringt
        beat += 1e-7;
        // Position im Loop (für Aufnahme und Lesen)
        const double lIdxD = std::floor (beat / loopBeats);
        const long long pIdx = (long long) lIdxD;
        double pp = beat - lIdxD * loopBeats;
        pp = std::min (std::max (pp, 0.0), loopBeats - 1e-9);

        // Phrase bestimmen
        const long long slot = (long long) std::floor (beat / slotBeats);
        if (slot != curSlot)
            buildSlot (slot);
        const double inSlot = beat - (double) slot * slotBeats;
        int ord = slotCount - 1;
        for (int k = 0; k < slotCount; ++k)
            if (inSlot < slotStart[(size_t) k] + slotLen[(size_t) k]) { ord = k; break; }
        const long long phraseId = slot * 64 + ord;
        if (phraseId != curPhrase)
        {
            curPhrase = phraseId;
            regenerate (slot, ord);
        }
        const double inPhrase = std::min (std::max (inSlot - slotStart[(size_t) ord], 0.0), slotLen[(size_t) ord] - 1e-9);
        const double stepF = inPhrase * sb;
        const int step = std::min (N - 1, (int) stepF);
        const double frac = std::min (1.0, stepF - (double) step);

        // Aufnehmen. Bei gleichmäßigem Tempo wird einfach Sample für Sample weitergezählt, damit
        // Rundungsfehler der Host-Position keine Doppel- oder Lücken-Samples in den Puffer schreiben.
        const double xw = pp * spb;
        int w;
        if (haveW && pIdx == lastWPhrase && std::abs (xw - lastXw - 1.0) < 0.02 && lastW + 1 < loopLen)
            w = lastW + 1;
        else
            w = std::min (loopLen - 1, (int) (xw + 0.37));
        haveW = true;
        lastW = w;
        lastXw = xw;
        lastWPhrase = pIdx;
        // Bruchteil-Versatz zwischen Host-Position und Schreib-Index: damit liest ein unverändertes
        // Stück genau das gerade geschriebene Sample (Interpolation braucht keine Zukunft)
        const double delta = xw - (double) w;

        buf[0][(size_t) w] = inL;
        buf[1][(size_t) w] = inR;
        filled[(size_t) (w >> kShift)] = 1;

        // Einheit wechseln?
        const int ui = pattern.unitOfStep[(size_t) step];
        if (! haveCur || curUi != ui || curSerial != serial || curPhraseOfUnit != phraseId)
        {
            if (haveCur)
            {
                prev = cur;
                prevT0 = lastT + 1.0;
                havePrev = true;
                sinceSwitch = 0;
            }
            cur = resolve (pattern.units[(size_t) ui]);
            curUi = ui;
            curSerial = serial;
            curPhraseOfUnit = phraseId;
            haveCur = true;
        }

        const Unit& u = pattern.units[(size_t) ui];
        const double t = ((double) (step - u.start) + frac) * stepSamples;
        lastT = t;

        float l, r;
        unitOut (cur, t, delta, inL, inR, l, r);

        if (havePrev && sinceSwitch < xfade)
        {
            float ol, orr;
            unitOut (prev, prevT0 + (double) sinceSwitch, delta, inL, inR, ol, orr);
            const float x = (float) sinceSwitch / (float) xfade;
            const float a = 0.5f - 0.5f * std::cos (3.14159265f * x);
            l = l * a + ol * (1.0f - a);
            r = r * a + orr * (1.0f - a);
        }
        if (sinceSwitch < (1 << 30)) ++sinceSwitch;

        outL = l;
        outR = r;

        vizStepPos = (float) stepF;
    }

    // --- Anzeige-Daten (vom Audio-Thread geschrieben, von der Oberfläche gelesen) ---
    std::atomic<int>  vizSteps { 0 };
    std::atomic<int>  vizLoop { 16 };
    std::atomic<float> vizPhraseBeats { 4.0f };
    std::atomic<int>  vizSerial { 0 };
    std::array<std::atomic<int16_t>, Pattern::kMax> vizSrc {};
    std::array<std::atomic<uint8_t>, Pattern::kMax> vizMoved {};
    std::array<std::atomic<uint8_t>, Pattern::kMax> vizRev {};
    float vizStepPos = 0.0f;

    int steps() const { return N; }
    bool isActive() const { return active; }

private:
    static constexpr int kShift = 8; // Puffer-Abschnitte à 256 Samples für das "schon aufgenommen"-Flag

    struct Resolved
    {
        double srcStart = 0.0;   // Samples in der Phrase
        double len = 0.0;        // Länge der Einheit in Samples
        double v = 1.0;          // Lese-Geschwindigkeit der Quelle (Streckfaktor)
        bool   rev = false;
        bool   stretched = false;
    };

    Resolved resolve (const Unit& u) const
    {
        Resolved r;
        r.len = (double) u.len * stepSamples;
        r.rev = u.rev;
        r.srcStart = (double) u.src * stepSamples + (u.rev ? r.len : 0.0);
        const bool sel = stretch.on && u.vsel < 0.25f + 0.6f * stretch.amount;
        r.v = sel ? std::exp ((double) u.vr * std::log (1.0 + (double) stretch.amount)) : 1.0;
        r.stretched = sel && std::abs (r.v - 1.0) > 0.02;
        return r;
    }

    // Phrasen-Liste eines "Slots" (= Max-Phrase Takte) würfeln. Hängt nur von Seed und Slot ab.
    void buildSlot (long long slot)
    {
        curSlot = slot;
        Rng r (hashCombine ((uint64_t) slicer.seed + 0x7701ull, slicer.lock ? 0ull : (uint64_t) slot));
        double remaining = slotBeats, pos = 0.0;
        slotCount = 0;
        while (remaining > 1e-9 && slotCount < kSlotMax)
        {
            const int bars = r.range (pMin, pMax);
            double len = std::min (std::min ((double) bars * bpb, maxPhraseBeats), remaining);
            if (slotCount == kSlotMax - 1) len = remaining;
            slotStart[(size_t) slotCount] = pos;
            slotLen[(size_t) slotCount] = len;
            ++slotCount;
            pos += len;
            remaining -= len;
        }
        if (slotCount == 0) { slotStart[0] = 0.0; slotLen[0] = slotBeats; slotCount = 1; }
    }

    void regenerate (long long slot, int ord)
    {
        const double len = slotLen[(size_t) ord];
        N = std::min (Pattern::kMax, std::max (1, (int) std::lround (len * sb)));
        const double startAbs = (double) slot * slotBeats + slotStart[(size_t) ord];
        const long long absStep = (long long) std::llround (startAbs * sb);

        GenParams g;
        g.N = N;
        g.M = M;
        g.base = (int) (((absStep % M) + M) % M);
        g.sb = sb;
        g.rp = std::lround (slicer.repeatProb * 100.0f) / 100.0f;
        g.repMin = slicer.repMin;
        g.repMax = slicer.repMax;
        const uint64_t key = hashCombine ((uint64_t) slicer.seed + 0x1234ull,
                                          hashCombine ((uint64_t) ord, slicer.lock ? 0ull : (uint64_t) slot));
        if (slicer.mode == 0) generateCut (pattern, g, key);
        else                  generateWarp (pattern, g, key);
        ++serial;

        for (int i = 0; i < N; ++i)
        {
            vizSrc[(size_t) i].store (pattern.srcOfStep[(size_t) i], std::memory_order_relaxed);
            vizMoved[(size_t) i].store (pattern.moved[(size_t) i], std::memory_order_relaxed);
            vizRev[(size_t) i].store (pattern.revOfStep[(size_t) i], std::memory_order_relaxed);
        }
        vizSteps.store (N, std::memory_order_relaxed);
        vizLoop.store (M, std::memory_order_relaxed);
        vizPhraseBeats.store ((float) len, std::memory_order_relaxed);
        vizSerial.store (serial, std::memory_order_relaxed);
    }

    void readBuf (double pos, float dryL, float dryR, float& l, float& r) const
    {
        double p = std::fmod (pos, (double) loopLen);
        if (p < 0.0) p += (double) loopLen;
        int i0 = (int) p;
        if (i0 >= loopLen) i0 = loopLen - 1;
        if (! filled[(size_t) (i0 >> kShift)])
        {
            l = dryL;
            r = dryR;
            return;
        }
        const float f = (float) (p - (double) i0);
        int i1 = i0 + 1;
        if (i1 >= loopLen) i1 = 0;
        const float* bl = buf[0].data();
        const float* br = buf[1].data();
        l = bl[i0] + (bl[i1] - bl[i0]) * f;
        r = br[i0] + (br[i1] - br[i0]) * f;
    }

    void unitOut (const Resolved& u, double t, double delta, float dryL, float dryR, float& l, float& r) const
    {
        const double dir = u.rev ? -1.0 : 1.0;
        const double base = u.srcStart - delta;
        readBuf (base + dir * t, dryL, dryR, l, r);

        if (! u.stretched || stretch.mix <= 0.0f)
            return;

        // Granular strecken: zwei überlappende Fenster, Lesetempo 1x, Startpunkte wandern mit v
        const double m = std::floor (t / (double) hop);
        float accL = 0.0f, accR = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            const double mm = m - (double) k;
            const double tau = t - mm * (double) hop;
            const float win = 0.5f - 0.5f * std::cos (6.28318531f * (float) (tau / (double) grainLen));
            float gl, gr;
            readBuf (base + dir * (mm * (double) hop * u.v + tau), dryL, dryR, gl, gr);
            accL += win * gl;
            accR += win * gr;
        }
        l += (accL - l) * stretch.mix;
        r += (accR - r) * stretch.mix;
    }

    double sr = 48000.0;
    int capacity = 0, grainLen = 1440, hop = 720, xfade = 120;
    std::vector<float> buf[2];
    std::vector<uint8_t> filled;

    // Geometrie des aktuellen Blocks
    double spb = 24000.0, beatsPerSample = 0.0, bpb = 4.0, loopBeats = 4.0, slotBeats = 16.0, maxPhraseBeats = 128.0,
           stepBeats = 0.25, stepSamples = 6000.0;
    int sb = 4, N = 16, M = 16, loopLen = 0, pMin = 1, pMax = 4;

    static constexpr int kSlotMax = 64;
    std::array<double, kSlotMax> slotStart {}, slotLen {};
    int slotCount = 1;
    long long curSlot = std::numeric_limits<long long>::min();
    bool active = false;
    double lastSpb = 0.0;
    int lastLen = 0;

    SlicerParams slicer;
    StretchParams stretch;

    Pattern pattern;
    int serial = 0;
    uint64_t patternSig = ~0ull;
    long long curPhrase = std::numeric_limits<long long>::min();

    Resolved cur, prev;
    bool haveCur = false, havePrev = false;
    int curUi = 0, curSerial = -1;
    long long curPhraseOfUnit = 0;
    double lastT = 0.0, prevT0 = 0.0;
    bool haveW = false;
    int lastW = 0;
    double lastXw = 0.0;
    long long lastWPhrase = 0;
    int sinceSwitch = 1 << 30;
};

} // namespace bc
