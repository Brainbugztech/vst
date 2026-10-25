#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

#include "EngineParams.h"
#include "ResponseModel.h"

namespace prizm
{
// Zero Latency: die Bandpass-Bank des ResponseModel direkt als Filter gerechnet.
// Liefert nur das gefilterte Signal; Pegelausgleich, Strength und Mix macht der Processor.
class FilterBankEngine
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, int numSamples, const EngineParams& p);

    // Ausgangsenergie pro Tonklasse seit dem letzten Aufruf (für die Noten-Anzeige).
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
        float g = 0.0f;           // tan(pi * f / fs)
        float fade = 0.0f;        // Ein-/Ausblenden bei Tonartwechsel
        float target = 0.0f;      // 1 = Note in der Tonart
        float gateGain = 1.0f;
        float level = 0.0f;       // Hüllkurve für das Gate
        float a1 = 0, a2 = 0, a3 = 0;
        std::array<float, kMaxChannels> ic1 {}, ic2 {};
    };

    ResponseModel model;
    double fs = 48000.0;
    std::vector<Band> bands;
    juce::AudioBuffer<float> wet;
    std::vector<float> dampCurve;

    PitchMask lastMask = 0xFFFF;
    float currentA4 = 440.0f;
    void retune (float a4);
    float qSmoothed = 40.0f;
    float envFast = 0.0f, envSlow = 0.0f, transient = 0.0f;
    float fastAtt = 0, fastRel = 0, slowAtt = 0, slowRel = 0, transientRel = 0;
    float fadeStep = 0.001f;

    std::array<float, 12> pitchEnergy {};
    bool pitchEnergyFresh = false;
};
} // namespace prizm
