#pragma once

// Kokoro-Stimme über ONNX Runtime. Die Runtime-DLL wird erst zur Laufzeit geladen (nicht beim Plugin-Scan),
// das Modell liegt im Datenordner (wird beim ersten Start heruntergeladen).

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace tts
{
class Phonemizer;

struct SynthRequest
{
    juce::String text;
    juce::String voiceA = "af_heart";
    juce::String voiceB;        // leer = kein Mix
    float blend = 0.0f;         // 0..1 Anteil von voiceB
    float speed = 1.0f;         // Kokoro-eigene Geschwindigkeit (0.5..2)
    bool british = false;
};

class KokoroEngine
{
public:
    static constexpr int sampleRate = 24000;

    KokoroEngine();
    ~KokoroEngine();

    // runtimeLib: onnxruntime-DLL/.so, modelFile: kokoro-v1.0.onnx, voicesFile: voices-v1.0.bin
    bool load (const juce::File& runtimeLib, const juce::File& modelFile, const juce::File& voicesFile, juce::String& error);
    bool isLoaded() const { return loaded.load(); }

    // Lexikon-Text (entpackt) für US und GB
    void setLexicons (std::string usText, std::string gbText);

    juce::StringArray getVoiceNames() const;

    // Liefert Mono-Audio mit 24 kHz. progress: 0..1, cancel: abbrechen wenn true
    std::vector<float> synthesize (const SynthRequest& req, std::function<void (float)> progress,
                                   const std::atomic<bool>& cancel, juce::String& error);

    // für Tests
    std::string phonemize (const juce::String& text, bool british);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<bool> loaded { false };
};
} // namespace tts
