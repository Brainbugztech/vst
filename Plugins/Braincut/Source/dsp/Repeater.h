#pragma once

// Repeater: Stotterer und Rolls hinter dem Slicer.
// An jeder Einheiten-Grenze (Rate) wird mit einer Wahrscheinlichkeit ein Ereignis gestartet: die erste
// Einheit läuft normal durch und wird dabei aufgenommen, danach wird sie 1..7 Mal wiederholt.
// Gate kürzt jede Wiederholung, Decay macht jede Wiederholung leiser.

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include "Params.h"
#include "Rng.h"

namespace bc
{

class Repeater
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        capSize = (int) (sr * 2.2) + 8;
        cap[0].assign ((size_t) capSize, 0.0f);
        cap[1].assign ((size_t) capSize, 0.0f);
        fade = std::max (16, (int) std::lround (0.0015 * sr));
        reset();
    }

    void reset()
    {
        state = Idle;
        lastU = std::numeric_limits<long long>::min();
        liveGain = 1.0f;
    }

    bool isRepeating() const { return state != Idle; }

    void process (float& l, float& r, double beat, double spb, const RepeatParams& p, uint64_t seedKey)
    {
        if (! p.on)
        {
            if (state != Idle) { state = Idle; liveGain = 0.0f; }
            lastU = std::numeric_limits<long long>::min();
            rampLive (l, r);
            return;
        }

        const double ub = kRepeatUnitBeats[std::min (4, std::max (0, p.rateIdx))];
        const double uPos = beat / ub;
        const long long uIdx = (long long) std::floor (uPos);
        const double uFrac = uPos - (double) uIdx;

        if (uIdx != lastU)
        {
            // Sprung im Transport: laufendes Ereignis verwerfen
            if (lastU != std::numeric_limits<long long>::min() && uIdx != lastU + 1 && state != Idle)
            {
                state = Idle;
                liveGain = 0.0f;
            }
            lastU = uIdx;
            boundary (uIdx, ub, spb, p, seedKey);
        }

        switch (state)
        {
            case Idle:
                rampLive (l, r);
                break;

            case Capture:
                if (recPos < unitLen)
                {
                    cap[0][(size_t) recPos] = l;
                    cap[1][(size_t) recPos] = r;
                    ++recPos;
                }
                break;

            case Play:
            {
                const double unitF = ub * spb;
                const double tS = uFrac * unitF;
                const int idx = (int) tS;
                if (idx >= unitLen) { l = r = 0.0f; break; }

                const double gateS = std::max (0.1, std::min (1.0, (double) p.gate)) * unitF;
                const float fi = (float) std::min (1.0, tS / (double) fade);
                const float fo = (float) std::max (0.0, std::min (1.0, (gateS - tS) / (double) fade));
                const float env = fi * fo * gainK;
                l = cap[0][(size_t) idx] * env;
                r = cap[1][(size_t) idx] * env;
                break;
            }
        }
    }

private:
    enum State { Idle, Capture, Play };

    void boundary (long long uIdx, double ub, double spb, const RepeatParams& p, uint64_t seedKey)
    {
        if (state == Capture)
        {
            state = Play;
            k = 1;
        }
        else if (state == Play)
        {
            if (++k >= total)
            {
                state = Idle;
                liveGain = 0.0f;
            }
        }

        if (state == Play)
        {
            const float d = std::max (0.0f, std::min (1.0f, p.decay)) * 0.5f;
            gainK = std::pow (1.0f - d, (float) k);
            return;
        }

        if (state == Idle)
        {
            const double c = std::max (0.0, std::min (1.0, (double) p.chance));
            const double prob = c >= 1.0 ? 1.0 : 1.0 - std::pow (1.0 - c, ub);
            const uint64_t key = hashCombine (seedKey ^ 0x5EEDull, (uint64_t) uIdx);
            if ((double) hash01 (key) < prob)
            {
                static constexpr int totals[6] { 2, 3, 4, 4, 6, 8 };
                total = totals[(int) (hash01 (key ^ 0xABCDull) * 5.999f)];
                state = Capture;
                unitLen = std::min (capSize, std::max (1, (int) (ub * spb)));
                recPos = 0;
                k = 0;
                gainK = 1.0f;
            }
        }
    }

    // nach einem Ereignis das Live-Signal kurz einblenden
    void rampLive (float& l, float& r)
    {
        if (liveGain < 1.0f)
        {
            liveGain = std::min (1.0f, liveGain + 1.0f / (float) fade);
            l *= liveGain;
            r *= liveGain;
        }
    }

    double sr = 48000.0;
    int capSize = 0, fade = 72;
    std::vector<float> cap[2];

    State state = Idle;
    long long lastU = std::numeric_limits<long long>::min();
    int unitLen = 0, recPos = 0, k = 0, total = 2;
    float gainK = 1.0f, liveGain = 1.0f;
};

} // namespace bc
