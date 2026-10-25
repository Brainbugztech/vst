#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

#include "FilterBankEngine.h"
#include "SpectralEngine.h"
#include "KeyAnalyzer.h"

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
    inline constexpr const char* shift      = "shift";
    inline constexpr const char* gate       = "gate";
    inline constexpr const char* rangeLow   = "rangeLow";
    inline constexpr const char* rangeHigh  = "rangeHigh";
    inline constexpr const char* tune       = "tune";
    inline constexpr const char* formant    = "formant";
    inline juce::String mapId (int pc) { return "map" + juce::String (pc); } // map0 (C) .. map11 (B)
    inline juce::String weightId (int pc) { return "weight" + juce::String (pc); } // weight0 (C) .. weight11 (B)
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
    double getTailLengthSeconds() const override { return 5.0; }

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
    // Noten-Map, Message-Thread: Vorlagen relativ zur Tonart
    enum class MapPreset { reset, majorToMinor, minorToMajor };
    void applyMapPreset (MapPreset preset);
    // Gewichtung pro Note, Message-Thread (0..1)
    float getWeight (int pc) const noexcept { return weightParams[(size_t) pc]->load() * 0.01f; }
    void setWeight (int pc, float weight01, bool gestureStart, bool gestureEnd);

    // Learn: Tonart und Stimmton aus dem Eingang erkennen
    void startLearn() noexcept { learnCommand.store (1); }
    void cancelLearn() noexcept { learnCommand.store (2); }
    bool isLearning() const noexcept { return learnActive.load(); }
    float getLearnProgress() const noexcept { return learnProgress.load(); }
    // Message-Thread: übernimmt ein fertiges Ergebnis in die Parameter. Gibt true zurück, wenn eins da war.
    bool applyLearnResult (prizm::KeyAnalyzer::Result& out);

private:
    prizm::PitchMask computeMask();
    void handleMidi (const juce::MidiBuffer& midi);

    prizm::FilterBankEngine filterBank;
    prizm::SpectralEngine spectral;

    // Gemeinsamer Signalweg beider Modi (Bereich, Pegelausgleich, Strength, Clear, Mix)
    struct DelayLine
    {
        std::array<std::vector<float>, 2> data;
        int pos = 0;
        void prepare (int length) { for (auto& d : data) d.assign ((size_t) juce::jmax (1, length), 0.0f); pos = 0; }
        void clear() { for (auto& d : data) std::fill (d.begin(), d.end(), 0.0f); pos = 0; }
        float process (int ch, float x)
        {
            auto& d = data[(size_t) ch];
            const float y = d[(size_t) pos];
            d[(size_t) pos] = x;
            return y;
        }
        void advance() { pos = (pos + 1) % (int) data[0].size(); }
    };

    void resetSignalPath();

    juce::AudioBuffer<float> midBuffer, dryMidBuffer, outerBuffer;
    DelayLine dryMidDelay, outerDelay;
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit, lowAllpass;
    bool splitActive = false;
    double sampleRateHz = 48000.0;

    float inMeanSquare = 0.0f, wetMeanSquare = 0.0f, autoGain = 1.0f;
    float envFast = 0.0f, envSlow = 0.0f, transient = 0.0f;
    float fastAtt = 0, fastRel = 0, slowAtt = 0, slowRel = 0, transientRel = 0;

    std::array<bool, 128> heldNotes {};
    prizm::PitchMask latchedMidiMask = 0;

    int currentMode = -1;
    juce::SmoothedValue<float> mixSmoothed, outputSmoothed;

    std::atomic<int> displayMask { 0 };
    std::atomic<bool> midiActive { false };
    std::array<std::atomic<float>, 12> pitchEnergy {};
    std::atomic<uint32_t> blockCounter { 0 };

    prizm::KeyAnalyzer keyAnalyzer;
    std::atomic<int> learnCommand { 0 }; // 1 = starten, 2 = abbrechen
    std::atomic<bool> learnActive { false }, learnDone { false };
    std::atomic<float> learnProgress { 0.0f };
    std::atomic<int> learnedKey { 0 };
    std::atomic<bool> learnedMinor { false };
    std::atomic<float> learnedA4 { 440.0f }, learnedConfidence { 0.0f };

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
    std::atomic<float>* shiftParam = nullptr;
    std::atomic<float>* gateParam = nullptr;
    std::atomic<float>* rangeLowParam = nullptr;
    std::atomic<float>* rangeHighParam = nullptr;
    std::atomic<float>* tuneParam = nullptr;
    std::atomic<float>* formantParam = nullptr;
    std::array<juce::AudioParameterChoice*, 12> mapParams {};
    std::array<std::atomic<float>*, 12> weightParams {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrizmAudioProcessor)
};
