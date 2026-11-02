// Kick Transmuter Engine-Tests (ohne JUCE)
// g++ -O2 -std=c++17 -Wall -Wextra -I../Source -I../../BrainFuzz/Source engine_test.cpp -o engine_test && ./engine_test
// Optional: ./engine_test <ordner> schreibt Vorher/Nachher-Spuren als Rohdaten (float32) für Diagramme
#include "dsp/TransmuterEngine.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

using namespace ktm;

static int fails = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (! (cond)) { ++fails; std::printf ("  FEHLER: "); std::printf (__VA_ARGS__); std::printf ("\n"); } \
        else { std::printf ("  ok: "); std::printf (__VA_ARGS__); std::printf ("\n"); } \
    } while (0)

using Buf = std::vector<float>;
static const double PI = 3.14159265358979323846;

// Psytrance-artige Kick: Klick + Pitch-Sweep 280 -> 50 Hz, exponentieller Ausklang
static Buf makeKick (double sr, double decayMs = 110.0, double lenMs = 400.0, float level = 1.0f, double subHz = 50.0)
{
    const int n = (int) (lenMs * 0.001 * sr);
    Buf b ((size_t) n, 0.0f);
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    double ph = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double f = subHz + 230.0 * std::exp (-t / 0.012);
        ph += 2.0 * PI * f / sr;
        double v = std::sin (ph) * std::exp (-t / (decayMs * 0.001));
        if (t < 0.002) v += 0.5 * d (rng) * (1.0 - t / 0.002);
        b[(size_t) i] = (float) v;
    }
    float peak = 0.0f;
    for (auto v : b) peak = std::max (peak, std::fabs (v));
    for (auto& v : b) v *= level / peak;
    // Sanftes Ende gegen Knacken
    const int fade = std::min (n / 8, (int) (0.005 * sr));
    for (int i = 0; i < fade; ++i) b[(size_t) (n - 1 - i)] *= (float) i / (float) fade;
    return b;
}

// Kick-Muster: count Schläge alle intervalMs, danach optional Pause
static Buf makePattern (double sr, int count, double intervalMs, float level = 1.0f, double pauseMs = 0.0, double decayMs = 110.0)
{
    const Buf k = makeKick (sr, decayMs, std::min (400.0, intervalMs * 0.98), level);
    const int step = (int) (intervalMs * 0.001 * sr);
    Buf b ((size_t) (step * count + (int) (pauseMs * 0.001 * sr)), 0.0f);
    for (int h = 0; h < count; ++h)
        for (size_t i = 0; i < k.size(); ++i)
            b[(size_t) (h * step) + i] += k[i];
    return b;
}

static void run (Engine& e, Buf& l, Buf& r, const Params& p, int block = 256)
{
    int pos = 0, n = (int) l.size();
    while (pos < n)
    {
        const int m = std::min (block, n - pos);
        e.process (l.data() + pos, r.data() + pos, m, p);
        pos += m;
    }
}

static double rmsDb (const Buf& b, int a, int e)
{
    double s = 0.0;
    a = std::max (0, a); e = std::min ((int) b.size(), e);
    for (int i = a; i < e; ++i) s += (double) b[(size_t) i] * b[(size_t) i];
    return 10.0 * std::log10 (s / std::max (1, e - a) + 1.0e-20);
}
static double peakOf (const Buf& b, int a, int e)
{
    float m = 0.0f;
    a = std::max (0, a); e = std::min ((int) b.size(), e);
    for (int i = a; i < e; ++i) m = std::max (m, std::fabs (b[(size_t) i]));
    return (double) m;
}

// Amplitude bei einer Frequenz (Goertzel-artig)
static double toneDb (const Buf& b, double hz, double sr, int a, int e)
{
    double re = 0.0, im = 0.0;
    int n = 0;
    for (int i = a; i < e; ++i, ++n)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * PI * n / (e - a - 1));
        re += w * b[(size_t) i] * std::cos (2.0 * PI * hz * i / sr);
        im += w * b[(size_t) i] * std::sin (2.0 * PI * hz * i / sr);
    }
    return 20.0 * std::log10 (std::sqrt (re * re + im * im) * 4.0 / (e - a) + 1.0e-12);
}

static Buf processMono (double sr, const Buf& in, const Params& p, int block = 256)
{
    Engine e; e.prepare (sr);
    Buf l = in, r = in;
    run (e, l, r, p, block);
    return l;
}

static void testNeutral (double sr)
{
    std::printf ("Neutral bit-genau, %.0f Hz\n", sr);
    const int lat = Engine::latencyFor (sr);
    for (float level : { 1.0f, 0.1f, 3.0f })
        for (int block : { 1, 64, 1024 })
        {
            Buf in = makePattern (sr, 3, 300.0, level, 200.0);
            Engine e; e.prepare (sr);
            Buf l = in, r = in;
            std::mt19937 rng (3); std::uniform_real_distribution<float> d (-0.02f, 0.02f);
            Buf inR = in; for (auto& v : inR) v = v * 0.7f + d (rng);
            r = inR;
            run (e, l, r, Params{}, block);
            int diff = 0;
            for (int i = lat; i < (int) in.size(); ++i)
                if (l[(size_t) i] != in[(size_t) (i - lat)] || r[(size_t) i] != inR[(size_t) (i - lat)]) ++diff;
            CHECK (diff == 0, "Pegel %.1f, Block %d: %d abweichende Samples", level, block, diff);
        }
}

static void testLatency (double sr)
{
    std::printf ("Latenz, %.0f Hz\n", sr);
    const int lat = Engine::latencyFor (sr);
    // Mit voller Sättigung muss der Klick trotzdem an derselben Stelle ankommen
    Params p; p.saturation = 1.0f; p.lowEnd = 0.0f;
    Buf in ((size_t) (sr * 0.2), 0.0f);
    int at = (int) (sr * 0.05);
    for (int i = 0; i < (int) (sr * 0.0004); ++i) in[(size_t) (at + i)] = 0.9f;
    Buf out = processMono (sr, in, p);
    double best = -1.0; int bestLag = 0;
    for (int lag = lat - 40; lag < lat + 40; ++lag)
    {
        double c = 0.0;
        for (int i = at - 20; i < at + (int) (sr * 0.002); ++i) c += (double) in[(size_t) i] * out[(size_t) (i + lag)];
        if (c > best) { best = c; bestLag = lag; }
    }
    CHECK (bestLag == lat, "Klick kommt bei %d Samples an, gemeldet %d (%.2f ms)", bestLag, lat, 1000.0 * lat / sr);
}

static void testAttack (double sr)
{
    std::printf ("Attack, %.0f Hz\n", sr);
    const int lat = Engine::latencyFor (sr);
    Buf in = makeKick (sr);
    auto ms = [&] (double v) { return (int) (v * 0.001 * sr); };

    // Lange Fenster messen ohne den Schlag selbst
    for (float a : { 1.0f, 0.5f, -1.0f })
    {
        Params p; p.attack = a;
        Buf out = processMono (sr, in, p);
        const double d0 = rmsDb (out, lat, lat + ms (5)) - rmsDb (in, 0, ms (5));
        const double dLate = rmsDb (out, lat + ms (100), lat + ms (250)) - rmsDb (in, ms (100), ms (250));
        std::printf ("    Attack %+.0f%%: erste 5 ms %+.1f dB, ab 100 ms %+.2f dB\n", a * 100, d0, dLate);
        if (a > 0) CHECK (d0 > 4.0 * a, "Anschlag lauter");
        else CHECK (d0 < -4.0, "Anschlag leiser");
        CHECK (std::fabs (dLate) < 0.6, "Ausklang bleibt unberührt");
    }

    // Pegelunabhängig (kein Threshold)
    Params p; p.attack = 1.0f;
    double res[3]; int idx = 0;
    for (float lvl : { 0.25f, 0.063f, 0.016f })
    {
        Buf k = makeKick (sr, 110.0, 400.0, lvl);
        Buf out = processMono (sr, k, p);
        res[idx++] = rmsDb (out, lat, lat + ms (5)) - rmsDb (k, 0, ms (5));
    }
    CHECK (std::fabs (res[0] - res[1]) < 0.5 && std::fabs (res[0] - res[2]) < 0.5,
           "Pegelunabhängig: -12 dB %+.2f, -24 dB %+.2f, -36 dB %+.2f", res[0], res[1], res[2]);
}

static void testSustain (double sr)
{
    std::printf ("Sustain, %.0f Hz\n", sr);
    const int lat = Engine::latencyFor (sr);
    Buf in = makeKick (sr);
    auto ms = [&] (double v) { return (int) (v * 0.001 * sr); };
    for (float s : { 1.0f, 0.5f, -1.0f, -0.5f })
    {
        Params p; p.sustain = s;
        Buf out = processMono (sr, in, p);
        const double head = rmsDb (out, lat, lat + ms (10)) - rmsDb (in, 0, ms (10));
        const double tail = rmsDb (out, lat + ms (120), lat + ms (300)) - rmsDb (in, ms (120), ms (300));
        std::printf ("    Sustain %+.0f%%: erste 10 ms %+.2f dB, 120-300 ms %+.1f dB\n", s * 100, head, tail);
        CHECK (std::fabs (head) < 0.3, "Anschlag bleibt");
        CHECK (s > 0 ? tail > 3.0 : tail < -3.0, "Ausklang reagiert");
    }
}

// Reine Lautstärke-Änderung darf bei einem stehenden Bass-Ton nichts verzerren
static void testNoRipple (double sr)
{
    std::printf ("Kein Flattern im Bass, %.0f Hz\n", sr);
    for (double hz : { 30.0, 40.0, 55.0, 80.0 })
    {
        Buf in ((size_t) (sr * 2.0));
        for (int i = 0; i < (int) in.size(); ++i) in[(size_t) i] = (float) (0.7 * std::sin (2.0 * PI * hz * i / sr));
        Params p; p.attack = 1.0f; p.sustain = 1.0f;
        Buf out = processMono (sr, in, p);
        const int a = (int) (sr * 1.0), e = (int) (sr * 2.0);
        const double f0 = toneDb (out, hz, sr, a, e);
        double h = -200.0;
        for (int k = 2; k <= 8; ++k) h = std::max (h, toneDb (out, hz * k, sr, a, e));
        CHECK (h - f0 < -60.0, "%.0f Hz: stärkste Oberwelle %.1f dB unter dem Ton", hz, f0 - h);
    }
}

static void testPunch (double sr)
{
    std::printf ("Punch, %.0f Hz\n", sr);
    const int lat = Engine::latencyFor (sr);
    auto ms = [&] (double v) { return (int) (v * 0.001 * sr); };
    Buf in = makeKick (sr);
    Params p; p.punch = 1.0f;
    Buf out = processMono (sr, in, p);
    const double early = rmsDb (out, lat, lat + ms (40)) - rmsDb (in, 0, ms (40));
    const double late = rmsDb (out, lat + ms (150), lat + ms (300)) - rmsDb (in, ms (150), ms (300));
    std::printf ("    erste 40 ms %+.1f dB, ab 150 ms %+.2f dB\n", early, late);
    CHECK (early > 1.5, "Punch hebt den Schlag an");
    CHECK (std::fabs (late) < 0.5, "Ausklang bleibt");
}

static void testLowEnd (double sr)
{
    std::printf ("Low End, %.0f Hz\n", sr);
    // Stereo-Sub + Stereo-Höhen
    const int n = (int) (sr * 1.5);
    Buf l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double sub = 0.3 * std::sin (2 * PI * 50 * t), subS = 0.2 * std::sin (2 * PI * 50 * t + 1.0);
        const double hi = 0.1 * std::sin (2 * PI * 2000 * t), hiS = 0.1 * std::sin (2 * PI * 2000 * t + 2.0);
        l[(size_t) i] = (float) (sub + subS + hi + hiS);
        r[(size_t) i] = (float) (sub - subS + hi - hiS);
    }
    Buf l0 = l, r0 = r;
    Engine e; e.prepare (sr);
    Params p; p.lowEnd = 1.0f;
    run (e, l, r, p);
    const int a = (int) (sr * 0.7);
    auto side = [&] (const Buf& x, const Buf& y) { Buf s ((size_t) n); for (int i = 0; i < n; ++i) s[(size_t) i] = 0.5f * (x[(size_t) i] - y[(size_t) i]); return s; };
    auto mid = [&] (const Buf& x, const Buf& y) { Buf s ((size_t) n); for (int i = 0; i < n; ++i) s[(size_t) i] = 0.5f * (x[(size_t) i] + y[(size_t) i]); return s; };
    const double sub0 = toneDb (side (l0, r0), 50, sr, a, n), sub1 = toneDb (side (l, r), 50, sr, a, n);
    const double hi0 = toneDb (side (l0, r0), 2000, sr, a, n), hi1 = toneDb (side (l, r), 2000, sr, a, n);
    const double m0 = toneDb (mid (l0, r0), 50, sr, a, n), m1 = toneDb (mid (l, r), 50, sr, a, n);
    const double h0 = toneDb (mid (l0, r0), 2000, sr, a, n), h1 = toneDb (mid (l, r), 2000, sr, a, n);
    CHECK (sub1 - sub0 < -24.0, "Sub wird mono: Seite bei 50 Hz %.1f dB", sub1 - sub0);
    CHECK (std::fabs (hi1 - hi0) < 0.1, "Stereo in den Höhen bleibt: %+.2f dB", hi1 - hi0);
    CHECK (m1 - m0 > 6.0, "Sub-Anhebung: %+.1f dB bei 50 Hz", m1 - m0);
    CHECK (std::fabs (h1 - h0) < 0.1, "Höhen bleiben: %+.2f dB", h1 - h0);
}

static void testSaturation (double sr)
{
    std::printf ("Saturation, %.0f Hz\n", sr);
    const int n = (int) (sr * 1.0);
    const int a = (int) (sr * 0.5);
    // Grundton und Pegelunabhängigkeit
    double h3[2]; int idx = 0;
    for (float lvl : { 0.9f, 0.1f })
    {
        Buf in ((size_t) n);
        for (int i = 0; i < n; ++i) in[(size_t) i] = (float) (lvl * std::sin (2 * PI * 200.0 * i / sr));
        Params p; p.saturation = 0.6f;
        Buf out = processMono (sr, in, p);
        h3[idx++] = toneDb (out, 600, sr, a, n) - toneDb (out, 200, sr, a, n);
    }
    CHECK (std::fabs (h3[0] - h3[1]) < 1.0 && h3[0] > -40, "Gleich dicht bei jedem Pegel: 3. Oberwelle %.1f / %.1f dB", h3[0], h3[1]);

    // Sub-Grundton bleibt stehen
    {
        Buf in ((size_t) n);
        for (int i = 0; i < n; ++i) in[(size_t) i] = (float) (0.9 * std::sin (2 * PI * 45.0 * i / sr));
        Params p; p.saturation = 1.0f;
        Buf out = processMono (sr, in, p);
        const double d = toneDb (out, 45, sr, a, n) - toneDb (in, 45, sr, a, n);
        CHECK (std::fabs (d) < 3.0, "Sub-Grundton bleibt bei voller Sättigung: %+.2f dB", d);
    }

    // Aliasing: 9 kHz-Ton, volle Sättigung, unharmonische Reste suchen
    if (sr < 60000.0)
    {
        Buf in ((size_t) n);
        const double f = 9000.0;
        for (int i = 0; i < n; ++i) in[(size_t) i] = (float) (0.9 * std::sin (2 * PI * f * i / sr));
        Params p; p.saturation = 1.0f;
        Buf out = processMono (sr, in, p);
        const double f0 = toneDb (out, f, sr, a, n);
        double worst = -300.0;
        for (double probe = 300.0; probe < sr * 0.45; probe += 137.0)
        {
            bool harmonic = false;
            for (int k = 1; k <= 3; ++k) if (std::fabs (probe - f * k) < 200.0) harmonic = true;
            if (! harmonic) worst = std::max (worst, toneDb (out, probe, sr, a, n));
        }
        CHECK (worst - f0 < -60.0, "Aliasing-Reste %.1f dB unter dem Ton", f0 - worst);
    }
}

static void testPeakProtection (double sr)
{
    std::printf ("Peak-Schutz und Abfangen, %.0f Hz\n", sr);
    Buf in = makeKick (sr);
    Params p; p.attack = 1.0f; p.punch = 1.0f; p.lowEnd = 1.0f;
    Buf out = processMono (sr, in, p);
    const double pk = peakOf (out, 0, (int) out.size());
    CHECK (pk < tune::ceilingLimit * 1.2, "Alles aufgedreht: Spitze %.2f, Grenze ca. %.1f", pk, (double) tune::ceilingLimit);

    Params a; a.attack = 1.0f;
    Params b = a; b.saturation = 0.7f;
    const double pa = peakOf (processMono (sr, in, a), 0, (int) in.size());
    const double pb = peakOf (processMono (sr, in, b), 0, (int) in.size());
    CHECK (pb < pa * 0.85, "Saturation rundet die Attack-Spitze: %.2f -> %.2f", pa, pb);
}

// K-gewichtete Lautheit wie im Plugin (Hochpass + Shelf vereinfacht: Fensterenergie reicht für den Vergleich)
static double hitLoudness (double sr, const Buf& b, int a, int e)
{
    Svf hp; hp.set (60.0, 0.5, sr);
    double s = 0.0;
    for (int i = std::max (0, a - (int) (sr * 0.05)); i < e; ++i)
    {
        const double v = hp.highpass ((double) b[(size_t) i]);
        if (i >= a) s += v * v;
    }
    return 10.0 * std::log10 (s + 1e-20);
}

static void testAutoGain (double sr)
{
    std::printf ("Auto Gain, %.0f Hz\n", sr);
    const int lat = Engine::latencyFor (sr);
    const int step = (int) (0.4 * sr);

    // 1) Wird nach ein bis zwei Kicks angeglichen und hält die Lautheit
    struct Case { const char* name; Params p; };
    Params a1; a1.attack = 1.0f; a1.autoGain = true;
    Params a2; a2.attack = -1.0f; a2.autoGain = true;
    Params a3; a3.sustain = 1.0f; a3.autoGain = true;
    Params a4; a4.saturation = 1.0f; a4.punch = 1.0f; a4.autoGain = true;
    Params a5; a5.lowEnd = 1.0f; a5.attack = 0.5f; a5.autoGain = true;
    Params a6; a6.sustain = -1.0f; a6.autoGain = true;
    for (const auto& c : { Case { "Attack +100", a1 }, Case { "Attack -100", a2 }, Case { "Sustain +100", a3 }, Case { "Sustain -100", a6 },
                           Case { "Sat+Punch", a4 }, Case { "LowEnd+Attack", a5 } })
    {
        Buf in = makePattern (sr, 8, 400.0);
        Engine e; e.prepare (sr);
        Buf l = in, r = in;
        run (e, l, r, c.p);
        const double last = hitLoudness (sr, l, lat + 6 * step, lat + 7 * step) - hitLoudness (sr, in, 6 * step, 7 * step);
        const double third = hitLoudness (sr, l, lat + 2 * step, lat + 3 * step) - hitLoudness (sr, in, 2 * step, 3 * step);
        CHECK (std::fabs (last) < 1.0 && std::fabs (third - last) < 0.5,
               "%s: Korrektur %+.1f dB, Lautheit 3. Kick %+.2f dB, 7. Kick %+.2f dB", c.name, e.autoGainTargetDb(), third, last);
    }

    // 2) Pausen: Der Wert bleibt stehen, auch mit Rauschen in der Pause
    {
        Params p; p.attack = 1.0f; p.autoGain = true;
        Buf in = makePattern (sr, 4, 400.0, 1.0f, 3000.0);
        std::mt19937 rng (1); std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        const int pauseStart = 4 * step;
        for (int i = pauseStart; i < (int) in.size(); ++i) in[(size_t) i] += 3.0e-5f * d (rng); // -90 dB Rauschen
        Engine e; e.prepare (sr);
        Buf l = in, r = in;
        run (e, l, r, p, pauseStart + step / 2);
        const float before = e.currentAutoGainDb();
        // Rest in kleinen Blöcken, Wert muss stehen bleiben
        CHECK (std::fabs (before) > 0.1f, "Korrektur aktiv: %+.2f dB", before);
        Buf k = makePattern (sr, 1, 400.0);
        Buf ll ((size_t) (sr * 5.0), 0.0f), rr = ll;
        for (auto& v : ll) v = 3.0e-5f * d (rng);
        rr = ll;
        run (e, ll, rr, p);
        CHECK (e.currentAutoGainDb() == before, "5 s Pause mit -90 dB Rauschen: %+.3f dB -> %+.3f dB", before, e.currentAutoGainDb());
        // Die Pause selbst wird nicht hochgezogen
        const double inR = rmsDb (ll, 0, 1); (void) inR;
        Buf silence ((size_t) (sr * 3.0), 0.0f), s2 = silence;
        run (e, silence, s2, p);
        CHECK (e.currentAutoGainDb() == before, "3 s echte Stille: Wert unverändert");
    }

    // 3) Transport-Stopp (reset) behält den Wert
    {
        Params p; p.attack = 1.0f; p.autoGain = true;
        Buf in = makePattern (sr, 4, 400.0);
        Engine e; e.prepare (sr);
        Buf l = in, r = in;
        run (e, l, r, p);
        const float tgt = e.autoGainTargetDb();
        e.reset();
        Buf z (512, 0.0f), z2 = z;
        e.process (z.data(), z2.data(), 512, p);
        CHECK (e.currentAutoGainDb() == tgt, "Wert übersteht reset(): %+.2f dB", tgt);
    }

    // 4) Kein Eingriff im Schlag: Während des Körpers bleibt die Korrektur konstant (Knob-Sprung mitten im Muster)
    {
        Params p; p.attack = 0.0f; p.autoGain = true;
        Buf in = makePattern (sr, 8, 400.0);
        Engine e; e.prepare (sr);
        Buf dl = in, dr = in;
        run (e, dl, dr, p, 2 * step);
        p.attack = 1.0f; p.saturation = 0.3f;
        int maxMoves = 0;
        Buf l = in, r = in;
        // Sample für Sample: Korrektur darf sich nur außerhalb des Körpers ändern
        float last = e.currentAutoGainDb();
        int moved = 0;
        for (int i = 0; i < (int) in.size(); ++i)
        {
            float a = in[(size_t) i], b = a;
            e.process (&a, &b, 1, p);
            const int rel = i % step;
            const float cur = e.currentAutoGainDb();
            if (cur != last && rel < (int) (0.025 * sr)) ++moved;
            last = cur;
        }
        (void) maxMoves;
        CHECK (moved == 0, "Innerhalb der ersten 25 ms jedes Schlags %d Änderungen", moved);
    }

    // 5) Zurück auf neutral: exakt 0 dB und wieder bit-genau
    {
        Params p; p.attack = 1.0f; p.autoGain = true;
        Buf in = makePattern (sr, 4, 400.0);
        Engine e; e.prepare (sr);
        Buf l = in, r = in;
        run (e, l, r, p);
        Params n; n.autoGain = true;
        Buf l2 = makePattern (sr, 10, 400.0), r2 = l2, ref = l2;
        run (e, l2, r2, n);
        CHECK (e.currentAutoGainDb() == 0.0f, "Regler neutral: Korrektur %.4f dB", e.currentAutoGainDb());
        int diff = 0;
        for (int i = (int) ref.size() - step; i < (int) ref.size(); ++i)
            if (l2[(size_t) i] != ref[(size_t) (i - lat)]) ++diff;
        CHECK (diff == 0, "Danach wieder bit-genau: %d abweichende Samples", diff);
    }

    // 6) Grenzen
    {
        Params p; p.attack = 1.0f; p.saturation = 1.0f; p.punch = 1.0f; p.lowEnd = 1.0f; p.sustain = 1.0f; p.autoGain = true;
        Buf in = makePattern (sr, 6, 400.0, 0.05f);
        Engine e; e.prepare (sr);
        Buf l = in, r = in;
        run (e, l, r, p);
        CHECK (std::fabs (e.autoGainTargetDb()) <= tune::autoGainRangeDb, "Korrektur bleibt im Bereich: %+.2f dB", e.autoGainTargetDb());
    }

    // 7) Schnelle Kicks ohne Lücke (lange Ausklänge, 180 BPM) werden ebenfalls erfasst
    {
        Params p; p.attack = 1.0f; p.autoGain = true;
        Buf in = makePattern (sr, 16, 333.0, 1.0f, 0.0, 400.0);
        Engine e; e.prepare (sr);
        Buf l = in, r = in;
        run (e, l, r, p);
        CHECK (std::fabs (e.currentAutoGainDb() - e.autoGainTargetDb()) < 0.5f && e.autoGainTargetDb() < -0.1f,
               "Lange Kick ohne Pause: Korrektur %+.2f dB", e.currentAutoGainDb());
    }
}

static void testRobust (double sr)
{
    std::printf ("Robustheit, %.0f Hz\n", sr);
    // Gleiches Ergebnis bei jeder Blockgröße
    Params p; p.attack = 0.6f; p.sustain = -0.3f; p.punch = 0.5f; p.saturation = 0.5f; p.lowEnd = 0.4f; p.mix = 0.8f; p.autoGain = true; p.outDb = -1.0f;
    Buf in = makePattern (sr, 3, 300.0);
    Buf a = processMono (sr, in, p, 7), b = processMono (sr, in, p, 512), c = processMono (sr, in, p, 4096);
    int diff = 0;
    for (size_t i = 0; i < in.size(); ++i) if (a[i] != b[i] || a[i] != c[i]) ++diff;
    CHECK (diff == 0, "Blockgrößen 7/512/4096 identisch (%d Abweichungen)", diff);

    // NaN, Inf, riesige Werte: Ausgang bleibt endlich
    Engine e; e.prepare (sr);
    Buf l ((size_t) (sr), 0.5f), r = l;
    for (size_t i = 0; i < l.size(); i += 97) { l[i] = std::nanf (""); r[i] = 1.0e9f; }
    for (size_t i = 50; i < l.size(); i += 131) { l[i] = -std::numeric_limits<float>::infinity(); }
    Params x; x.attack = 1.0f; x.sustain = 1.0f; x.punch = 1.0f; x.saturation = 1.0f; x.lowEnd = 1.0f; x.autoGain = true; x.outDb = 18.0f;
    run (e, l, r, x);
    bool finite = true;
    for (size_t i = 0; i < l.size(); ++i) finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]);
    CHECK (finite, "NaN/Inf/1e9 am Eingang: Ausgang endlich");

    // DC und Stille: keine Denormal-/Log-Probleme, kein Rauschen aus dem Nichts
    Engine e2; e2.prepare (sr);
    Buf z ((size_t) (sr * 2), 0.0f), z2 = z;
    run (e2, z, z2, x);
    CHECK (peakOf (z, 0, (int) z.size()) == 0.0, "Stille rein, Stille raus (alles aufgedreht)");

    // Knacken beim Drehen: Ton mit laufendem Automations-Sprung
    Engine e3; e3.prepare (sr);
    const int n = (int) (sr * 2.0);
    Buf t ((size_t) n), t2;
    for (int i = 0; i < n; ++i) t[(size_t) i] = (float) (0.5 * std::sin (2 * PI * 220.0 * i / sr));
    t2 = t;
    Params y;
    int pos = 0; int blk = 128; int count = 0;
    while (pos < n)
    {
        const int m = std::min (blk, n - pos);
        if (count++ % 400 == 200) { y.attack = y.attack > 0 ? -1.0f : 1.0f; y.sustain = -y.attack; y.punch = 1.0f - y.punch; y.saturation = 1.0f - y.saturation; y.lowEnd = 1.0f - y.lowEnd; y.mix = y.mix > 0.5f ? 0.2f : 1.0f; y.outDb = y.outDb > 0 ? -6.0f : 6.0f; }
        e3.process (t.data() + pos, t2.data() + pos, m, y);
        pos += m;
    }
    double maxStep = 0.0;
    for (int i = (int) (sr * 0.01); i < n; ++i) maxStep = std::max (maxStep, (double) std::fabs (t[(size_t) i] - t[(size_t) (i - 1)]));
    CHECK (maxStep < 0.45, "Reglersprünge ohne Knacken: größter Sample-Schritt %.3f", maxStep);

    // Tempo
    Engine e4; e4.prepare (sr);
    Buf perf = makePattern (sr, 20, 500.0), perf2 = perf;
    const auto t0 = std::chrono::steady_clock::now();
    run (e4, perf, perf2, x);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double audio = (double) perf.size() / sr;
    CHECK (secs < audio * 0.25, "CPU: %.1f %% eines Kerns", 100.0 * secs / audio);
}

static void writeRaw (const std::string& path, const Buf& b)
{
    if (FILE* f = std::fopen (path.c_str(), "wb")) { std::fwrite (b.data(), sizeof (float), b.size(), f); std::fclose (f); }
}

int main (int argc, char** argv)
{
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        testNeutral (sr);
        testLatency (sr);
        testAttack (sr);
        testSustain (sr);
        testNoRipple (sr);
        testPunch (sr);
        testLowEnd (sr);
        testSaturation (sr);
        testPeakProtection (sr);
        testAutoGain (sr);
        testRobust (sr);
    }

    if (argc > 1)
    {
        const double sr = 48000.0;
        const std::string dir = argv[1];
        Buf in = makePattern (sr, 4, 500.0);
        writeRaw (dir + "/dry.raw", in);
        auto save = [&] (const char* name, Params p) { Buf o = processMono (sr, in, p); o.erase (o.begin(), o.begin() + Engine::latencyFor (sr)); writeRaw (dir + "/" + name + ".raw", o); };
        { Params p; p.attack = 1.0f; save ("attack_plus", p); }
        { Params p; p.attack = -1.0f; save ("attack_minus", p); }
        { Params p; p.sustain = 1.0f; save ("sustain_plus", p); }
        { Params p; p.sustain = -1.0f; save ("sustain_minus", p); }
        { Params p; p.punch = 1.0f; save ("punch", p); }
        { Params p; p.saturation = 0.7f; save ("sat", p); }
        { Params p; p.lowEnd = 1.0f; save ("low", p); }
        { Params p; p.attack = 1.0f; p.autoGain = true; save ("attack_ag", p); }
    }

    std::printf ("\n%s (%d Fehler)\n", fails == 0 ? "ALLES OK" : "FEHLER", fails);
    return fails == 0 ? 0 : 1;
}
