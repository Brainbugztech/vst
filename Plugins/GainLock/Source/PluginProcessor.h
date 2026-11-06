#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GainLockEngine.h"

namespace ParamIDs
{
    inline constexpr const char* role   = "role";
    inline constexpr const char* group  = "group";
    inline constexpr const char* speed  = "speed";
    inline constexpr const char* metric = "metric";
    inline constexpr const char* trim   = "trim";
    inline constexpr const char* range  = "range";
    inline constexpr const char* hold   = "hold";
    inline constexpr const char* listen = "listen";
}

// Zwei Instanzen pro Kette:
//   QUELLE an den Anfang (reicht das Signal unverändert durch und teilt es),
//   LOCK ans Ende (misst Anfang und Ende und gleicht die Lautheit automatisch an).
// Ohne Quelle nimmt LOCK den Sidechain-Eingang als Referenz.
class GainLockProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    enum Role { roleSource = 0, roleLock = 1 };
    enum RefKind { refNone = 0, refSource = 1, refSidechain = 2 };

    GainLockProcessor();
    ~GainLockProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Gain Lock"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateTrackProperties (const TrackProperties& properties) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Anzeige-Werte für die GUI (Audio-Thread schreibt, GUI liest)
    struct Status
    {
        std::atomic<int>   role { roleLock };
        std::atomic<int>   refKind { refNone };
        std::atomic<bool>  sourceStale { false };    // Quelle da, aber verarbeitet gerade nichts
        std::atomic<bool>  groupTaken { false };     // Quelle: Gruppe hat schon eine andere Quelle
        std::atomic<bool>  lockSeen { false };       // Quelle: ein Lock hört auf diese Gruppe
        std::atomic<bool>  rateMismatch { false };
        std::atomic<float> refLufs { -180.0f };      // Referenz (Quelle / Sidechain)
        std::atomic<float> postLufs { -180.0f };     // Ende der Kette, vor der Korrektur
        std::atomic<float> outLufs { -180.0f };      // nach der Korrektur
        std::atomic<float> gainDb { 0.0f };
        std::atomic<float> targetDb { 0.0f };
        std::atomic<float> latencyMs { 0.0f };
        std::atomic<bool>  latencyKnown { false };
        std::atomic<bool>  polarityFlip { false };
        std::atomic<float> refPeak { 0.0f };         // für die Pegel-Anzeige (Spitze seit letztem Abholen)
        std::atomic<float> outPeak { 0.0f };
    } status;

    juce::String getLinkedTrackName();
    int getGroupIndex() const;

private:
    void timerCallback() override;
    void processSource (juce::AudioBuffer<float>& io, int n);
    void processLock (juce::AudioBuffer<float>& io, int n);
    void releaseSourceSlot();
    void resetMeasurement();
    void startAnalysis();
    void stopAnalysis();

    // Analyse-Thread: sucht die Latenz der Kette zwischen Quelle und Lock
    class AnalysisThread : public juce::Thread
    {
    public:
        explicit AnalysisThread (GainLockProcessor& p) : juce::Thread ("GainLock Latenz"), owner (p) {}
        void run() override;
    private:
        GainLockProcessor& owner;
    };
    friend class AnalysisThread;

    const int instanceId;
    double sr = 48000.0;

    std::atomic<float>* pRole = nullptr;
    std::atomic<float>* pGroup = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pMetric = nullptr;
    std::atomic<float>* pTrim = nullptr;
    std::atomic<float>* pRange = nullptr;
    std::atomic<float>* pHold = nullptr;
    std::atomic<float>* pListen = nullptr;

    int ownedGroup = -1;            // Quelle: welche Gruppe gehört uns gerade
    int lastRole = -1, lastGroup = -1, lastRef = -1, lastMetric = -1;

    gl::LoudnessMeter refMeter, postMeter, outMeter;
    gl::GainComputer gc;
    float abMix = 0.0f;             // 0 = bearbeitet, 1 = Original
    float trimSmoothed = 0.0f;

    // Lock: eigenes Eingangssignal (mono) für die Latenz-Analyse
    std::unique_ptr<std::atomic<float>[]> lockMono;
    std::atomic<int64_t> lockWritePos { 0 };
    std::atomic<int64_t> offsetD { 0 };        // Quell-Index = Lock-Index + D (bei Latenz 0)
    std::atomic<bool> offsetValid { false };
    std::atomic<int> latencySamples { 0 };
    uint32_t lastHeartbeat = 0;
    int staleBlocks = 0;

    std::unique_ptr<AnalysisThread> analysis;
    std::unique_ptr<gl::LatencyFinder> finder;
    std::mutex finderLock;

    juce::SpinLock nameLock;
    juce::String trackName;

    std::atomic<float> restoredGain { 0.0f };
    std::atomic<bool> hasRestoredGain { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainLockProcessor)
};
