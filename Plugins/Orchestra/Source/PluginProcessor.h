#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"

namespace orch
{
// Werkspreset: Name, Kategorie, Parameter, die vom Standard abweichen
struct PresetValue { const char* id; float value; };
struct Preset { const char* name; const char* category; std::vector<PresetValue> values; };
const std::vector<Preset>& factoryPresets();
}

class OrchestraProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    OrchestraProcessor();
    ~OrchestraProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Orchestra"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Eigene Presets (Ordner: AppData/Brain Bugz/Orchestra/Presets)
    static juce::File userPresetFolder();
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String currentPresetName;

    void setParam (const char* id, float value);
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<orch::SampleBank> bank;
    orch::Engine engine;
    juce::MidiKeyboardState keyboardState;
    std::atomic<bool> loading { true };
    std::atomic<int> presetChanged { 0 };
    std::atomic<int> padNote { -1 };      // Klick auf Drum-Pad in der GUI
    int currentPreset = 0;

private:
    void timerCallback() override;
    orch::Settings readSettings() const;

    std::set<int> wanted;
    int lastArticParam = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchestraProcessor)
};
