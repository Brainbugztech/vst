// Testet Aussprache, Kokoro und die Offline-Effekte ohne Plugin-Host.
// Aufruf: tts_engine_test <datenordner mit kokoro-v1.0.onnx + voices-v1.0.bin> <onnxruntime-lib> <lexikon-ordner> <ausgabe-ordner>
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/KokoroEngine.h"
#include "dsp/LpcPsola.h"
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
    // Rohstimme erzeugen, analysieren und mit der Echtzeit-Engine in verschiedenen Einstellungen abspielen
    struct Case { const char* name; juce::String a, b; float blend; bool gb; double pitch, formant, speed; };
    Case cases[] = {
        { "heart", "af_heart", "", 0, false, 0, 1, 1 },
        { "george_gb", "bm_george", "", 0, true, 0, 1, 1 },
        { "mix_heart_adam", "af_heart", "am_adam", 0.4f, false, 0, 1, 1 },
        { "pitch_up7", "af_heart", "", 0, false, 7, 1, 1 },
        { "pitch_down12", "am_michael", "", 0, false, -12, 1, 1 },
        { "formant_up", "af_heart", "", 0, false, 0, 1.35, 1 },
        { "formant_down", "af_heart", "", 0, false, 0, 0.7, 1 },
        { "speed_4x", "af_heart", "", 0, false, 0, 1, 4 },
        { "speed_0_25", "af_heart", "", 0, false, 0, 1, 0.25 },
        { "speed_0_01", "af_heart", "", 0, false, 0, 1, 0.01 },
    };
    for (auto& c : cases)
    {
        tts::SynthRequest r;
        r.text = text; r.voiceA = c.a; r.voiceB = c.b; r.blend = c.blend; r.british = c.gb; r.speed = 1.0f;
        t0 = juce::Time::getMillisecondCounterHiRes();
        auto raw = eng.synthesize (r, {}, cancel, err);
        const double tSynth = juce::Time::getMillisecondCounterHiRes() - t0;
        if (raw.empty()) { std::cerr << c.name << " failed: " << err << "\n"; return 1; }
        t0 = juce::Time::getMillisecondCounterHiRes();
        auto an = tts::dsp::analyseVoice (raw, tts::KokoroEngine::sampleRate);
        const double tAn = juce::Time::getMillisecondCounterHiRes() - t0;
        tts::dsp::VoiceSynth s;
        s.setSource (an);
        s.start();
        std::vector<float> out, blk (256);
        const size_t cap = (size_t) tts::KokoroEngine::sampleRate * 30;
        while (s.isActive() && out.size() < cap)
        {
            s.render (blk.data(), 256, c.speed, std::pow (2.0, c.pitch / 12.0), c.formant);
            out.insert (out.end(), blk.begin(), blk.end());
        }
        float peak = 0;
        for (auto v : out) peak = std::max (peak, std::abs (v));
        std::cout << c.name << ": raw " << raw.size() / 24000.0 << " s (" << tSynth << " ms), analysis " << tAn
                  << " ms, out " << out.size() / 24000.0 << " s, peak " << peak << "\n";
        writeWav (outDir.getChildFile (juce::String (c.name) + ".wav"), out, 24000);
    }
    return 0;
}
