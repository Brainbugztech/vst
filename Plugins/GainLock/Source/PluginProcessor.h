#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GainLockEngine.h"

namespace ParamIDs
{
    inline constexpr const char* role    = "role";
    inline constexpr const char* group   = "group";
    inline constexpr const char* point   = "point";
    inline constexpr const char* speed   = "speed";
    inline constexpr const char* metric  = "metric";
    inline constexpr const char* trim    = "trim";
    inline constexpr const char* range   = "range";
    inline constexpr const char* hold    = "hold";
    inline constexpr const char* monitor = "monitor";
}

// Instanzen pro Kette (alle in derselben Gruppe):
//   SOURCE an den Anfang (reicht durch und teilt das Signal),
//   POINT 1-3 optional dazwischen (Vergleichspunkte, z. B. "nach EQ"),
//   LOCK ans Ende (gleicht die Lautheit an, Monitor, Delta, Blindtest).
// Ohne Source nimmt LOCK den Sidechain-Eingang als Referenz.
class GainLockProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    enum Role { roleSource = 0, rolePoint = 1, roleLock = 2 };
    enum RefKind { refNone = 0, refSource = 1, refSidechain = 2 };
    // Monitor-Quellen (Reihenfolge = Parameter "monitor")
    enum Mon { monFinal = 0, monOriginal = 1, monP1 = 2, monP2 = 3, monP3 = 4, monDelta = 5, kNumMon = 6 };
    static constexpr int kNumTaps = gl::LinkSlot::kNumTaps;

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
        std::atomic<bool>  sourceStale { false };
        std::atomic<bool>  slotTaken { false };      // Source/Point: Platz hat schon eine andere Instanz
        std::atomic<bool>  lockSeen { false };       // Source/Point: ein Lock hört zu
        std::atomic<bool>  rateMismatch { false };
        std::atomic<float> ownLufs { -180.0f };      // Source/Point: eigener Pegel
        std::atomic<float> postLufs { -180.0f };     // Lock: Ende der Kette, vor der Korrektur
        std::atomic<float> outLufs { -180.0f };      // Lock: Ausgang
        std::atomic<float> gainDb { 0.0f };
        std::atomic<float> targetDb { 0.0f };
        std::atomic<float> refPeak { 0.0f };
        std::atomic<float> outPeak { 0.0f };
        std::atomic<int>   monitorActive { monFinal };   // was gerade wirklich zu hören ist

        // pro Abgriff (0 = Source/Sidechain, 1..3 = Points)
        std::array<std::atomic<bool>,  kNumTaps> tapLinked {};
        std::array<std::atomic<float>, kNumTaps> tapLufs {};
        std::array<std::atomic<float>, kNumTaps> tapLatencyMs {};
        std::array<std::atomic<bool>,  kNumTaps> tapLatencyKnown {};
        std::array<std::atomic<bool>,  kNumTaps> tapPolarity {};
        std::array<std::atomic<float>, kNumTaps> tapGainDb {};
    } status;

    juce::String getTapName (int tap);
    int getGroupIndex() const;
    int getRole() const;
    bool isMonitorAvailable (int mon) const;

    // Name dieses Vergleichspunkts (Rolle POINT), wird im Projekt gespeichert
    void setPointName (const juce::String& n);
    juce::String getPointName() const;

    // Hover-Erklärungen an/aus (im Projekt gespeichert)
    std::atomic<bool> tooltipsOn { true };

    //==========================================================================
    // Blind-Vorliebetest (läuft im Prozessor, damit er das Schließen des Fensters übersteht)
    struct BlindResult
    {
        juce::String nameA, nameB, when;
        int prefA = 0, prefB = 0, none = 0;
    };
    struct BlindState
    {
        bool running = false, finished = false;
        int a = monOriginal, b = monFinal;
        int rounds = 10, round = 0;
        bool xIsA = true, listenX = true;
        int prefA = 0, prefB = 0, none = 0;
    };
    void blindStart (int a, int b, int rounds);
    void blindListen (bool x);
    void blindAnswer (int choice);   // 0 = X besser, 1 = Y besser, 2 = kein Unterschied
    void blindStop();
    void blindDismiss();
    BlindState getBlindState() const;
    std::vector<BlindResult> getBlindHistory() const;
    void clearBlindHistory();
    juce::String monitorName (int mon);

private:
    void timerCallback() override;
    void processTapRole (juce::AudioBuffer<float>& io, int n, int tapIndex);
    void processLock (juce::AudioBuffer<float>& buffer, int n);
    void releaseOwnedTap();
    void resetMeasurement();
    void startAnalysis();
    void stopAnalysis();
    void blindNextRound();
    void updateBlindAudio();

    class AnalysisThread : public juce::Thread
    {
    public:
        explicit AnalysisThread (GainLockProcessor& p) : juce::Thread ("GainLock Latency"), owner (p) {}
        void run() override;
    private:
        GainLockProcessor& owner;
    };
    friend class AnalysisThread;

    const int instanceId;
    double sr = 48000.0;

    std::atomic<float>* pRole = nullptr;
    std::atomic<float>* pGroup = nullptr;
    std::atomic<float>* pPoint = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pMetric = nullptr;
    std::atomic<float>* pTrim = nullptr;
    std::atomic<float>* pRange = nullptr;
    std::atomic<float>* pHold = nullptr;
    std::atomic<float>* pMonitor = nullptr;

    int ownedGroup = -1, ownedTap = -1;     // Source/Point: welcher Platz gehört uns
    int lastRole = -1, lastGroup = -1, lastTapIdx = -1, lastMetric = -1;
    uint32_t lastLockHb = 0;
    int lockStale = 0;

    // Lock: Zustand pro Abgriff
    struct TapState
    {
        gl::LoudnessMeter meter;
        float gainDb = 0.0f, targetDb = 0.0f;
        uint32_t lastHb = 0;
        int stale = 0;
        bool wasOk = false;
        std::atomic<int64_t> offsetD { 0 };
        std::atomic<bool> offsetValid { false };
        std::atomic<int> latency { 0 };
    };
    std::array<TapState, kNumTaps> taps;

    gl::LoudnessMeter postMeter, outMeter;
    gl::GainComputer gc;
    std::array<float, kNumMon> monMix {};    // weiche Überblendung zwischen Monitor-Quellen
    float trimSmoothed = 0.0f;
    int lastRef = -1;

    std::unique_ptr<std::atomic<float>[]> lockMono;
    std::atomic<int64_t> lockWritePos { 0 };

    std::unique_ptr<AnalysisThread> analysis;
    std::unique_ptr<gl::LatencyFinder> finder;
    std::mutex finderLock;

    juce::SpinLock nameLock;
    juce::String trackName, pointName;

    std::atomic<float> restoredGain { 0.0f };
    std::atomic<bool> hasRestoredGain { false };

    // Blindtest: GUI-Thread verwaltet, Audio liest nur blindSource
    mutable juce::CriticalSection blindLock;
    BlindState blind;
    std::vector<BlindResult> blindHistory;
    std::atomic<int> blindSource { -1 };     // -1 = aus, sonst Monitor-Quelle
    juce::Random blindRng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainLockProcessor)
};
