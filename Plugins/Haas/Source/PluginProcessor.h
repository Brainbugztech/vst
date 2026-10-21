#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "HaasEngine.h"

namespace ParamIDs
{
    inline constexpr const char* width    = "width";
    inline constexpr const char* lms      = "lms";
    inline constexpr const char* rms      = "rms";
    inline constexpr const char* monoBass = "monobass";
    inline constexpr const char* flip     = "flip";
    inline constexpr const char* mono     = "monocheck";
}

class HaasProcessor : public juce::AudioProcessor
{
public:
    HaasProcessor();
    ~HaasProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Haas"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bb::HaasParams readParams() const;

    // Setzt die angezeigte Delay-Zeit einer Seite (0 = links, 1 = rechts) in ms
    void setDisplayedDelay (int side, float ms);
    void setParam (const char* id, float plainValue);

    juce::AudioProcessorValueTreeState apvts;

    // --- Werte für die Anzeige (Audio-Thread schreibt, Editor liest) ---
    static constexpr int kScopeSize = 256;
    std::array<std::atomic<float>, kScopeSize * 2> scope {};
    std::atomic<int>    scopeWrite { 0 };
    std::atomic<float>  vizEnv { 0.0f }, vizCorr { 1.0f };
    std::atomic<double> vizBeats { 0.0 }, vizBpm { 120.0 };
    std::atomic<bool>   vizPlaying { false };

private:
    bb::HaasEngine engine;
    double sr = 48000.0;
    double freeBeats = 0.0;
    double env = 0.0, sLL = 0.0, sRR = 0.0, sLR = 0.0;
    int scopeDecim = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaasProcessor)
};
