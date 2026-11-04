// Braincut Engine-Tests (ohne JUCE)
// g++ -O2 -std=c++17 -Wall -Wextra -I../Source engine_test.cpp -o engine_test && ./engine_test
// Mit Prüfung auf Speicherfehler: zusätzlich -fsanitize=address,undefined -g
#include "dsp/BraincutEngine.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace bc;

static const double fs = 48000.0;
static int fails = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (! (cond)) { ++fails; std::printf ("FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } \
        else { std::printf ("ok:   "); std::printf (__VA_ARGS__); std::printf ("\n"); } \
    } while (0)

struct Stereo { std::vector<float> l, r; };

static Stereo noise (int n, float amp, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-amp, amp);
    Stereo s; s.l.resize ((size_t) n); s.r.resize ((size_t) n);
    for (int i = 0; i < n; ++i) { s.l[(size_t) i] = d (rng); s.r[(size_t) i] = d (rng); }
    return s;
}

// Wiederholt ein Stück Rauschen mit der Länge "period" Samples
static Stereo loopNoise (int n, int period, float amp, unsigned seed = 3)
{
    Stereo base = noise (period, amp, seed), s;
    s.l.resize ((size_t) n); s.r.resize ((size_t) n);
    for (int i = 0; i < n; ++i) { s.l[(size_t) i] = base.l[(size_t) (i % period)]; s.r[(size_t) i] = base.r[(size_t) (i % period)]; }
    return s;
}

static Stereo sine (int n, double hz, float amp)
{
    Stereo s; s.l.resize ((size_t) n); s.r.resize ((size_t) n);
    for (int i = 0; i < n; ++i) s.l[(size_t) i] = s.r[(size_t) i] = amp * (float) std::sin (2.0 * 3.14159265358979 * hz * i / fs);
    return s;
}

// Verarbeitet das Signal blockweise bei fester Geschwindigkeit
static Stereo run (BraincutEngine& e, EngineParams p, Stereo in, double bpm = 120.0, int blk = 256, double ppq0 = 0.0)
{
    double ppq = ppq0;
    for (size_t s = 0; s < in.l.size(); s += (size_t) blk)
    {
        const int n = (int) std::min ((size_t) blk, in.l.size() - s);
        Transport tr; tr.ppq = ppq; tr.bpm = bpm; tr.playing = true;
        e.process (in.l.data() + s, in.r.data() + s, n, tr, p);
        ppq += n * bpm / 60.0 / fs;
    }
    return in;
}

// Alles aus, Dichte NUTS (= Effekte dürfen überall arbeiten, wie vor dem Dichte-Schalter)
static EngineParams allOff()
{
    EngineParams p;
    p.density = kNuts;
    p.stretch.on = false; p.repeat.on = false; p.glitch.on = false;
    p.slicer.repeatProb = 0.0f;
    return p;
}

static bool finiteAll (const Stereo& s, float limit = 16.0f)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]) || std::abs (s.l[i]) > limit || std::abs (s.r[i]) > limit) return false;
    return true;
}
static double rms (const std::vector<float>& x, size_t from = 0, size_t to = 0)
{
    if (to == 0) to = x.size();
    double e = 0; for (size_t i = from; i < to; ++i) e += (double) x[i] * x[i];
    return std::sqrt (e / (double) std::max<size_t> (1, to - from));
}
static double maxDiff (const std::vector<float>& a, const std::vector<float>& b, size_t from = 0)
{
    double m = 0; for (size_t i = from; i < std::min (a.size(), b.size()); ++i) m = std::max (m, (double) std::abs (a[i] - b[i]));
    return m;
}

// Leistung bei einer Frequenz (Goertzel)
static double power (const std::vector<float>& x, size_t from, size_t n, double hz)
{
    const double w = 2.0 * 3.14159265358979 * hz / fs, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < from + n; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) n;
}

int main()
{
    const int bar = (int) (fs * 2.0);          // 1 Takt bei 120 bpm = 2 s
    const int total = bar * 12;

    // 1) Slicer mit repeatProb 0 = unverändert durchreichen (auch im ersten Durchlauf)
    {
        BraincutEngine e; e.prepare (fs);
        auto in = noise (total, 0.5f);
        auto out = run (e, allOff(), in);
        CHECK (maxDiff (in.l, out.l, 4096) < 1e-4, "Slicer ohne Wiederholung reicht Audio durch (max Abweichung %.2g)", maxDiff (in.l, out.l, 4096));
    }

    // 2) Hohe Wiederholung: Ausgabe verändert, aber endlich und ähnlich laut
    for (int mode = 0; mode < 2; ++mode)
    {
        BraincutEngine e; e.prepare (fs);
        auto p = allOff(); p.slicer.mode = mode; p.slicer.repeatProb = 1.0f;
        auto in = loopNoise (total, bar, 0.5f);
        auto out = run (e, p, in);
        const double ratio = rms (out.l, (size_t) bar * 3) / rms (in.l, (size_t) bar * 3);
        CHECK (finiteAll (out) && maxDiff (in.l, out.l, (size_t) bar * 3) > 0.05, "Modus %d verändert das Signal", mode);
        CHECK (ratio > 0.5 && ratio < 1.5, "Modus %d Pegel bleibt ähnlich (Faktor %.2f)", mode, ratio);
    }

    // 3) Seed: gleicher Seed = gleiche Ausgabe, anderer Seed = andere Ausgabe
    {
        auto p = allOff(); p.slicer.repeatProb = 0.8f; p.slicer.seed = 42;
        auto in = loopNoise (total, bar, 0.5f);
        BraincutEngine e1, e2, e3; e1.prepare (fs); e2.prepare (fs); e3.prepare (fs);
        auto o1 = run (e1, p, in), o2 = run (e2, p, in, 120.0, 100);
        p.slicer.seed = 43;
        auto o3 = run (e3, p, in);
        CHECK (maxDiff (o1.l, o2.l) < 1e-3, "gleicher Seed, andere Blockgröße: gleiches Ergebnis (%.2g)", maxDiff (o1.l, o2.l));
        CHECK (maxDiff (o1.l, o3.l, (size_t) bar * 2) > 0.05, "anderer Seed: anderes Ergebnis");
    }

    // 4) Lock: jede Phrase hat dasselbe Muster
    {
        auto p = allOff(); p.slicer.repeatProb = 0.8f; p.slicer.seed = 7; p.slicer.lock = true; p.slicer.phraseMin = p.slicer.phraseMax = 1;
        auto in = loopNoise (total, bar, 0.5f);
        BraincutEngine e; e.prepare (fs);
        auto out = run (e, p, in);
        double d = 0; for (int k = 4; k < 9; ++k)
            for (int i = 0; i < bar; ++i) d = std::max (d, (double) std::abs (out.l[(size_t) (k * bar + i)] - out.l[(size_t) ((k + 1) * bar + i)]));
        CHECK (d < 1e-3, "Lock an: Phrasen sind identisch (%.2g)", d);

        p.slicer.lock = false;
        BraincutEngine e2; e2.prepare (fs);
        auto out2 = run (e2, p, in);
        double d2 = 0; for (int i = 0; i < bar; ++i) d2 = std::max (d2, (double) std::abs (out2.l[(size_t) (6 * bar + i)] - out2.l[(size_t) (7 * bar + i)]));
        CHECK (d2 > 0.05, "Lock aus: Phrasen unterscheiden sich");
    }

    // 5) Stretcher: Tonhöhe bleibt (Sinus 440 Hz bleibt bei 440 Hz)
    {
        auto p = allOff(); p.stretch.on = true; p.stretch.amount = 1.0f; p.stretch.mix = 1.0f; p.slicer.repeatProb = 0.0f;
        BraincutEngine e; e.prepare (fs);
        auto in = sine (total, 440.0, 0.5f);
        auto out = run (e, p, in);
        const size_t from = (size_t) bar * 3, n = (size_t) bar * 4;
        const double p440 = power (out.l, from, n, 440.0);
        double other = 0; for (double hz : { 330.0, 370.0, 520.0, 590.0, 880.0 }) other = std::max (other, power (out.l, from, n, hz));
        CHECK (finiteAll (out) && p440 > other * 20.0, "Stretcher behält die Tonhöhe (440 Hz: %.4f, stärkste andere: %.5f)", p440, other);
        CHECK (maxDiff (in.l, out.l, from) > 0.02, "Stretcher verändert das Signal");
    }

    // 5b) Schnitte klicken nicht: Überblendung glättet Sprünge zwischen Slices
    {
        auto p = allOff(); p.slicer.repeatProb = 1.0f; p.slicer.mode = 1;
        BraincutEngine e; e.prepare (fs);
        auto in = sine (total, 197.5, 0.5f);
        auto out = run (e, p, in);
        double inJump = 0, outJump = 0;
        for (size_t i = (size_t) bar * 3 + 1; i < out.l.size(); ++i)
        {
            inJump = std::max (inJump, (double) std::abs (in.l[i] - in.l[i - 1]));
            outJump = std::max (outJump, (double) std::abs (out.l[i] - out.l[i - 1]));
        }
        CHECK (outJump < 0.07, "Schnitte sind überblendet (größter Sprung %.3f, Eingang %.3f)", outJump, inJump);
    }

    // 6) Repeater erzeugt Wiederholungen mit Gate-Lücken
    {
        auto p = allOff(); p.repeat.on = true; p.repeat.chance = 1.0f; p.repeat.rateIdx = 2; p.repeat.gate = 0.5f; p.repeat.decay = 0.0f;
        BraincutEngine e; e.prepare (fs);
        auto in = noise (total, 0.5f);
        auto out = run (e, p, in);
        size_t zeros = 0; for (size_t i = (size_t) bar; i < out.l.size(); ++i) if (out.l[i] == 0.0f) ++zeros;
        CHECK (finiteAll (out) && zeros > out.l.size() / 8, "Repeater: Gate lässt Lücken (%zu Nullen)", zeros);
    }

    // 7) Glitcher: jeder Typ einzeln, endlich und verändernd
    {
        const char* names[5] { "Bit", "Rev", "Stop", "Freeze", "Pitch" };
        for (int t = 0; t < 5; ++t)
        {
            auto p = allOff(); p.glitch.on = true; p.glitch.amount = 1.0f; p.glitch.mix = 1.0f;
            p.glitch.bit = (t == 0); p.glitch.rev = (t == 1); p.glitch.stop = (t == 2); p.glitch.freeze = (t == 3); p.glitch.pitch = (t == 4);
            BraincutEngine e; e.prepare (fs);
            auto in = loopNoise (total, bar, 0.5f, 5);
            auto out = run (e, p, in);
            CHECK (finiteAll (out) && maxDiff (in.l, out.l) > 0.05, "Glitch %s: endlich und hörbar", names[t]);
        }
    }

    // 8) Transport steht: unverändert
    {
        BraincutEngine e; e.prepare (fs);
        auto in = noise (4800, 0.5f), out = in;
        Transport tr; tr.playing = false;
        auto p = allOff(); p.slicer.repeatProb = 1.0f;
        e.process (out.l.data(), out.r.data(), 4800, tr, p);
        CHECK (maxDiff (in.l, out.l) < 1e-6, "Transport aus: Signal unverändert");
    }

    // 9) Zufallstest: wilde Parameter, Blockgrößen, Tempowechsel, Sprünge
    {
        std::mt19937 rng (99);
        auto uni = [&] { return std::uniform_real_distribution<float> (0.0f, 1.0f) (rng); };
        bool ok = true; double peak = 0;
        for (int round = 0; round < 12; ++round)
        {
            BraincutEngine e; e.prepare (round % 3 == 0 ? 44100.0 : round % 3 == 1 ? 48000.0 : 96000.0);
            double ppq = 0.0;
            for (int b = 0; b < 400; ++b)
            {
                EngineParams p;
                p.slicer.mode = (int) (uni() * 1.99f); p.slicer.loopIdx = (int) (uni() * 2.99f); p.slicer.phraseMin = 1 + (int) (uni() * 7.99f); p.slicer.phraseMax = 1 + (int) (uni() * 7.99f); p.slicer.repMin = (int) (uni() * 16.99f); p.slicer.repMax = (int) (uni() * 16.99f); p.slicer.subdivIdx = (int) (uni() * 3.99f);
                p.slicer.repeatProb = uni(); p.slicer.seed = (int) (uni() * 9999); p.slicer.lock = uni() < 0.3f;
                p.stretch.on = uni() < 0.7f; p.stretch.amount = uni(); p.stretch.mix = uni();
                p.repeat.on = uni() < 0.7f; p.repeat.chance = uni(); p.repeat.rateIdx = (int) (uni() * 4.99f); p.repeat.gate = 0.1f + 0.9f * uni(); p.repeat.decay = uni();
                p.glitch.on = uni() < 0.7f; p.glitch.amount = uni(); p.glitch.mix = uni();
                p.glitch.bit = uni() < 0.6f; p.glitch.rev = uni() < 0.6f; p.glitch.stop = uni() < 0.6f; p.glitch.freeze = uni() < 0.6f; p.glitch.pitch = uni() < 0.6f;
                p.mix = uni(); p.outDb = -12.0f + 24.0f * uni();
                p.density = (int) (uni() * 2.99f);
                p.slicer.reverse = uni(); p.slicer.jump = uni(); p.slicer.fadeMs = 0.5f + 19.5f * uni();
                p.stretch.chance = uni(); p.stretch.grainMs = 10.0f + 140.0f * uni(); p.stretch.glide = uni();
                p.stretch.dir = (int) (uni() * 2.99f); p.stretch.tape = uni() < 0.4f; p.stretch.musical = uni() < 0.4f;
                Transport tr; tr.bpm = 40.0 + 200.0 * uni(); tr.beatsPerBar = uni() < 0.8f ? 4.0 : 3.5;
                tr.playing = uni() < 0.9f;
                if (uni() < 0.1f) ppq = uni() * 200.0 - 5.0;              // Sprung
                tr.ppq = ppq;
                const int n = 1 + (int) (uni() * 1023.0f);
                std::vector<float> l ((size_t) n), r ((size_t) n);
                for (int i = 0; i < n; ++i) { l[(size_t) i] = (uni() - 0.5f); r[(size_t) i] = (uni() - 0.5f); }
                e.process (l.data(), r.data(), n, tr, p);
                for (int i = 0; i < n; ++i)
                {
                    if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) ok = false;
                    peak = std::max (peak, (double) std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])));
                }
                ppq += n * tr.bpm / 60.0 / 48000.0;
            }
        }
        CHECK (ok && peak < 12.0, "Zufallstest: keine NaN/Inf, Spitze %.2f", peak);
    }

    // 10) Rechenzeit
    {
        EngineParams p; p.slicer.repeatProb = 0.7f; p.stretch.amount = 1.0f; p.repeat.chance = 0.6f; p.glitch.amount = 0.6f;
        BraincutEngine e; e.prepare (fs);
        auto in = noise ((int) fs * 20, 0.5f);
        const auto t0 = std::chrono::steady_clock::now();
        run (e, p, in, 140.0, 512);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("info: 20 s Audio in %.3f s berechnet (%.1f%% einer CPU-Kern-Last)\n", sec, sec / 20.0 * 100.0);
        CHECK (sec / 20.0 < 0.25, "Rechenzeit unkritisch");
    }

    // 11) Dichte-Zonen: DECENT nur am Ende von 4 Takten, MEDIUM am Ende von 2 Takten plus Shots, NUTS immer
    {
        double onD = 0, onM = 0, onN = 0, fillD = 0, shotsM = 0, cntOut = 0;
        const double bpb = 4.0, dt = 1.0 / 64.0;
        bool decentOk = true;
        for (double b = 0.0; b < 64.0 * bpb; b += dt)
        {
            const auto zd = densityZone (kDecent, b + 1e-7, bpb, 5, false);
            const auto zm = densityZone (kMedium, b + 1e-7, bpb, 5, false);
            const auto zn = densityZone (kNuts, b + 1e-7, bpb, 5, false);
            const double inCyc = std::fmod (b, 4.0 * bpb);
            if (zd.on != (inCyc >= 15.0)) decentOk = false;
            onD += zd.on; onN += zn.on; onM += zm.on;
            if (zd.on) fillD += 1;
            const double inCycM = std::fmod (b, 2.0 * bpb);
            if (zm.on && inCycM < 7.0) shotsM += 1;
            ++cntOut;
        }
        CHECK (decentOk, "DECENT: Zone genau auf dem letzten Beat von 4 Takten");
        CHECK (onN == cntOut, "NUTS: Zone immer an");
        const double shotPerCycle = shotsM * dt / kShotBeats / 32.0;
        CHECK (shotPerCycle > 0.4 && shotPerCycle < 2.0, "MEDIUM: einige Shots pro 2 Takte (%.2f)", shotPerCycle);
        CHECK (onD / cntOut < 0.07 && onM / cntOut > onD / cntOut && onM / cntOut < 0.25,
               "Anteil aktiv: DECENT %.1f%%, MEDIUM %.1f%%", 100.0 * onD / cntOut, 100.0 * onM / cntOut);

        // Lock: Shots liegen in jeder Runde gleich, ohne Lock nicht
        bool same = true, diff = false;
        for (double b = 0.0; b < 8.0; b += dt)
        {
            const bool a = densityZone (kMedium, b + 1e-7, bpb, 9, true).on;
            for (int c = 1; c < 6; ++c)
                if (densityZone (kMedium, b + 8.0 * c + 1e-7, bpb, 9, true).on != a) same = false;
            if (densityZone (kMedium, b + 1e-7, bpb, 9, false).on != densityZone (kMedium, b + 8.0 * 3 + 1e-7, bpb, 9, false).on) diff = true;
        }
        CHECK (same, "MEDIUM mit Lock: Shots in jeder Runde an derselben Stelle");
        CHECK (diff, "MEDIUM ohne Lock: Shots wandern");
        CHECK (zoneBoost (kMedium, 0.0f) == 0.0f && zoneBoost (kMedium, 0.3f) > 0.6f && zoneBoost (kNuts, 0.3f) == 0.3f,
               "Boost: 0 bleibt 0, in Zonen mehr, NUTS unverändert");
    }

    // 12) DECENT in der Engine: außerhalb des Fills exakt das Original, im Fill passiert etwas
    for (int dens = 0; dens < 2; ++dens)
    {
        EngineParams p; p.density = dens; p.slicer.repeatProb = 1.0f; p.repeat.chance = 1.0f; p.glitch.amount = 1.0f;
        p.stretch.chance = 1.0f; p.stretch.amount = 1.0f;
        BraincutEngine e; e.prepare (fs);
        auto in = loopNoise (bar * 16, bar, 0.5f);
        auto out = run (e, p, in);
        const double spbS = fs * 0.5;
        double outside = 0, inside = 0;
        for (size_t i = (size_t) bar * 2; i < out.l.size(); ++i)
        {
            const double beat = (double) i / spbS;
            const auto z = densityZone (dens, beat + 1e-7, 4.0, p.slicer.seed, p.slicer.lock);
            // kurz nach dem Ende einer Zone wird noch überblendet
            const auto zBefore = densityZone (dens, beat - 0.03 + 1e-7, 4.0, p.slicer.seed, p.slicer.lock);
            const double d = std::abs (out.l[i] - in.l[i]);
            if (z.on) inside = std::max (inside, d);
            else if (! zBefore.on) outside = std::max (outside, d);
        }
        CHECK (outside < 1e-4, "%s: außerhalb der Zonen unverändert (%.2g)", dens == 0 ? "DECENT" : "MEDIUM", outside);
        CHECK (inside > 0.1 && finiteAll (out), "%s: in den Zonen wird umgebaut (%.2f)", dens == 0 ? "DECENT" : "MEDIUM", inside);
    }

    // 13) Muster: REVERSE 0 = nie rückwärts, 1 = oft; JUMP 0 = nie aus anderen Beats (CUT)
    {
        int revOff = 0, revOn = 0, jumpOff = 0, jumpOn = 0, warpRevOff = 0;
        for (int k = 0; k < 200; ++k)
        {
            Pattern P; GenParams g; g.N = 64; g.M = 64; g.sb = 4; g.rp = 1.0f; g.repMin = 0; g.repMax = 3;
            g.rev = 0.0f; g.jump = 0.0f; generateCut (P, g, (uint64_t) k);
            // Sprung = ein Stück beginnt in einem anderen Beat als dem eigenen
            auto jumps = [] (const Pattern& Q) { int j = 0; for (int u = 0; u < Q.count; ++u) if (Q.units[(size_t) u].src / 4 != Q.units[(size_t) u].start / 4) ++j; return j; };
            for (int i = 0; i < 64; ++i) revOff += P.revOfStep[(size_t) i];
            jumpOff += jumps (P);
            g.rev = 1.0f; g.jump = 1.0f; generateCut (P, g, (uint64_t) k);
            for (int i = 0; i < 64; ++i) revOn += P.revOfStep[(size_t) i];
            jumpOn += jumps (P);
            g.rev = 0.0f; generateWarp (P, g, (uint64_t) k);
            for (int i = 0; i < 64; ++i) warpRevOff += P.revOfStep[(size_t) i];
        }
        CHECK (revOff == 0 && warpRevOff == 0 && revOn > 64 * 200 / 2, "REVERSE: 0 = nie rückwärts, 1 = meistens (%d)", revOn);
        CHECK (jumpOff == 0 && jumpOn > 16 * 200 / 5, "JUMP: 0 = bleibt im Beat, 1 = springt (%d)", jumpOn);
    }

    // 14) Streckfaktoren: Richtung und Raster
    {
        bool slowOk = true, fastOk = true, gridOk = true, bothOk = false, zeroOk = true;
        for (int k = 0; k <= 200; ++k)
        {
            const float vr = -1.0f + 2.0f * (float) k / 200.0f;
            for (float a : { 0.1f, 0.4f, 0.7f, 1.0f })
            {
                if (stretchFactor (vr, a, kDirSlow, false) > 1.0 + 1e-9) slowOk = false;
                if (stretchFactor (vr, a, kDirFast, false) < 1.0 - 1e-9) fastOk = false;
                const double m = stretchFactor (vr, a, kDirBoth, true);
                bool inSet = false;
                for (double f : { 0.5, 2.0 / 3.0, 0.75, 4.0 / 3.0, 1.5, 2.0 }) if (std::abs (m - f) < 1e-9) inSet = true;
                if (! inSet) gridOk = false;
                if (stretchFactor (vr, a, kDirBoth, false) < 0.9 && stretchFactor (-vr, a, kDirBoth, false) > 1.1) bothOk = true;
            }
            if (stretchFactor (vr, 0.0f, kDirBoth, true) != 1.0) zeroOk = false;
        }
        CHECK (slowOk && fastOk && bothOk, "Richtung: SLOW nur langsamer, FAST nur schneller, BOTH beides");
        CHECK (gridOk && zeroOk, "MUSIC: nur Faktoren 1/2, 2/3, 3/4, 4/3, 3/2, 2; Amount 0 = keine Streckung");
        CHECK (stretchFactor (1.0f, 0.2f, kDirFast, true) == 4.0 / 3.0 && stretchFactor (1.0f, 1.0f, kDirFast, true) == 2.0,
               "MUSIC: kleiner Amount = kleine Schritte, voller Amount bis Faktor 2");
    }

    // 15) KEEP behält die Tonhöhe auch mit großem Grain und Glide, TAPE verschiebt sie
    {
        auto runSine = [&] (bool tape, float grainMs, float glide)
        {
            auto p = allOff(); p.stretch.on = true; p.stretch.amount = 1.0f; p.stretch.chance = 1.0f; p.stretch.mix = 1.0f;
            p.stretch.tape = tape; p.stretch.grainMs = grainMs; p.stretch.glide = glide; p.stretch.dir = kDirSlow; p.stretch.musical = true;
            BraincutEngine e; e.prepare (fs);
            return run (e, p, sine (total, 440.0, 0.5f));
        };
        const size_t from = (size_t) bar * 3, n = (size_t) bar * 4;
        auto keep = runSine (false, 120.0f, 0.5f);
        double other = 0; for (double hz : { 220.0, 293.3, 330.0, 370.0, 520.0 }) other = std::max (other, power (keep.l, from, n, hz));
        CHECK (finiteAll (keep) && power (keep.l, from, n, 440.0) > other * 20.0, "KEEP mit Grain 120 ms und Glide: Tonhöhe bleibt");
        auto tape = runSine (true, 30.0f, 0.0f);
        double low = 0; for (double hz : { 220.0, 293.3, 330.0 }) low = std::max (low, power (tape.l, from, n, hz));
        CHECK (finiteAll (tape) && low > power (tape.l, from, n, 440.0) * 0.2, "TAPE: Tonhöhe sinkt mit dem Tempo (tief %.4f)", low);
    }

    // 16) SMOOTH: längere Überblendung = kleinere Sprünge an Schnitten
    {
        auto jumpWith = [&] (float ms)
        {
            auto p = allOff(); p.slicer.repeatProb = 1.0f; p.slicer.mode = 1; p.slicer.fadeMs = ms;
            BraincutEngine e; e.prepare (fs);
            auto out = run (e, p, sine (total, 197.5, 0.5f));
            double j = 0; for (size_t i = (size_t) bar * 3 + 1; i < out.l.size(); ++i) j = std::max (j, (double) std::abs (out.l[i] - out.l[i - 1]));
            return j;
        };
        const double hard = jumpWith (0.5f), soft = jumpWith (20.0f);
        CHECK (soft < hard && soft < 0.03, "SMOOTH: 20 ms glatter als 0,5 ms (%.3f gegen %.3f)", soft, hard);
    }

    // 17) Anzeige-Raster: bei DECENT tragen nur die Felder im Fill (letzter Beat) das Zonen-Merkmal
    {
        EngineParams p; p.density = kDecent; p.slicer.repeatProb = 1.0f;
        BraincutEngine e; e.prepare (fs);
        run (e, p, loopNoise (bar * 9, bar, 0.5f));
        const int cols = e.vizCols.load();
        bool ok = cols == 16;
        int zoneCells = 0, played = 0;
        for (int c = 0; c < cols * 4; ++c)
        {
            const uint8_t f = e.vizCellFlags[(size_t) c].load();
            if (f & BraincutEngine::cfPlayed) ++played;
            const bool z = (f & BraincutEngine::cfZone) != 0;
            if (z) ++zoneCells;
            if (z != (c >= cols * 4 - 4)) ok = false;
        }
        CHECK (ok && zoneCells == 4 && played == cols * 4, "Raster: 64 Felder, nur die letzten 4 (letzter Beat) in der Zone");
    }

    std::printf (fails == 0 ? "\nALLE TESTS BESTANDEN\n" : "\n%d TEST(S) FEHLGESCHLAGEN\n", fails);
    return fails == 0 ? 0 : 1;
}
