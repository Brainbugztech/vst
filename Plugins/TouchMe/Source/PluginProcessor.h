#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Presets.h"

// Eingangs-Monitor für das Waveform-Display (lock-frei: Audio schreibt, GUI liest)
struct WaveMonitor
{
    static constexpr int size = 512;
    float mn[size] {}, mx[size] {};
    std::atomic<int> writePos { 0 };
    int bucketLen = 100, count = 0;
    float curMin = 0, curMax = 0;

    void prepare (double sr) { bucketLen = std::max (16, (int) (sr * 1.6 / size)); count = 0; curMin = curMax = 0; }
    void push (float v)
    {
        if (count == 0) { curMin = curMax = v; }
        curMin = std::min (curMin, v); curMax = std::max (curMax, v);
        if (++count >= bucketLen)
        {
            const int w = writePos.load (std::memory_order_relaxed);
            mn[w] = curMin; mx[w] = curMax;
            writePos.store ((w + 1) % size, std::memory_order_release);
            count = 0;
        }
    }
};

class TouchMeProcessor : public juce::AudioProcessor
{
public:
    TouchMeProcessor();
    ~TouchMeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return swarm::numPresets(); }
    int getCurrentProgram() override { return activeProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Preset aus der GUI laden: Effekt + Startposition + Dry/Wet
    void selectProgram (int idx, bool applyPadAndMix);

    std::atomic<int> activeProgram { 0 }; // was gerade wirklich klingt (GUI-Anzeige)
    std::atomic<float> envMeter { 0.0f };
    WaveMonitor wave;

private:
    void loadProgram (int idx);
    static void setParam (juce::RangedAudioParameter* p, float plainValue);

    swarm::AlgoBank bank[swarm::kSlots];
    swarm::SlotDef slot[swarm::kSlots];
    swarm::Algo* algo[swarm::kSlots] { nullptr, nullptr, nullptr };
    double lfoPhase[swarm::kSlots] {};
    float lfoHold[swarm::kSlots] {};
    juce::Random lfoRnd;
    int currentProgram = -1, lastProgParam = -1, lastFxParam = -1;

    std::atomic<float>* pX = nullptr; std::atomic<float>* pY = nullptr;
    std::atomic<float>* pTouch = nullptr; std::atomic<float>* pHold = nullptr;
    std::atomic<float>* pMix = nullptr; std::atomic<float>* pProgram = nullptr;
    std::atomic<float>* pFx = nullptr;

    double sr = 44100.0;
    float xs = 0.5f, ys = 0.5f, env = 0.0f, mixSm = 0.75f;
    bool wasTouched = false;
    int ctrlCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TouchMeProcessor)
};
