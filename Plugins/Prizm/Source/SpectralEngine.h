#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

#include "EngineParams.h"

namespace prizm
{
// "High Quality" mode: a phase vocoder. Each FFT bin's true frequency is measured,
// mapped to the nearest in-key pitch and resynthesised there, so out-of-key partials
// are moved (not just filtered away). Color sharpens the spectral peaks (0..100 %)
// and adds a spectral sustain tail above 100 %. Clear resets phases on transients
// and lets the original attack through. Latency = one FFT frame.
class SpectralEngine
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate);
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

    int getLatencySamples() const noexcept { return fftSize; }

private:
    struct Channel
    {
        std::vector<float> input, output;            // circular, fftSize long
        std::vector<float> frame;                     // 2 * fftSize (JUCE real FFT layout)
        std::vector<float> mag, phase, lastPhase, trueBin;
        std::vector<float> outMag, outBin, bestMag;
        std::vector<float> synthPhase, tailMag, tailBin;
    };

    void processFrame (const EngineParams& p);
    void analyse (Channel& c);
    void synthesise (Channel& c, const EngineParams& p, float transientAmount);

    double fs = 48000.0;
    int fftOrder = 12, fftSize = 4096, hop = 1024, numBins = 2049;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, prefix;
    std::array<Channel, kMaxChannels> channels;
    std::vector<float> prevMixMag;
    int numActiveChannels = 2;
    int writePos = 0, hopCounter = 0;
    float olaScale = 1.0f;
    std::array<float, 12> pitchEnergy {};
    bool pitchEnergyFresh = false;
};
} // namespace prizm
