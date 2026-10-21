#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BugFuzzEngine.h"

namespace ParamIDs
{
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* sat      = "saturation";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* out      = "out";
    inline constexpr const char* band[4]  { "band1", "band2", "band3", "band4" };
    inline constexpr const char* xover[3] { "xover1", "xover2", "xover3" };
    inline constexpr const char* octa     = "octa";
    inline constexpr const char* autoGain = "autogain";
}

class BugFuzzProcessor : public juce::AudioProcessor
{
public:
    BugFuzzProcessor();
    ~BugFuzzProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz BugFuzz"; }
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

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bb::FuzzParams readParams() const;
    void setParam (const char* id, float plainValue);

    static constexpr float kBandDefaults[4]  { 0.55f, 0.8f, 0.85f, 0.6f };
    static constexpr float kXoverDefaults[3] { 160.0f, 900.0f, 4200.0f };

    juce::AudioProcessorValueTreeState apvts;

    // --- Werte für die Anzeige (Audio-Thread schreibt, Editor liest) ---
    static constexpr int kScopeSize = 512;
    std::array<std::atomic<float>, kScopeSize> scope {};
    std::atomic<int>    scopeWrite { 0 };
    std::array<std::atomic<float>, 4> vizBand {};
    std::atomic<float>  vizEnv { 0.0f };
    std::atomic<double> vizBeats { 0.0 }, vizBpm { 120.0 };
    std::atomic<bool>   vizPlaying { false };

private:
    bb::BugFuzzEngine engine;
    double sr = 48000.0;
    int maxBlock = 512;
    double freeBeats = 0.0;
    double env = 0.0;
    int scopeDecim = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BugFuzzProcessor)
};
