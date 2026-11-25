#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

namespace orch
{
struct Preset { const char* name; int mode, artic; float attack, release, tone, reverb, space, width, ensemble, humanize, air, shimmer, motion, punch, decay; };

// Werkspresets: attack/release in Sekunden, tone -1..1, Rest 0..1 (width 0..2)
inline const Preset factoryPresets[] =
{
    { "Epic Strings",        M_STRINGS, A_SUS,  0.08f, 0.6f,  0.15f, 0.35f, 0.70f, 1.2f, 0.45f, 0.25f, 0, 0, 0, 0, 1 },
    { "Soft Strings",        M_STRINGS, A_SUS,  0.45f, 1.2f, -0.35f, 0.40f, 0.75f, 1.1f, 0.35f, 0.15f, 0, 0, 0, 0, 1 },
    { "Spiccato Drive",      M_STRINGS, A_SPIC, 0.00f, 0.2f,  0.30f, 0.22f, 0.50f, 1.1f, 0.00f, 0.35f, 0, 0, 0, 0, 1 },
    { "Pizzicato Hall",      M_STRINGS, A_PIZZ, 0.00f, 0.4f,  0.05f, 0.35f, 0.65f, 1.2f, 0.00f, 0.30f, 0, 0, 0, 0, 1 },
    { "Tremolo Tension",     M_STRINGS, A_TREM, 0.25f, 0.8f,  0.10f, 0.40f, 0.80f, 1.3f, 0.30f, 0.20f, 0, 0, 0, 0, 1 },
    { "Cello Section",       M_CELLOS,  A_SUS,  0.10f, 0.7f,  0.00f, 0.30f, 0.65f, 1.0f, 0.40f, 0.20f, 0, 0, 0, 0, 1 },
    { "Dark Cellos",         M_CELLOS,  A_SUS,  0.35f, 1.0f, -0.55f, 0.38f, 0.80f, 1.1f, 0.50f, 0.15f, 0, 0, 0, 0, 1 },
    { "Cello Ostinato",      M_CELLOS,  A_SPIC, 0.00f, 0.2f,  0.20f, 0.20f, 0.50f, 1.0f, 0.00f, 0.40f, 0, 0, 0, 0, 1 },
    { "Cello Pizz",          M_CELLOS,  A_PIZZ, 0.00f, 0.4f,  0.00f, 0.30f, 0.60f, 1.0f, 0.00f, 0.30f, 0, 0, 0, 0, 1 },
    { "Atmos Dark Matter",   M_ATMOS,   A_SUS,  1.20f, 3.0f, -0.45f, 0.60f, 0.90f, 1.5f, 0, 0, 0.45f, 0.15f, 0.55f, 0, 1 },
    { "Atmos Shimmer Sky",   M_ATMOS,   A_SUS,  0.80f, 2.5f,  0.25f, 0.65f, 0.95f, 1.6f, 0, 0, 0.30f, 0.60f, 0.40f, 0, 1 },
    { "Atmos Pure Strings",  M_ATMOS,   A_SUS,  0.60f, 2.0f,  0.00f, 0.50f, 0.85f, 1.4f, 0, 0, 0.00f, 0.20f, 0.25f, 0, 1 },
    { "Cinematic Drums",     M_DRUMS,   A_SUS,  0.00f, 0.3f,  0.20f, 0.35f, 0.75f, 1.1f, 0, 0, 0, 0, 0, 0.45f, 1.0f },
    { "Tight Battle Drums",  M_DRUMS,   A_SUS,  0.00f, 0.3f,  0.35f, 0.18f, 0.45f, 1.0f, 0, 0, 0, 0, 0, 0.70f, 0.45f },
    { "Huge Hall Drums",     M_DRUMS,   A_SUS,  0.00f, 0.3f,  0.00f, 0.60f, 1.00f, 1.3f, 0, 0, 0, 0, 0, 0.25f, 1.0f },
};
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

    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<orch::SampleBank> bank;
    orch::Engine engine;
    std::atomic<bool> loading { true };
    std::atomic<int> presetChanged { 0 };
    std::atomic<int> padNote { -1 };   // Klick auf Drum-Pad in der GUI
    int currentPreset = 0;

private:
    void timerCallback() override;
    orch::Settings readSettings() const;

    std::atomic<float> *pMode, *pArtic, *pAttack, *pRelease, *pTone, *pTune, *pReverb, *pSpace, *pWidth, *pVolume,
                       *pEnsemble, *pHumanize, *pAir, *pShimmer, *pMotion, *pPunch, *pDecay;
    std::set<int> wanted;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrchestraProcessor)
};
