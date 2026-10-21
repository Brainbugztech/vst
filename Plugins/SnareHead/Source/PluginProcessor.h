#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SnareEngine.h"
#include "Presets.h"

// Snare-Synth: Ring (Resonator-Cluster) + Noise -> Res-EQ -> XTREME NOISY -> Reverb mit Gate -> Width/Out

namespace ParamIDs
{
    inline constexpr const char* tune      = "tune";
    inline constexpr const char* spread    = "spread";
    inline constexpr const char* metal     = "metal";
    inline constexpr const char* ring      = "ring";
    inline constexpr const char* punch     = "punch";
    inline constexpr const char* noise     = "noise";
    inline constexpr const char* noiseTone = "noise_tone";
    inline constexpr const char* hold      = "hold";
    inline constexpr const char* release   = "release";
    inline constexpr const char* eqFreq    = "eq_freq";
    inline constexpr const char* eqGain    = "eq_gain";
    inline constexpr const char* eqQ       = "eq_q";
    inline constexpr const char* eqLink    = "eq_link";
    inline constexpr const char* noisy     = "noisy";
    inline constexpr const char* verbSize  = "verb_size";
    inline constexpr const char* verbMix   = "verb_mix";
    inline constexpr const char* verbGate  = "verb_gate";
    inline constexpr const char* verbSync  = "verb_sync";
    inline constexpr const char* verbCut   = "verb_cut";
    inline constexpr const char* width     = "width";
    inline constexpr const char* volume    = "volume";
    inline constexpr const char* keyTrack  = "key_track";
}

class SnareHeadProcessor : public juce::AudioProcessor
{
public:
    SnareHeadProcessor();
    ~SnareHeadProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Snare Head"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    snarehead::Params readParams() const;

    juce::AudioProcessorValueTreeState apvts;

    // --- Presets (Message-Thread) ---
    juce::StringArray getPresetNames() const;
    int getNumFactoryPresets() const { return (int) factory.size(); }
    bool isFactoryNoisy (int index) const { return index >= 0 && index < (int) factory.size() && factory[(size_t) index].xtremeNoisy; }
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    void refreshUserPresets();
    juce::File getPresetFolder() const;
    int getCurrentPreset() const { return currentPreset; }
    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool isPresetModified() const;

    // --- UI ---
    void audition (float velocity = 0.9f) { pendingAudition.store (velocity); }
    std::atomic<int> hitCounter { 0 };
    std::atomic<float> lastVelocity { 0.9f };
    std::atomic<float> lastNoteHz { 800.0f };
    std::atomic<float> hostBpm { 145.0f };
    std::atomic<float> vizLevel { 0.0f };

private:
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParamPlain (const char* id, float value);
    juce::String fingerprint() const;
    void markClean() { cleanPrint = fingerprint(); }

    std::vector<snarehead::Preset> factory;
    juce::Array<juce::File> userPresets;
    int currentPreset = 0;
    juce::String currentPresetName { "Init" }, cleanPrint;

    snarehead::SnareEngine engine;
    std::atomic<float> pendingAudition { -1.0f };
    juce::AudioBuffer<float> scratch;
    float levelEnv = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareHeadProcessor)
};
