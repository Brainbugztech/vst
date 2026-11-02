#pragma once

// Kick Transmuter: Transient-Designer für Kicks (ohne JUCE, damit tools/engine_test.cpp ihn direkt testen kann)
//
// Signalweg: Eingang -> Lookahead -> Attack/Sustain (nur Lautstärke) -> Punch -> Low End
//            -> 4x-Oversampling: Saturation + Peak-Schutz -> Mix -> Auto Gain -> Output
//
// Attack und Sustain reagieren auf die Steilheit der Hüllkurve (Differenz zweier Hüllkurven in dB).
// Dadurch gibt es weder Threshold noch Ratio, und die Regler greifen bei jeder Lautstärke gleich.
//
// Sauberkeit: Jede Stufe rechnet "Original + Änderung". Stehen alle Regler neutral, ist die Änderung
// exakt 0 und das Signal kommt bit-genau wieder raus (nur um die gemeldete Latenz verzögert).

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>
#include "dsp/Oversampler.h"

namespace ktm
{

struct Params
{
    float attack = 0.0f;   // -1 .. +1
    float sustain = 0.0f;  // -1 .. +1
    float punch = 0.0f;    //  0 .. 1
    float saturation = 0.0f; // 0 .. 1
    float lowEnd = 0.0f;   //  0 .. 1
    float mix = 1.0f;      //  0 .. 1
    float outDb = 0.0f;    // dB
    bool autoGain = false;
};

// Abstimmung an einer Stelle
namespace tune
{
    constexpr double lookaheadMs = 1.5;
    constexpr double holdMs = 20.0;        // überbrückt die Halbwellen bis 25 Hz, sonst flattert die Verstärkung im Bass

    constexpr double fastRiseMs = 0.2;
    constexpr double slowRiseMs = 10.0;    // Länge des Attack-Fensters
    constexpr double attackFallMs = 15.0;
    constexpr float  attackKneeDb = 8.0f;
    constexpr float  attackBoostDb = 15.0f;
    constexpr float  attackCutDb = 18.0f;

    constexpr double sustainFastMs = 8.0;
    constexpr double sustainSlowMs = 800.0;
    constexpr float  sustainBoostRatio = 0.85f; // verlangsamt das Abklingen
    constexpr float  sustainCutRatio = 2.5f;    // beschleunigt das Abklingen
    constexpr float  sustainBoostMaxDb = 18.0f;
    constexpr float  sustainCutMaxDb = 40.0f;
    constexpr float  sustainFadeStartDb = 50.0f; // so weit unter dem Schlag läuft die Anhebung aus
    constexpr float  sustainFadeRangeDb = 20.0f;

    constexpr float  floorDb = -80.0f;          // darunter gilt das Signal als Stille

    constexpr double gainSmoothMs = 0.4;

    constexpr double punchHz = 130.0;
    constexpr double punchQ = 0.75;
    constexpr double punchRiseMs = 35.0;
    constexpr float  punchKneeDb = 6.0f;
    constexpr float  punchMaxDb = 12.0f;

    constexpr double shelfHz = 75.0;
    constexpr float  shelfMaxDb = 10.0f;
    constexpr double monoHz = 120.0;
    constexpr double subsonicHz = 18.0;

    constexpr float  driveMax = 6.0f;
    constexpr double peakReleaseMs = 300.0;

    constexpr float  ceilingKnee = 1.41f;       // Peak-Schutz: weich ab +3 dB, Grenze +6 dB über Vollaussteuerung
    constexpr float  ceilingLimit = 2.0f;

    // Auto Gain
    constexpr float  hitOnsetDb = 3.0f;
    constexpr float  hitMinDb = -60.0f;
    constexpr float  hitEndDb = 36.0f;          // Fenster endet so weit unter der Spitze des Schlags
    constexpr float  hitBodyDb = 12.0f;         // innerhalb dieses Bereichs bewegt sich die Korrektur nie
    constexpr double hitMinMs = 10.0;
    constexpr double hitMaxMs = 1500.0;
    constexpr double hitRetriggerMs = 40.0;
    constexpr double hitBodyMinMs = 30.0;
    constexpr double hitBodyMaxMs = 250.0;      // lange Kicks ohne Pause: danach darf die Korrektur sich bewegen
    constexpr float  autoGainRangeDb = 12.0f;
    constexpr float  autoGainFollow = 0.75f;    // Anteil pro Schlag: nach zwei Kicks praktisch angeglichen
    constexpr double autoGainMoveMs = 25.0;
    constexpr double autoGainToggleMs = 120.0;

    constexpr double paramRampMs = 20.0;
}

inline double coefFromMs (double ms, double sr) { return 1.0 - std::exp (-1000.0 / (ms * sr)); }
inline float toDb (float v) { return 20.0f * std::log10 (std::max (v, 1.0e-5f)); }
inline float fromDb (float db) { return db == 0.0f ? 1.0f : std::pow (10.0f, db * 0.05f); }

// Reglerwert: läuft linear und landet exakt auf dem Ziel (sonst wäre "neutral" nie bit-genau)
struct Ramp
{
    float cur = 0.0f, target = 0.0f, step = 0.0f;
    int left = 0, len = 1;

    void setLength (int samples) { len = std::max (1, samples); }
    void jumpTo (float v) { cur = target = v; left = 0; }
    void setTarget (float v)
    {
        if (v == target) return;
        target = v;
        left = len;
        step = (target - cur) / (float) len;
    }
    inline float next()
    {
        if (left > 0)
        {
            cur += step;
            if (--left == 0) cur = target;
        }
        return cur;
    }
};

// Schiebefenster-Maximum in O(1); der Speicher wird in prepare() angelegt
class HoldPeak
{
public:
    void prepare (int windowSamples)
    {
        win = std::max (1, windowSamples);
        cap = win + 2;
        val.assign ((size_t) cap, 0.0f);
        pos.assign ((size_t) cap, 0);
        reset();
    }
    void reset() { head = tail = 0; now = 0; std::fill (val.begin(), val.end(), 0.0f); }
    inline float push (float x)
    {
        while (tail != head && val[(size_t) prev (tail)] <= x)
            tail = prev (tail);
        val[(size_t) tail] = x;
        pos[(size_t) tail] = now;
        tail = nxt (tail);
        if (now - pos[(size_t) head] >= win)
            head = nxt (head);
        ++now;
        if (now > (1 << 30)) // Zähler zurücksetzen, ohne das Fenster zu stören
        {
            for (auto& p : pos) p -= (1 << 29);
            now -= (1 << 29);
        }
        return val[(size_t) head];
    }
private:
    inline int nxt (int i) const { return i + 1 == cap ? 0 : i + 1; }
    inline int prev (int i) const { return i == 0 ? cap - 1 : i - 1; }
    std::vector<float> val;
    std::vector<int> pos;
    int win = 1, cap = 3, head = 0, tail = 0, now = 0;
};

// Zustandsvariablen-Filter (Trapez-Integration): bleibt stabil, auch wenn Verstärkung und Frequenz moduliert werden
struct Svf
{
    double a1 = 0, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0;
    double v1 = 0, v2 = 0; // Band- und Tiefpass des letzten Samples

    void set (double hz, double q, double sr)
    {
        const double g = std::tan (3.14159265358979323846 * std::min (hz, sr * 0.45) / sr);
        setG (g, 1.0 / q);
    }
    void setG (double g, double kk)
    {
        k = kk;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = v1 = v2 = 0.0; }
    inline void tick (double x)
    {
        const double v3 = x - ic2;
        v1 = a1 * ic1 + a2 * v3;
        v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
    }
    inline double highpass (double x) { tick (x); return x - k * v1 - v2; }
    inline double band (double x) { tick (x); return k * v1; }          // Spitze = 0 dB
};

class Engine
{
public:
    static constexpr int kOsLatency = bfz::Oversampler4x::kLatency;

    static int latencyFor (double sampleRate)
    {
        return (int) std::lround (tune::lookaheadMs * 0.001 * sampleRate) + kOsLatency;
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        laSamples = (int) std::lround (tune::lookaheadMs * 0.001 * sr);
        latency = laSamples + kOsLatency;
        hold.prepare (std::max (1, (int) std::lround (tune::holdMs * 0.001 * sr)));

        cFastRise = coefFromMs (tune::fastRiseMs, sr);
        cSlowRise = coefFromMs (tune::slowRiseMs, sr);
        cAttackFall = coefFromMs (tune::attackFallMs, sr);
        cSusFast = coefFromMs (tune::sustainFastMs, sr);
        cSusSlow = coefFromMs (tune::sustainSlowMs, sr);
        cGain = coefFromMs (tune::gainSmoothMs, sr);
        cPunchRise = coefFromMs (tune::punchRiseMs, sr);
        cPeakFall = coefFromMs (tune::peakReleaseMs, sr);
        cPeakSmooth = coefFromMs (0.5, sr);
        cAgMove = coefFromMs (tune::autoGainMoveMs, sr);
        cAgToggle = coefFromMs (tune::autoGainToggleMs / 5.0, sr);

        const int ramp = std::max (1, (int) std::lround (tune::paramRampMs * 0.001 * sr));
        for (auto* r : { &rAttack, &rSustain, &rPunch, &rSat, &rLow, &rMix, &rOut })
            r->setLength (ramp);

        // Verzögerungen: Lookahead (Nassweg), Lookahead + OS (trockener Weg), OS (Weg an der Sättigung vorbei)
        for (auto& d : laDelay) d.assign ((size_t) laSamples + 1, 0.0f);
        for (auto& d : osDelay) d.assign ((size_t) kOsLatency + 1, 0.0f);
        for (auto& d : dryDelay) d.assign ((size_t) kOsLatency + 1, 0.0f);

        shelfG = std::tan (3.14159265358979323846 * tune::shelfHz / sr);
        for (auto& f : punchF) f.set (tune::punchHz, tune::punchQ, sr);
        subsonic.set (tune::subsonicHz, 0.7071, sr);
        mono1.set (tune::monoHz, 0.5412, sr);   // Butterworth 4. Ordnung
        mono2.set (tune::monoHz, 1.3066, sr);

        // Lautheits-Gewichtung für Auto Gain (angelehnt an K-Weighting)
        for (int s = 0; s < 2; ++s)
            for (int c = 0; c < 2; ++c)
            {
                loudHp[s][c].set (60.0, 0.5, sr);
                setHighShelf (loudHs[s][c], 1700.0, 4.0, sr);
            }

        cMonoAmount = 5.0f;
        reset();
    }

    // Auto-Gain-Wert bleibt erhalten: Transport-Stopp und Pausen dürfen ihn nicht verlieren
    void reset()
    {
        hold.reset();
        af = as = ps = sf = ss = 0.0;
        gainDb = 0.0f;
        peak = peakSmooth = 1.0e-3f;
        for (auto& d : laDelay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& d : osDelay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& d : dryDelay) std::fill (d.begin(), d.end(), 0.0f);
        laPos = osPos = 0;
        for (auto& f : punchF) f.reset();
        shelf.reset(); subsonic.reset(); mono1.reset(); mono2.reset();
        for (auto& o : os) o.reset();
        for (auto& a : loudHp) for (auto& f : a) f.reset();
        for (auto& a : loudHs) for (auto& f : a) f.reset();
        inHit = false; hitLen = 0; sinceOnset = 1 << 30; eDry = eWet = 0.0;
        hitPeak = 0.0f;
        firstParams = true;
        agDb = agOnLast ? agTarget : 0.0f;
    }

    // Für den gespeicherten Plugin-Zustand (thread-sicher)
    float getAutoGainDb() const { return agShared.load (std::memory_order_relaxed); }
    void  restoreAutoGainDb (float db) { agRestore.store (db, std::memory_order_relaxed); agRestorePending.store (true, std::memory_order_release); }

    int getLatency() const { return latency; }

    // Anzeige (Audio-Thread schreibt, GUI liest und nimmt die Werte mit exchange ab)
    std::atomic<float> vizTransientDb { 0.0f }; // stärkste Attack/Sustain-Änderung seit dem letzten Abholen
    std::atomic<float> vizOutPeak { 0.0f };
    std::atomic<float> vizAutoGainDb { 0.0f };
    std::atomic<int>   vizHits { 0 };

    void process (float* left, float* right, int n, const Params& p)
    {
        pollRestore();

        if (firstParams)
        {
            rAttack.jumpTo (p.attack); rSustain.jumpTo (p.sustain); rPunch.jumpTo (p.punch);
            rSat.jumpTo (p.saturation); rLow.jumpTo (p.lowEnd); rMix.jumpTo (p.mix); rOut.jumpTo (p.outDb);
            lastLow = p.lowEnd + 1.0f; // erzwingt die erste Filter-Berechnung
            agOnLast = p.autoGain;
            agDb = p.autoGain ? agTarget : 0.0f;
            firstParams = false;
        }
        rAttack.setTarget (p.attack); rSustain.setTarget (p.sustain); rPunch.setTarget (p.punch);
        rSat.setTarget (p.saturation); rLow.setTarget (p.lowEnd); rMix.setTarget (p.mix); rOut.setTarget (p.outDb);

        if (p.autoGain != agOnLast)
        {
            agOnLast = p.autoGain;
            agToggleLeft = (int) (tune::autoGainToggleMs * 0.001 * sr);
        }

        float vizT = 0.0f, vizP = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float x[2] = { sanitize (left[i]), sanitize (right[i]) };

            const float attack = rAttack.next(), sustain = rSustain.next(), punch = rPunch.next();
            const float sat = rSat.next(), low = rLow.next(), mix = rMix.next(), outDb = rOut.next();

            //==================================================================
            // Erkennung auf dem unverzögerten Signal: die Verstärkung steht schon, wenn der Schlag ankommt
            const float h = hold.push (std::max (std::fabs (x[0]), std::fabs (x[1])));

            af += (h > af ? cFastRise : cAttackFall) * ((double) h - af);
            as += (h > as ? cSlowRise : cAttackFall) * ((double) h - as);
            ps += (h > ps ? cPunchRise : cAttackFall) * ((double) h - ps);
            sf = h > sf ? (double) h : sf + cSusFast * ((double) h - sf);
            ss = h > ss ? (double) h : ss + cSusSlow * ((double) h - ss);

            const float afDb = levelDb (af), asDb = levelDb (as), psDb = levelDb (ps);
            const float sfDb = levelDb (sf), ssDb = levelDb (ss);

            const float attackDiff = std::max (0.0f, afDb - asDb);
            const float punchDiff = std::max (0.0f, afDb - psDb);
            const float fallDiff = std::max (0.0f, ssDb - sfDb);

            // Attack
            float targetDb = 0.0f;
            if (attack != 0.0f)
            {
                const float shape = 1.0f - std::exp (-attackDiff / tune::attackKneeDb);
                targetDb += shape * attack * (attack > 0.0f ? tune::attackBoostDb : tune::attackCutDb);
            }

            // Sustain: ändert, wie schnell der Schlag abklingt. Am Ende und in Pausen läuft die Anhebung aus,
            // damit Rauschen im Ausklang nicht hochgezogen wird.
            if (sustain > 0.0f)
            {
                const float fade = clamp01 ((tune::sustainFadeStartDb + tune::sustainFadeRangeDb - fallDiff) / tune::sustainFadeRangeDb);
                targetDb += std::min (tune::sustainBoostMaxDb, sustain * tune::sustainBoostRatio * fallDiff) * fade;
            }
            else if (sustain < 0.0f)
            {
                targetDb -= std::min (tune::sustainCutMaxDb, -sustain * tune::sustainCutRatio * fallDiff);
            }

            gainDb += (float) cGain * (targetDb - gainDb);
            if (targetDb == 0.0f && std::fabs (gainDb) < 1.0e-5f) gainDb = 0.0f;
            const float g = fromDb (gainDb);
            if (std::fabs (gainDb) > std::fabs (vizT)) vizT = gainDb;

            // Punch: Anhebung um 130 Hz, nur direkt nach dem Anschlag
            float punchLin = 0.0f;
            if (punch > 0.0f)
            {
                const float shape = 1.0f - std::exp (-punchDiff / tune::punchKneeDb);
                punchLin = fromDb (punch * shape * tune::punchMaxDb) - 1.0f;
            }

            // Spitzenwert für die pegelunabhängige Sättigung; in Pausen bleibt er stehen
            if (h > peak) peak = h;
            else if (h > peak * 0.03f) peak += (float) cPeakFall * (h - peak);
            peak = std::max (peak, 1.0e-3f);
            peakSmooth += (float) cPeakSmooth * (peak - peakSmooth);

            trackHit (h, attackDiff);

            //==================================================================
            // Audio
            float s[2], delayedIn[2];
            for (int c = 0; c < 2; ++c)
            {
                delayedIn[c] = laDelay[c][(size_t) laPos];
                laDelay[c][(size_t) laPos] = x[c];

                float v = delayedIn[c] * g;
                const float pb = (float) punchF[c].band (v);
                if (punchLin != 0.0f) v += punchLin * pb;
                s[c] = v;
            }
            if (++laPos >= laSamples) laPos = 0;

            lowEndStage (s, low);

            const float drive = sat > 0.0f ? tune::driveMax * sat * std::sqrt (sat) : 0.0f;
            const float invTanh = drive > 1.0e-3f ? 1.0f / std::tanh (drive) : 1.0f;
            const float pk = peakSmooth;
            const float refCeil = std::max (1.0f, pk);
            const float knee = refCeil * tune::ceilingKnee, lim = refCeil * tune::ceilingLimit;

            float y[2], dry[2];
            for (int c = 0; c < 2; ++c)
            {
                // Sättigung und Peak-Schutz rechnen nur die Änderung, das saubere Signal läuft daran vorbei
                const float res = os[c].process (s[c], [&, c] (float u)
                {
                    float w = u;
                    if (drive > 1.0e-3f)
                    {
                        const float vn = u / pk;
                        w = std::tanh (drive * vn) * invTanh * pk;
                    }
                    const float mag = std::fabs (w);
                    if (mag > knee)
                        w = std::copysign (knee + (lim - knee) * std::tanh ((mag - knee) / (lim - knee)), w);
                    return w - u;
                });

                const float clean = osDelay[c][(size_t) osPos];
                osDelay[c][(size_t) osPos] = s[c];
                y[c] = clean + res;

                dry[c] = dryDelay[c][(size_t) osPos];
                dryDelay[c][(size_t) osPos] = delayedIn[c];
            }
            if (++osPos >= kOsLatency) osPos = 0;

            // Mix, Messung, Auto Gain, Output
            float mixed[2];
            for (int c = 0; c < 2; ++c)
                mixed[c] = mix >= 1.0f ? y[c] : dry[c] + mix * (y[c] - dry[c]);

            measureLoudness (dry, mixed);
            moveAutoGain (h, p.autoGain);

            const float outGain = fromDb (outDb) * fromDb (agDb);
            for (int c = 0; c < 2; ++c)
                mixed[c] *= outGain;

            left[i] = mixed[0];
            right[i] = mixed[1];
            vizP = std::max (vizP, std::max (std::fabs (mixed[0]), std::fabs (mixed[1])));
        }

        agShared.store (agTarget, std::memory_order_relaxed);
        vizAutoGainDb.store (agDb, std::memory_order_relaxed);

        // Stärksten Wert für die Anzeige behalten, bis die GUI ihn abholt
        if (std::fabs (vizT) > std::fabs (vizTransientDb.load (std::memory_order_relaxed)))
            vizTransientDb.store (vizT, std::memory_order_relaxed);
        if (vizP > vizOutPeak.load (std::memory_order_relaxed))
            vizOutPeak.store (vizP, std::memory_order_relaxed);
    }

    // Für Tests
    bool  isInHit() const { return inHit; }
    float currentAutoGainDb() const { return agDb; }
    float autoGainTargetDb() const { return agTarget; }

private:
    static inline float sanitize (float v)
    {
        if (! std::isfinite (v)) return 0.0f;
        return std::max (-64.0f, std::min (64.0f, v));
    }
    static inline float clamp01 (float v) { return std::max (0.0f, std::min (1.0f, v)); }
    static inline float levelDb (double v) { return std::max (tune::floorDb, toDb ((float) v)); }

    void lowEndStage (float* s, float low)
    {
        if (low != lastLow)
        {
            lastLow = low;
            const float db = low * tune::shelfMaxDb;
            shelfA = std::pow (10.0, (double) db / 40.0);
            shelfOn = db != 0.0f;
            const double k = 1.0 / 0.7071;
            shelf.setG (shelfG / std::sqrt (shelfA), k);
            shelfM1 = k * (shelfA - 1.0);
            shelfM2 = shelfA * shelfA - 1.0;
            monoAmt = std::min (1.0f, low * cMonoAmount);
        }

        const double m = 0.5 * ((double) s[0] + (double) s[1]);
        const double d = 0.5 * ((double) s[0] - (double) s[1]);

        shelf.tick (m);
        double delta = shelfOn ? shelfM1 * shelf.v1 + shelfM2 * shelf.v2 : 0.0;
        delta = subsonic.highpass (delta);

        const double hpSide = mono2.highpass (mono1.highpass (d));

        if (delta == 0.0 && monoAmt == 0.0f)
            return; // neutral: Signal bleibt bit-genau

        const double mOut = m + delta;
        const double dOut = d + (double) monoAmt * (hpSide - d);
        s[0] = (float) (mOut + dOut);
        s[1] = (float) (mOut - dOut);
    }

    //==========================================================================
    // Auto Gain: misst pro Schlag die Lautheit von Eingang und Ausgang. Das Fenster gilt nur für den Schlag selbst,
    // Pausen fließen also nie in die Messung ein, und ohne Schlag bleibt der Wert stehen.
    inline void trackHit (float h, float attackDiff)
    {
        ++sinceOnset;
        const float hDb = levelDb (h);
        const bool onset = attackDiff >= tune::hitOnsetDb
                           && hDb >= tune::hitMinDb
                           && (double) sinceOnset >= tune::hitRetriggerMs * 0.001 * sr;

        if (onset)
        {
            if (inHit) closeHit();
            openHit (h);
            sinceOnset = 0;
            vizHits.fetch_add (1, std::memory_order_relaxed);
            return;
        }

        if (! inHit) return;

        hitPeak = std::max (hitPeak, h);
        ++hitLen;
        const float hitPeakDb = levelDb (hitPeak);
        if (hDb <= hitPeakDb - tune::hitEndDb || hDb <= tune::floorDb)
        {
            closeHit();
        }
        else if ((double) hitLen >= tune::hitMaxMs * 0.001 * sr)
        {
            // Dauerton ohne neuen Anschlag: Fenster abschließen und direkt ein neues beginnen
            closeHit();
            openHit (h);
        }
    }

    inline void openHit (float h)
    {
        inHit = true;
        hitLen = 0;
        hitPeak = h;
        eDry = eWet = 0.0;
    }

    inline void closeHit()
    {
        inHit = false;
        if ((double) hitLen < tune::hitMinMs * 0.001 * sr) return;
        if (eDry < 1.0e-14 || eWet < 1.0e-14) return;

        float delta = eDry == eWet ? 0.0f : (float) (10.0 * std::log10 (eDry / eWet));
        delta = std::max (-tune::autoGainRangeDb, std::min (tune::autoGainRangeDb, delta));

        agTarget = haveMeasure ? agTarget + tune::autoGainFollow * (delta - agTarget) : delta;
        if (std::fabs (agTarget - delta) < 0.01f) agTarget = delta;
        haveMeasure = true;
    }

    inline void measureLoudness (const float* dry, const float* wet)
    {
        // Filter laufen immer mit, damit das Fenster sauber einschwingt
        for (int c = 0; c < 2; ++c)
        {
            const double a = shelfTick (loudHs[0][c], loudHp[0][c].highpass ((double) dry[c]));
            const double b = shelfTick (loudHs[1][c], loudHp[1][c].highpass ((double) wet[c]));
            if (inHit)
            {
                eDry += a * a;
                eWet += b * b;
            }
        }
    }

    inline void moveAutoGain (float h, bool on)
    {
        const float goal = on ? agTarget : 0.0f;
        if (agDb == goal) return;

        // Innerhalb des Schlags bleibt die Korrektur stehen: bewegt wird nur im Ausklang und in Pausen
        const double lenMs = (double) hitLen * 1000.0 / sr;
        const bool inBody = inHit && lenMs < tune::hitBodyMaxMs
                            && (lenMs < tune::hitBodyMinMs || levelDb (h) > levelDb (hitPeak) - tune::hitBodyDb);
        double c = 0.0;
        if (agToggleLeft > 0) c = cAgToggle;
        else if (! inBody) c = cAgMove;

        if (c > 0.0)
        {
            agDb += (float) c * (goal - agDb);
            if (std::fabs (agDb - goal) < 0.002f) agDb = goal;
        }
        if (agToggleLeft > 0) --agToggleLeft;
    }

    inline void pollRestore()
    {
        if (agRestorePending.load (std::memory_order_acquire))
        {
            agRestorePending.store (false, std::memory_order_relaxed);
            const float v = std::max (-tune::autoGainRangeDb, std::min (tune::autoGainRangeDb, agRestore.load (std::memory_order_relaxed)));
            agTarget = std::isfinite (v) ? v : 0.0f;
            haveMeasure = true;
            agDb = agOnLast ? agTarget : 0.0f;
        }
    }

    // Einfacher Biquad für das Höhen-Shelf der Lautheits-Gewichtung
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0; void reset() { z1 = z2 = 0.0; } };
    static void setHighShelf (Biquad& q, double hz, double db, double sr)
    {
        const double a = std::pow (10.0, db / 40.0);
        const double w = 2.0 * 3.14159265358979323846 * std::min (hz, sr * 0.45) / sr;
        const double cw = std::cos (w), sw = std::sin (w), alpha = sw / 2.0 * std::sqrt (2.0);
        const double sa = 2.0 * std::sqrt (a) * alpha;
        const double a0 = (a + 1) - (a - 1) * cw + sa;
        q.b0 = a * ((a + 1) + (a - 1) * cw + sa) / a0;
        q.b1 = -2 * a * ((a - 1) + (a + 1) * cw) / a0;
        q.b2 = a * ((a + 1) + (a - 1) * cw - sa) / a0;
        q.a1 = 2 * ((a - 1) - (a + 1) * cw) / a0;
        q.a2 = ((a + 1) - (a - 1) * cw - sa) / a0;
    }
    static inline double shelfTick (Biquad& q, double x)
    {
        const double y = q.b0 * x + q.z1;
        q.z1 = q.b1 * x - q.a1 * y + q.z2;
        q.z2 = q.b2 * x - q.a2 * y;
        return y;
    }

    double sr = 48000.0;
    int laSamples = 72, latency = 100;

    HoldPeak hold;
    double af = 0, as = 0, ps = 0, sf = 0, ss = 0;
    double cFastRise = 0, cSlowRise = 0, cAttackFall = 0, cSusFast = 0, cSusSlow = 0, cGain = 0, cPunchRise = 0,
           cPeakFall = 0, cPeakSmooth = 0, cAgMove = 0, cAgToggle = 0;
    float cMonoAmount = 5.0f;
    float gainDb = 0.0f, peak = 1.0e-3f, peakSmooth = 1.0e-3f;

    Ramp rAttack, rSustain, rPunch, rSat, rLow, rMix, rOut;
    bool firstParams = true;

    std::vector<float> laDelay[2], osDelay[2], dryDelay[2];
    int laPos = 0, osPos = 0;

    Svf punchF[2], shelf, subsonic, mono1, mono2;
    double shelfG = 0.0, shelfA = 1.0, shelfM1 = 0.0, shelfM2 = 0.0;
    bool shelfOn = false;
    float monoAmt = 0.0f, lastLow = -1.0f;

    bfz::Oversampler4x os[2];

    Svf loudHp[2][2];
    Biquad loudHs[2][2];

    bool inHit = false, haveMeasure = false, agOnLast = false;
    int hitLen = 0, sinceOnset = 1 << 30, agToggleLeft = 0;
    float hitPeak = 0.0f, agTarget = 0.0f, agDb = 0.0f;
    double eDry = 0.0, eWet = 0.0;

    std::atomic<float> agShared { 0.0f }, agRestore { 0.0f };
    std::atomic<bool> agRestorePending { false };
};

} // namespace ktm
