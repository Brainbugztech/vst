#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "WormholeEngine.h"
#include "Worker.h"

class WormholeProcessor : public juce::AudioProcessor
{
public:
    WormholeProcessor();
    ~WormholeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brainbugz Wormhole"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- Render-Modus ---
    wh::RenderSettings currentRenderSettings() const;
    double getSourceBpm() const { return sourceBpm.load(); }
    void setSourceBpm (double b) { sourceBpm = b; }
    double getHostBpm() const { return hostBpm.load(); }
    double getTempoPercent() const;
    void setTempoPercent (double pct);

    // Aufnahme direkt von der Spur
    bool startCapture();
    void stopCapture();
    bool isCapturing() const { return capturing.load(); }
    double captureSeconds() const { return capturePos.load() / juce::jmax (1.0, captureRate); }

    double getPreparedRate() const { return captureRate; }
    bool isLiveMode() const { return apvts.getRawParameterValue ("mode")->load() < 0.5f; }

    juce::AudioProcessorValueTreeState apvts;
    wh::Worker worker;

    static constexpr double maxCaptureSeconds = 180.0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    wormhole::RealtimeShifter shifter;
    juce::AudioBuffer<float> delayBuf;      // Render-Modus: trockenes Signal mit gleicher Latenz
    int delayPos = 0, latency = 0, numCh = 2;

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pPitch = nullptr;
    std::atomic<float>* pFine = nullptr;
    std::atomic<float>* pFormant = nullptr;
    std::atomic<float>* pFormantLock = nullptr;
    float lastSemis = 1e9f, lastFormant = 1e9f; bool lastLock = false;

    std::atomic<double> sourceBpm { 0.0 }, hostBpm { 0.0 };

    juce::AudioBuffer<float> captureBuf;
    std::atomic<int> capturePos { 0 };
    std::atomic<bool> capturing { false };
    double captureRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WormholeProcessor)
};
