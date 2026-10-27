#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/BraincutEngine.h"
#include "Presets.h"

namespace ParamIDs
{
    // Slicer
    inline constexpr const char* mode    = "mode";
    inline constexpr const char* loop    = "loop";
    inline constexpr const char* phMin   = "phMin";
    inline constexpr const char* phMax   = "phMax";
    inline constexpr const char* repMin  = "repMin";
    inline constexpr const char* repMax  = "repMax";
    inline constexpr const char* subdiv  = "subdiv";
    inline constexpr const char* repeat  = "repeat";
    inline constexpr const char* seed    = "seed";
    inline constexpr const char* lock    = "lock";
    // Stretch
    inline constexpr const char* strOn   = "strOn";
    inline constexpr const char* strAmt  = "strAmt";
    inline constexpr const char* strMix  = "strMix";
    // Repeat
    inline constexpr const char* repOn     = "repOn";
    inline constexpr const char* repChance = "repChance";
    inline constexpr const char* repRate   = "repRate";
    inline constexpr const char* repGate   = "repGate";
    inline constexpr const char* repDecay  = "repDecay";
    // Glitch
    inline constexpr const char* gliOn   = "gliOn";
    inline constexpr const char* gliAmt  = "gliAmt";
    inline constexpr const char* gliMix  = "gliMix";
    inline constexpr const char* gBit    = "gBit";
    inline constexpr const char* gRev    = "gRev";
    inline constexpr const char* gStop   = "gStop";
    inline constexpr const char* gFrz    = "gFrz";
    inline constexpr const char* gPit    = "gPit";
    // Ausgang
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* out     = "out";
}

class BraincutProcessor : public juce::AudioProcessor
{
public:
    BraincutProcessor();
    ~BraincutProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Braincut"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Die Werks-Presets erscheinen auch als Programme im Host
    int getNumPrograms() override { return bcpresets::kNumPresets; }
    int getCurrentProgram() override { return getPresetIndex(); }
    void setCurrentProgram (int index) override { applyPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bc::EngineParams readParams() const;

    void applyPreset (int index);
    int  getPresetIndex() const;
    void setParam (const char* id, float plainValue);
    void rollSeed();

    juce::AudioProcessorValueTreeState apvts;
    bc::BraincutEngine engine;

    std::atomic<bool> vizPlaying { false };

private:
    double sr = 48000.0;
    double freeBeats = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BraincutProcessor)
};
