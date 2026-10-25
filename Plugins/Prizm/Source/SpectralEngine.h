#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

#include "EngineParams.h"
#include "ResponseModel.h"

namespace prizm
{
// High Quality: Phase-Vocoder. Jedes FFT-Bin bekommt die Verstärkung der gemeinsamen
// Filterkurve (ResponseModel) bei seiner gemessenen Frequenz, damit klingt es wie
// Zero Latency, aber ohne das Verschmieren der Filter. Ausklingen wie bei Resonatoren
// gleicher Güte. SHIFT verschiebt zusätzlich schiefe Teiltöne (Phase-Locking um Peaks).
// Latenz = eine FFT-Länge.
class SpectralEngine
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, int numSamples, const EngineParams& p);
    int getLatencySamples() const noexcept { return fftSize; }

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
    struct Channel
    {
        std::vector<float> input, output, frame;
        std::vector<float> mag, phase, lastPhase, trueBin;
        std::vector<float> outMag, outBin, outPhase, best;
        std::vector<float> synthPhase;
        std::vector<float> heldMag, heldBin, heldPhase;
        std::vector<float> gain;   // Filterkurve beim Teilton, zu dem das Bin gehört
        std::vector<float> gainSmooth; // über die Zeit geglättet (kein Flackern = keine Nebentöne)
        std::vector<int> gainNote; // Note dieses Teiltons (für das Gate)
    };

    void processFrame (const EngineParams& p);
    void analyse (Channel& c);
    void shiftStage (Channel& c, float shift, PitchMask mask);
    void computeGains (Channel& c);
    void smoothGains (Channel& c);
    void findRegions (const std::vector<float>& mags);
    void synthesise (Channel& c, float clearAmount);

    double fs = 48000.0;
    int fftOrder = 12, fftSize = 4096, hop = 1024, numBins = 2049;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::array<Channel, kMaxChannels> channels;
    std::vector<float> prevMixMag;
    std::vector<int> peaks, regionPeak;
    ResponseModel model;
    float qSmoothed = 40.0f;
    std::array<float, 128> noteLevel {}, noteSum {}, noteGate {};
    int numActiveChannels = 2;
    int writePos = 0, hopCounter = 0;
    float olaScale = 1.0f;
    float gainAttack = 0.5f, gainRelease = 0.5f;

    std::array<float, 12> pitchEnergy {};
    bool pitchEnergyFresh = false;
};
} // namespace prizm
