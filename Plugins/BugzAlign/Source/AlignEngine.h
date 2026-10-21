#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>

// Brainbugz Bugz Align DSP
// Richtet die Phase des Basses (Sub-Bereich) automatisch an der Kick aus.
//
// - Kick kommt per Sidechain, Bass ist der Haupteingang
// - Bass-Sub (Linkwitz-Riley 4. Ordnung) wird per Hilbert-Paar (2x 6 Allpässe, 90 Grad Differenz)
//   in I/Q zerlegt; Phasendrehung um theta: y = I*cos(theta) - Q*sin(theta)
//   (Weiche: LR4-Tief- und Hochpass, zusammen phasengleich -> bei 0 Grad bleibt der Klang unverändert)
// - Bei jedem erkannten Kick wird im Punch-Fenster (die ersten X ms) die Korrelation zwischen Kick-Sub
//   und Bass-Sub gemessen. Der beste Winkel lässt sich direkt ausrechnen:
//       theta = atan2(-sum(k*Q), sum(k*I))
// - Look-ahead: das Audio wird um eine feste Latenz verzögert, damit der neue Winkel schon sanft
//   eingeblendet ist, bevor die Kick im Ausgang ankommt
// - Höhen bleiben unangetastet (laufen nur durch denselben Allpass, damit bei 0 Grad alles phasengleich ist)
//   Hinweis: direkt an der Weichenfrequenz überlagern sich gedrehter Sub und Höhen, deshalb die Weiche
//   oberhalb des Bass-Grundtons ansetzen

namespace bb
{

struct AlignParams
{
    float amount   = 1.0f;   // 0..1  Anteil der automatischen Korrektur
    float offsetDeg = 0.0f;  // -180..180 manueller Zusatz-Winkel
    float splitHz  = 180.0f; // Sub-Weiche (>= kFullHz = ganzer Bass wird gedreht)
    float windowMs = 50.0f;  // Punch-Fenster nach der Kick
    float glideMs  = 15.0f;  // Übergangszeit zum neuen Winkel
    float sens     = 0.5f;   // Kick-Erkennung 0..1
    bool  hold     = false;  // Winkel einfrieren
    bool  bypass   = false;  // A/B: Korrektur aus (0 Grad)
    bool  subCheck = false;  // Abhören: Sub von Bass + Kick, mono
};

class AlignEngine
{
public:
    static constexpr double kLatencyMs = 180.0;
    static constexpr double kMaxWindowMs = 120.0;
    static constexpr double kMaxGlideMs = 50.0;
    static constexpr double kPreMs = 1.5;
    static constexpr int kSnapPoints = 240;
    static constexpr float kFullHz = 400.0f;
    static constexpr double kAnalysisHz = 200.0;

    // ---- Anzeige-Daten (Audio-Thread schreibt, UI liest) ----
    struct Snapshot
    {
        std::array<std::atomic<float>, kSnapPoints> kick {}, before {}, after {};
        std::atomic<float> corrBefore { 0.0f }, corrAfter { 0.0f }, thetaDeg { 0.0f };
        std::atomic<int> serial { 0 };
    };
    Snapshot snap;
    std::atomic<float> appliedDeg { 0.0f };   // aktuell angewendeter Winkel (gleitet)
    std::atomic<int>   kickCount { 0 };       // erkannte Kicks (für Blinken in der UI)
    std::atomic<float> kickLevel { 0.0f }, bassLevel { 0.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        latency = (int) std::lround (fs * kLatencyMs / 1000.0);
        const int needed = latency + (int) std::ceil (fs * (kMaxWindowMs + kPreMs + 10.0) / 1000.0);
        size = 1;
        while (size < needed) size <<= 1;
        mask = size - 1;
        for (auto* v : { &kLowRing, &iMonoRing, &qMonoRing })
            v->assign ((size_t) size, 0.0f);
        for (int c = 0; c < 2; ++c)
            for (auto* v : { &iHighRing[c], &iLowRing[c], &qLowRing[c] })
                v->assign ((size_t) size, 0.0f);
        setupHilbert();
        lastSplit = -1.0f;
        reset();
    }

    int getLatencySamples() const { return latency; }

    void reset()
    {
        for (auto& c : ch) c.clear();
        kickLp.clear(); kickA.clear(); iA.clear(); qA.clear();
        for (auto* v : { &kLowRing, &iMonoRing, &qMonoRing }) std::fill (v->begin(), v->end(), 0.0f);
        for (int c = 0; c < 2; ++c)
            for (auto* v : { &iHighRing[c], &iLowRing[c], &qLowRing[c] }) std::fill (v->begin(), v->end(), 0.0f);
        t = 0;
        fastEnv = slowEnv = 0.0;
        lastOnset = -1000000000LL;
        windowEnd = -1;
        evHead = evTail = 0;
        thetaOpt = thetaSched = 0.0;
        cur = 0.0; glideFrom = 0.0; glideTo = 0.0; glidePos = 1.0; glideStep = 1.0;
        lastAmount = -1.0f; lastOffset = 1.0e9f; lastBypass = false;
        kEnvView = bEnvView = 0.0;
    }

    // bass: 1 oder 2 Kanäle (in-place), kick: Sidechain (1-2 Kanäle) oder nullptr
    void process (float* const* bass, int numBassCh, const float* const* kick, int numKickCh, int n, const AlignParams& p)
    {
        if (std::abs (p.splitHz - lastSplit) > 0.01f)
        {
            lastSplit = p.splitHz;
            setupCrossover (p.splitHz);
        }
        const bool full = p.splitHz >= kFullHz;

        const int win = std::max (8, (int) std::lround (fs * std::clamp ((double) p.windowMs, 5.0, kMaxWindowMs) / 1000.0));
        const int pre = (int) std::lround (fs * kPreMs / 1000.0);
        const int glide = std::max (1, (int) std::lround (fs * std::clamp ((double) p.glideMs, 0.5, kMaxGlideMs) / 1000.0));
        const long long minGap = (long long) std::lround (fs * 0.07);
        // Empfindlichkeit: Verhältnis schnell/langsam und Mindestpegel
        const double ratio = 3.2 - 2.0 * p.sens;                    // 3.2 .. 1.2
        const double thresh = std::pow (10.0, (-30.0 - 30.0 * p.sens) / 20.0); // -30 .. -60 dBFS
        const double aFastA = coef (0.3), aFastR = coef (12.0), aSlowA = coef (12.0), aSlowR = coef (120.0);

        // Regler-Änderungen (Amount, Offset, A/B) sofort weich übernehmen
        if (std::abs (p.amount - lastAmount) > 0.002f || std::abs (p.offsetDeg - lastOffset) > 0.2f || p.bypass != lastBypass)
        {
            lastAmount = p.amount; lastOffset = p.offsetDeg; lastBypass = p.bypass;
            startGlide (targetFor (thetaOpt, p), (int) std::lround (fs * 0.02));
        }

        const bool stereo = numBassCh > 1;
        for (int i = 0; i < n; ++i, ++t)
        {
            const int w = (int) (t & mask);

            // ---- Kick (Sidechain) ----
            double k = 0.0;
            if (kick != nullptr && numKickCh > 0)
                k = numKickCh > 1 ? 0.5 * ((double) kick[0][i] + (double) kick[1][i]) : (double) kick[0][i];
            // Analyse-Signal der Kick: gleiche Filter wie der Bass-Sub, damit die Phasen vergleichbar sind
            const double kLow = kickA.process (full ? k : kickLp.process (k));
            kLowRing[(size_t) w] = (float) kLow;

            const double ak = std::abs (k);
            fastEnv += (ak - fastEnv) * (ak > fastEnv ? aFastA : aFastR);
            slowEnv += (ak - slowEnv) * (ak > slowEnv ? aSlowA : aSlowR);
            kEnvView += (std::abs (kLow) - kEnvView) * 0.0005;

            if (fastEnv > thresh && fastEnv > ratio * slowEnv + 1.0e-9 && t - lastOnset > minGap)
            {
                if (windowEnd > t)   // vorheriges Fenster noch offen: verkürzt auswerten
                {
                    windowEnd = t;
                    analyse (p, glide);
                }
                lastOnset = t;
                windowStart = t - pre;
                windowEnd = t + win;
                kickCount.fetch_add (1, std::memory_order_relaxed);
            }

            // ---- Bass: Weiche + Hilbert pro Kanal, alles in die Verzögerung ----
            double im = 0.0, qm = 0.0;
            for (int c = 0; c < (stereo ? 2 : 1); ++c)
            {
                auto& s = ch[(size_t) c];
                const double x = bass[c][i];
                const double low = full ? x : s.lp.process (x);
                const double iHigh = full ? 0.0 : s.hHigh.process (s.hp.process (x));
                const double iLow = s.hI.process (low);
                const double qLow = s.hQ.process (low);
                iHighRing[c][(size_t) w] = (float) iHigh;
                iLowRing[c][(size_t) w] = (float) iLow;
                qLowRing[c][(size_t) w] = (float) qLow;
                im += iLow; qm += qLow;
            }
            if (stereo) { im *= 0.5; qm *= 0.5; }
            im = iA.process (im);
            qm = qA.process (qm);
            iMonoRing[(size_t) w] = (float) im;
            qMonoRing[(size_t) w] = (float) qm;
            bEnvView += (std::sqrt (im * im + qm * qm) - bEnvView) * 0.0005;

            // ---- Fenster fertig: besten Winkel ausrechnen ----
            if (t == windowEnd)
            {
                analyse (p, glide);
                windowEnd = -1;
            }

            // ---- Ausgang (um die Latenz verzögert) ----
            const long long to = t - latency;
            while (evHead != evTail && events[(size_t) evHead].start <= to)
            {
                const auto& e = events[(size_t) evHead];
                thetaOpt = e.theta;
                startGlide (targetFor (thetaOpt, p), e.glide);
                evHead = (evHead + 1) % kMaxEvents;
            }
            if (glidePos < 1.0)
            {
                glidePos = std::min (1.0, glidePos + glideStep);
                const double sm = glidePos * glidePos * (3.0 - 2.0 * glidePos);
                cur = glideFrom + (glideTo - glideFrom) * sm;
            }
            const double cs = std::cos (cur), sn = std::sin (cur);

            const int r = (int) (to & mask);
            if (p.subCheck)
            {
                const double y = (double) iMonoRing[(size_t) r] * cs - (double) qMonoRing[(size_t) r] * sn;
                const float o = (float) (y + (double) kLowRing[(size_t) r]);
                for (int c = 0; c < numBassCh; ++c) bass[c][i] = o;
            }
            else
            {
                for (int c = 0; c < (stereo ? 2 : 1); ++c)
                {
                    const double iH = iHighRing[c][(size_t) r], iL = iLowRing[c][(size_t) r], qL = qLowRing[c][(size_t) r];
                    bass[c][i] = (float) (iH + cs * iL - sn * qL);
                }
            }
        }

        appliedDeg.store ((float) wrapDeg (cur * 180.0 / pi), std::memory_order_relaxed);
        kickLevel.store ((float) kEnvView, std::memory_order_relaxed);
        bassLevel.store ((float) bEnvView, std::memory_order_relaxed);
    }

    static double wrap (double a)
    {
        while (a > pi) a -= 2.0 * pi;
        while (a < -pi) a += 2.0 * pi;
        return a;
    }
    static double wrapDeg (double a)
    {
        while (a > 180.0) a -= 360.0;
        while (a < -180.0) a += 360.0;
        return a;
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    static constexpr int kMaxEvents = 32;

    // ---------- Bausteine ----------
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() { z1 = z2 = 0; }
    };

    struct LR4  // Linkwitz-Riley 4. Ordnung (2x Butterworth), Tief- oder Hochpass
    {
        Biquad a, b;
        double process (double x) { return b.process (a.process (x)); }
        void clear() { a.clear(); b.clear(); }
        void set (double fc, double fs, bool high = false)
        {
            const double w0 = 2.0 * pi * fc / fs, cw = std::cos (w0), al = std::sin (w0) / (2.0 * 0.70710678118654752);
            const double a0 = 1.0 + al;
            Biquad q;
            if (high) { q.b0 = (1.0 + cw) * 0.5 / a0; q.b1 = -(1.0 + cw) / a0; }
            else      { q.b0 = (1.0 - cw) * 0.5 / a0; q.b1 = (1.0 - cw) / a0; }
            q.b2 = q.b0;
            q.a1 = -2.0 * cw / a0; q.a2 = (1.0 - al) / a0;
            a.b0 = b.b0 = q.b0; a.b1 = b.b1 = q.b1; a.b2 = b.b2 = q.b2;
            a.a1 = b.a1 = q.a1; a.a2 = b.a2 = q.a2;
        }
    };

    struct AllpassChain  // 6 Allpässe 1. Ordnung
    {
        std::array<double, 6> c {}, x1 {}, y1 {};
        double process (double x)
        {
            for (size_t j = 0; j < 6; ++j)
            {
                const double y = c[j] * x + x1[j] - c[j] * y1[j];
                x1[j] = x; y1[j] = y;
                x = y;
            }
            return x;
        }
        void clear() { x1.fill (0); y1.fill (0); }
    };

    struct Channel
    {
        LR4 lp, hp;
        AllpassChain hHigh, hI, hQ;
        void clear() { lp.clear(); hp.clear(); hHigh.clear(); hI.clear(); hQ.clear(); }
    };

    struct Event { long long start = 0; double theta = 0; int glide = 1; };

    // ---------- Einrichtung ----------
    void setupHilbert()
    {
        // per Optimierung entworfen: 90 Grad Differenz zwischen den Ketten von ca. 15 Hz bis 2.5 kHz
        static constexpr double qPoles[6] { 3.253315, 22.964646, 80.568962, 270.646641, 917.805710, 3632.504083 };
        static constexpr double iPoles[6] { 11.011687, 43.582204, 147.794168, 496.469042, 1741.807646, 12295.149883 };
        auto coefFor = [this] (double fp)
        {
            fp = std::min (fp, fs * 0.45);
            const double K = 2.0 * fs, p = 2.0 * fs * std::tan (pi * fp / fs);
            return (p - K) / (p + K);
        };
        for (auto& c : ch)
            for (size_t j = 0; j < 6; ++j)
            {
                c.hHigh.c[j] = c.hI.c[j] = coefFor (iPoles[j]);
                c.hQ.c[j] = coefFor (qPoles[j]);
            }
    }

    void setupCrossover (float fc)
    {
        const double f = std::clamp ((double) fc, 30.0, fs * 0.2);
        for (auto& c : ch) { c.lp.set (f, fs); c.hp.set (f, fs, true); }
        kickLp.set (f, fs);
        kickA.set (kAnalysisHz, fs); iA.set (kAnalysisHz, fs); qA.set (kAnalysisHz, fs);
    }

    double coef (double ms) const { return 1.0 - std::exp (-1.0 / (fs * ms / 1000.0)); }

    double targetFor (double theta, const AlignParams& p) const
    {
        if (p.bypass) return 0.0;
        return wrap ((double) p.amount * theta + (double) p.offsetDeg * pi / 180.0);
    }

    void startGlide (double target, int samples)
    {
        glideFrom = cur;
        glideTo = cur + wrap (target - cur);  // kürzester Weg
        // cur in einem vernünftigen Bereich halten
        if (std::abs (glideTo) > 4.0 * pi)
        {
            const double shift = 2.0 * pi * std::round (glideTo / (2.0 * pi));
            glideFrom -= shift; glideTo -= shift; cur -= shift;
        }
        glidePos = 0.0;
        glideStep = 1.0 / (double) std::max (1, samples);
    }

    void analyse (const AlignParams& p, int glide)
    {
        double kk = 0, ii = 0, qq = 0, ki = 0, kq = 0, iq = 0;
        const long long len = windowEnd - windowStart;
        const int dec = (int) std::max<long long> (1, len / kSnapPoints);
        std::array<float, kSnapPoints> sk {}, si {}, sq {};
        int sIdx = 0, sCnt = 0;
        double ak = 0, ai = 0, aq = 0;

        for (long long s = windowStart; s < windowEnd; ++s)
        {
            const size_t r = (size_t) (s & mask);
            const double k = kLowRing[r], I = iMonoRing[r], Q = qMonoRing[r];
            kk += k * k; ii += I * I; qq += Q * Q;
            ki += k * I; kq += k * Q; iq += I * Q;
            ak += k; ai += I; aq += Q;
            if (++sCnt == dec && sIdx < kSnapPoints)
            {
                sk[(size_t) sIdx] = (float) (ak / dec); si[(size_t) sIdx] = (float) (ai / dec); sq[(size_t) sIdx] = (float) (aq / dec);
                ++sIdx; sCnt = 0; ak = ai = aq = 0;
            }
        }

        const double nn = (double) std::max<long long> (1, len);
        const double rmsK = std::sqrt (kk / nn), rmsB = std::sqrt ((ii + qq) * 0.5 / nn);
        if (rmsK < 1.0e-4 || rmsB < 1.0e-4)   // keine Kick oder kein Bass im Fenster: nichts ändern
            return;

        auto corr = [&] (double th)
        {
            const double c = std::cos (th), s = std::sin (th);
            const double ky = c * ki - s * kq;
            const double yy = c * c * ii + s * s * qq - 2.0 * c * s * iq;
            return ky / std::sqrt (kk * yy + 1.0e-30);
        };

        // bester Winkel: grob in 1-Grad-Schritten, dann fein (I und Q sind nicht exakt gleich laut,
        // darum wird die normierte Korrelation gesucht statt nur atan2(-kq, ki))
        double best = std::atan2 (-kq, ki), bestC = corr (best);
        for (int d = 0; d < 360; ++d)
        {
            const double th = d * pi / 180.0, cc = corr (th);
            if (cc > bestC) { bestC = cc; best = th; }
        }
        for (int d = -10; d <= 10; ++d)
        {
            const double th = best + d * pi / 1800.0, cc = corr (th);
            if (cc > bestC) { bestC = cc; best = th; }
        }
        best = wrap (best);

        // Hysterese: lohnt sich die Änderung überhaupt?
        const double curTarget = targetFor (thetaSched, p);
        const double newTarget = targetFor (best, p);
        const bool worth = corr (newTarget) - corr (curTarget) > 0.02;

        const double before = corr (0.0);
        snap.corrBefore.store ((float) before, std::memory_order_relaxed);
        snap.corrAfter.store ((float) corr (worth && ! p.hold ? newTarget : curTarget), std::memory_order_relaxed);
        snap.thetaDeg.store ((float) (best * 180.0 / pi), std::memory_order_relaxed);
        const double shown = worth && ! p.hold ? newTarget : curTarget;
        const double c = std::cos (shown), s = std::sin (shown);
        for (int j = 0; j < kSnapPoints; ++j)
        {
            const size_t u = (size_t) j;
            const bool valid = j < sIdx;
            snap.kick[u].store (valid ? sk[u] : 0.0f, std::memory_order_relaxed);
            snap.before[u].store (valid ? si[u] : 0.0f, std::memory_order_relaxed);
            snap.after[u].store (valid ? (float) (c * si[u] - s * sq[u]) : 0.0f, std::memory_order_relaxed);
        }
        snap.serial.fetch_add (1, std::memory_order_release);

        if (p.hold || ! worth)
            return;

        // Übergang so planen, dass er fertig ist, wenn der Fensteranfang im Ausgang ankommt
        const int next = (evTail + 1) % kMaxEvents;
        if (next == evHead) return; // Warteschlange voll
        events[(size_t) evTail] = { windowStart - glide, best, glide };
        evTail = next;
        thetaSched = best;
    }

    // ---------- Zustand ----------
    double fs = 48000.0;
    int latency = 0, size = 1, mask = 0;
    long long t = 0;

    std::array<Channel, 2> ch;
    LR4 kickLp, kickA, iA, qA;
    std::vector<float> kLowRing, iMonoRing, qMonoRing;
    std::array<std::vector<float>, 2> iHighRing, iLowRing, qLowRing;

    double fastEnv = 0, slowEnv = 0, kEnvView = 0, bEnvView = 0;
    long long lastOnset = 0, windowStart = 0, windowEnd = -1;

    std::array<Event, kMaxEvents> events {};
    int evHead = 0, evTail = 0;

    double thetaOpt = 0.0, thetaSched = 0.0;
    double cur = 0.0, glideFrom = 0.0, glideTo = 0.0, glidePos = 1.0, glideStep = 1.0;
    float lastSplit = -1.0f, lastAmount = -1.0f, lastOffset = 1.0e9f;
    bool lastBypass = false;
};

} // namespace bb
