#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Etappe 1: Grundgeruest. Instrument mit MIDI-Eingang und einem einfachen
// Test-Ton pro Note. Generator, Slots, Arp und FX folgen in den naechsten Etappen.

namespace ParamIDs
{
    inline constexpr const char* output = "output";
}

struct TestSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Kurzer Sinus-Blip mit perkussiver Huellkurve
class TestVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<TestSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    double phase = 0.0, inc = 0.0;
    float level = 0.0f;
    juce::ADSR adsr;
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
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // --- Werte fuer die Anzeige (Audio-Thread schreibt, Editor liest) ---
    std::atomic<int> lastNote { -1 };
    std::atomic<int> noteCounter { 0 };   // zaehlt jedes Note-On, fuer die Blink-Anzeige
    std::atomic<float> vizLevel { 0.0f };

private:
    static constexpr int kNumVoices = 8;
    juce::Synthesiser synth;
    juce::SmoothedValue<float> outGain;
    float env = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepProcessor)
};
