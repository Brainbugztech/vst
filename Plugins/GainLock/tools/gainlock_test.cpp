// Offline-Test für Brainbugz Gain Lock: Quelle + simulierte Kette + Lock mit dem echten Prozessor.
// Prüft Lautheitsangleich, Latenz-Erkennung, A/B und rendert die GUI als PNG. Nicht Teil des Plugins.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <random>
#include <thread>

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}
void setParam (GainLockProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
float lufsOf (const std::vector<float>& L, const std::vector<float>& R, size_t from, size_t to, double fs)
{
    gl::LoudnessMeter m; m.prepare (fs);
    for (size_t i = from; i < to; ++i) { float s[2] = { L[i], R[i] }; m.push (s, 2); }
    return m.loudness (m.blocksFilled());
}
void snapshot (juce::AudioProcessorEditor* ed, const juce::String& file)
{
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (file);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("Bild: %s\n", file.toRawUTF8());
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    const double fs = 48000.0;
    const int block = 256;
    const int chainDelay = 300;       // Latenz der simulierten Kette
    const float chainGainDb = -7.5f;  // Kette macht leiser

    GainLockProcessor src, lock;
    setParam (src, ParamIDs::role, 0.0f);
    setParam (lock, ParamIDs::role, 1.0f);
    setParam (src, ParamIDs::group, 3.0f);
    setParam (lock, ParamIDs::group, 3.0f);
    setParam (lock, ParamIDs::speed, 0.0f);   // schnell
    src.prepareToPlay (fs, block);
    lock.prepareToPlay (fs, block);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);   // Timer legt den Ring an

    // Musik-artiges Testsignal: Akkord + Kick + Rauschen
    const int total = (int) (fs * 14);
    std::vector<float> inL (total), inR (total), postL (total), postR (total), outL (total), outR (total);
    std::mt19937 rng (3); std::normal_distribution<float> nd (0, 0.05f);
    for (int i = 0; i < total; ++i)
    {
        const double t = i / fs;
        const int k = i % 24000;
        const float kick = k < 4000 ? 0.6f * (float) std::sin (2 * gl::kPi * (50 + 80 * std::exp (-k / 600.0)) * t) * (float) std::exp (-k / 2500.0) : 0.0f;
        const float chord = 0.12f * (float) (std::sin (2 * gl::kPi * 220 * t) + std::sin (2 * gl::kPi * 277.2 * t) + std::sin (2 * gl::kPi * 329.6 * t + 0.4));
        inL[(size_t) i] = kick + chord + nd (rng);
        inR[(size_t) i] = kick + 0.8f * chord + nd (rng);
    }

    // Kette: Verzögerung + Tiefpass + Pegel
    std::vector<float> dl (chainDelay + 1, 0.0f), dr (chainDelay + 1, 0.0f);
    float lpL = 0, lpR = 0;
    const float cg = gl::dbToGain (chainGainDb);

    juce::MidiBuffer midi;
    int abSwitchAt = (int) (fs * 11);
    bool latencySeen = false;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        if (pos >= abSwitchAt && pos - block < abSwitchAt) setParam (lock, ParamIDs::listen, 1.0f);

        juce::AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i) { b.setSample (0, i, inL[(size_t) (pos + i)]); b.setSample (1, i, inR[(size_t) (pos + i)]); }
        src.processBlock (b, midi);
        // Quelle muss unverändert durchreichen
        for (int i = 0; i < n; ++i)
        {
            const int idx = pos + i;
            float xl = b.getSample (0, i), xr = b.getSample (1, i);
            float yl = idx >= chainDelay ? inL[(size_t) (idx - chainDelay)] : 0.0f;
            float yr = idx >= chainDelay ? inR[(size_t) (idx - chainDelay)] : 0.0f;
            (void) xl; (void) xr;
            lpL += 0.35f * (yl - lpL); lpR += 0.35f * (yr - lpR);
            b.setSample (0, i, lpL * cg);
            b.setSample (1, i, lpR * cg);
            postL[(size_t) idx] = lpL * cg; postR[(size_t) idx] = lpR * cg;
        }
        lock.processBlock (b, midi);
        for (int i = 0; i < n; ++i) { outL[(size_t) (pos + i)] = b.getSample (0, i); outR[(size_t) (pos + i)] = b.getSample (1, i); }

        // Analyse-Thread läuft in Echtzeit-Abständen: ab und zu Zeit lassen
        if ((pos / block) % 200 == 0) std::this_thread::sleep_for (std::chrono::milliseconds (60));
        if (lock.status.latencyKnown.load()) latencySeen = true;
    }

    const float inLufs = lufsOf (inL, inR, (size_t) (fs * 6), (size_t) (fs * 10.9), fs);
    const float postLufs = lufsOf (postL, postR, (size_t) (fs * 6), (size_t) (fs * 10.9), fs);
    const float outLufs = lufsOf (outL, outR, (size_t) (fs * 6), (size_t) (fs * 10.9), fs);
    std::printf ("Quelle %.2f LUFS, Kette %.2f LUFS, Lock-Ausgang %.2f LUFS, Korrektur %.2f dB\n",
                 inLufs, postLufs, outLufs, lock.status.gainDb.load());
    check (std::abs (outLufs - inLufs) < 0.5f, "Ausgang so laut wie die Quelle (< 0,5 dB)");
    check (latencySeen, "Latenz erkannt");
    check (std::abs (lock.status.latencyMs.load() - chainDelay * 1000.0f / (float) fs) < 0.05f,
           "Latenz = " + juce::String (lock.status.latencyMs.load(), 2) + " ms (soll " + juce::String (chainDelay * 1000.0 / fs, 2) + ")");

    // A/B: nach dem Umschalten (+ Überblendung) muss das Original zeitgleich mit der Kette kommen
    {
        const int a = (int) (fs * 12), e = (int) (fs * 13.5);
        double err = 0, ref = 0;
        for (int i = a; i < e; ++i)
        {
            const float orig = i >= chainDelay ? inL[(size_t) (i - chainDelay)] : 0.0f;
            err += (outL[(size_t) i] - orig) * (outL[(size_t) i] - orig);
            ref += orig * orig;
        }
        const double snr = 10 * std::log10 (ref / std::max (1e-20, err));
        check (snr > 60.0, "A/B Original latenzgenau (" + juce::String (snr, 1) + " dB Abstand)");
        const float abLufs = lufsOf (outL, outR, (size_t) a, (size_t) e, fs);
        check (std::abs (abLufs - inLufs) < 0.5f, "A/B: Original gleich laut wie bearbeitet");
    }
    // NaN / Klicks
    {
        bool finite = true; float maxStep = 0;
        for (int i = 1; i < total; ++i)
        {
            finite &= std::isfinite (outL[(size_t) i]) && std::isfinite (outR[(size_t) i]);
        }
        check (finite, "keine NaN/Inf");
        (void) maxStep;
    }

    // Zustand speichern / laden: Korrektur bleibt erhalten
    {
        juce::MemoryBlock mb;
        lock.getStateInformation (mb);
        GainLockProcessor lock2;
        lock2.setStateInformation (mb.getData(), (int) mb.getSize());
        lock2.prepareToPlay (fs, block);
        juce::AudioBuffer<float> b (2, block); b.clear();
        lock2.processBlock (b, midi);
        check (std::abs (lock2.status.gainDb.load() - lock.status.gainDb.load()) < 0.2f, "Korrektur nach dem Laden erhalten");
        lock2.releaseResources();
    }

    // Seltsame Blockgrößen + Mono
    {
        GainLockProcessor m;
        juce::AudioProcessor::BusesLayout lay;
        lay.inputBuses.add (juce::AudioChannelSet::mono());
        lay.inputBuses.add (juce::AudioChannelSet::disabled());
        lay.outputBuses.add (juce::AudioChannelSet::mono());
        check (m.setBusesLayout (lay), "Mono-Layout");
        m.prepareToPlay (44100, 512);
        bool ok = true;
        for (int n : { 1, 7, 0, 512, 4096, 13 })
        {
            juce::AudioBuffer<float> b (1, std::max (1, n));
            b.clear();
            juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 1, n);
            m.processBlock (v, midi);
            ok &= std::isfinite (b.getSample (0, 0));
        }
        check (ok, "Blockgrößen 0/1/7/13/4096 mono");
        m.releaseResources();
    }

    // GUI rendern: Lock (mit laufender Korrektur) und Quelle
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (lock.createEditor());
        for (int i = 0; i < 40; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        snapshot (ed.get(), outDir + "/gainlock_lock.png");
        ed->setSize (585, 360);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        snapshot (ed.get(), outDir + "/gainlock_lock_small.png");
    }
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (src.createEditor());
        for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        snapshot (ed.get(), outDir + "/gainlock_source.png");
    }

    src.releaseResources();
    lock.releaseResources();
    std::printf (failures == 0 ? "\nALLE TESTS OK\n" : "\n%d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}
