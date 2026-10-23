#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ParamSpec.h"
#include "dsp/Presets.h"

// Brainbugz Brainfuzz: 4-Band-Saettigung mit Width/Spread, Crush/Chorus/Reverb pro Band

// Lock-freier Ringpuffer fuer Anzeige-Daten (Audio-Thread schreibt, Editor liest die letzten N Werte)
template <int Size>
struct VizRing
{
    std::array<std::atomic<float>, (size_t) Size> data {};
    std::atomic<int> write { 0 };

    void push (const float* x, int n, int step = 1)
    {
        int w = write.load (std::memory_order_relaxed);
        for (int i = 0; i < n; i += step)
        {
            data[(size_t) w].store (x[i], std::memory_order_relaxed);
            w = (w + 1) % Size;
        }
        write.store (w, std::memory_order_release);
    }
    // die letzten n Werte, aeltester zuerst
    void read (float* out, int n) const
    {
        const int w = write.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            out[i] = data[(size_t) ((w - n + i + Size * 4) % Size)].load (std::memory_order_relaxed);
    }
};

class BrainfuzzProcessor : public juce::AudioProcessor
{
public:
    BrainfuzzProcessor();
    ~BrainfuzzProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Brainfuzz"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String formatValue (const bfz::PSpec& s, float v);

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;

    juce::RangedAudioParameter* param (int index) const { return params[(size_t) index]; }
    float value (int index) const { return raw[(size_t) index]->load(); }
    void setValue (int index, float plain);       // mit Gesten (fuer UI-Aktionen)
    juce::String paramID (int index) const { return bfz::paramSpecs()[(size_t) index].id; }

    // --- Presets (Message-Thread) ---
    struct PresetInfo { juce::String name, group; bool user = false; };
    const std::vector<PresetInfo>& getPresetList() const { return presetList; }
    int getNumFactoryPresets() const { return (int) factory.size(); }
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    void refreshUserPresets();
    juce::File getPresetFolder() const;
    int getCurrentPreset() const { return currentPreset; }
    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool isPresetModified() const;

    // Preset-Sperren: Input, Output, Effekte, Tight bleiben beim Durchblaettern
    enum Lock { lockInput = 0, lockOutput, lockFx, lockTight, kNumLocks };
    std::array<bool, kNumLocks> locks { false, false, false, false };

    // UI-Zustand, der im Projekt mitgespeichert wird
    std::array<int, 4> fxTab { 0, 0, 0, 0 };
    float uiScale = 1.0f;
    bool ioLink = false;

    // --- Anzeige ---
    static constexpr int kSpecRing = 8192, kScopeRing = 1024;
    std::array<VizRing<kSpecRing>, 4> bandSpec;   // Mono-Summe pro Band
    VizRing<kSpecRing> inSpec, outSpec;
    std::array<VizRing<kScopeRing>, 4> scopeL, scopeR; // Stereo pro Band (jedes 2. Sample)
    std::array<std::atomic<float>, 4> bandLevel {};
    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::atomic<float> limiterGain { 1.0f };
    std::atomic<double> sampleRateViz { 48000.0 };

private:
    void markClean() { cleanPrint = fingerprint(); }
    juce::String fingerprint() const;
    bool isLocked (int paramIndex) const;

    std::array<juce::RangedAudioParameter*, bfz::kNumParams> params {};
    std::array<std::atomic<float>*, bfz::kNumParams> raw {};
    std::array<float, bfz::kNumParams> values {};

    bfz::BrainFuzzEngine engine;
    int maxBlock = 512;
    std::vector<float> monoTmp, scopeTmpL, scopeTmpR;
    float inEnv[2] {}, outEnv[2] {};
    std::array<float, 4> bandEnv {};

    std::vector<bfz::FactoryPreset> factory;
    juce::Array<juce::File> userPresets;
    std::vector<PresetInfo> presetList;
    int currentPreset = 0;
    juce::String currentPresetName { "Init" }, cleanPrint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrainfuzzProcessor)
};
