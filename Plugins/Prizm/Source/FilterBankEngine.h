#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

#include "EngineParams.h"

namespace prizm
{
// "Zero Latency" mode: a bank of resonant band-pass filters, one per in-key note
// across the whole audible range. The input excites the resonators, so the output
// only contains energy at in-key pitches. Color sets the resonance (Q); high values
// make the resonators ring on as a tail. Clear protects transients and damps old
// ringing when a new hit arrives. No latency.
class FilterBankEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLowestNote = 24;   // C1, about 33 Hz
    static constexpr int kHighestNote = 127; // G9, limited further by the sample rate

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, int numSamples, const EngineParams& p);

    // Output energy per pitch class (C..B) since the last call, for the key display.
    // Returns false if nothing new was produced.
    bool takePitchEnergy (std::array<float, 12>& out)
    {
        if (! pitchEnergyFresh)
            return false;
        out = pitchEnergy;
        pitchEnergy.fill (0.0f);
        pitchEnergyFresh = false;
        return true;
    }

private:
    struct Band
    {
        int note = 0;
        float g = 0.0f;            // tan(pi * f / fs)
        float gain = 0.0f;         // current fade gain (0..1)
        float target = 0.0f;       // 1 if the note is in key
        float a1 = 0, a2 = 0, a3 = 0;
        std::array<float, kMaxChannels> ic1 {}, ic2 {};
    };

    static float qFromColor (float color);

    double fs = 48000.0;
    std::vector<Band> bands;
    juce::AudioBuffer<float> wet;
    std::vector<float> transientCurve, dampCurve;

    PitchMask lastMask = 0xFFFF;
    float qSmoothed = 40.0f;

    float inMeanSquare = 0.0f, wetMeanSquare = 0.0f, autoGain = 1.0f;
    float envFast = 0.0f, envSlow = 0.0f, transient = 0.0f;
    float fastAtt = 0, fastRel = 0, slowAtt = 0, slowRel = 0, transientRel = 0;
    float fadeStep = 0.001f;
    std::array<float, 12> pitchEnergy {};
    bool pitchEnergyFresh = false;
};
} // namespace prizm
