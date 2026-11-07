// Offline-Test für Brainbugz Club EQ mit dem echten Prozessor (nicht Teil des Plugins).
// Prüft: Neutralität bei 0 dB, DJM-Werte (-26/+6 an 70 Hz, 1 kHz, 13 kHz), ISO-Kill, Raste,
// Blockgrößen/Mono, Rechenzeit, und rendert die GUI als PNG.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <chrono>

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}
void setParam (ClubEQProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}
// Frequenzgang über Impulsantwort (FFT 2^16)
std::vector<float> magnitudeDb (ClubEQProcessor& p, double fs)
{
    constexpr int order = 16, n = 1 << order;
    p.prepareToPlay (fs, 512);
    juce::AudioBuffer<float> buf (2, n);
    buf.clear();
    buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, juce::jmin (512, n - pos));
        p.processBlock (view, midi);
    }
    std::vector<float> fft ((size_t) n * 2, 0.0f);
    std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, fft.begin());
    juce::dsp::FFT (order).performFrequencyOnlyForwardTransform (fft.data());
    std::vector<float> db ((size_t) n / 2);
    for (size_t i = 0; i < db.size(); ++i) db[i] = juce::Decibels::gainToDecibels (fft[i], -300.0f);
    return db;
}
float at (const std::vector<float>& db, double f, double fs) { return db[(size_t) std::lround (f / fs * (db.size() * 2))]; }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    const double fs = 48000.0;

    {
        ClubEQProcessor p;
        auto db = magnitudeDb (p, fs);
        float worst = 0.0f;
        for (size_t i = 1; i < db.size() * 19 / 20; ++i) worst = juce::jmax (worst, std::abs (db[i]));
        check (worst < 0.001f, "0 dB: Frequenzgang flach, max. Abweichung " + juce::String (worst, 5) + " dB");
    }

    struct Case { const char* id; double f; float pos; float want; float tol; };
    const Case cases[] = {
        { ParamIDs::low, 70.0, 0.0f, -26.0f, 0.1f }, { ParamIDs::mid, 1000.0, 0.0f, -26.0f, 0.1f },
        { ParamIDs::hi, 13000.0, 0.0f, -26.0f, 0.1f }, { ParamIDs::low, 70.0, 1.0f, 6.0f, 0.1f },
        { ParamIDs::mid, 1000.0, 1.0f, 6.0f, 0.3f },   { ParamIDs::hi, 13000.0, 1.0f, 6.0f, 0.1f } };
    for (auto& c : cases)
    {
        ClubEQProcessor p;
        setParam (p, c.id, c.pos);
        auto db = magnitudeDb (p, fs);
        const float got = at (db, c.f, fs);
        check (std::abs (got - c.want) < c.tol, juce::String (c.id) + (c.pos > 0.5f ? " ganz rechts" : " ganz links") + " @ "
                                                + juce::String (c.f, 0) + " Hz: " + juce::String (got, 2) + " dB (Soll "
                                                + juce::String (c.want, 0) + ")");
    }

    {   // alle drei ganz links: alles -26 dB
        ClubEQProcessor p;
        for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::hi }) setParam (p, id, 0.0f);
        auto db = magnitudeDb (p, fs);
        float lo = 1e9f, hi = -1e9f;
        for (double f = 30.0; f < 18000.0; f *= 1.05) { lo = juce::jmin (lo, at (db, f, fs)); hi = juce::jmax (hi, at (db, f, fs)); }
        check (lo > -26.2f && hi < -25.8f, "alle links: durchgehend -26 dB (" + juce::String (lo, 2) + " … " + juce::String (hi, 2) + ")");
    }

    {   // Bitgenau bei 0 dB: hart limitiertes Signal, Spitzen dürfen nicht steigen
        ClubEQProcessor p;
        p.prepareToPlay (fs, 512);
        juce::MidiBuffer midi;
        juce::Random rnd (5);
        const int n = (int) fs * 4;
        juce::AudioBuffer<float> in (2, n);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs;   // Kick + Bass + Hihat-Rauschen, hart auf 1.0 geklippt
            double v = 1.8 * std::sin (2 * juce::MathConstants<double>::pi * (55 + 120 * std::exp (-std::fmod (t, 0.4) * 30)) * t)
                     + 0.9 * std::sin (2 * juce::MathConstants<double>::pi * 110 * t) + 0.4 * (rnd.nextFloat() - 0.5);
            for (int c = 0; c < 2; ++c) in.setSample (c, i, (float) juce::jlimit (-1.0, 1.0, v));
        }
        auto run = [&] (std::function<void (int)> atBlock)
        {
            juce::AudioBuffer<float> out (in);
            for (int pos = 0, k = 0; pos < n; pos += 512, ++k)
            {
                atBlock (k);
                juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, juce::jmin (512, n - pos));
                p.processBlock (view, midi);
            }
            return out;
        };
        auto diffFrom = [&] (const juce::AudioBuffer<float>& out, int from)
        {
            float d = 0.0f;
            for (int c = 0; c < 2; ++c) for (int i = from; i < n; ++i) d = juce::jmax (d, std::abs (out.getSample (c, i) - in.getSample (c, i)));
            return d;
        };
        auto out = run ([] (int) {});
        check (diffFrom (out, 0) == 0.0f, "0 dB: Ausgang bitgenau gleich Eingang, Peak " + juce::String (out.getMagnitude (0, n), 4) + " (Eingang " + juce::String (in.getMagnitude (0, n), 4) + ")");

        // Regler kurz runter (Bass-Swap) und zurück auf 0: danach wieder bitgenau
        auto out2 = run ([&] (int k) { if (k == 20) setParam (p, ParamIDs::low, 0.0f); if (k == 120) setParam (p, ParamIDs::low, 0.5f); });
        int backAt = -1;
        for (int i = 120 * 512; i < n && backAt < 0; i += 512)
            if (diffFrom (out2, i) == 0.0f) backAt = i;
        check (backAt > 0 && backAt - 120 * 512 < (int) fs, "nach Zurückdrehen wieder bitgenau nach " + juce::String ((backAt - 120 * 512) / fs * 1000.0, 0) + " ms");
    }

    {   // ISO: Bass komplett weg
        ClubEQProcessor p;
        setParam (p, ParamIDs::iso, 1.0f);
        setParam (p, ParamIDs::low, 0.0f);
        auto db = magnitudeDb (p, fs);
        check (at (db, 50.0, fs) < -50.0f && at (db, 30.0, fs) < -70.0f, "ISO LOW links: 50 Hz " + juce::String (at (db, 50.0, fs), 1) + " dB, 30 Hz " + juce::String (at (db, 30.0, fs), 1) + " dB");
        check (std::abs (at (db, 6000.0, fs)) < 0.1f, "ISO LOW links, Höhen unberührt: " + juce::String (at (db, 6000.0, fs), 2) + " dB");
    }

    // Raste und Kurve
    check (ceq::positionToDb (0.51f, false) == 0.0f && ceq::positionToDb (0.49f, false) == 0.0f, "Mittenraste: 0.49/0.51 = exakt 0 dB");
    std::printf ("Kurve EQ (Position -> dB): ");
    for (float pos : { 0.0f, 0.1f, 0.25f, 0.4f, 0.5f, 0.6f, 0.75f, 1.0f })
        std::printf ("%.2f:%s  ", pos, ceq::formatDb (pos, false).toRawUTF8());
    std::printf ("\nKurve ISO: ");
    for (float pos : { 0.0f, 0.1f, 0.25f, 0.4f })
        std::printf ("%.2f:%s  ", pos, ceq::formatDb (pos, true).toRawUTF8());
    std::printf ("\n");

    {   // Mono, krumme Blockgrößen, schnelles Drehen: keine NaN, keine Sprünge
        ClubEQProcessor p;
        p.setPlayConfigDetails (1, 1, fs, 4096);
        p.prepareToPlay (fs, 4096);
        juce::MidiBuffer midi;
        bool ok = true;
        float maxStep = 0.0f, prev = 0.0f;
        int pos = 0;
        juce::Random rnd (1);
        for (int blk : { 0, 1, 7, 13, 64, 4096, 333, 1 })
        {
            juce::AudioBuffer<float> b (1, juce::jmax (1, blk));
            for (int i = 0; i < blk; ++i, ++pos) b.setSample (0, i, 0.5f * std::sin ((float) pos * 0.0123f));
            setParam (p, ParamIDs::low, rnd.nextFloat());
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 1, 0, blk);
            p.processBlock (view, midi);
            for (int i = 0; i < blk; ++i)
            {
                const float v = b.getSample (0, i);
                ok = ok && std::isfinite (v);
                maxStep = juce::jmax (maxStep, std::abs (v - prev));
                prev = v;
            }
        }
        check (ok, "Mono + Blockgrößen 0/1/7/13/4096: keine NaN");
        check (maxStep < 0.05f, "Regler-Sprünge klickfrei (max. Schritt " + juce::String (maxStep, 4) + ")");
    }

    {   // Rechenzeit: 60 s Stereo
        ClubEQProcessor p;
        setParam (p, ParamIDs::low, 0.2f);
        p.prepareToPlay (fs, 128);
        juce::AudioBuffer<float> b (2, 128);
        juce::MidiBuffer midi;
        juce::Random rnd (3);
        const int blocks = (int) (60.0 * fs / 128);
        auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < blocks; ++k)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) b.setSample (c, i, rnd.nextFloat() - 0.5f);
            if (k % 400 == 0) setParam (p, ParamIDs::mid, rnd.nextFloat());
            p.processBlock (b, midi);
        }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (sec < 0.6, "60 s Stereo in " + juce::String (sec * 1000.0, 1) + " ms (" + juce::String (sec / 60.0 * 100.0, 3) + " % eines Kerns, inkl. Testsignal)");
        check (p.getLatencySamples() == 0, "Latenz 0 Samples");
    }

    {   // Zustand speichern/laden + GUI-Bild
        ClubEQProcessor a, b;
        setParam (a, ParamIDs::hi, 0.8f); setParam (a, ParamIDs::low, 0.15f); setParam (a, ParamIDs::xlow, 300.0f);
        juce::MemoryBlock mb; a.getStateInformation (mb); b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (b.apvts.getRawParameterValue (ParamIDs::low)->load() - 0.15f) < 1e-4f
               && std::abs (b.apvts.getRawParameterValue (ParamIDs::xlow)->load() - 300.0f) < 0.5f, "Zustand speichern/laden");

        for (int iso = 0; iso < 2; ++iso)
        {
            setParam (a, ParamIDs::iso, (float) iso);
            if (iso) { setParam (a, ParamIDs::low, 0.0f); setParam (a, ParamIDs::mid, 0.5f); setParam (a, ParamIDs::hi, 0.62f); }
            else { setParam (a, ParamIDs::mid, 0.32f); }
            std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            for (float sc : { 1.0f, 2.0f })
            {
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, sc);
                juce::File f (outDir + "/clubeq_" + (iso ? "iso" : "eq") + (sc > 1 ? "_2x" : "") + ".png");
                f.deleteFile();
                juce::FileOutputStream os (f);
                juce::PNGImageFormat().writeImageToStream (img, os);
                std::printf ("Bild: %s\n", f.getFullPathName().toRawUTF8());
            }
        }
    }

    std::printf (failures == 0 ? "\nALLES OK\n" : "\n%d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}
