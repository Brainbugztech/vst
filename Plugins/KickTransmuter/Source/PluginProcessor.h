#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/TransmuterEngine.h"

namespace ParamIDs
{
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* sustain  = "sustain";
    inline constexpr const char* punch    = "punch";
    inline constexpr const char* sat      = "saturation";
    inline constexpr const char* lowEnd   = "lowend";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* output   = "output";
    inline constexpr const char* autoGain = "autogain";
}

class KickTransmuterProcessor : public juce::AudioProcessor
{
public:
    KickTransmuterProcessor();
    ~KickTransmuterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Kick Transmuter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    ktm::Engine engine;

private:
    ktm::Params readParams() const;

    // Beim Bypass bleibt das Signal um die gemeldete Latenz verzögert, damit nichts versetzt
    std::vector<float> bypassDelay[2];
    int bypassPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickTransmuterProcessor)
};
