#pragma once

// Hintergrund-Arbeiter für den Render-Modus: Audio laden, BPM erkennen,
// offline stretchen/pitchen und als WAV ablegen. Läuft nie im Audio-Thread.

#include <juce_audio_formats/juce_audio_formats.h>
#include "WormholeEngine.h"
#include <atomic>
#include <memory>

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

    double lengthSeconds() const { return ch.empty() ? 0.0 : (double) ch[0].size() / sampleRate; }
};

struct RenderSettings
{
    double timeRatio = 1.0;               // Ausgabe-Länge / Eingangs-Länge
    double pitchSemitones = 0.0;
    double formantSemitones = 0.0;
    bool formantLock = true;
    bool attackLock = true;

    bool operator== (const RenderSettings& o) const
    {
        return std::abs (timeRatio - o.timeRatio) < 1e-9 && std::abs (pitchSemitones - o.pitchSemitones) < 1e-9
            && std::abs (formantSemitones - o.formantSemitones) < 1e-9 && formantLock == o.formantLock && attackLock == o.attackLock;
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
    int sourceGeneration = 0;
};

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

    static juce::File outputFolder();
    static std::vector<float> makePeaks (const std::vector<std::vector<float>>& ch, int bins);

private:
    void run() override;
    void doLoad (const juce::File& f);
    void doCaptured();
    void doRender (const RenderSettings& s);
    void fail (const juce::String& msg);

    enum class Job { none, load, captured, render };
    juce::CriticalSection jobLock, dataLock;
    Job pendingJob = Job::none;
    juce::File pendingFile;
    juce::AudioBuffer<float> pendingCapture;
    double pendingCaptureRate = 44100.0;
    RenderSettings pendingSettings;

    std::shared_ptr<const Clip> clip;
    std::shared_ptr<const Result> result;
    juce::String message;
    std::atomic<int> status { 0 }, generation { 0 }, clipGeneration { 0 };
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> cancelFlag { false };

    juce::AudioFormatManager formats;
};
} // namespace wh
