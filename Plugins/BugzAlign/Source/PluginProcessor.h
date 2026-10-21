#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AlignEngine.h"

namespace ParamIDs
{
    inline constexpr const char* amount   = "amount";
    inline constexpr const char* offset   = "offset";
    inline constexpr const char* split    = "split";
    inline constexpr const char* window   = "window";
    inline constexpr const char* glide    = "glide";
    inline constexpr const char* sens     = "sens";
    inline constexpr const char* hold     = "hold";
    inline constexpr const char* bypass   = "ab";
    inline constexpr const char* subCheck = "subcheck";
}

// Plugin auf den Bass-Kanal, Kick per Sidechain ("Audio From" -> Kick-Spur)
class BugzAlignProcessor : public juce::AudioProcessor
{
public:
    BugzAlignProcessor();
    ~BugzAlignProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Bugz Align"; }
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
    bb::AlignParams readParams() const;

    juce::AudioProcessorValueTreeState apvts;
    bb::AlignEngine engine;

    std::atomic<bool>  sidechainConnected { false };
    std::atomic<float> latencyMs { 0.0f };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BugzAlignProcessor)
};
