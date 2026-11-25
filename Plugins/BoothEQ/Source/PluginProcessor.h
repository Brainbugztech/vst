#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BoothEQEngine.h"

// Parameter-IDs (stabil, nie umbenennen). Reihenfolge = Launch Control XL: Knopf 1, 2, 3, (kein Fader), Taster 1, 2.
namespace ParamIDs
{
    inline constexpr const char* low     = "low";      // Knopf 1: Knopfposition 0…1, 0,5 = 0 dB
    inline constexpr const char* mid     = "mid";      // Knopf 2
    inline constexpr const char* high    = "high";     // Knopf 3
    inline constexpr const char* killLow = "killLow";  // Taster 1 (Hold)
    inline constexpr const char* killMid = "killMid";  // Taster 2 (Hold)
    inline constexpr const char* curve   = "curve";    // Extra: 0 = Isolator (links = Kill), 1 = EQ (links = -26 dB)
    inline constexpr const char* xlow    = "xlow";     // Extra: Trennfrequenz LOW|MID
    inline constexpr const char* xhigh   = "xhigh";    // Extra: Trennfrequenz MID|HIGH
}

class BoothEQProcessor : public juce::AudioProcessor
{
public:
    BoothEQProcessor();
    ~BoothEQProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Booth EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (std::function<bool()> isIso);
    bool isoCurve() const noexcept { return pCurve == nullptr || pCurve->load() < 0.5f; }

    // für Anzeige und Test
    double bandGain (int band) const noexcept { return engine.bandGain (band); }
    const beq::Engine& getEngine() const noexcept { return engine; }
    beq::Engine& getEngine() noexcept { return engine; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void pushParams (bool snap);

    beq::Engine engine;
    std::atomic<float>* pLow = nullptr;
    std::atomic<float>* pMid = nullptr;
    std::atomic<float>* pHigh = nullptr;
    std::atomic<float>* pKillLow = nullptr;
    std::atomic<float>* pKillMid = nullptr;
    std::atomic<float>* pCurve = nullptr;
    std::atomic<float>* pXLow = nullptr;
    std::atomic<float>* pXHigh = nullptr;
    bool lastIso = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoothEQProcessor)
};
