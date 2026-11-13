#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

// 1 = Instrument (BugWobble), 0 = Effekt (BugWobble FX); wird in CMake gesetzt
#ifndef BUGWOBBLE_IS_SYNTH
 #define BUGWOBBLE_IS_SYNTH 1
#endif

class BugWobbleProcessor : public juce::AudioProcessor
{
public:
    struct Preset
    {
        const char* name;
        int osc, driveType, filterType, shape, division;
        float cutoff, depth, resonance, drive, stereo, attack, decay, sustain, release;
    };

    static constexpr int numPresets = 20;

    BugWobbleProcessor();
    ~BugWobbleProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BugWobble"; }
    bool acceptsMidi() const override { return BUGWOBBLE_IS_SYNTH != 0; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets laufen nur über den Editor, nicht über die Programmliste des Hosts
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    int getCurrentPreset() const { return currentPreset; }
    void changeProgramName (int, const juce::String&) override {}

    void applyPreset (int index);
    juce::StringArray getPresetNames() const;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Monophone Stimme: letzte gedrückte Note gewinnt, Legato ohne Neustart der Hüllkurve
    void noteOn (int note, float vel);
    void noteOff (int note);
    void allNotesOff();
    void setNoteFrequency (int note);
    float renderVoice();

    // TPT-SVF (Simper) plus DC-Blocker pro Kanal
    struct ChannelState { float ic1 = 0.0f, ic2 = 0.0f, dcX1 = 0.0f, dcY1 = 0.0f; };
    std::array<ChannelState, 2> channels;

    enum class EnvStage { idle, attack, decay, sustain, release };

    double sampleRate = 44100.0;
    double lfoPhase = 0.0;
    double oscPhase = 0.0;
    float noteFreq = 440.0f;
    float velocity = 0.0f;
    float envLevel = 0.0f;
    EnvStage envStage = EnvStage::idle;
    std::vector<int> heldNotes;
    int currentNote = -1;
    int currentPreset = 0;

    // pro Block berechnet
    int oscShape = 2;
    float attackInc = 0.0f, decayInc = 0.0f, releaseInc = 0.0f, sustainLevel = 0.8f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BugWobbleProcessor)
};
