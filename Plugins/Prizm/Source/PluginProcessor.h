#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

#include "FilterBankEngine.h"
#include "SpectralEngine.h"

namespace prizm
{
namespace ParamIDs
{
    inline constexpr const char* key        = "key";
    inline constexpr const char* scale      = "scale";
    inline constexpr const char* customMask = "customMask";
    inline constexpr const char* midiKey    = "midiKey";
    inline constexpr const char* mode       = "mode";
    inline constexpr const char* strength   = "strength";
    inline constexpr const char* color      = "color";
    inline constexpr const char* clear      = "clear";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* output     = "output";
}

enum Mode { zeroLatency = 0, highQuality = 1 };
} // namespace prizm

class PrizmAudioProcessor : public juce::AudioProcessor
{
public:
    PrizmAudioProcessor();
    ~PrizmAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Prizm"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // For the editor.
    juce::AudioProcessorValueTreeState apvts;
    prizm::PitchMask getDisplayMask() const noexcept { return (prizm::PitchMask) displayMask.load(); }
    bool isMidiControllingKey() const noexcept { return midiActive.load(); }
    float getPitchEnergy (int pitchClass) const noexcept { return pitchEnergy[(size_t) pitchClass].load(); }
    uint32_t getBlockCounter() const noexcept { return blockCounter.load(); }
    void togglePitchClass (int pitchClass); // message thread: edits the custom scale

private:
    prizm::PitchMask computeMask();
    void handleMidi (const juce::MidiBuffer& midi);

    prizm::FilterBankEngine filterBank;
    prizm::SpectralEngine spectral;

    juce::AudioBuffer<float> dryBuffer;
    std::array<std::vector<float>, 2> dryDelay;
    int dryDelayPos = 0;

    std::array<bool, 128> heldNotes {};
    prizm::PitchMask latchedMidiMask = 0;

    int currentMode = -1;
    juce::SmoothedValue<float> mixSmoothed, outputSmoothed;

    std::atomic<int> displayMask { 0 };
    std::atomic<bool> midiActive { false };
    std::array<std::atomic<float>, 12> pitchEnergy {};
    std::atomic<uint32_t> blockCounter { 0 };

    juce::AudioParameterChoice* keyParam = nullptr;
    juce::AudioParameterChoice* scaleParam = nullptr;
    juce::AudioParameterInt* customMaskParam = nullptr;
    juce::AudioParameterBool* midiKeyParam = nullptr;
    juce::AudioParameterChoice* modeParam = nullptr;
    std::atomic<float>* strengthParam = nullptr;
    std::atomic<float>* colorParam = nullptr;
    std::atomic<float>* clearParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrizmAudioProcessor)
};
