#include "FilterBankEngine.h"

#include <algorithm>
#include <cmath>

namespace prizm
{
namespace
{
    float onePoleCoef (double fs, double seconds)
    {
        return (float) (1.0 - std::exp (-1.0 / (fs * seconds)));
    }

    float blockCoef (double fs, int numSamples, double seconds)
    {
        return (float) (1.0 - std::exp (-(double) numSamples / (fs * seconds)));
    }
} // namespace

float FilterBankEngine::qFromColor (float color)
{
    // 0% -> Q 8 (soft), 100% -> Q ~110 (clearly tonal), 200% -> Q 1500 (long ringing tail)
    return 8.0f * std::pow (187.5f, juce::jlimit (0.0f, 2.0f, color) * 0.5f);
}

void FilterBankEngine::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    bands.clear();

    for (int note = kLowestNote; note <= kHighestNote; ++note)
    {
        const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        if (f > 0.45 * fs)
            break;

        Band b;
        b.note = note;
        b.g = (float) std::tan (juce::MathConstants<double>::pi * f / fs);
        bands.push_back (b);
    }

    wet.setSize (kMaxChannels, maxBlockSize, false, false, true);
    transientCurve.assign ((size_t) maxBlockSize, 0.0f);
    dampCurve.assign ((size_t) maxBlockSize, 1.0f);

    fastAtt = onePoleCoef (fs, 0.001);
    fastRel = onePoleCoef (fs, 0.020);
    slowAtt = onePoleCoef (fs, 0.040);
    slowRel = onePoleCoef (fs, 0.250);
    transientRel = onePoleCoef (fs, 0.060);
    fadeStep = (float) (1.0 / (0.03 * fs)); // 30 ms fade when the key changes

    reset();
}

void FilterBankEngine::reset()
{
    for (auto& b : bands)
    {
        b.ic1.fill (0.0f);
        b.ic2.fill (0.0f);
        b.gain = b.target;
    }

    inMeanSquare = wetMeanSquare = 0.0f;
    autoGain = 1.0f;
    envFast = envSlow = transient = 0.0f;
}

void FilterBankEngine::process (juce::AudioBuffer<float>& buffer, int numSamples, const EngineParams& p)
{
    const int numCh = std::min (buffer.getNumChannels(), kMaxChannels);
    if (numCh == 0 || numSamples <= 0)
        return;

    // Key change: fade bands in/out instead of switching hard.
    if (p.mask != lastMask)
    {
        const bool first = (lastMask == 0xFFFF);
        for (auto& b : bands)
        {
            b.target = isAllowed (p.mask, b.note) ? 1.0f : 0.0f;
            if (first)
                b.gain = b.target;
        }
        lastMask = p.mask;
    }

    // Resonance (Q), smoothed per block.
    qSmoothed += (qFromColor (p.color) - qSmoothed) * blockCoef (fs, numSamples, 0.05);
    const float k = 1.0f / qSmoothed;

    for (auto& b : bands)
    {
        b.a1 = 1.0f / (1.0f + b.g * (b.g + k));
        b.a2 = b.g * b.a1;
        b.a3 = b.g * b.a2;
    }

    // Transient detector on the summed input (shared by both channels).
    const float clear = juce::jlimit (0.0f, 1.0f, p.clear);
    for (int n = 0; n < numSamples; ++n)
    {
        float x = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            x += std::abs (buffer.getSample (ch, n));
        x /= (float) numCh;

        envFast += (x - envFast) * (x > envFast ? fastAtt : fastRel);
        envSlow += (x - envSlow) * (x > envSlow ? slowAtt : slowRel);

        float raw = 0.0f;
        if (envFast > 1.0e-4f)
            raw = juce::jlimit (0.0f, 1.0f, (envFast / (envSlow + 1.0e-6f) - 1.3f) / 1.5f);

        if (raw > transient)
            transient = raw;
        else
            transient += (raw - transient) * transientRel;

        transientCurve[(size_t) n] = transient;
        dampCurve[(size_t) n] = 1.0f - clear * transient * 0.004f; // shortens old ringing on new hits
    }

    // Run the resonator bank into the wet buffer.
    wet.clear (0, numSamples);

    for (auto& b : bands)
    {
        if (b.gain <= 0.0f && b.target <= 0.0f)
            continue;

        const float gainStart = b.gain;
        const float step = (b.target > b.gain ? fadeStep : -fadeStep);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* in = buffer.getReadPointer (ch);
            float* out = wet.getWritePointer (ch);
            float ic1 = b.ic1[(size_t) ch], ic2 = b.ic2[(size_t) ch];
            float gain = gainStart;
            float energy = 0.0f;

            for (int n = 0; n < numSamples; ++n)
            {
                const float v3 = in[n] - ic2;
                const float v1 = b.a1 * ic1 + b.a2 * v3;
                const float v2 = ic2 + b.a2 * ic1 + b.a3 * v3;
                const float damp = dampCurve[(size_t) n];
                ic1 = (2.0f * v1 - ic1) * damp;
                ic2 = (2.0f * v2 - ic2) * damp;

                gain = (step > 0.0f) ? std::min (b.target, gain + step) : std::max (b.target, gain + step);

                const float y = gain * k * v1; // k * band = unity-peak band-pass
                out[n] += y;
                energy += y * y;
            }

            // Flush denormals / tiny states.
            if (std::abs (ic1) < 1.0e-15f) ic1 = 0.0f;
            if (std::abs (ic2) < 1.0e-15f) ic2 = 0.0f;
            b.ic1[(size_t) ch] = ic1;
            b.ic2[(size_t) ch] = ic2;

            if (ch == 0)
                pitchEnergy[(size_t) (b.note % 12)] += energy;

            if (ch == numCh - 1)
                b.gain = gain;
        }

        if (b.target <= 0.0f && b.gain <= 0.0f)
            for (int ch = 0; ch < kMaxChannels; ++ch)
                b.ic1[(size_t) ch] = b.ic2[(size_t) ch] = 0.0f;
    }

    // Auto gain: keep the wet signal about as loud as the input.
    double inSum = 0.0, wetSum = 0.0;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* in = buffer.getReadPointer (ch);
        const float* w = wet.getReadPointer (ch);
        for (int n = 0; n < numSamples; ++n)
        {
            inSum += (double) in[n] * in[n];
            wetSum += (double) w[n] * w[n];
        }
    }

    const float denom = (float) (numSamples * numCh);
    const float msCoef = blockCoef (fs, numSamples, 0.25);
    inMeanSquare += ((float) inSum / denom - inMeanSquare) * msCoef;
    wetMeanSquare += ((float) wetSum / denom - wetMeanSquare) * msCoef;

    float newGain = autoGain;
    if (inMeanSquare > 1.0e-8f && wetMeanSquare > 1.0e-12f)
    {
        const float target = juce::jlimit (0.25f, 40.0f, std::sqrt (inMeanSquare / wetMeanSquare));
        newGain = autoGain + (target - autoGain) * blockCoef (fs, numSamples, 0.15);
    }

    // Blend: Strength decides how much of the in-key resonance replaces the input,
    // Clear lets the dry transient through.
    const float strength = juce::jlimit (0.0f, 1.0f, p.strength);
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* io = buffer.getWritePointer (ch);
        const float* w = wet.getReadPointer (ch);

        for (int n = 0; n < numSamples; ++n)
        {
            const float g = autoGain + (newGain - autoGain) * ((float) n / (float) numSamples);
            const float s = strength * (1.0f - clear * transientCurve[(size_t) n]);
            io[n] = io[n] * (1.0f - s) + w[n] * g * s;
        }
    }

    autoGain = newGain;
    pitchEnergyFresh = true;
}
} // namespace prizm
