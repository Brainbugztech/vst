// Brainfuzz Engine-Tests (ohne JUCE)
// g++ -O2 -std=c++17 -I../Source engine_test.cpp -o engine_test && ./engine_test
#include "dsp/BrainFuzzEngine.h"
#include <cstdio>
#include <random>
#include <chrono>

using namespace bfz;

static const double fs = 48000.0;
static const int blk = 256;

struct Stereo { std::vector<float> l, r; };

static Stereo run (BrainFuzzEngine& e, const EngineParams& p, Stereo in)
{
    for (size_t s = 0; s < in.l.size(); s += blk)
    {
        const int n = (int) std::min ((size_t) blk, in.l.size() - s);
        float* ch[2] { in.l.data() + s, in.r.data() + s };
        e.process (ch, n, p);
    }
    return in;
}

static Stereo noise (int n, float amp, unsigned seed = 1, bool mono = false)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> d (0.0f, amp);
    Stereo s; s.l.resize ((size_t) n); s.r.resize ((size_t) n);
    // leicht rosa (einfacher Tiefpass), damit es musikalischer verteilt ist
    float bl = 0, br = 0;
    for (int i = 0; i < n; ++i)
    {
        bl = 0.97f * bl + 0.03f * d (rng) * 6.0f; br = 0.97f * br + 0.03f * d (rng) * 6.0f;
        const float a = d (rng) * 0.4f + bl, b = mono ? a : d (rng) * 0.4f + br;
        s.l[(size_t) i] = a; s.r[(size_t) i] = b;
    }
    return s;
}

static double rms (const std::vector<float>& x, size_t from = 0)
{
    double e = 0; for (size_t i = from; i < x.size(); ++i) e += (double) x[i] * x[i];
    return std::sqrt (e / std::max<size_t> (1, x.size() - from));
}
static double db (double v) { return 20.0 * std::log10 (std::max (v, 1e-12)); }

// Energie unterhalb einer Frequenz (einfacher Tiefpass 2. Ordnung mehrfach)
static double lowEnergyDb (const std::vector<float>& x, double fc)
{
    auto f = bilinear (butterworth (8), fc, fs);
    SosRunner<1> r; r.f = f;
    double e = 0; for (size_t i = 4800; i < x.size(); ++i) { const double y = r.process (0, x[i]); e += y * y; }
    return db (std::sqrt (e / (x.size() - 4800)));
}

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { printf ("%s %s\n", ok ? "[ OK ]" : "[FAIL]", what); if (! ok) ++fails; };

    EngineParams base;
    for (auto& b : base.band) { b.driveDb = 0; b.width = 1; b.spread = 0; }
    base.limit = false;

    // 1) Latenz: Impuls mit Drive 0 / LR -> Summe = Allpass, Spitze kurz nach 28 Samples
    {
        BrainFuzzEngine e; e.prepare (fs, blk);
        Stereo s; s.l.assign (4096, 0.0f); s.r.assign (4096, 0.0f); s.l[0] = s.r[0] = 1.0f;
        auto o = run (e, base, s);
        double en = 0; for (auto v : o.l) en += v * v;
        int first = 0; for (int i = 0; i < 4096; ++i) if (std::abs (o.l[(size_t) i]) > 1e-4) { first = i; break; }
        printf ("   Impuls: erste Antwort bei %d, Energie %.4f\n", first, en);
        check (first == BrainFuzzEngine::kLatency, "Latenz = 28 Samples");
        check (std::abs (en - 1.0) < 0.01, "Drive 0 + LR: Summe energie-erhaltend (Allpass)");
    }

    // 2) Mix 0 und Mix 1 bei Drive 0 identisch (Dry ist phasengleich)
    {
        BrainFuzzEngine e1, e2; e1.prepare (fs, blk); e2.prepare (fs, blk);
        auto in = noise (48000, 0.1f);
        auto p0 = base; p0.mix = 0.0f;
        auto a = run (e1, base, in), b = run (e2, p0, in);
        double d = 0; for (size_t i = 0; i < a.l.size(); ++i) d = std::max (d, (double) std::abs (a.l[i] - b.l[i]));
        printf ("   max. Unterschied Mix 0/1: %.2e\n", d);
        check (d < 1e-4, "Dry-Weg phasengleich zum Wet-Weg");
    }

    // 3) Mono-Sicherheit: Mono-Quelle, Spread 100 %, Width 200 % -> L+R unveraendert
    {
        BrainFuzzEngine e1, e2; e1.prepare (fs, blk); e2.prepare (fs, blk);
        auto in = noise (48000, 0.1f, 3, true);
        auto pw = base; for (auto& b : pw.band) { b.spread = 1.0f; b.width = 2.0f; }
        auto a = run (e1, base, in), b = run (e2, pw, in);
        double d = 0, side = 0;
        for (size_t i = 0; i < a.l.size(); ++i)
        {
            d = std::max (d, (double) std::abs ((a.l[i] + a.r[i]) - (b.l[i] + b.r[i])));
            side += (b.l[i] - b.r[i]) * (b.l[i] - b.r[i]);
        }
        printf ("   Mono-Summe Unterschied %.2e, Seiten-Pegel %.1f dB\n", d, db (std::sqrt (side / a.l.size())));
        check (d < 1e-4, "Spread + Width sind mono-sicher, auch bei Mono-Quellen");
        check (side > 1.0, "Spread erzeugt Breite auf Mono-Quelle");
    }

    // 4) Tight: High-Band hart verzerrt -> Anteile unter 100 Hz mit/ohne Tight HP
    {
        auto in = noise (96000, 0.15f, 5);
        auto p = base;
        p.xover = { 200.0f, 1500.0f, 6000.0f };
        p.satType = (int) SatType::splat; p.bite = 1.0f;
        for (int b = 0; b < 3; ++b) p.band[(size_t) b].mute = true;
        p.band[3].driveDb = 36.0f;
        BrainFuzzEngine e1, e2; e1.prepare (fs, blk); e2.prepare (fs, blk);
        auto a = run (e1, p, in);
        auto pt = p; pt.tightHp = true; pt.tightSlope = 1;
        auto b = run (e2, pt, in);
        const double la = lowEnergyDb (a.l, 100.0), lb = lowEnergyDb (b.l, 100.0);
        printf ("   High-Band verzerrt, Anteil < 100 Hz: ohne Tight %.1f dB, mit Tight %.1f dB\n", la, lb);
        check (lb < la - 20.0, "Tight HP entfernt Verzerrungs-Anteile unterhalb des Bandes");
    }

    // 5) Tight aendert bei sauberem Signal nichts (Uebergaenge bleiben gleich)
    {
        auto in = noise (48000, 0.05f, 7);
        auto pt = base; pt.tightHp = pt.tightLp = true;
        for (auto& b : pt.band) b.driveDb = 0.0f;
        BrainFuzzEngine e1, e2; e1.prepare (fs, blk); e2.prepare (fs, blk);
        auto a = run (e1, base, in), b = run (e2, pt, in);
        double d = 0; for (size_t i = 0; i < a.l.size(); ++i) d = std::max (d, (double) std::abs (a.l[i] - b.l[i]));
        check (d < 1e-4, "Tight laesst das saubere Band unangetastet");
    }

    // 6) Pegel ueber Drive pro Typ (zum Abstimmen der Teil-Kompensation)
    {
        printf ("   Pegel (dB relativ zu Drive 0), Rauschen -20 dBFS:\n          ");
        for (float d : { 6.f, 12.f, 18.f, 24.f, 36.f }) printf ("%6.0f", d);
        printf ("\n");
        auto in = noise (48000, 0.1f, 9);
        const double ref = rms (in.l, 4800);
        for (int t = 0; t < 7; ++t)
        {
            printf ("   %-7s", satTypeName (t));
            for (float d : { 6.f, 12.f, 18.f, 24.f, 36.f })
            {
                auto p = base; p.satType = t; p.bite = 0.5f;
                for (auto& b : p.band) b.driveDb = d;
                BrainFuzzEngine e; e.prepare (fs, blk);
                auto o = run (e, p, in);
                printf ("%+6.1f", db (rms (o.l, 4800) / ref));
            }
            printf ("\n");
        }
    }

    // 7) Alle Effekte an, alle Typen: keine NaN, Pegel im Rahmen
    {
        auto in = noise (96000, 0.2f, 11);
        for (int xt = 0; xt < 8; ++xt)
            for (int sl = 0; sl < 3; ++sl)
            {
                auto p = base; p.xoverType = xt; p.slope = sl; p.limit = true;
                p.tightHp = p.tightLp = true; p.tightSlope = sl % 2;
                for (int b = 0; b < 4; ++b)
                {
                    auto& bp = p.band[(size_t) b];
                    bp.driveDb = 24; bp.spread = 0.7f;
                    bp.crushSlot = { true, b % 2, b % 3 };
                    bp.chorusSlot = { true, (b + 1) % 2, (b + 1) % 3 };
                    bp.verbSlot = { true, posPost, (b + 2) % 3 };
                    bp.verb.length = 6.0f; bp.verb.mix = 0.6f;
                }
                BrainFuzzEngine e; e.prepare (fs, blk);
                auto o = run (e, p, in);
                double pk = 0; bool finite = true;
                for (size_t i = 0; i < o.l.size(); ++i) { finite = finite && std::isfinite (o.l[i]) && std::isfinite (o.r[i]); pk = std::max (pk, (double) std::abs (o.l[i])); }
                if (! finite || pk > 1.0) { printf ("   Typ %d Steilheit %d: finite %d peak %.2f\n", xt, sl, finite, pk); ++fails; }
            }
        check (fails == 0, "Alle Weichen-Typen + alle FX: stabil, Limiter haelt -0.1 dBFS");
    }

    // 8) CPU: 4 Baender, alle FX auf Band + Breite
    {
        auto in = noise (48000 * 10, 0.2f, 13);
        auto p = base;
        for (auto& bp : p.band)
        {
            bp.driveDb = 18; bp.spread = 0.5f;
            bp.crushSlot = { true, posPre, targetBoth };
            bp.chorusSlot = { true, posPost, targetBoth };
            bp.verbSlot = { true, posPost, targetBoth };
        }
        p.slope = 2;
        BrainFuzzEngine e; e.prepare (fs, blk);
        const auto t0 = std::chrono::steady_clock::now();
        run (e, p, in);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        printf ("   CPU Volllast: %.2f s fuer 10 s Audio (%.1f %% eines Kerns)\n", sec, sec * 10.0);
    }

    printf (fails ? "\n%d FEHLER\n" : "\nAlles gut.\n", fails);
    return fails ? 1 : 0;
}
