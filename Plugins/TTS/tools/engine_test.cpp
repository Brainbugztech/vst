// Testet Aussprache, Kokoro und die Offline-Effekte ohne Plugin-Host.
// Aufruf: tts_engine_test <datenordner mit kokoro-v1.0.onnx + voices-v1.0.bin> <onnxruntime-lib> <lexikon-ordner> <ausgabe-ordner>
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/KokoroEngine.h"
#include "dsp/VoiceFx.h"
#include <iostream>

static std::string gunzip (const juce::File& f)
{
    juce::FileInputStream fin (f);
    juce::GZIPDecompressorInputStream gz (&fin, false, juce::GZIPDecompressorInputStream::gzipFormat);
    return gz.readEntireStreamAsString().toStdString();
}

static void writeWav (const juce::File& f, const std::vector<float>& a, int sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (1).withBitsPerSample (16));
    if (w == nullptr) return;
    const float* ch[] = { a.data() };
    w->writeFromFloatArrays (ch, 1, (int) a.size());
}

int main (int argc, char** argv)
{
    if (argc < 5) { std::cerr << "args!\n"; return 1; }
    const juce::File data (argv[1]), ort (argv[2]), lex (argv[3]), outDir (argv[4]);
    outDir.createDirectory();
    tts::KokoroEngine eng;
    eng.setLexicons (gunzip (lex.getChildFile ("lexicon_us.txt.gz")), gunzip (lex.getChildFile ("lexicon_gb.txt.gz")));
    juce::String err;
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    if (! eng.load (ort, data.getChildFile ("kokoro-v1.0.onnx"), data.getChildFile ("voices-v1.0.bin"), err))
    { std::cerr << "load failed: " << err << "\n"; return 1; }
    std::cout << "loaded in " << (juce::Time::getMillisecondCounterHiRes() - t0) << " ms, voices " << eng.getVoiceNames().size() << "\n";

    const juce::String text = "Welcome to the Brain Bugz lab. Your bass is now perfectly aligned, and it's 10:30 PM!";
    std::cout << "US: " << eng.phonemize (text, false) << "\nGB: " << eng.phonemize (text, true) << "\n";
    std::atomic<bool> cancel { false };
    struct Case { const char* name; juce::String a, b; float blend; bool gb; tts::fx::VoiceParams p; };
    Case cases[] = {
        { "heart", "af_heart", "", 0, false, {} },
        { "george_gb", "bm_george", "", 0, true, {} },
        { "mix_heart_adam", "af_heart", "am_adam", 0.4f, false, {} },
        { "pitch_up7", "af_heart", "", 0, false, { 7.0f, 1.0f, 1.0f } },
        { "pitch_down12", "am_michael", "", 0, false, { -12.0f, 1.0f, 1.0f } },
        { "formant_up", "af_heart", "", 0, false, { 0.0f, 1.35f, 1.0f } },
        { "formant_down", "af_heart", "", 0, false, { 0.0f, 0.7f, 1.0f } },
        { "speed_4x", "af_heart", "", 0, false, { 0.0f, 1.0f, 4.0f } },
        { "speed_0_25", "af_heart", "", 0, false, { 0.0f, 1.0f, 0.25f } },
        { "speed_0_01", "af_heart", "", 0, false, { 0.0f, 1.0f, 0.01f } },
    };
    std::map<juce::String, std::vector<float>> rawCache;
    for (auto& c : cases)
    {
        tts::SynthRequest r;
        r.text = text; r.voiceA = c.a; r.voiceB = c.b; r.blend = c.blend; r.british = c.gb;
        r.speed = tts::fx::kokoroSpeedFor (c.p.speed);
        t0 = juce::Time::getMillisecondCounterHiRes();
        auto raw = eng.synthesize (r, {}, cancel, err);
        const double tSynth = juce::Time::getMillisecondCounterHiRes() - t0;
        if (raw.empty()) { std::cerr << c.name << " failed: " << err << "\n"; return 1; }
        t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = tts::fx::processVoice (raw, tts::KokoroEngine::sampleRate, c.p, {}, &cancel);
        const double tFx = juce::Time::getMillisecondCounterHiRes() - t0;
        float peak = 0; double rms = 0;
        for (auto v : out) { peak = std::max (peak, std::abs (v)); rms += v * v; }
        rms = std::sqrt (rms / std::max<size_t> (1, out.size()));
        std::cout << c.name << ": raw " << raw.size() / 24000.0 << " s (" << tSynth << " ms), out " << out.size() / 24000.0
                  << " s (fx " << tFx << " ms) peak " << peak << " rms " << rms << "\n";
        writeWav (outDir.getChildFile (juce::String (c.name) + ".wav"), out, 24000);
    }
    return 0;
}
