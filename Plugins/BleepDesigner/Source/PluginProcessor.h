#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BleepGenerator.h"
#include "SlotSampler.h"
#include "Arpeggiator.h"
#include "FxChain.h"
#include "Presets.h"
#include "BleepCharacter.h"

// Signalweg: Generator -> 4 Slots -> Arp/Sampler -> (Etappe 6: Comb, Crush, Filter, Delay)

namespace ParamIDs
{
    inline constexpr const char* sqOct     = "sq_oct";
    inline constexpr const char* sqLevel   = "sq_level";
    inline constexpr const char* sqPw      = "sq_pw";
    inline constexpr const char* shape     = "osc_shape";
    inline constexpr const char* modFine   = "mod_fine";
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
    inline constexpr const char* slotLen   = "slot_len";
    inline constexpr const char* rootNote  = "root_note";
    inline constexpr const char* source    = "source";    // 0 = Live, 1 = Slots
    inline constexpr const char* variation = "variation";
    inline constexpr const char* slotMode  = "slot_mode";
    inline constexpr const char* arpOn     = "arp_on";
    inline constexpr const char* arpRate   = "arp_rate";
    inline constexpr const char* arpSteps  = "arp_steps";
    inline constexpr const char* arpSwing  = "arp_swing";
    inline constexpr const char* arpGate   = "arp_gate";
    inline constexpr const char* arpMode   = "arp_mode";
    inline constexpr const char* arpScale  = "arp_scale";
    inline constexpr const char* arpRange  = "arp_range";
    inline constexpr const char* arpRestart= "arp_restart";
    inline constexpr const char* density   = "roll_density";
    inline constexpr const char* autoRoll  = "roll_auto";
    inline constexpr const char* randMode  = "rand_mode";   // 0 = Seed, 1 = Free
    inline constexpr const char* seed      = "seed";

    inline constexpr const char* combOn    = "comb_on";
    inline constexpr const char* combFreq  = "comb_freq";
    inline constexpr const char* combFb    = "comb_fb";
    inline constexpr const char* combDamp  = "comb_damp";
    inline constexpr const char* combKey   = "comb_key";
    inline constexpr const char* combCount = "comb_count";
    inline constexpr const char* combRatio2= "comb_ratio2";
    inline constexpr const char* combRatio3= "comb_ratio3";
    inline constexpr const char* combMix   = "comb_mix";
    inline constexpr const char* crushOn   = "crush_on";
    inline constexpr const char* crushBits = "crush_bits";
    inline constexpr const char* crushRate = "crush_rate";
    inline constexpr const char* crushMix  = "crush_mix";
    inline constexpr const char* filterOn  = "filter_on";
    inline constexpr const char* filterType= "filter_type";
    inline constexpr const char* filterCut = "filter_cut";
    inline constexpr const char* filterRes = "filter_res";
    inline constexpr const char* filterEnv = "filter_env";
    inline constexpr const char* filterDec = "filter_decay";
    inline constexpr const char* filterKey = "filter_key";
    inline constexpr const char* delayOn   = "delay_on";
    inline constexpr const char* delayTime = "delay_time";
    inline constexpr const char* delayFb   = "delay_fb";
    inline constexpr const char* delayPP   = "delay_pp";
    inline constexpr const char* delayLow  = "delay_lowcut";
    inline constexpr const char* delayHigh = "delay_highcut";
    inline constexpr const char* delayMix  = "delay_mix";

    inline juce::String slotLock (int i)   { return "slot_lock_" + juce::String (i + 1); }
    inline juce::String slotWeight (int i) { return "slot_weight_" + juce::String (i + 1); }
    inline juce::String slotLevel (int i)  { return "slot_level_" + juce::String (i + 1); }
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

class BleepProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int kNumSlots = bleep::kNumSlots;

    BleepProcessor();
    ~BleepProcessor() override { stopTimer(); }

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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bleep::GenParams readGenParams() const;
    bleep::ArpSettings readArpSettings() const;
    bleep::FxParams readFxParams() const;

    juce::AudioProcessorValueTreeState apvts;

    // --- Slots (alles Message-Thread) ---
    void generate();               // neuer Seed, alle nicht gesperrten Slots neu
    void renderSlots();            // gleicher Seed, aktuelle Generator-Einstellungen
    void auditionSlot (int index); // Slot einmal auf der Root-Note vorhoeren
    bleep::SlotBuffer::Ptr getSlot (int index) const;

    // --- Presets (Message-Thread) ---
    // Liste: erst die Werkspresets, dann die eigenen aus dem Preset-Ordner
    juce::StringArray getPresetNames() const;
    int getNumFactoryPresets() const { return (int) bleep::factoryPresets().size(); }
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    void refreshUserPresets();
    juce::File getPresetFolder() const;
    int getCurrentPreset() const { return currentPreset; }
    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool isPresetModified();   // Einstellungen seit dem Laden/Speichern geaendert?

    // --- Arp: Steps werden von UI und Audio-Thread lock-frei geteilt ---
    bleep::Arpeggiator arp;
    void rollSteps();

    // --- Werte fuer die Anzeige (Audio-Thread schreibt, Editor liest) ---
    std::atomic<int> lastNote { -1 };
    std::atomic<int> noteCounter { 0 };
    std::array<std::atomic<int>, kNumSlots> slotHits {};
    std::atomic<int> accentHits { 0 };   // jeder Akzent-Step = ein Laserschuss im Fenster
    std::atomic<int> accentSlot { 0 };
    std::atomic<float> vizLevel { 0.0f };

private:
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    float raw (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParamPlain (const juce::String& id, float value);
    void timerCallback() override;
    void setSlot (int index, bleep::SlotBuffer::Ptr newSlot);
    std::unique_ptr<juce::XmlElement> createStateXml();
    void applyStateXml (const juce::XmlElement& xml);
    juce::String fingerprint() const;
    void markClean() { cleanPrint = fingerprint(); }

    juce::Array<juce::File> userPresets;
    int currentPreset = 0;
    juce::String currentPresetName { "Init" }, cleanPrint;
    void purgeSlots();

    static constexpr int kNumVoices = 8;
    bleep::GenParams genParams;   // wird pro Block aktualisiert, alle Live-Voices lesen davon
    juce::Synthesiser liveSynth, slotSynth;
    int lastSource = 0, directSlot = 0;
    juce::MidiBuffer emptyMidi, arpMidi, slotMidi;

    // "slots" setzt der Message-Thread, "audioSlots" ist die Kopie des Audio-Threads.
    // "pool" haelt alle Puffer am Leben, bis kein Voice sie mehr benutzt (geloescht wird nur im Timer).
    mutable juce::SpinLock slotLock;
    bleep::SlotArray slots, audioSlots;
    std::array<float, kNumSlots> audioLevels { 1.0f, 1.0f, 1.0f, 1.0f };
    juce::ReferenceCountedArray<bleep::SlotBuffer> pool;
    juce::CriticalSection poolLock;
    std::atomic<double> currentSR { 48000.0 };
    std::atomic<int> pendingAudition { -1 };
    int lastRenderedSeed = -1;

    bleep::FxChain fx;
    std::array<bleep::FxTrigger, 256> fxTriggers {};

    juce::SmoothedValue<float> outGain;
    float env = 0.0f;
    float dcX[2] {}, dcY[2] {};
    float dcR = 0.999f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepProcessor)
};
