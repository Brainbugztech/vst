#pragma once

// Glitcher: kurze, zufällige Ereignisse am Ende der Kette.
//   Bit    - Bitcrusher + Sample-Rate-Reduktion
//   Rev    - das gerade Gespielte rückwärts
//   Stop   - Tape-Stop (Tempo und Tonhöhe fallen auf null)
//   Freeze - ein kurzes Stück wird eingefroren und als Loop gehalten
//   Pitch  - Tonhöhensprünge (Oktaven, Quinten, Quarten)
// Ein Ereignis startet nur auf einem Raster von 1/16 und dauert 1/16 bis 1 Beat.

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include "Params.h"
#include "Rng.h"

namespace bc
{

class Glitcher
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size_t n = 1u << 15;
        while ((double) n < sr * 2.6) n <<= 1;
        ring[0].assign (n, 0.0f);
        ring[1].assign (n, 0.0f);
        mask = n - 1;
        ramp = std::max (16, (int) std::lround (0.003 * sr));
        pitchWin = std::max (64.0, 0.04 * sr);
        reset();
    }

    void reset()
    {
        active = false;
        lastTick = std::numeric_limits<long long>::min();
        phi = 0.0;
    }

    bool isGlitching() const { return active; }

    void process (float& l, float& r, double beat, double spb, const GlitchParams& p, uint64_t seedKey)
    {
        const float inL = l, inR = r;
        ring[0][wIdx & mask] = inL;
        ring[1][wIdx & mask] = inR;

        const bool anyType = p.bit || p.rev || p.stop || p.freeze || p.pitch;
        if (! p.on || ! anyType)
        {
            active = false;
            lastTick = std::numeric_limits<long long>::min();
            ++wIdx;
            return;
        }

        constexpr double tick = 0.25;
        const long long tIdx = (long long) std::floor (beat / tick);
        if (tIdx != lastTick)
        {
            if (active && lastTick != std::numeric_limits<long long>::min() && tIdx != lastTick + 1)
                active = false; // Sprung im Transport
            lastTick = tIdx;
            if (! active)
                maybeStart (tIdx, spb, p, seedKey);
        }

        if (active)
        {
            float gl = inL, gr = inR;
            render (p, spb, seedKey, inL, inR, gl, gr);

            const float a = (float) std::min ({ 1.0, t / (double) ramp, (len - t) / (double) ramp });
            const float m = p.mix * std::max (0.0f, a);
            l = inL + (gl - inL) * m;
            r = inR + (gr - inR) * m;

            if (++t >= len)
                active = false;
        }
        ++wIdx;
    }

private:
    enum Type { Bit, Rev, Stop, Freeze, Pitch };

    void maybeStart (long long tIdx, double spb, const GlitchParams& p, uint64_t seedKey)
    {
        constexpr double tick = 0.25;
        const double a = std::max (0.0, std::min (0.9, (double) p.amount * 0.9));
        const double prob = 1.0 - std::pow (1.0 - a, tick);
        const uint64_t key = hashCombine (seedKey ^ 0x61177Cull, (uint64_t) tIdx);
        if ((double) hash01 (key) >= prob)
            return;

        int avail[5], n = 0;
        if (p.bit)    avail[n++] = Bit;
        if (p.rev)    avail[n++] = Rev;
        if (p.stop)   avail[n++] = Stop;
        if (p.freeze) avail[n++] = Freeze;
        if (p.pitch)  avail[n++] = Pitch;

        type = avail[std::min (n - 1, (int) (hash01 (key ^ 0x1111ull) * (float) n))];
        static constexpr int ticks[4] { 1, 2, 2, 4 };
        const int nt = ticks[std::min (3, (int) (hash01 (key ^ 0x2222ull) * 4.0f))];
        len = (double) nt * tick * spb;
        if (len > (double) (mask / 2)) len = (double) (mask / 2);
        t = 0.0;
        t0 = (double) wIdx;
        freezeLen = (hash01 (key ^ 0x3333ull) < 0.5f ? 0.125 : 0.25) * spb;
        active = true;
        holdCount = 0;
    }

    float rd (int ch, double pos) const
    {
        const double fl = std::floor (pos);
        const float f = (float) (pos - fl);
        const long long i0 = (long long) fl;
        const float a = ring[ch][(size_t) i0 & mask];
        const float b = ring[ch][(size_t) (i0 + 1) & mask];
        return a + (b - a) * f;
    }

    static float hann (double x) { return 0.5f - 0.5f * std::cos (6.28318531f * (float) x); }

    void render (const GlitchParams& p, double spb, uint64_t seedKey, float inL, float inR, float& gl, float& gr)
    {
        switch (type)
        {
            case Bit:
            {
                const int bits = std::max (3, 9 - (int) (p.amount * 6.0f));
                const int hold = 1 + (int) (p.amount * 14.0f);
                if (holdCount++ % hold == 0) { heldL = inL; heldR = inR; }
                const float lv = (float) (1 << (bits - 1));
                gl = std::round (heldL * lv) / lv;
                gr = std::round (heldR * lv) / lv;
                break;
            }
            case Rev:
            {
                const double pos = t0 - t;
                gl = rd (0, pos);
                gr = rd (1, pos);
                break;
            }
            case Stop:
            {
                const double pos = t0 + t - t * t / (2.0 * len);
                const float fade = (float) std::max (0.0, 1.0 - std::pow (t / len, 3.0));
                gl = rd (0, pos) * fade;
                gr = rd (1, pos) * fade;
                break;
            }
            case Freeze:
            {
                const double g = std::max (32.0, freezeLen);
                const double t1 = std::fmod (t, g), t2 = std::fmod (t + g * 0.5, g);
                const float w1 = hann (t1 / g), w2 = hann (t2 / g);
                gl = w1 * rd (0, t0 - g + t1) + w2 * rd (0, t0 - g + t2);
                gr = w1 * rd (1, t0 - g + t1) + w2 * rd (1, t0 - g + t2);
                break;
            }
            case Pitch:
            {
                static constexpr int semis[6] { 12, -12, 7, -5, 5, -7 };
                const long long sub = (long long) (t / (spb * 0.125));
                const int st = semis[(int) (hash01 (hashCombine (seedKey ^ 0x9177ull, (uint64_t) lastTick * 64 + (uint64_t) sub)) * 5.999f)];
                const double ratio = std::pow (2.0, (double) st / 12.0);
                phi += (1.0 - ratio) / pitchWin;
                phi -= std::floor (phi);
                const double ph2 = phi + 0.5 - std::floor (phi + 0.5);
                const double w = (double) wIdx;
                const float w1 = hann (phi), w2 = hann (ph2);
                const double p1 = w - phi * pitchWin, p2 = w - ph2 * pitchWin;
                gl = w1 * rd (0, p1) + w2 * rd (0, p2);
                gr = w1 * rd (1, p1) + w2 * rd (1, p2);
                break;
            }
        }
    }

    double sr = 48000.0;
    std::vector<float> ring[2];
    size_t mask = 0;
    long long wIdx = 0;
    int ramp = 144;
    double pitchWin = 1920.0;

    bool active = false;
    int type = Bit;
    double len = 0.0, t = 0.0, t0 = 0.0, freezeLen = 6000.0, phi = 0.0;
    long long lastTick = std::numeric_limits<long long>::min();
    int holdCount = 0;
    float heldL = 0.0f, heldR = 0.0f;
};

} // namespace bc
