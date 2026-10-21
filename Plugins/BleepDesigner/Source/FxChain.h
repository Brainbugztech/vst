#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

// Etappe 6: Ausgangskette Comb -> Crush -> Filter -> Delay -> Soft-Clip.
// Laeuft live auf dem ganzen Pattern. Filter-Huellkurve und Keytrack folgen jedem Arp-Step.

namespace bleep
{

struct FxParams
{
    bool  combOn = false;
    float combFreq = 880.0f;    // Hz
    float combFb = -0.72f;      // -0.99 .. 0.99 (negativ = hohl, metallisch)
    float combDamp = 0.35f;     // 0 .. 1
    float combKey = 1.0f;       // 0 .. 1
    int   combCount = 1;        // 1 .. 3
    float combRatio2 = 1.5f, combRatio3 = 2.3f;
    float combMix = 0.4f;

    bool  crushOn = false;
    float crushBits = 8.0f;     // 1 .. 16
    int   crushRate = 4;        // Samplerate geteilt durch 1 .. 32
    float crushMix = 0.25f;

    bool  filterOn = true;
    int   filterType = 0;       // LP, BP, HP
    float filterCut = 2400.0f;  // Hz
    float filterRes = 0.3f;     // 0 .. 1
    float filterEnv = 0.4f;     // -1 .. 1 (max +-5 Oktaven)
    float filterDecay = 90.0f;  // ms
    float filterKey = 0.0f;     // 0 .. 1

    bool  delayOn = true;
    double delayBeats = 0.75;   // Laenge in Vierteln
    float delayFb = 0.45f;      // 0 .. 0.95
    bool  delayPingPong = true;
    float delayLowCut = 150.0f, delayHighCut = 6000.0f;
    float delayMix = 0.3f;
};

// Ein Ereignis pro Arp-Step: wann (Sample im Block) und welche Note (fuer Keytrack)
struct FxTrigger { int sample; int note; };

class FxChain
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        combSize = juce::nextPowerOfTwo ((int) (sr / 15.0) + 8);
        for (auto& c : comb)
            c.assign ((size_t) combSize * 3, 0.0f);
        delaySize = juce::nextPowerOfTwo ((int) (sr * 4.5) + 8);
        for (auto& d : delay)
            d.assign ((size_t) delaySize, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& c : comb) std::fill (c.begin(), c.end(), 0.0f);
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        combPos = delayPos = 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int k = 0; k < 3; ++k) combLp[ch][k] = 0.0f;
            crushHold[ch] = 0.0f;
            ic1[ch] = ic2[ch] = 0.0f;
            fbLp[ch] = fbHp[ch] = fbHpX[ch] = 0.0f;
        }
        crushCount = 0;
        filterEnvVal = 0.0f;
        combDelaySm = -1.0;
        delaySm = -1.0;
    }

    void process (juce::AudioBuffer<float>& buffer, const FxParams& p, const FxTrigger* triggers, int numTriggers, double bpm)
    {
        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        if (numCh == 0)
            return;
        float* L = buffer.getWritePointer (0);
        float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        const float envCoef = (float) std::exp (-1.0 / (juce::jmax (1.0, (double) p.filterDecay) * 0.001 * sr));
        const double delayTarget = juce::jlimit (1.0, (double) delaySize - 4.0, p.delayBeats * 60.0 / juce::jmax (20.0, bpm) * sr);
        if (delaySm < 0.0)
            delaySm = delayTarget;

        // Koeffizienten der Feedback-Filter im Delay
        const float lpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (p.delayHighCut, (float) (sr * 0.45)) / (float) sr);
        const float hpA = std::exp (-juce::MathConstants<float>::twoPi * p.delayLowCut / (float) sr);

        const double ratios[3] = { 1.0, (double) p.combRatio2, (double) p.combRatio3 };
        const int combs = juce::jlimit (1, 3, p.combCount);
        const float fb = juce::jlimit (-0.99f, 0.99f, p.combFb);
        const float damp = juce::jlimit (0.0f, 1.0f, p.combDamp) * 0.95f;
        const float combComp = 1.0f - 0.65f * std::abs (fb);

        const float q = 0.5f + p.filterRes * p.filterRes * 14.5f;
        const float k = 1.0f / q;

        int t = 0;
        for (int i = 0; i < n; ++i)
        {
            // Neuer Step: Huellkurve neu starten, Keytrack-Note merken
            while (t < numTriggers && triggers[t].sample <= i)
            {
                keyNote = triggers[t].note;
                filterEnvVal = 1.0f;
                ++t;
            }

            float x[2] = { L[i], R != nullptr ? R[i] : L[i] };
            const float keyOct = (float) (keyNote - 60) / 12.0f;

            // ---- Comb ----
            if (p.combOn)
            {
                const double freq = juce::jlimit (20.0, sr * 0.45, (double) p.combFreq * std::exp2 ((double) (keyOct * p.combKey)));
                const double target = sr / freq;
                combDelaySm = combDelaySm < 0.0 ? target : combDelaySm + (target - combDelaySm) * 0.002;

                for (int ch = 0; ch < 2; ++ch)
                {
                    float wet = 0.0f;
                    for (int c = 0; c < combs; ++c)
                    {
                        auto& buf = comb[(size_t) ch];
                        const int base = c * combSize;
                        const double d = juce::jlimit (2.0, (double) combSize - 3.0, combDelaySm / ratios[c]);
                        const double rp = (double) combPos - d;
                        const int i0 = (int) std::floor (rp);
                        const float fr = (float) (rp - i0);
                        const float a = buf[(size_t) (base + (i0 & (combSize - 1)))];
                        const float b = buf[(size_t) (base + ((i0 + 1) & (combSize - 1)))];
                        const float delayed = a + (b - a) * fr;
                        combLp[ch][c] += (1.0f - damp) * (delayed - combLp[ch][c]);
                        // Soft-Clip im Feedback, damit hohe Werte nicht explodieren
                        const float y = x[ch] + fb * softClip (combLp[ch][c]);
                        buf[(size_t) (base + combPos)] = y;
                        wet += y;
                    }
                    wet *= combComp / (float) combs;
                    x[ch] += (wet - x[ch]) * p.combMix;
                }
            }
            else
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int c = 0; c < 3; ++c)
                        comb[(size_t) ch][(size_t) (c * combSize + combPos)] = 0.0f;
            }
            combPos = (combPos + 1) & (combSize - 1);

            // ---- Crush ----
            if (p.crushOn)
            {
                if (crushCount <= 0)
                {
                    const float levels = std::exp2 (juce::jlimit (1.0f, 16.0f, p.crushBits) - 1.0f);
                    for (int ch = 0; ch < 2; ++ch)
                        crushHold[ch] = std::round (x[ch] * levels) / levels;
                    crushCount = juce::jlimit (1, 32, p.crushRate);
                }
                --crushCount;
                for (int ch = 0; ch < 2; ++ch)
                    x[ch] += (crushHold[ch] - x[ch]) * p.crushMix;
            }

            // ---- Filter (TPT-SVF) ----
            if (p.filterOn)
            {
                const float oct = p.filterEnv * 5.0f * filterEnvVal + p.filterKey * keyOct;
                const float fc = juce::jlimit (20.0f, (float) (sr * 0.45), p.filterCut * std::exp2 (oct));
                const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
                const float a1 = 1.0f / (1.0f + g * (g + k));
                const float a2 = g * a1, a3 = g * a2;
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float v3 = x[ch] - ic2[ch];
                    const float v1 = a1 * ic1[ch] + a2 * v3;
                    const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
                    ic1[ch] = 2.0f * v1 - ic1[ch];
                    ic2[ch] = 2.0f * v2 - ic2[ch];
                    x[ch] = p.filterType == 0 ? v2 : (p.filterType == 1 ? v1 : x[ch] - k * v1 - v2);
                }
            }
            filterEnvVal *= envCoef;

            // ---- Delay ----
            if (p.delayOn || p.delayMix > 0.0f)
            {
                delaySm += (delayTarget - delaySm) * 0.0005;
                float wet[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    const double rp = (double) delayPos - delaySm;
                    const int i0 = (int) std::floor (rp);
                    const float fr = (float) (rp - i0);
                    const auto& buf = delay[(size_t) ch];
                    const float a = buf[(size_t) (i0 & (delaySize - 1))];
                    const float b = buf[(size_t) ((i0 + 1) & (delaySize - 1))];
                    wet[ch] = a + (b - a) * fr;

                    // Feedback durch Low-Cut und High-Cut
                    fbLp[ch] += lpA * (wet[ch] - fbLp[ch]);
                    const float hpIn = fbLp[ch];
                    fbHp[ch] = hpA * (fbHp[ch] + hpIn - fbHpX[ch]);
                    fbHpX[ch] = hpIn;
                }

                const float g = p.delayOn ? juce::jlimit (0.0f, 0.95f, p.delayFb) : 0.0f;
                const float in = p.delayOn ? 1.0f : 0.0f;
                if (p.delayPingPong)
                {
                    delay[0][(size_t) delayPos] = softClip ((x[0] + x[1]) * 0.5f * in + g * fbHp[1]);
                    delay[1][(size_t) delayPos] = softClip (g * fbHp[0]);
                }
                else
                {
                    delay[0][(size_t) delayPos] = softClip (x[0] * in + g * fbHp[0]);
                    delay[1][(size_t) delayPos] = softClip (x[1] * in + g * fbHp[1]);
                }
                delayPos = (delayPos + 1) & (delaySize - 1);

                if (p.delayOn)
                    for (int ch = 0; ch < 2; ++ch)
                        x[ch] += wet[ch] * p.delayMix;
            }

            L[i] = x[0];
            if (R != nullptr)
                R[i] = x[1];
        }
    }

    static float softClip (float x)
    {
        // Weich ab etwa 0.9, harte Grenze bei 1.5
        const float a = std::abs (x);
        if (a < 0.9f)
            return x;
        const float y = 0.9f + 0.6f * std::tanh ((a - 0.9f) / 0.6f);
        return x < 0.0f ? -y : y;
    }

private:
    double sr = 48000.0;
    std::vector<float> comb[2], delay[2];
    int combSize = 4096, combPos = 0, delaySize = 262144, delayPos = 0;
    float combLp[2][3] {};
    double combDelaySm = -1.0, delaySm = -1.0;
    float crushHold[2] {};
    int crushCount = 0;
    float ic1[2] {}, ic2[2] {};
    float filterEnvVal = 0.0f;
    int keyNote = 60;
    float fbLp[2] {}, fbHp[2] {}, fbHpX[2] {};
};

} // namespace bleep
