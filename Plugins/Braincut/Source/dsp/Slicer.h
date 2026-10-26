#pragma once

// Slicer + Stretcher
//
// Der Slicer nimmt das eingehende Audio laufend in einen Phrasen-Puffer auf (Position im Puffer =
// Position in der Phrase, vom Host-Takt abgeleitet) und spielt es nach dem Schnitt-Muster wieder ab.
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

        const int bars = kPhraseBars[std::min (2, std::max (0, sp.phraseIdx))];
        phraseBeats = std::min (64.0, (double) bars * std::max (1.0, tr.beatsPerBar));
        sb = kSubdivSteps[std::min (3, std::max (0, sp.subdivIdx))];
        N = std::min (Pattern::kMax, std::max (1, (int) std::lround (phraseBeats * sb)));
        stepBeats = phraseBeats / N;
        stepSamples = stepBeats * spb;
        phraseLen = (int) (phraseBeats * spb);

        active = phraseLen >= 64 && phraseLen <= capacity;
        if (! active)
            return false;

        // Tempo oder Länge geändert: bisher Aufgenommenes passt nicht mehr zum Raster
        if (std::abs (spb - lastSpb) > lastSpb * 0.002 || phraseLen != lastLen)
        {
            std::fill (filled.begin(), filled.end(), (uint8_t) 0);
            lastSpb = spb;
            lastLen = phraseLen;
        }

        const uint64_t sig = hashCombine (hashCombine ((uint64_t) sp.mode + 1, (uint64_t) N * 16 + (uint64_t) sb),
                                          hashCombine ((uint64_t) std::lround (sp.repeatProb * 100.0f),
                                                       hashCombine ((uint64_t) sp.seed, sp.lock ? 7 : 3)));
        if (sig != patternSig)
        {
            patternSig = sig;
            curPhrase = std::numeric_limits<long long>::min(); // neu erzeugen
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
        const double pIdxD = std::floor (beat / phraseBeats);
        const long long pIdx = (long long) pIdxD;
        double pp = beat - pIdxD * phraseBeats;
        pp = std::min (std::max (pp, 0.0), phraseBeats - 1e-9);

        const double stepF = pp / stepBeats;
        const int step = std::min (N - 1, (int) stepF);
        const double frac = stepF - (double) step;

        if (pIdx != curPhrase)
        {
            curPhrase = pIdx;
            regenerate (pIdx);
        }

        // Aufnehmen. Bei gleichmäßigem Tempo wird einfach Sample für Sample weitergezählt, damit
        // Rundungsfehler der Host-Position keine Doppel- oder Lücken-Samples in den Puffer schreiben.
        const double xw = pp * spb;
        int w;
        if (haveW && pIdx == lastWPhrase && std::abs (xw - lastXw - 1.0) < 0.02 && lastW + 1 < phraseLen)
            w = lastW + 1;
        else
            w = std::min (phraseLen - 1, (int) (xw + 0.37));
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
        if (! haveCur || curUi != ui || curSerial != serial || curPhraseOfUnit != pIdx)
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
            curPhraseOfUnit = pIdx;
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
    std::atomic<int>  vizSerial { 0 };
    std::array<std::atomic<int16_t>, Pattern::kMax> vizSrc {};
    std::array<std::atomic<uint8_t>, Pattern::kMax> vizMoved {};
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

    void regenerate (long long pIdx)
    {
        const uint64_t key = hashCombine ((uint64_t) slicer.seed + 0x1234ull, slicer.lock ? 0ull : (uint64_t) pIdx);
        const float rp = std::lround (slicer.repeatProb * 100.0f) / 100.0f;
        if (slicer.mode == 0) generateCut (pattern, N, sb, rp, key);
        else                  generateWarp (pattern, N, sb, rp, key);
        ++serial;

        for (int i = 0; i < N; ++i)
        {
            vizSrc[(size_t) i].store (pattern.srcOfStep[(size_t) i], std::memory_order_relaxed);
            vizMoved[(size_t) i].store (pattern.moved[(size_t) i], std::memory_order_relaxed);
        }
        vizSteps.store (N, std::memory_order_relaxed);
        vizSerial.store (serial, std::memory_order_relaxed);
    }

    void readBuf (double pos, float dryL, float dryR, float& l, float& r) const
    {
        double p = std::fmod (pos, (double) phraseLen);
        if (p < 0.0) p += (double) phraseLen;
        int i0 = (int) p;
        if (i0 >= phraseLen) i0 = phraseLen - 1;
        if (! filled[(size_t) (i0 >> kShift)])
        {
            l = dryL;
            r = dryR;
            return;
        }
        const float f = (float) (p - (double) i0);
        int i1 = i0 + 1;
        if (i1 >= phraseLen) i1 = 0;
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
    double spb = 24000.0, beatsPerSample = 0.0, phraseBeats = 4.0, stepBeats = 0.25, stepSamples = 6000.0;
    int sb = 4, N = 16, phraseLen = 0;
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
