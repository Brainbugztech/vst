#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SnareEngine.h"
#include "Presets.h"

// Snare-Synth: Ring (Resonator-Cluster) + Noise -> Res-EQ -> XTREME NOISY -> Reverb mit Gate -> Width/Out

namespace ParamIDs
{
    inline constexpr const char* tune      = "tune";
    inline constexpr const char* spread    = "spread";
    inline constexpr const char* metal     = "metal";
    inline constexpr const char* ring      = "ring";
    inline constexpr const char* punch     = "punch";
    inline constexpr const char* noise     = "noise";
    inline constexpr const char* noiseTone = "noise_tone";
    inline constexpr const char* hold      = "hold";
    inline constexpr const char* release   = "release";
    inline constexpr const char* eqFreq    = "eq_freq";
    inline constexpr const char* eqGain    = "eq_gain";
    inline constexpr const char* eqQ       = "eq_q";
    inline constexpr const char* eqLink    = "eq_link";
    inline constexpr const char* noisy     = "noisy";
    inline constexpr const char* verbSize  = "verb_size";
    inline constexpr const char* verbMix   = "verb_mix";
    inline constexpr const char* verbGate  = "verb_gate";
    inline constexpr const char* verbSync  = "verb_sync";
    inline constexpr const char* verbCut   = "verb_cut";
    inline constexpr const char* width     = "width";
    inline constexpr const char* volume    = "volume";
    inline constexpr const char* keyTrack  = "key_track";
    inline constexpr const char* smpLevel  = "smp_level";
    inline constexpr const char* smpPitch  = "smp_pitch";
    inline constexpr const char* smpStart  = "smp_start";
    inline constexpr const char* smpRev    = "smp_reverse";
    inline constexpr const char* smpShape  = "smp_shape";
}

// Frequenz-Text: "880", "1.2k", "A", "A5", "F#5", "Bb" -> Hz (Noten ohne/mit falscher Oktave landen im Bereich lo..hi)
float parseFreqText (const juce::String& text, float lo, float hi);
juce::String noteNameForHz (float hz, bool withCents = true);

class SnareHeadProcessor : public juce::AudioProcessor
{
public:
    SnareHeadProcessor();
    ~SnareHeadProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Snare Head"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    snarehead::Params readParams() const;

    juce::AudioProcessorValueTreeState apvts;

    // --- Presets (Message-Thread) ---
    juce::StringArray getPresetNames() const;
    int getNumFactoryPresets() const { return (int) factory.size(); }
    bool isFactoryNoisy (int index) const { return index >= 0 && index < (int) factory.size() && factory[(size_t) index].xtremeNoisy; }
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    void refreshUserPresets();
    juce::File getPresetFolder() const;
    int getCurrentPreset() const { return currentPreset; }
    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool isPresetModified() const;

    // --- Eigenes Sample (Message-Thread) ---
    bool loadSampleFile (const juce::File& file, juce::String& error);
    void clearSample();
    std::shared_ptr<const snarehead::SampleData> getSample() const;
    juce::String getSampleName() const { return sampleName; }
    std::atomic<int> sampleVersion { 0 };   // zaehlt hoch, wenn sich das Sample aendert (fuer die Anzeige)
    float analyzeSample();                  // staerkste Resonanz 650 Hz - 1,7 kHz, 0 = nichts gefunden
    static bool isSupportedAudioFile (const juce::String& path);

    // Hz/Note-Anzeige (wird im Projekt gespeichert)
    bool getNoteMode (const juce::String& key) const { return (bool) apvts.state.getProperty (key, false); }
    void setNoteMode (const juce::String& key, bool on) { apvts.state.setProperty (key, on, nullptr); }

    // --- UI ---
    void audition (float velocity = 0.9f) { pendingAudition.store (velocity); }
    std::atomic<int> hitCounter { 0 };
    std::atomic<float> lastVelocity { 0.9f };
    std::atomic<float> lastNoteHz { 800.0f };
    std::atomic<float> hostBpm { 145.0f };
    std::atomic<float> vizLevel { 0.0f };

private:
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool setSampleFromWav (const juce::MemoryBlock& wav, const juce::String& name);
    void addSampleTo (juce::ValueTree& state) const;
    void restoreSampleFrom (const juce::ValueTree& state, bool clearIfMissing);
    void timerPurge();

    // Sample: Message-Thread setzt "sample", Audio-Thread holt sich die Kopie per try-lock.
    // Alte Samples bleiben in "retired", bis der Audio-Thread sie sicher losgelassen hat.
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<const snarehead::SampleData> sample, audioSample;
    std::vector<std::shared_ptr<const snarehead::SampleData>> retired;
    juce::MemoryBlock sampleWav;   // Original als WAV fuer Projekt/Preset
    juce::String sampleName;
    double currentSR = 48000.0;
    std::unique_ptr<juce::Timer> purgeTimer;
    void setParamPlain (const char* id, float value);
    juce::String fingerprint() const;
    void markClean() { cleanPrint = fingerprint(); }

    std::vector<snarehead::Preset> factory;
    juce::Array<juce::File> userPresets;
    int currentPreset = 0;
    juce::String currentPresetName { "Init" }, cleanPrint;

    snarehead::SnareEngine engine;
    std::atomic<float> pendingAudition { -1.0f };
    juce::AudioBuffer<float> scratch;
    float levelEnv = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareHeadProcessor)
};
