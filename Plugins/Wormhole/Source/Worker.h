#pragma once

// Hintergrund-Arbeit für den Render-Modus: Audio laden, BPM erkennen,
// stückweise rendern (auch stundenlange Extrem-Stretches) und vorhören.
// Nichts davon läuft im Audio-Thread; der liest nur fertige Samples aus einem FIFO.

#include <juce_audio_formats/juce_audio_formats.h>
#include "WormholeEngine.h"
#include <atomic>
#include <memory>
#include <set>

namespace wh
{
struct Clip
{
    std::vector<std::vector<float>> ch;   // max. 2 Kanäle
    double sampleRate = 44100.0;
    juce::String name;
    juce::File file;                      // leer bei Aufnahme
    double detectedBpm = 0.0;
    std::vector<float> peaks;             // Hüllkurve für die Anzeige (0..1)
    std::vector<long> transients;         // Attack-Positionen (Eingangs-Samples)

    double lengthSeconds() const { return ch.empty() ? 0.0 : (double) ch[0].size() / sampleRate; }
};

struct RenderSettings
{
    double timeRatio = 1.0;               // Ausgabe-Länge / Eingangs-Länge
    double pitchSemitones = 0.0;
    double formantSemitones = 0.0;
    bool formantLock = true;
    bool attackLock = true;
    double textureSeconds = 0.0;          // 0 = automatisch

    bool isExtreme() const { return timeRatio > wormhole::kExtremeRatio; }
    wormhole::Params toParams() const
    {
        wormhole::Params p;
        p.timeRatio = juce::jlimit (0.25, wormhole::kMaxRatio, timeRatio);
        p.pitchSemitones = pitchSemitones;
        p.formantPreserve = formantLock;
        p.formantSemitones = formantSemitones;
        p.transientLock = attackLock;
        p.textureSeconds = textureSeconds;
        return p;
    }
    bool operator== (const RenderSettings& o) const
    {
        return std::abs (timeRatio - o.timeRatio) < 1e-9 && std::abs (pitchSemitones - o.pitchSemitones) < 1e-9
            && std::abs (formantSemitones - o.formantSemitones) < 1e-9 && formantLock == o.formantLock
            && attackLock == o.attackLock && std::abs (textureSeconds - o.textureSeconds) < 1e-6;
    }
    bool operator!= (const RenderSettings& o) const { return ! (*this == o); }
};

struct Result
{
    juce::File file;
    RenderSettings settings;
    double lengthSeconds = 0.0;
    std::vector<float> peaks;
    std::vector<float> transients;        // Positionen 0..1
};

static constexpr double maxRenderSeconds = 2.0 * 3600.0;   // längstes Render-Ergebnis

// ---------------------------------------------------------------------------
class Worker : private juce::Thread
{
public:
    enum class Status { idle, loading, rendering, error };

    Worker();
    ~Worker() override;

    void loadFile (const juce::File& f);
    void loadCaptured (juce::AudioBuffer<float>&& buffer, double sampleRate);
    void render (const RenderSettings& s);
    void cancel() { cancelFlag = true; }

    Status getStatus() const { return (Status) status.load(); }
    float getProgress() const { return progress.load(); }
    juce::String getMessage() const;

    std::shared_ptr<const Clip> getClip() const;
    std::shared_ptr<const Result> getResult() const;
    int getGeneration() const { return generation.load(); }   // ändert sich bei jedem neuen Ergebnis
    int getRenderCount() const { return renderCount.load(); } // zählt fertige Renders (für den Lichtimpuls)

    // Datei wurde nach Ableton gezogen oder im Explorer gezeigt -> bleibt erhalten
    void markExported (const juce::File& f);

    static juce::File outputFolder();
    static std::vector<float> makePeaks (const std::vector<std::vector<float>>& ch, int bins);

private:
    void run() override;
    void doLoad (const juce::File& f);
    void doCaptured();
    void doRender (const RenderSettings& s);
    void fail (const juce::String& msg);
    void finishClip (std::shared_ptr<Clip> c);
    void deleteUnexported (const std::shared_ptr<const Result>& r);

    enum class Job { none, load, captured, render };
    juce::CriticalSection jobLock, dataLock;
    Job pendingJob = Job::none;
    juce::File pendingFile;
    juce::AudioBuffer<float> pendingCapture;
    double pendingCaptureRate = 44100.0;
    RenderSettings pendingSettings;

    std::shared_ptr<const Clip> clip;
    std::shared_ptr<const Result> result;
    std::set<juce::String> exported;
    juce::String message;
    std::atomic<int> status { 0 }, generation { 0 }, renderCount { 0 };
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> cancelFlag { false };

    juce::AudioFormatManager formats;
};

// ---------------------------------------------------------------------------
//  Vorhören: rechnet das Ergebnis im Hintergrund voraus und legt es in einen
//  FIFO, den der Audio-Thread abspielt. Regler-Änderungen starten an der
//  aktuellen Stelle neu (mit kurzer Blende), am Ende geht es von vorn los.
// ---------------------------------------------------------------------------
class Previewer : private juce::Thread
{
public:
    Previewer();
    ~Previewer() override;

    void prepare (double hostRate, int numChannels);      // aus prepareToPlay

    // Nachrichten-Thread
    void play (std::shared_ptr<const Clip> clip, const RenderSettings& s, double startFraction);
    void update (const RenderSettings& s);                // bei Regler-Änderung
    void stop();
    bool isPlaying() const { return playing.load(); }
    double getPositionFraction() const { return posFraction.load(); }
    float getLevel() const { return level.load(); }

    // Audio-Thread: ersetzt den Puffer durch die Vorschau. Gibt false zurück, wenn nichts läuft.
    bool readAudio (juce::AudioBuffer<float>& buffer, int numChannels);

private:
    void run() override;
    void restartStream (double fraction);
    int produce (int numHostSamples);

    // geteilt mit dem Nachrichten-Thread
    juce::CriticalSection lock;
    std::shared_ptr<const Clip> pendingClip;
    RenderSettings pendingSettings;
    double pendingFraction = 0.0;
    std::atomic<bool> restartRequest { false }, playing { false }, stopRequest { false };

    // nur Preview-Thread
    std::shared_ptr<const Clip> clip;
    RenderSettings settings;
    wormhole::StretchStream stream;
    std::vector<std::vector<float>> staging;  // Stream-Ausgabe (Clip-Rate)
    int stagingFill = 0;
    std::vector<float*> stagingPtr;
    juce::LagrangeInterpolator interp[2];
    juce::AudioBuffer<float> chunk;
    long streamStartOut = 0;

    // FIFO zum Audio-Thread
    juce::AbstractFifo fifo { 1 << 16 };
    juce::AudioBuffer<float> fifoBuf;
    std::atomic<bool> flushRequest { false };   // Audio-Thread: alten Inhalt ausblenden + verwerfen
    std::atomic<long long> fifoBaseOut { 0 };   // Ausgabe-Position (Clip-Samples) des FIFO-Anfangs
    std::atomic<long long> written { 0 }, consumed { 0 };
    std::atomic<double> posFraction { 0.0 };
    std::atomic<float> level { 0.0f };
    std::atomic<long> outLength { 1 };

    double hostRate = 44100.0;
    int hostChannels = 2;
    float fadeGain = 0.0f;                     // Audio-Thread
    std::atomic<double> clipRate { 44100.0 };
};
} // namespace wh
