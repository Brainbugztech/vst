#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "StepDelayEngine.h"

namespace ParamIDs
{
    inline constexpr const char* steps  = "steps";
    inline constexpr const char* sync   = "sync";
    inline constexpr const char* rate   = "rate";
    inline constexpr const char* mode   = "mode";
    inline constexpr const char* timeMs = "timems";
    inline constexpr const char* mix    = "mix";
    inline constexpr const char* out    = "out";
    inline constexpr const char* loCut  = "locut";
    inline constexpr const char* hiCut  = "hicut";

    // Lautstärke pro Step: "lvl1" … "lvl32"
    inline juce::String level (int step) { return "lvl" + juce::String (step + 1); }
}

class StepDelayProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kMaxSteps = bb::StepDelayParams::kMaxSteps;

    StepDelayProcessor();
    ~StepDelayProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Step Delay"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return bb::StepDelayEngine::kMaxSeconds; }

    // Presets als Programme, damit Ableton sie auch im Geräte-Menü zeigt
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Hilfen für den Editor (Message-Thread)
    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const;
    float getLevel (int step) const { return levelPtr[(size_t) step]->load(); }
    void setLevel (int step, float v) { setParam (ParamIDs::level (step), juce::jlimit (0.0f, 1.0f, v)); }

    // Musterwerkzeuge
    void randomizePattern();
    void resetPattern();
    void invertPattern();
    void shiftPattern (int dir);

    // Länge eines Steps in ms bei aktuellem Tempo (für die Anzeige)
    double stepLengthMs() const;
    juce::String gridName() const;

    juce::AudioProcessorValueTreeState apvts;

    // Anzeige: Pegel jedes Echos (Audio-Thread schreibt, Editor liest)
    std::array<std::atomic<float>, kMaxSteps> vizTap {};
    std::atomic<double> vizBpm { 120.0 };

private:
    bb::StepDelayEngine engine;
    bb::StepDelayParams params;
    double sr = 48000.0;
    int currentPreset = 0;
    juce::Random rng;

    std::array<std::atomic<float>*, kMaxSteps> levelPtr {};
    std::atomic<float>* pSteps = nullptr; std::atomic<float>* pSync = nullptr;
    std::atomic<float>* pRate = nullptr;  std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pTime = nullptr;  std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOut = nullptr;   std::atomic<float>* pLo = nullptr;
    std::atomic<float>* pHi = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepDelayProcessor)
};
