#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BleepGenerator.h"

// Etappe 2: Generator live per MIDI spielbar (Square + stumme Triangle mit PD,
// Pitch-Env, LFO, Amp-Env). Slots, Arp und FX folgen in den naechsten Etappen.

namespace ParamIDs
{
    inline constexpr const char* sqOct     = "sq_oct";
    inline constexpr const char* sqLevel   = "sq_level";
    inline constexpr const char* sqPw      = "sq_pw";
    inline constexpr const char* modOct    = "mod_oct";
    inline constexpr const char* modPd     = "mod_pd";
    inline constexpr const char* pitchLink = "pitch_link";
    inline constexpr const char* penvAmt   = "penv_amt";
    inline constexpr const char* penvDecay = "penv_decay";
    inline constexpr const char* lfoRate   = "lfo_rate";
    inline constexpr const char* lfoPitch  = "lfo_pitch";
    inline constexpr const char* lfoPd     = "lfo_pd";
    inline constexpr const char* ampA      = "amp_a";
    inline constexpr const char* ampD      = "amp_d";
    inline constexpr const char* ampS      = "amp_s";
    inline constexpr const char* ampR      = "amp_r";
    inline constexpr const char* output    = "output";
}

struct BleepSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class BleepVoice : public juce::SynthesiserVoice
{
public:
    explicit BleepVoice (const bleep::GenParams& sharedParams) : params (sharedParams) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<BleepSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    const bleep::GenParams& params;
    bleep::BleepCore core;
};

class BleepProcessor : public juce::AudioProcessor
{
public:
    BleepProcessor();
    ~BleepProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Bleep Designer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bleep::GenParams readGenParams() const;

    juce::AudioProcessorValueTreeState apvts;

    // --- Werte fuer die Anzeige (Audio-Thread schreibt, Editor liest) ---
    std::atomic<int> lastNote { -1 };
    std::atomic<int> noteCounter { 0 };
    std::atomic<float> vizLevel { 0.0f };

private:
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    static constexpr int kNumVoices = 8;
    bleep::GenParams genParams;   // wird pro Block aktualisiert, alle Voices lesen davon
    juce::Synthesiser synth;
    juce::SmoothedValue<float> outGain;
    float env = 0.0f;
    float dcX[2] {}, dcY[2] {};
    float dcR = 0.999f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepProcessor)
};
