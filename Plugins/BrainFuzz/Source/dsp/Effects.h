#pragma once

// Brainfuzz – Effekte pro Band: Bit Crusher, Chorus (bis 3 Stimmen), Reverb (FDN)
// plus Hilfsbausteine: Verzoegerung, Allpass, einfache Filter, Dekorrelator fuer "Spread".

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace bfz
{

// ------------------------------------------------------------------ Bausteine
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d = 1: zuletzt geschriebenes Sample
    inline float read (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    inline float readFrac (float d) const
    {
        d = std::max (1.0f, d);
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = read (i), b = read (i + 1);
        return a + (b - a) * f;
    }
};

struct OnePole
{
    float z = 0.0f, a = 1.0f;
    void setLowpass (float hz, float fs) { a = 1.0f - std::exp (-6.2831853f * std::clamp (hz, 1.0f, fs * 0.49f) / fs); }
    void reset() { z = 0.0f; }
    inline float lp (float x) { z += (x - z) * a; return z; }
    inline float hp (float x) { z += (x - z) * a; return x - z; }
};

struct Allpass
{
    DelayLine d;
    int len = 1;
    float g = 0.6f;
    void prepare (int maxLen) { d.prepare (maxLen); }
    void reset() { d.reset(); }
    inline float process (float x)
    {
        const float v = d.read (len);
        const float u = x + g * v;
        d.push (u);
        return v - g * u;
    }
};

// ------------------------------------------------------------------ Bit Crusher
struct CrushParams { float bits = 8.0f, down = 4.0f, mix = 1.0f; };

struct Crusher
{
    float holdL = 0.0f, holdR = 0.0f, phase = 1e9f;
    void reset() { holdL = holdR = 0.0f; phase = 1e9f; }

    inline void process (float& l, float& r, const CrushParams& p, float mix)
    {
        phase += 1.0f;
        if (phase >= p.down) { phase = std::fmod (phase, std::max (1.0f, p.down)); holdL = l; holdR = r; }
        const float q = std::exp2 (std::clamp (p.bits, 1.0f, 24.0f) - 1.0f);
        const float cl = std::round (holdL * q) / q;
        const float cr = std::round (holdR * q) / q;
        l += (cl - l) * mix;
        r += (cr - r) * mix;
    }
};

// ------------------------------------------------------------------ Chorus
struct ChorusParams { int voices = 2; float rate = 0.6f, depth = 0.5f, mix = 0.5f; };

struct Chorus
{
    DelayLine dl[2];
    double phase = 0.0;
    float fs = 48000.0f;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : dl) d.prepare ((int) (0.035f * fs));
        phase = 0.0;
    }
    void reset() { for (auto& d : dl) d.reset(); }

    inline void process (float& l, float& r, const ChorusParams& p, float mix)
    {
        dl[0].push (l);
        dl[1].push (r);
        phase += p.rate / fs;
        if (phase >= 1.0) phase -= 1.0;
        const int v = std::clamp (p.voices, 1, 3);
        static constexpr float base[3] { 7.0f, 11.5f, 16.0f };
        const float swing = 6.0f * std::clamp (p.depth, 0.0f, 1.0f);
        float wl = 0.0f, wr = 0.0f;
        for (int i = 0; i < v; ++i)
        {
            const double ph = phase + (double) i / (double) v;
            const float sl = (float) std::sin (6.283185307 * ph);
            const float sr = (float) std::sin (6.283185307 * (ph + 0.25));
            wl += dl[0].readFrac ((base[i] + swing * 0.5f * (1.0f + sl)) * fs * 0.001f);
            wr += dl[1].readFrac ((base[i] + swing * 0.5f * (1.0f + sr)) * fs * 0.001f);
        }
        const float norm = 1.0f / std::sqrt ((float) v);
        wl *= norm; wr *= norm;
        l += (wl - l) * mix;
        r += (wr - r) * mix;
    }
};

// ------------------------------------------------------------------ Reverb (8er-FDN)
struct ReverbParams
{
    float size = 0.5f, length = 1.8f, width = 1.0f, lowCut = 150.0f, highCut = 9000.0f, predelay = 10.0f, mix = 0.3f;
};

struct Reverb
{
    static constexpr int N = 8;
    std::array<DelayLine, N> lines;
    std::array<int, N> len {};
    std::array<float, N> g {};
    std::array<OnePole, N> damp;
    DelayLine pre[2];
    std::array<Allpass, 4> diff;
    OnePole inHp[2], inLp[2];
    float fs = 48000.0f;
    ReverbParams last { -1, -1, -1, -1, -1, -1, -1 };

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& l : lines) l.prepare ((int) (0.11f * fs));
        for (auto& p : pre) p.prepare ((int) (0.26f * fs));
        for (auto& a : diff) a.prepare ((int) (0.012f * fs));
        last.size = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& p : pre) p.reset();
        for (auto& a : diff) a.reset();
        for (auto& d : damp) d.reset();
        for (int c = 0; c < 2; ++c) { inHp[c].reset(); inLp[c].reset(); }
    }

    void update (const ReverbParams& p)
    {
        auto same = [] (float a, float b) { return std::abs (a - b) < 1e-6f; };
        if (same (p.size, last.size) && same (p.length, last.length) && same (p.lowCut, last.lowCut) && same (p.highCut, last.highCut))
            return;
        last = p;
        static constexpr float baseMs[N] { 31.3f, 37.9f, 43.7f, 51.1f, 59.3f, 67.9f, 77.1f, 89.3f };
        const float scale = 0.25f + 0.75f * std::clamp (p.size, 0.0f, 1.0f);
        const float rt = std::clamp (p.length, 0.1f, 20.0f);
        for (int i = 0; i < N; ++i)
        {
            len[(size_t) i] = std::max (8, (int) (baseMs[i] * scale * 0.001f * fs));
            g[(size_t) i] = std::pow (10.0f, -3.0f * (float) len[(size_t) i] / (rt * fs));
            damp[(size_t) i].setLowpass (std::min (p.highCut * 1.6f, fs * 0.45f), fs);
        }
        static constexpr float apMs[4] { 4.7f, 6.1f, 5.3f, 7.3f };
        for (int i = 0; i < 4; ++i) { diff[(size_t) i].len = (int) (apMs[i] * scale * 0.001f * fs) + 1; diff[(size_t) i].g = 0.62f; }
        for (int c = 0; c < 2; ++c) { inHp[c].setLowpass (p.lowCut, fs); inLp[c].setLowpass (p.highCut, fs); }
    }

    inline void process (float& l, float& r, const ReverbParams& p, float mix)
    {
        // Vorverzoegerung + Eingangsfilter + Diffusion
        pre[0].push (l); pre[1].push (r);
        const int pd = std::max (1, (int) (p.predelay * 0.001f * fs));
        float il = inLp[0].lp (inHp[0].hp (pre[0].read (pd)));
        float ir = inLp[1].lp (inHp[1].hp (pre[1].read (pd)));
        il = diff[1].process (diff[0].process (il));
        ir = diff[3].process (diff[2].process (ir));

        float y[N];
        for (int i = 0; i < N; ++i) y[i] = damp[(size_t) i].lp (lines[(size_t) i].read (len[(size_t) i]));

        // Hadamard-Mischung (orthogonal)
        float v[N];
        for (int i = 0; i < N; ++i) v[i] = y[i];
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j) { const float a = v[j], b = v[j + h]; v[j] = a + b; v[j + h] = a - b; }
        const float k = 0.35355339f; // 1/sqrt(8)
        for (int i = 0; i < N; ++i)
            lines[(size_t) i].push (v[i] * k * g[(size_t) i] + ((i & 1) ? ir : il) * 0.5f);

        float ol = (y[0] - y[2] + y[4] - y[6]) * 0.5f;
        float orr = (y[1] - y[3] + y[5] - y[7]) * 0.5f;
        // Breite
        const float m = 0.5f * (ol + orr), s = 0.5f * (ol - orr) * std::clamp (p.width, 0.0f, 1.0f);
        ol = m + s; orr = m - s;
        l += (ol - l) * mix;
        r += (orr - r) * mix;
    }
};

// ------------------------------------------------------------------ Dekorrelator fuer Spread
// Erzeugt aus der Mitte ein Seiten-Signal. Weil es als +S / -S addiert wird, faellt es
// in Mono komplett weg: Spread ist dadurch mono-sicher, auch bei Mono-Quellen.
struct Decorrelator
{
    DelayLine d;
    Allpass ap1, ap2;
    int delay = 1;

    void prepare (float fs, float delayMs, float ap1Ms, float ap2Ms)
    {
        d.prepare ((int) (0.03f * fs));
        ap1.prepare ((int) (0.01f * fs));
        ap2.prepare ((int) (0.01f * fs));
        delay = std::max (1, (int) (delayMs * 0.001f * fs));
        ap1.len = std::max (1, (int) (ap1Ms * 0.001f * fs)); ap1.g = 0.5f;
        ap2.len = std::max (1, (int) (ap2Ms * 0.001f * fs)); ap2.g = -0.45f;
    }
    void reset() { d.reset(); ap1.reset(); ap2.reset(); }

    inline float process (float m)
    {
        d.push (m);
        return ap2.process (ap1.process (d.read (delay)));
    }
};

} // namespace bfz
