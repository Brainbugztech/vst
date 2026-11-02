#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include "Params.h"
#include "engine/EngineHolder.h"
#include "dsp/LpcPsola.h"
#include "dsp/GrainEngine.h"
#include "dsp/FxChain.h"
#include "dsp/Resampler.h"

// Eine Stimme aus der Kokoro-Liste
struct TtsVoice
{
    const char* id;
    const char* name;
    bool female;
    bool british;
};

inline const TtsVoice ttsVoices[] = {
    { "af_heart", "Heart", true, false },   { "af_bella", "Bella", true, false },     { "af_nicole", "Nicole", true, false },
    { "af_sarah", "Sarah", true, false },   { "af_sky", "Sky", true, false },         { "af_nova", "Nova", true, false },
    { "af_jessica", "Jessica", true, false }, { "af_river", "River", true, false },   { "af_alloy", "Alloy", true, false },
    { "af_aoede", "Aoede", true, false },   { "af_kore", "Kore", true, false },
    { "am_adam", "Adam", false, false },    { "am_michael", "Michael", false, false }, { "am_eric", "Eric", false, false },
    { "am_liam", "Liam", false, false },    { "am_onyx", "Onyx", false, false },      { "am_fenrir", "Fenrir", false, false },
    { "am_puck", "Puck", false, false },    { "am_echo", "Echo", false, false },      { "am_santa", "Santa", false, false },
    { "bf_emma", "Emma", true, true },      { "bf_isabella", "Isabella", true, true }, { "bf_alice", "Alice", true, true },
    { "bf_lily", "Lily", true, true },      { "bm_george", "George", false, true },   { "bm_lewis", "Lewis", false, true },
    { "bm_daniel", "Daniel", false, true }, { "bm_fable", "Fable", false, true } };

inline const TtsVoice* findVoice (const juce::String& id)
{
    for (auto& v : ttsVoices) if (id == v.id) return &v;
    return nullptr;
}

// Die komplette Echtzeit-Kette (Stimme -> Grain -> 24k->Host -> Volume -> FX). Wird im Audio-Thread und für den Export benutzt.
class TtsChain
{
public:
    struct Settings
    {
        float pitchSemis = 0, speed = 1, volumeDb = 0, formant = 1;
        bool grainOn = false;
        float grainMix = 0.7f;
        tts::dsp::GrainParams grain;
        tts::dsp::FxParams fx;
    };

    void prepare (double hostRate, int maxBlock);
    void setSource (std::shared_ptr<const tts::dsp::VoiceAnalysis> s) { voice.setSource (std::move (s)); }
    const std::shared_ptr<const tts::dsp::VoiceAnalysis>& getSource() const { return voice.getSource(); }
    void start (float noteSemis, float velocityGain);
    void stop();
    void process (float* L, float* R, int num, const Settings& s);
    bool voiceActive() const { return voice.isActive(); }
    bool anyActive() const { return voice.isActive() || grains.isBusy(); }
    double position() const { return voice.getPosition(); }
    double length() const { return voice.getLength(); }
    tts::dsp::GrainEngine& grainEngine() { return grains; }

private:
    void renderInternal (int n, const Settings& s);

    double hostRate = 48000.0;
    static constexpr int innerRate = 24000;
    tts::dsp::VoiceSynth voice;
    tts::dsp::GrainEngine grains;
    tts::dsp::FxChain fx;
    tts::dsp::SincResampler rsL, rsR;
    void clearFifo();
    std::vector<float> fifoL, fifoR, mono, gL, gR;
    int fifoCount = 0;
    float noteSemis = 0.0f, noteGain = 1.0f;
    juce::SmoothedValue<float> volume, grainMix;
};

class TtsProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    TtsProcessor();
    ~TtsProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz TTS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- für die GUI
    juce::AudioProcessorValueTreeState apvts;
    tts::EngineHolder& engineHolder() { return *holder; }

    juce::String getText() const { const juce::ScopedLock sl (stateLock); return text; }
    void setText (const juce::String& t) { const juce::ScopedLock sl (stateLock); text = t; }
    juce::String getVoice() const { const juce::ScopedLock sl (stateLock); return voice; }
    void setVoice (const juce::String& v) { const juce::ScopedLock sl (stateLock); voice = v; }
    bool isBritish() const { return british.load(); }
    void setBritish (bool b) { british = b; }
    juce::String getBlendVoice() const { const juce::ScopedLock sl (stateLock); return blendVoice; }
    void setBlendVoice (const juce::String& v) { const juce::ScopedLock sl (stateLock); blendVoice = v; }
    float getBlend() const { return blend.load(); }
    void setBlend (float b) { blend = juce::jlimit (0.0f, 1.0f, b); }
    int getUiTab() const { return uiTab.load(); }
    void setUiTab (int t) { uiTab = t; }

    void generate();
    bool isRendering() const { return rendering.load(); }
    float getRenderProgress() const { return renderProgress.load(); }
    juce::String getRenderError() const { const juce::ScopedLock sl (stateLock); return renderError; }

    void play() { if (hasAudio()) startRequest = true; }
    void stop() { stopRequest = true; }
    bool isPlaying() const { return playingFlag.load(); }
    bool isVoicePlaying() const { return voiceFlag.load(); }
    float getPlayhead() const { return playhead.load(); }
    float getLevel() const { return level.load(); }
    bool hasAudio() const;
    double getAudioSeconds() const;
    std::vector<float> getPeaks (int count) const;
    int getAudioVersion() const { return sourceVersion.load(); }
    int grainSnapshot (tts::dsp::GrainVis* out, int maxCount) { return chain.grainEngine().snapshot (out, maxCount); }

    bool exportWav (const juce::File& file, juce::String& error, std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr);
    // Export im Hintergrund (lange WAVs bei sehr langsamem Speed)
    void startExport (const juce::File& file, const juce::String& key);
    bool isExporting() const { return exporting.load(); }
    float getExportProgress() const { return exportProgress.load(); }
    // liefert true genau einmal, wenn ein Hintergrund-Export fertig ist
    bool takeExportResult (juce::File& file, juce::String& key, juce::String& error);
    juce::String exportKey() const;
    juce::String suggestedFileName() const;

    TtsChain::Settings readSettings (double bpm) const;

private:
    class RenderThread;
    struct Job { tts::SynthRequest req; };

    void timerCallback() override;
    void runJob (const Job& job);
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::SharedResourcePointer<tts::EngineHolder> holder;

    juce::CriticalSection stateLock;
    juce::String text, voice { "af_heart" }, blendVoice, renderError;
    std::atomic<bool> british { false };
    std::atomic<float> blend { 0.0f };
    std::atomic<int> uiTab { 0 };

    std::unique_ptr<RenderThread> renderThread;
    std::atomic<bool> rendering { false }, pendingRender { false };
    std::atomic<float> renderProgress { 0.0f };
    bool generatedOnce = false;

    // neue Stimme an den Audio-Thread übergeben (alte erst im Timer freigeben)
    juce::SpinLock sourceLock;
    std::shared_ptr<const tts::dsp::VoiceAnalysis> current;
    std::vector<std::shared_ptr<const tts::dsp::VoiceAnalysis>> retired;
    std::atomic<int> sourceVersion { 0 }, appliedVersion { 0 };

    std::unique_ptr<juce::Thread> exportThread;
    std::atomic<bool> exporting { false }, exportFinished { false }, exportCancel { false };
    std::atomic<float> exportProgress { 0.0f };
    juce::File exportFile;
    juce::String exportKeyStr, exportError;

    TtsChain chain;
    double hostRate = 48000.0;
    std::atomic<bool> startRequest { false }, stopRequest { false };
    std::atomic<bool> playingFlag { false }, voiceFlag { false };
    std::atomic<float> playhead { 0.0f }, level { 0.0f };
    std::atomic<juce::uint32> lastProcessMs { 0 };
    std::atomic<double> lastBpm { 120.0 };
    juce::AudioBuffer<float> work;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TtsProcessor)
};
