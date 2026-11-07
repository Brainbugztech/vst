#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ClubEQEngine.h"

namespace ParamIDs
{
    inline constexpr const char* hi    = "hi";     // Knopfposition 0…1, 0,5 = 0 dB
    inline constexpr const char* mid   = "mid";
    inline constexpr const char* low   = "low";
    inline constexpr const char* iso   = "iso";    // 0 = EQ (-26 dB wie DJM-900), 1 = ISO (ganz links = weg)
    inline constexpr const char* xlow  = "xlow";   // Trennfrequenz LOW|MID
    inline constexpr const char* xhigh = "xhigh";  // Trennfrequenz MID|HI
}

class ClubEQProcessor : public juce::AudioProcessor
{
public:
    ClubEQProcessor();
    ~ClubEQProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Club EQ"; }
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

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bool isoMode() const noexcept { return pIso != nullptr && pIso->load() > 0.5f; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void pushParams (bool snap);

    ceq::Engine engine;
    std::atomic<float>* pHi = nullptr;
    std::atomic<float>* pMid = nullptr;
    std::atomic<float>* pLow = nullptr;
    std::atomic<float>* pIso = nullptr;
    std::atomic<float>* pXLow = nullptr;
    std::atomic<float>* pXHigh = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClubEQProcessor)
};
