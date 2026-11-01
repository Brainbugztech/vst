#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include "engine/EngineHolder.h"
#include "dsp/PlaybackFx.h"
#include "dsp/VoiceFx.h"

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

class TtsProcessor : public juce::AudioProcessor,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::Timer
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
    double getTailLengthSeconds() const override { return 4.0; }

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
    void setVoice (const juce::String& v);
    bool isBritish() const { return british.load(); }
    void setBritish (bool b);
    juce::String getBlendVoice() const { const juce::ScopedLock sl (stateLock); return blendVoice; }
    void setBlendVoice (const juce::String& v);
    float getBlend() const { return blend.load(); }
    void setBlend (float b);

    // Text sprechen lassen (komplett neu) bzw. nur die Effekte neu rechnen
    void generate();
    bool isRendering() const { return rendering.load(); }
    float getRenderProgress() const { return renderProgress.load(); }
    juce::String getRenderError() const { const juce::ScopedLock sl (stateLock); return renderError; }

    void play();
    void stop();
    bool isPlaying() const { return playing.load(); }
    float getPlayhead() const { return playhead.load(); }   // 0..1
    float getLevel() const { return level.load(); }
    bool hasAudio() const;
    double getAudioSeconds() const;

    // Wellenform-Vorschau (Beträge, 0..1) und Versionszähler, damit die GUI weiß, wann neu zu zeichnen ist
    std::vector<float> getPeaks (int count) const;
    int getAudioVersion() const { return audioVersion.load(); }

    // WAV mit Effekten (für Save und Drag & Drop)
    bool exportWav (const juce::File& file, juce::String& error);
    juce::String suggestedFileName() const;

    static constexpr const char* pPitch = "pitch";
    static constexpr const char* pSpeed = "speed";
    static constexpr const char* pVolume = "volume";
    static constexpr const char* pFormant = "formant";
    static constexpr const char* pReverb = "reverb";
    static constexpr const char* pDelay = "delay";
    static constexpr const char* pEq = "eq";
    static constexpr const char* pFxOn = "fxOn";

private:
    struct Rendered
    {
        std::vector<float> audio; // 24 kHz mono, bearbeitet
    };

    struct RenderJob
    {
        tts::SynthRequest req;
        tts::fx::VoiceParams params;
        bool needsSynth = true;
    };

    class RenderThread;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float) override;
    void timerCallback() override;
    void startJob (bool resynth);
    void runJob (const RenderJob& job);
    tts::fx::VoiceParams currentVoiceParams() const;
    tts::SynthRequest currentRequest() const;
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void applyFx (juce::AudioBuffer<float>& b, int start, int num, double bpm);

    juce::SharedResourcePointer<tts::EngineHolder> holder;

    juce::CriticalSection stateLock;
    juce::String text, voice { "af_heart" }, blendVoice, renderError;
    std::atomic<bool> british { false };
    std::atomic<float> blend { 0.0f };

    // Render
    std::unique_ptr<RenderThread> renderThread;
    std::atomic<bool> rendering { false }, pendingRender { false };
    std::atomic<float> renderProgress { 0.0f };
    std::vector<float> rawCache;            // nur im Render-Thread benutzt
    juce::String rawKey;
    std::atomic<int> audioVersion { 0 };
    std::atomic<juce::uint32> paramsDirtyAt { 0 };
    bool generatedOnce = false;

    // Wiedergabe
    juce::SpinLock swapLock;
    std::shared_ptr<Rendered> current;      // nur unter swapLock tauschen
    std::atomic<bool> playing { false }, startRequest { false }, stopRequest { false };
    double readPos = 0.0, rate = 1.0, noteRatio = 1.0;
    float noteGain = 1.0f;
    std::atomic<float> playhead { 0.0f }, level { 0.0f };
    double hostRate = 48000.0;
    juce::AudioBuffer<float> work;
    tts::PlaybackFx fx;
    juce::SmoothedValue<float> gainSmooth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TtsProcessor)
};
