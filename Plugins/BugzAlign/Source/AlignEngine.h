#pragma once

#include <array>
#include <vector>
#include <complex>
#include <memory>
#include <cmath>
#include <algorithm>
#include <atomic>

// Brainbugz Bugz Align DSP (v0.2, linearphasig)
// Richtet die Phase des Bass-Subs automatisch an der Kick aus, ohne den restlichen Klang anzufassen.
//
// Ausgang = Dry (nur verzögert) + Korrektur:
//     out = x + (cos(theta) - 1) * I - sin(theta) * Q
//   I = linearphasiger FIR-Tiefpass des Basses (Sub), Q = FIR-Hilbert davon (90 Grad, gleiche Höhe)
//   -> bei 0 Grad ist der Ausgang bit-genau der Eingang, über dem Split bleibt alles unangetastet,
//      und es gibt keine Phasenverschiebung der Höhen (nichts wird "dumpf")
//   Die FIRs laufen als partitionierte FFT-Faltung.
//
// Analyse (latenzarm, IIR): Kick-Sub und Bass-Sub laufen durch exakt dieselben Filter, dadurch hebt sich
// deren Phase heraus und der gefundene Winkel gilt direkt für den sauberen FIR-Weg.
//   Bei jeder Kick: Korrelation im Punch-Fenster, bester Winkel über atan2 + Feinsuche.
//   Look-ahead: der neue Winkel wird per Glide eingeblendet, bevor die Kick im Ausgang ankommt.

namespace bb
{

struct AlignParams
{
    float amount    = 1.0f;   // 0..1  Anteil der automatischen Korrektur
    float offsetDeg = 0.0f;   // -180..180 manueller Zusatz-Winkel
    float splitHz   = 180.0f; // Sub-Grenze (>= kFullHz = ganzer Bass wird gedreht)
    float windowMs  = 50.0f;  // Punch-Fenster nach der Kick
    float glideMs   = 15.0f;  // Übergangszeit zum neuen Winkel
    float sens      = 0.5f;   // Kick-Erkennung 0..1
    bool  hold      = false;  // Winkel einfrieren
    bool  bypass    = false;  // A/B: Korrektur aus (0 Grad)
    bool  subCheck  = false;  // Abhören: Sub von Bass + Kick, mono
};

// ---------------------------------------------------------------------------------------------
// kleine Radix-2-FFT
class SimpleFFT
{
public:
    using cd = std::complex<double>;

    void prepare (int size)
    {
        n = size;
        bits = 0;
        while ((1 << bits) < n) ++bits;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) (n / 2));
        for (int i = 0; i < n / 2; ++i)
            tw[(size_t) i] = std::polar (1.0, -2.0 * 3.14159265358979323846 * i / n);
    }

    int size() const { return n; }

    void forward (cd* d) const { run (d, false); }
    void inverse (cd* d) const // inklusive 1/n
    {
        run (d, true);
        const double s = 1.0 / n;
        for (int i = 0; i < n; ++i) d[i] *= s;
    }

private:
    void run (cd* d, bool inv) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (d[i], d[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const cd w = inv ? std::conj (tw[(size_t) (j * step)]) : tw[(size_t) (j * step)];
                    const cd u = d[i + j], v = d[i + j + half] * w;
                    d[i + j] = u + v;
                    d[i + j + half] = u - v;
                }
        }
    }

    int n = 0, bits = 0;
    std::vector<int> rev;
    std::vector<cd> tw;
};

// ---------------------------------------------------------------------------------------------
class AlignEngine
{
public:
    static constexpr double kMaxWindowMs = 100.0;
    static constexpr double kMaxGlideMs  = 40.0;
    static constexpr double kPreMs       = 1.5;
    static constexpr float  kFullHz      = 400.0f;
    static constexpr double kAnalysisHz  = 200.0;
    static constexpr double kHistorySeconds = 8.0;

    // ---- Anzeige: Verlauf im Ausgangs-Takt (also genau das, was man hört) ----
    struct History
    {
        std::atomic<int> length { 0 };
        double rate = 12000.0;                       // Punkte pro Sekunde
        std::unique_ptr<std::atomic<float>[]> kick, before, after, theta, marker;
        std::atomic<long long> written { 0 };        // Anzahl geschriebener Punkte (fortlaufend)
    };
    History hist;
    static constexpr int kHistCapacity = 110000;

    AlignEngine()
    {
        // einmalig anlegen (die UI liest parallel mit, darf also nie neu allokiert werden)
        for (auto* p : { &hist.kick, &hist.before, &hist.after, &hist.theta, &hist.marker })
        {
            p->reset (new std::atomic<float>[(size_t) kHistCapacity]);
            for (int i = 0; i < kHistCapacity; ++i) (*p)[(size_t) i].store (0.0f, std::memory_order_relaxed);
        }
        hist.length = kHistCapacity;
    }

    std::atomic<float> corrBefore { 0.0f }, corrAfter { 0.0f }, detectedDeg { 0.0f };
    std::atomic<int>   analysisCount { 0 };
    std::atomic<float> appliedDeg { 0.0f };
    std::atomic<int>   kickCount { 0 };
    std::atomic<float> kickLevel { 0.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;

        // FIR-Länge ~0.16 s (Auflösung tief genug für Sub ab ca. 25 Hz), 16 Partitionen
        irLen = 1;
        while (irLen < (int) (fs * 0.16)) irLen <<= 1;
        part = irLen / kParts;
        nfft = 2 * part;
        bins = nfft / 2 + 1;
        fftBlock.prepare (nfft);
        fftDesign.prepare (irLen);

        firDelay = irLen / 2 + part;
        const int analysisNeed = (int) std::ceil (fs * (kMaxWindowMs + kPreMs + kMaxGlideMs + 6.0) / 1000.0);
        latency = std::max (firDelay + 2, analysisNeed);

        const int needed = latency + irLen + (int) std::ceil (fs * (kMaxWindowMs + 10.0) / 1000.0);
        size = 1;
        while (size < needed) size <<= 1;
        mask = size - 1;

        for (int c = 0; c < 2; ++c)
        {
            dryRing[c].assign ((size_t) size, 0.0f);
            iRing[c].assign ((size_t) size, 0.0f);
            qRing[c].assign ((size_t) size, 0.0f);
            conv[c].allocate (part, bins);
        }
        kickConv.allocate (part, bins);
        kfRing.assign ((size_t) size, 0.0f);
        kaRing.assign ((size_t) size, 0.0f);
        iaRing.assign ((size_t) size, 0.0f);
        qaRing.assign ((size_t) size, 0.0f);

        for (auto* hs : { &hI, &hQ, &hInew, &hQnew })
            hs->assign ((size_t) (kParts * bins), {});
        work.assign ((size_t) nfft, {});
        work2.assign ((size_t) nfft, {});
        accI.assign ((size_t) bins, {});
        accQ.assign ((size_t) bins, {});

        // Verlauf für die Anzeige
        histDec = std::max (1, (int) std::lround (fs / 12000.0));
        hist.rate = fs / histDec;
        hist.length = std::min (kHistCapacity, (int) std::ceil (kHistorySeconds * hist.rate));
        hist.written.store (0);

        setupHilbertIIR();
        designedSplit = -1.0f;
        lastSplit = -1.0f;
        reset();
    }

    int getLatencySamples() const { return latency; }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill (dryRing[c].begin(), dryRing[c].end(), 0.0f);
            std::fill (iRing[c].begin(), iRing[c].end(), 0.0f);
            std::fill (qRing[c].begin(), qRing[c].end(), 0.0f);
            conv[c].clear();
        }
        kickConv.clear();
        for (auto* v : { &kfRing, &kaRing, &iaRing, &qaRing }) std::fill (v->begin(), v->end(), 0.0f);
        anaLp.clear(); anaB.clear(); anaA.clear(); anaLa.clear(); anaLaQ.clear();
        kLp.clear(); kB.clear(); kLa.clear();
        t = 0; blockFill = 0;
        fastEnv = slowEnv = kEnvView = 0.0;
        lastOnset = -1000000000LL;
        windowEnd = -1;
        evHead = evTail = 0;
        onHead = onTail = 0;
        thetaOpt = thetaSched = 0.0;
        cur = glideFrom = glideTo = 0.0; glidePos = 1.0; glideStep = 1.0;
        lastAmount = -1.0f; lastOffset = 1.0e9f; lastBypass = false;
        histAcc = 0; histK = histB = histA = 0.0; histMark = 0.0f;
    }

    // bass: 1 oder 2 Kanäle (in-place), kick: Sidechain (1-2 Kanäle) oder nullptr
    void process (float* const* bass, int numBassCh, const float* const* kick, int numKickCh, int n, const AlignParams& p)
    {
        const bool full = p.splitHz >= kFullHz - 0.5f;

        if (std::abs (p.splitHz - lastSplit) > 0.01f)
        {
            lastSplit = p.splitHz;
            setupAnalysisFilters (p.splitHz);
        }
        if (designedSplit < 0.0f)  // erster Durchlauf: sofort
        {
            designFIR (p.splitHz, hI, hQ);
            designedSplit = p.splitHz;
        }

        const int win = std::max (8, (int) std::lround (fs * std::clamp ((double) p.windowMs, 5.0, kMaxWindowMs) / 1000.0));
        const int pre = (int) std::lround (fs * kPreMs / 1000.0);
        const int glide = std::max (1, (int) std::lround (fs * std::clamp ((double) p.glideMs, 0.5, kMaxGlideMs) / 1000.0));
        const long long minGap = (long long) std::lround (fs * 0.07);
        const double ratio = 3.2 - 2.0 * p.sens;                                 // 3.2 .. 1.2
        const double thresh = std::pow (10.0, (-30.0 - 30.0 * p.sens) / 20.0);   // -30 .. -60 dBFS
        const double aFastA = coef (0.3), aFastR = coef (12.0), aSlowA = coef (12.0), aSlowR = coef (120.0);

        if (std::abs (p.amount - lastAmount) > 0.002f || std::abs (p.offsetDeg - lastOffset) > 0.2f || p.bypass != lastBypass)
        {
            lastAmount = p.amount; lastOffset = p.offsetDeg; lastBypass = p.bypass;
            startGlide (targetFor (thetaOpt, p), (int) std::lround (fs * 0.02));
        }

        const bool stereo = numBassCh > 1;
        const int nch = stereo ? 2 : 1;

        for (int i = 0; i < n; ++i, ++t)
        {
            const int w = (int) (t & mask);

            // ---- Eingänge in die Ringe / FIR-Blöcke ----
            double k = 0.0;
            if (kick != nullptr && numKickCh > 0)
                k = numKickCh > 1 ? 0.5 * ((double) kick[0][i] + (double) kick[1][i]) : (double) kick[0][i];

            double xm = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                const float x = bass[c][i];
                dryRing[c][(size_t) w] = x;
                conv[c].in[(size_t) (part + blockFill)] = x;
                xm += x;
            }
            if (stereo) xm *= 0.5;
            kickConv.in[(size_t) (part + blockFill)] = k;

            if (++blockFill == part)
            {
                blockFill = 0;
                runBlock (nch, p.splitHz);
            }

            // ---- Analyse (IIR, gleiche Filter für Kick und Bass) ----
            const double kS = full ? k : kLp.process (k);
            const double kA = kLa.process (kB.process (kS));
            const double bS = full ? xm : anaLp.process (xm);
            const double iA = anaLa.process (anaB.process (bS));
            const double qA = anaLaQ.process (anaA.process (bS));
            kaRing[(size_t) w] = (float) kA;
            iaRing[(size_t) w] = (float) iA;
            qaRing[(size_t) w] = (float) qA;

            // ---- Kick-Erkennung ----
            const double ak = std::abs (k);
            fastEnv += (ak - fastEnv) * (ak > fastEnv ? aFastA : aFastR);
            slowEnv += (ak - slowEnv) * (ak > slowEnv ? aSlowA : aSlowR);
            kEnvView += (std::abs (kS) - kEnvView) * 0.0005;

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
                const int nx = (onTail + 1) % kMaxEvents;
                if (nx != onHead) { onsets[(size_t) onTail] = t; onTail = nx; }
            }

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
            const double cm1 = glidePos >= 1.0 && cur == 0.0 ? 0.0 : cs - 1.0;

            const int r = (int) (to & mask);
            double im = 0.0, qm = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                const double iL = iRing[c][(size_t) r], qL = qRing[c][(size_t) r];
                im += iL; qm += qL;
                if (! p.subCheck)
                {
                    const float dry = dryRing[c][(size_t) r];
                    bass[c][i] = (cm1 == 0.0 && sn == 0.0) ? dry : (float) ((double) dry + cm1 * iL - sn * qL);
                }
            }
            if (stereo) { im *= 0.5; qm *= 0.5; }
            const double after = im * cs - qm * sn;
            const double kf = kfRing[(size_t) r];
            if (p.subCheck)
                for (int c = 0; c < numBassCh; ++c) bass[c][i] = (float) (after + kf);

            // ---- Verlauf für die Anzeige ----
            if (onHead != onTail && onsets[(size_t) onHead] <= to) { histMark = 1.0f; onHead = (onHead + 1) % kMaxEvents; }
            histK += kf; histB += im; histA += after;
            if (++histAcc >= histDec)
            {
                const long long wcount = hist.written.load (std::memory_order_relaxed);
                const size_t hw = (size_t) (wcount % hist.length);
                const double s = 1.0 / histDec;
                hist.kick[hw].store ((float) (histK * s), std::memory_order_relaxed);
                hist.before[hw].store ((float) (histB * s), std::memory_order_relaxed);
                hist.after[hw].store ((float) (histA * s), std::memory_order_relaxed);
                hist.theta[hw].store ((float) wrapDeg (cur * 180.0 / pi), std::memory_order_relaxed);
                hist.marker[hw].store (histMark, std::memory_order_relaxed);
                hist.written.store (wcount + 1, std::memory_order_release);
                histAcc = 0; histK = histB = histA = 0.0; histMark = 0.0f;
            }
        }

        appliedDeg.store ((float) wrapDeg (cur * 180.0 / pi), std::memory_order_relaxed);
        kickLevel.store ((float) kEnvView, std::memory_order_relaxed);
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

    // für Tests: Frequenzgang des FIR-Tiefpasses (Betrag) an Frequenz f
    double designTarget (double f, double split) const { return shape (f, split); }

private:
    static constexpr double pi = 3.14159265358979323846;
    static constexpr int kMaxEvents = 32;
    static constexpr int kParts = 16;
    using cd = std::complex<double>;

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

    struct LR4  // Linkwitz-Riley Tiefpass 4. Ordnung (nur für die Analyse)
    {
        Biquad a, b;
        double process (double x) { return b.process (a.process (x)); }
        void clear() { a.clear(); b.clear(); }
        void set (double fc, double fs)
        {
            const double w0 = 2.0 * pi * fc / fs, cw = std::cos (w0), al = std::sin (w0) / (2.0 * 0.70710678118654752);
            const double a0 = 1.0 + al;
            for (auto* q : { &a, &b })
            {
                q->b0 = (1.0 - cw) * 0.5 / a0; q->b1 = (1.0 - cw) / a0; q->b2 = q->b0;
                q->a1 = -2.0 * cw / a0; q->a2 = (1.0 - al) / a0;
            }
        }
    };

    struct AllpassChain  // 6 Allpässe 1. Ordnung (Hilbert-Paar für die Analyse)
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

    // partitionierte Faltung: Eingangsblock + Frequenz-Verzögerungskette
    struct ConvState
    {
        std::vector<double> in;          // [vorheriger Block | aktueller Block]
        std::vector<cd> fdl;             // kParts Spektren
        int head = 0;
        void allocate (int partLen, int nBins)
        {
            in.assign ((size_t) (2 * partLen), 0.0);
            fdl.assign ((size_t) (kParts * nBins), {});
            head = 0;
        }
        void clear()
        {
            std::fill (in.begin(), in.end(), 0.0);
            std::fill (fdl.begin(), fdl.end(), cd {});
            head = 0;
        }
    };

    struct Event { long long start = 0; double theta = 0; int glide = 1; };

    // ---------- FIR-Entwurf ----------
    // Zielbetrag: ab ca. 22 Hz voll, bis zum Split voll, dann weicher Übergang (Split = ca. -6 dB)
    static double shape (double f, double split)
    {
        auto rc = [] (double x) { x = std::clamp (x, 0.0, 1.0); return 0.5 - 0.5 * std::cos (pi * x); };
        const double low = rc ((f - 8.0) / (22.0 - 8.0));
        if (split >= kFullHz - 0.5) return low * (1.0 - rc ((f - 18000.0) / 3000.0));
        const double f1 = split / 1.3, f2 = split * 1.3;
        return low * (1.0 - rc ((f - f1) / (f2 - f1)));
    }

    void designFIR (double split, std::vector<cd>& outI, std::vector<cd>& outQ)
    {
        std::vector<cd> si ((size_t) irLen), sq ((size_t) irLen);
        for (int kk = 0; kk <= irLen / 2; ++kk)
        {
            const double f = (double) kk * fs / irLen;
            const double a = shape (f, split);
            si[(size_t) kk] = a;
            sq[(size_t) kk] = (kk == 0 || kk == irLen / 2) ? cd {} : cd (0.0, -a);   // Hilbert: -j
            if (kk > 0 && kk < irLen / 2)
            {
                si[(size_t) (irLen - kk)] = std::conj (si[(size_t) kk]);
                sq[(size_t) (irLen - kk)] = std::conj (sq[(size_t) kk]);
            }
        }
        fftDesign.inverse (si.data());
        fftDesign.inverse (sq.data());

        // nullphasig -> um irLen/2 verschieben, Blackman-Harris-Fenster (sehr wenig Leck in die Höhen)
        std::vector<double> hi ((size_t) irLen), hq ((size_t) irLen);
        for (int nn = 0; nn < irLen; ++nn)
        {
            const int src = (nn - irLen / 2 + irLen) % irLen;
            const double x = (double) nn / irLen;
            const double wnd = 0.35875 - 0.48829 * std::cos (2.0 * pi * x) + 0.14128 * std::cos (4.0 * pi * x)
                             - 0.01168 * std::cos (6.0 * pi * x);
            hi[(size_t) nn] = si[(size_t) src].real() * wnd;
            hq[(size_t) nn] = sq[(size_t) src].real() * wnd;
        }

        for (int pp = 0; pp < kParts; ++pp)
        {
            for (auto* src : { &hi, &hq })
            {
                std::fill (work.begin(), work.end(), cd {});
                for (int j = 0; j < part; ++j) work[(size_t) j] = (*src)[(size_t) (pp * part + j)];
                fftBlock.forward (work.data());
                auto& dst = src == &hi ? outI : outQ;
                for (int b = 0; b < bins; ++b) dst[(size_t) (pp * bins + b)] = work[(size_t) b];
            }
        }
    }

    // ein Block ist voll: FFT, mit allen Partitionen falten, I/Q in die Ringe schreiben
    void runBlock (int nch, float split)
    {
        // Split geändert? Neue Filter entwerfen und über diesen Block überblenden
        bool doFade = false;
        if (std::abs (split - designedSplit) > 0.5f && ++designWait >= 3)
        {
            designFIR (split, hInew, hQnew);
            designedSplit = split;
            designWait = 0;
            doFade = true;
        }

        const long long blockStartInput = t - (part - 1);           // Eingangszeit des ersten Samples im Block
        const long long alignedStart = blockStartInput - irLen / 2; // FIR-Verzögerung herausrechnen

        auto convolve = [&] (ConvState& cs, bool wantQ, std::vector<double>& outI, std::vector<double>& outQ, bool fade)
        {
            for (int j = 0; j < nfft; ++j) work[(size_t) j] = cs.in[(size_t) j];
            fftBlock.forward (work.data());
            cs.head = (cs.head + kParts - 1) % kParts;
            std::copy (work.begin(), work.begin() + bins, cs.fdl.begin() + cs.head * bins);

            auto accumulate = [&] (const std::vector<cd>& H, std::vector<cd>& acc)
            {
                std::fill (acc.begin(), acc.end(), cd {});
                for (int pp = 0; pp < kParts; ++pp)
                {
                    const cd* X = cs.fdl.data() + ((cs.head + pp) % kParts) * bins;
                    const cd* h = H.data() + pp * bins;
                    for (int b = 0; b < bins; ++b) acc[(size_t) b] += X[b] * h[b];
                }
            };
            auto toTime = [&] (const std::vector<cd>& acc, std::vector<double>& out)
            {
                for (int b = 0; b < bins; ++b) work2[(size_t) b] = acc[(size_t) b];
                for (int b = 1; b < bins - 1; ++b) work2[(size_t) (nfft - b)] = std::conj (acc[(size_t) b]);
                fftBlock.inverse (work2.data());
                for (int j = 0; j < part; ++j) out[(size_t) j] = work2[(size_t) (part + j)].real();
            };

            accumulate (hI, accI); toTime (accI, outI);
            if (wantQ) { accumulate (hQ, accQ); toTime (accQ, outQ); }
            if (fade)
            {
                accumulate (hInew, accI); toTime (accI, tmpI);
                if (wantQ) { accumulate (hQnew, accQ); toTime (accQ, tmpQ); }
                for (int j = 0; j < part; ++j)
                {
                    const double g = (j + 0.5) / part;
                    outI[(size_t) j] += (tmpI[(size_t) j] - outI[(size_t) j]) * g;
                    if (wantQ) outQ[(size_t) j] += (tmpQ[(size_t) j] - outQ[(size_t) j]) * g;
                }
            }

            // aktueller Block wird zum vorherigen
            std::copy (cs.in.begin() + part, cs.in.end(), cs.in.begin());
        };

        if ((int) blkI.size() != part)
        {
            blkI.assign ((size_t) part, 0.0); blkQ.assign ((size_t) part, 0.0);
            tmpI.assign ((size_t) part, 0.0); tmpQ.assign ((size_t) part, 0.0);
        }

        for (int c = 0; c < nch; ++c)
        {
            convolve (conv[c], true, blkI, blkQ, doFade);
            for (int j = 0; j < part; ++j)
            {
                const size_t r = (size_t) ((alignedStart + j) & mask);
                iRing[c][r] = (float) blkI[(size_t) j];
                qRing[c][r] = (float) blkQ[(size_t) j];
            }
        }
        convolve (kickConv, false, blkI, blkQ, doFade);
        for (int j = 0; j < part; ++j)
            kfRing[(size_t) ((alignedStart + j) & mask)] = (float) blkI[(size_t) j];

        if (doFade) { hI.swap (hInew); hQ.swap (hQnew); }
    }

    // ---------- Analyse-Filter ----------
    void setupHilbertIIR()
    {
        // per Optimierung entworfen: 90 Grad Differenz zwischen den Ketten von ca. 15 Hz bis 2.5 kHz
        static constexpr double qPoles[6] { 3.253315, 22.964646, 80.568962, 270.646641, 917.805710, 3632.504083 };
        static constexpr double iPoles[6] { 11.011687, 43.582204, 147.794168, 496.469042, 1741.807646, 12295.149883 };
        auto coefFor = [this] (double fp)
        {
            fp = std::min (fp, fs * 0.45);
            const double K = 2.0 * fs, pp = 2.0 * fs * std::tan (pi * fp / fs);
            return (pp - K) / (pp + K);
        };
        for (size_t j = 0; j < 6; ++j)
        {
            anaB.c[j] = kB.c[j] = coefFor (iPoles[j]);
            anaA.c[j] = coefFor (qPoles[j]);
        }
    }

    void setupAnalysisFilters (float fc)
    {
        const double f = std::clamp ((double) fc, 30.0, fs * 0.2);
        anaLp.set (f, fs); kLp.set (f, fs);
        anaLa.set (kAnalysisHz, fs); anaLaQ.set (kAnalysisHz, fs); kLa.set (kAnalysisHz, fs);
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
        if (std::abs (glideTo) > 4.0 * pi)
        {
            const double shift = 2.0 * pi * std::round (glideTo / (2.0 * pi));
            glideFrom -= shift; glideTo -= shift; cur -= shift;
        }
        if (std::abs (glideTo - glideFrom) < 1.0e-9 && std::abs (wrap (glideTo)) < 1.0e-9)
        {
            cur = glideFrom = glideTo = 0.0;   // exakt 0 halten -> bit-genauer Durchgang
            glidePos = 1.0;
            return;
        }
        glidePos = 0.0;
        glideStep = 1.0 / (double) std::max (1, samples);
    }

    void analyse (const AlignParams& p, int glide)
    {
        double kk = 0, ii = 0, qq = 0, ki = 0, kq = 0, iq = 0;
        const long long len = windowEnd - windowStart;
        for (long long s = windowStart; s < windowEnd; ++s)
        {
            const size_t r = (size_t) (s & mask);
            const double k = kaRing[r], I = iaRing[r], Q = qaRing[r];
            kk += k * k; ii += I * I; qq += Q * Q;
            ki += k * I; kq += k * Q; iq += I * Q;
        }

        const double nn = (double) std::max<long long> (1, len);
        const double rmsK = std::sqrt (kk / nn), rmsB = std::sqrt ((ii + qq) * 0.5 / nn);
        if (rmsK < 1.0e-4 || rmsB < 1.0e-4)
            return;

        auto corr = [&] (double th)
        {
            const double c = std::cos (th), s = std::sin (th);
            const double ky = c * ki - s * kq;
            const double yy = c * c * ii + s * s * qq - 2.0 * c * s * iq;
            return ky / std::sqrt (kk * yy + 1.0e-30);
        };

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

        const double curTarget = targetFor (thetaSched, p);
        const double newTarget = targetFor (best, p);
        const bool worth = corr (newTarget) - corr (curTarget) > 0.02;
        const bool apply = worth && ! p.hold;

        corrBefore.store ((float) corr (0.0), std::memory_order_relaxed);
        corrAfter.store ((float) corr (apply ? newTarget : curTarget), std::memory_order_relaxed);
        detectedDeg.store ((float) (best * 180.0 / pi), std::memory_order_relaxed);
        analysisCount.fetch_add (1, std::memory_order_release);

        if (! apply)
            return;

        const int next = (evTail + 1) % kMaxEvents;
        if (next == evHead) return;
        events[(size_t) evTail] = { windowStart - glide, best, glide };
        evTail = next;
        thetaSched = best;
    }

    // ---------- Zustand ----------
    double fs = 48000.0;
    int irLen = 8192, part = 512, nfft = 1024, bins = 513;
    int firDelay = 0, latency = 0, size = 1, mask = 0;
    long long t = 0;
    int blockFill = 0, designWait = 0;

    SimpleFFT fftBlock, fftDesign;
    std::array<ConvState, 2> conv;
    ConvState kickConv;
    std::vector<cd> hI, hQ, hInew, hQnew, work, work2, accI, accQ;
    std::vector<double> blkI, blkQ, tmpI, tmpQ;
    float designedSplit = -1.0f;

    std::array<std::vector<float>, 2> dryRing, iRing, qRing;
    std::vector<float> kfRing, kaRing, iaRing, qaRing;

    LR4 anaLp, anaLa, anaLaQ, kLp, kLa;
    AllpassChain anaB, anaA, kB;

    double fastEnv = 0, slowEnv = 0, kEnvView = 0;
    long long lastOnset = 0, windowStart = 0, windowEnd = -1;

    std::array<Event, kMaxEvents> events {};
    int evHead = 0, evTail = 0;
    std::array<long long, kMaxEvents> onsets {};
    int onHead = 0, onTail = 0;

    double thetaOpt = 0.0, thetaSched = 0.0;
    double cur = 0.0, glideFrom = 0.0, glideTo = 0.0, glidePos = 1.0, glideStep = 1.0;
    float lastSplit = -1.0f, lastAmount = -1.0f, lastOffset = 1.0e9f;
    bool lastBypass = false;

    int histDec = 4, histAcc = 0;
    double histK = 0, histB = 0, histA = 0;
    float histMark = 0.0f;
};

} // namespace bb
