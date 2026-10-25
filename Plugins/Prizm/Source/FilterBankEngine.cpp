#include "FilterBankEngine.h"

#include <algorithm>
#include <cmath>

namespace prizm
{
namespace
{
    float onePoleCoef (double fs, double seconds) { return (float) (1.0 - std::exp (-1.0 / (fs * seconds))); }
    float blockCoef (double fs, int n, double seconds) { return (float) (1.0 - std::exp (-(double) n / (fs * seconds))); }
} // namespace

void FilterBankEngine::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    model.prepare (fs);

    bands.clear();
    const auto& notes = model.getNotes();
    const auto& g = model.getPrewarped();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        Band b;
        b.note = notes[i];
        b.g = (float) g[i];
        bands.push_back (b);
    }

    wet.setSize (kMaxChannels, maxBlockSize, false, false, true);
    dampCurve.assign ((size_t) maxBlockSize, 1.0f);

    fastAtt = onePoleCoef (fs, 0.001);
    fastRel = onePoleCoef (fs, 0.020);
    slowAtt = onePoleCoef (fs, 0.040);
    slowRel = onePoleCoef (fs, 0.250);
    transientRel = onePoleCoef (fs, 0.060);
    fadeStep = (float) (1.0 / (0.03 * fs)); // 30 ms bei Tonartwechsel

    reset();
}

void FilterBankEngine::reset()
{
    for (auto& b : bands)
    {
        b.ic1.fill (0.0f);
        b.ic2.fill (0.0f);
        b.fade = b.target;
        b.gateGain = 1.0f;
        b.level = 0.0f;
    }
    envFast = envSlow = transient = 0.0f;
}

void FilterBankEngine::process (juce::AudioBuffer<float>& buffer, int numSamples, const EngineParams& p)
{
    const int numCh = std::min (buffer.getNumChannels(), kMaxChannels);
    if (numCh == 0 || numSamples <= 0)
        return;

    if (p.mask != lastMask)
    {
        const bool first = (lastMask == 0xFFFF);
        for (auto& b : bands)
        {
            b.target = isAllowed (p.mask, b.note) ? 1.0f : 0.0f;
            if (first)
                b.fade = b.target;
        }
        lastMask = p.mask;
    }

    qSmoothed += (ResponseModel::qFromColor (p.color) - qSmoothed) * blockCoef (fs, numSamples, 0.05);
    const float k = 1.0f / qSmoothed;
    for (auto& b : bands)
    {
        b.a1 = 1.0f / (1.0f + b.g * (b.g + k));
        b.a2 = b.g * b.a1;
        b.a3 = b.g * b.a2;
    }

    // Clear: bei neuen Anschlägen klingen alte Resonanzen schneller ab
    const float clear = juce::jlimit (0.0f, 1.0f, p.clear);
    for (int n = 0; n < numSamples; ++n)
    {
        float x = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            x += std::abs (buffer.getSample (ch, n));
        x /= (float) numCh;

        envFast += (x - envFast) * (x > envFast ? fastAtt : fastRel);
        envSlow += (x - envSlow) * (x > envSlow ? slowAtt : slowRel);
        const float raw = envFast > 1.0e-4f ? juce::jlimit (0.0f, 1.0f, (envFast / (envSlow + 1.0e-6f) - 1.3f) / 1.5f) : 0.0f;
        transient = raw > transient ? raw : transient + (raw - transient) * transientRel;
        dampCurve[(size_t) n] = 1.0f - clear * transient * 0.004f;
    }

    // Gate: Zielverstärkung pro Note aus den Pegeln des letzten Blocks
    float loudest = 0.0f;
    for (auto& b : bands)
        loudest = std::max (loudest, b.level);
    const float gateCoef = blockCoef (fs, numSamples, 0.03);

    wet.clear (0, numSamples);

    for (auto& b : bands)
    {
        const float gateTarget = gateGain (b.level, loudest, p.gate);
        const float gateStart = b.gateGain;
        const float gateEnd = gateStart + (gateTarget - gateStart) * gateCoef;

        if (b.fade <= 0.0f && b.target <= 0.0f)
        {
            b.gateGain = gateEnd;
            continue;
        }

        const float fadeStart = b.fade;
        const float step = (b.target > b.fade ? fadeStep : -fadeStep);
        float energy = 0.0f, levelSum = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* in = buffer.getReadPointer (ch);
            float* out = wet.getWritePointer (ch);
            float ic1 = b.ic1[(size_t) ch], ic2 = b.ic2[(size_t) ch];
            float fade = fadeStart;

            for (int n = 0; n < numSamples; ++n)
            {
                const float v3 = in[n] - ic2;
                const float v1 = b.a1 * ic1 + b.a2 * v3;
                const float v2 = ic2 + b.a2 * ic1 + b.a3 * v3;
                const float damp = dampCurve[(size_t) n];
                ic1 = (2.0f * v1 - ic1) * damp;
                ic2 = (2.0f * v2 - ic2) * damp;

                fade = (step > 0.0f) ? std::min (b.target, fade + step) : std::max (b.target, fade + step);
                const float band = k * v1; // Spitze = 1
                levelSum += band * band;

                const float gate = gateStart + (gateEnd - gateStart) * ((float) n / (float) numSamples);
                const float y = fade * gate * band;
                out[n] += y;
                energy += y * y;
            }

            if (std::abs (ic1) < 1.0e-15f) ic1 = 0.0f;
            if (std::abs (ic2) < 1.0e-15f) ic2 = 0.0f;
            b.ic1[(size_t) ch] = ic1;
            b.ic2[(size_t) ch] = ic2;
            if (ch == numCh - 1)
                b.fade = fade;
        }

        // Gate-Hüllkurve: schnell hoch, langsam runter
        const float lvl = std::sqrt (levelSum / (float) (numSamples * numCh)) * b.target;
        b.level = lvl > b.level ? lvl : b.level + (lvl - b.level) * blockCoef (fs, numSamples, 0.08);
        b.gateGain = gateEnd;

        pitchEnergy[(size_t) (b.note % 12)] += energy;

        if (b.target <= 0.0f && b.fade <= 0.0f)
            for (int ch = 0; ch < kMaxChannels; ++ch)
                b.ic1[(size_t) ch] = b.ic2[(size_t) ch] = 0.0f;
    }

    for (int ch = 0; ch < numCh; ++ch)
        buffer.copyFrom (ch, 0, wet, ch, 0, numSamples);

    pitchEnergyFresh = true;
}
} // namespace prizm
