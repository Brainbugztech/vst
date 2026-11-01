#pragma once

// Live-Effekte auf der Ausgabe: Tilt-EQ, Ping-Pong-Delay (punktierte Achtel zum Host-Tempo), Reverb

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace tts
{
class PlaybackFx
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate > 0 ? sampleRate : 48000.0;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlock), 1 };
        for (auto* f : { &lowL, &lowR, &highL, &highR }) { f->prepare (spec); f->reset(); }
        const int maxDelay = (int) (sr * 2.5) + 8;
        delayBuf.setSize (2, maxDelay, false, true, true);
        delayBuf.clear();
        writePos = 0;
        reverb.setSampleRate (sr);
        reverb.reset();
        lastEq = 1000.0f;
        delaySmoothed.reset (sr, 0.25);
        delaySmoothed.setCurrentAndTargetValue (delayTimeFor (120.0));
    }

    void reset()
    {
        for (auto* f : { &lowL, &lowR, &highL, &highR }) f->reset();
        delayBuf.clear();
        reverb.reset();
    }

    // eqDb: -12..+12 (positiv = heller), delayMix/reverbMix: 0..1
    void process (juce::AudioBuffer<float>& b, int start, int num, float eqDb, float delayMix, float reverbMix, double bpm)
    {
        if (num <= 0 || b.getNumChannels() < 2) return;
        auto* L = b.getWritePointer (0, start);
        auto* R = b.getWritePointer (1, start);

        // Tilt-EQ
        if (std::abs (eqDb - lastEq) > 0.01f)
        {
            lastEq = eqDb;
            const float g = eqDb * 0.5f;
            auto lo = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 900.0, 0.7071f, juce::Decibels::decibelsToGain (-g));
            auto hi = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1100.0, 0.7071f, juce::Decibels::decibelsToGain (g));
            *lowL.coefficients = *lo; *lowR.coefficients = *lo;
            *highL.coefficients = *hi; *highR.coefficients = *hi;
        }
        if (std::abs (eqDb) > 0.05f)
            for (int i = 0; i < num; ++i)
            {
                L[i] = highL.processSample (lowL.processSample (L[i]));
                R[i] = highR.processSample (lowR.processSample (R[i]));
            }

        // Ping-Pong-Delay
        delaySmoothed.setTargetValue (delayTimeFor (bpm));
        const int len = delayBuf.getNumSamples();
        auto* dl = delayBuf.getWritePointer (0);
        auto* dr = delayBuf.getWritePointer (1);
        const float fb = 0.38f;
        const float wet = delayMix * 0.75f;
        for (int i = 0; i < num; ++i)
        {
            const float d = delaySmoothed.getNextValue();
            float rp = (float) writePos - d;
            while (rp < 0) rp += (float) len;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % len;
            const float fr = rp - (float) i0;
            const float yl = dl[i0] + fr * (dl[i1] - dl[i0]);
            const float yr = dr[i0] + fr * (dr[i1] - dr[i0]);
            const float in = 0.5f * (L[i] + R[i]);
            // links bekommt den Eingang, rechts das Echo von links -> springt hin und her
            dl[writePos] = in * (delayMix > 0.0001f ? 1.0f : 0.0f) + yr * fb;
            dr[writePos] = yl * fb;
            writePos = (writePos + 1) % len;
            L[i] += yl * wet;
            R[i] += yr * wet;
        }

        // Reverb
        if (reverbMix > 0.001f)
        {
            juce::Reverb::Parameters p;
            p.roomSize = 0.78f;
            p.damping = 0.45f;
            p.width = 1.0f;
            p.wetLevel = reverbMix * 0.55f;
            p.dryLevel = 1.0f - reverbMix * 0.25f;
            reverb.setParameters (p);
            reverb.processStereo (L, R, num);
        }
    }

private:
    float delayTimeFor (double bpm) const
    {
        const double b = (bpm > 20.0 && bpm < 400.0) ? bpm : 120.0;
        return (float) juce::jlimit (1.0, sr * 2.4, sr * 60.0 / b * 0.75); // punktierte Achtel
    }

    double sr = 48000.0;
    juce::dsp::IIR::Filter<float> lowL, lowR, highL, highR;
    float lastEq = 1000.0f;
    juce::AudioBuffer<float> delayBuf;
    int writePos = 0;
    juce::SmoothedValue<float> delaySmoothed;
    juce::Reverb reverb;
};
} // namespace tts
