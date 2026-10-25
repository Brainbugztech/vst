#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

#include "FilterBankEngine.h"
#include "SpectralEngine.h"
#include "KeyAnalyzer.h"
#include "ChordTracker.h"
#include "Presets.h"
#include "Modulation.h"

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
    inline constexpr const char* stereo     = "stereo"; // 0 = Stereo, 1 = nur Mitte, 2 = nur Seite
    inline constexpr const char* scKey      = "scKey";  // Tonart aus dem Sidechain-Eingang
    inline juce::String mapId (int pc) { return "map" + juce::String (pc); } // map0 (C) .. map11 (B)
    inline juce::String weightId (int pc) { return "weight" + juce::String (pc); } // weight0 (C) .. weight11 (B)

    // Modulation: LFO n = 1, 2; Slots i = 0..7
    inline juce::String lfoShape (int n) { return "lfo" + juce::String (n) + "Shape"; }
    inline juce::String lfoSync (int n)  { return "lfo" + juce::String (n) + "Sync"; }
    inline juce::String lfoRate (int n)  { return "lfo" + juce::String (n) + "Rate"; }
    inline juce::String lfoDiv (int n)   { return "lfo" + juce::String (n) + "Div"; }
    inline juce::String lfoMode (int n)  { return "lfo" + juce::String (n) + "Mode"; } // Free / Retrig / Envelope
    inline constexpr const char* lfoTrig = "lfoTrig";                                   // Audio / MIDI / beides
    inline constexpr const char* envAttack  = "envAttack";
    inline constexpr const char* envRelease = "envRelease";
    inline constexpr const char* envSens    = "envSens";
    inline juce::String modSrc (int i) { return "modSrc" + juce::String (i); }
    inline juce::String modDst (int i) { return "modDst" + juce::String (i); }
    inline juce::String modAmt (int i) { return "modAmt" + juce::String (i); }
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
    prizm::PitchMask getDisplayMask() const noexcept { return (prizm::PitchMask) displayMask.load(); } // wirksam (inkl. Map)
    prizm::PitchMask getScaleMask() const noexcept { return (prizm::PitchMask) scaleMask.load(); }     // gewählte Tonleiter / MIDI / Sidechain
    bool isMidiControllingKey() const noexcept { return midiActive.load(); }
    bool isSidechainControllingKey() const noexcept { return scActive.load(); }
    bool isSidechainConnected() const noexcept { return scConnected.load(); }
    float getPitchEnergy (int pitchClass) const noexcept { return pitchEnergy[(size_t) pitchClass].load(); }
    // Eingehende Noten (nur Grundtöne) für die Pitch Matrix; wird nur gemessen, solange das Fenster offen ist
    float getInputEnergy (int pitchClass) const noexcept { return inputEnergy[(size_t) pitchClass].load(); }
    void setEditorOpen (bool isOpen) noexcept { editorOpen.store (isOpen); }
    uint32_t getBlockCounter() const noexcept { return blockCounter.load(); }
    void togglePitchClass (int pitchClass); // message thread: edits the custom scale
    // Noten-Map, Message-Thread: Vorlagen relativ zur Tonart
    enum class MapPreset { reset, majorToMinor, minorToMajor };
    void applyMapPreset (MapPreset preset);
    // Noten-Map einzeln, Message-Thread: 0 = normal, 1..12 = Ziel C..B, 13 = stumm
    void setMap (int pc, int index);
    int getMapIndex (int pc) const { return mapParams[(size_t) pc]->getIndex(); }
    // Presets, Message-Thread. Werkspresets zuerst, dann eigene (Dokumente/Brain Bugz/Prizm/Presets/*.przm)
    juce::StringArray getPresetNames() const;
    int getNumFactoryPresets() const { return (int) prizm::factoryPresets().size(); }
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    void refreshUserPresets();
    juce::File getPresetFolder() const;
    void setPresetFolder (const juce::File& folder) { presetFolderOverride = folder; refreshUserPresets(); } // für Tests
    int getCurrentPreset() const noexcept { return currentPreset; }
    juce::String getPresetHint (int index) const
    {
        const auto& f = prizm::factoryPresets();
        return index >= 0 && index < (int) f.size() ? juce::String::fromUTF8 (f[(size_t) index].hint) : juce::String();
    }
    static bool isKeyContextParam (const juce::String& id); // bleibt beim Preset-Wechsel stehen

    // Modulation, für den Editor
    float getModNow (int target) const noexcept { return modNow[(size_t) target].load(); } // modulierter Wert 0..1
    float getLfoValue (int lfo) const noexcept { return lfoValue[(size_t) lfo].load(); }    // -1..1
    float getLfoPhase (int lfo) const noexcept { return lfoPhase[(size_t) lfo].load(); }    // 0..1
    float getEnvValue() const noexcept { return envValue.load(); }                         // 0..1
    uint32_t getTrigCount (int lfo) const noexcept { return trigCount[(size_t) lfo].load(); } // zählt Retrig/Envelope-Starts
    // Message-Thread: Quelle auf ein Ziel legen (freier Slot, Tiefe 50 %). Gibt den Slot zurück oder -1, wenn alle belegt sind.
    int assignModulation (int source, int target);
    void clearModSlot (int slot);
    // Gezeichnete LFO-Kurven (lfo 0 = LFO 1). Message-Thread: setLfoCurve; Audio liest eine fertige Tabelle.
    const prizm::LfoCurve& getLfoCurve (int lfo) const noexcept { return lfoCurves[(size_t) lfo]; }
    void setLfoCurve (int lfo, std::vector<prizm::LfoCurve::Point> points);

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

    std::atomic<int> displayMask { 0 }, scaleMask { 0 };
    std::atomic<bool> midiActive { false }, scActive { false }, scConnected { false };
    prizm::ChordTracker chordTracker;
    prizm::ChordTracker inputMeter { 4 };
    std::array<std::atomic<float>, 12> inputEnergy {};
    std::atomic<bool> editorOpen { false };
    juce::AudioParameterBool* scKeyParam = nullptr;
    std::array<std::atomic<float>, 12> pitchEnergy {};
    std::atomic<uint32_t> blockCounter { 0 };

    juce::Array<juce::File> userPresets;
    juce::File presetFolderOverride;
    int currentPreset = 0;
    void setParamPlain (const juce::String& id, float value);

    // Modulation
    prizm::Lfo lfo1 { 1117 }, lfo2 { 2293 };
    std::array<prizm::LfoCurve, 2> lfoCurves;
    prizm::EnvFollower envFollower;
    std::array<juce::RangedAudioParameter*, prizm::kNumModTargets> targetParams {};
    std::array<float, prizm::kNumModTargets> modSmooth {};
    std::array<juce::AudioParameterChoice*, prizm::kNumModSlots> modSrcParams {}, modDstParams {};
    std::array<std::atomic<float>*, prizm::kNumModSlots> modAmtParams {};
    std::array<juce::AudioParameterChoice*, 2> lfoShapeParams {}, lfoDivParams {};
    std::array<juce::AudioParameterBool*, 2> lfoSyncParams {};
    std::array<juce::AudioParameterChoice*, 2> lfoModeParams {};
    juce::AudioParameterChoice* lfoTrigParam = nullptr;
    prizm::OnsetDetector onsetDetector;
    std::array<int, 64> noteOnPos {}; // Note-On-Positionen im aktuellen Block (für MIDI-Trigger)
    int numNoteOns = 0;
    std::array<std::atomic<uint32_t>, 2> trigCount {};
    std::array<std::atomic<float>*, 2> lfoRateParams {};
    std::atomic<float>* envAttackParam = nullptr;
    std::atomic<float>* envReleaseParam = nullptr;
    std::atomic<float>* envSensParam = nullptr;
    std::array<std::atomic<float>, prizm::kNumModTargets> modNow {};
    std::array<std::atomic<float>, 2> lfoValue {}, lfoPhase {};
    std::atomic<float> envValue { 0.0f };
    bool modulationActive() const;
    // Werte aller Ziele für einen Abschnitt (in Parametereinheiten), mit Modulation
    std::array<float, prizm::kNumModTargets> targetValues (bool modulate, int numSamples, const prizm::Transport& t,
                                                           const float* const* input, int numInputChannels, int chunkStart = 0);

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
    juce::AudioParameterChoice* stereoParam = nullptr;
    std::array<juce::AudioParameterChoice*, 12> mapParams {};
    std::array<std::atomic<float>*, 12> weightParams {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrizmAudioProcessor)
};
