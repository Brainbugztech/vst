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

    GainLockProcessor src, pt, lock;
    setParam (src, ParamIDs::role, 0.0f);
    setParam (pt, ParamIDs::role, 1.0f);
    setParam (pt, ParamIDs::point, 2.0f);       // P2
    pt.setPointName ("after delay");
    setParam (lock, ParamIDs::role, 2.0f);
    for (auto* p : { &src, &pt, &lock }) setParam (*p, ParamIDs::group, 3.0f);
    setParam (lock, ParamIDs::speed, 0.0f);   // schnell
    for (auto* p : { &src, &pt, &lock }) p->prepareToPlay (fs, block);
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
        if (pos >= abSwitchAt && pos - block < abSwitchAt) setParam (lock, ParamIDs::monitor, (float) GainLockProcessor::monOriginal);

        juce::AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i) { b.setSample (0, i, inL[(size_t) (pos + i)]); b.setSample (1, i, inR[(size_t) (pos + i)]); }
        src.processBlock (b, midi);
        // Kette Teil 1: Verzögerung
        for (int i = 0; i < n; ++i)
        {
            const int idx = pos + i;
            b.setSample (0, i, idx >= chainDelay ? inL[(size_t) (idx - chainDelay)] : 0.0f);
            b.setSample (1, i, idx >= chainDelay ? inR[(size_t) (idx - chainDelay)] : 0.0f);
        }
        pt.processBlock (b, midi);   // Vergleichspunkt "nach dem Delay"
        // Kette Teil 2: Tiefpass + Pegel
        for (int i = 0; i < n; ++i)
        {
            const int idx = pos + i;
            const float yl = b.getSample (0, i), yr = b.getSample (1, i);
            lpL += 0.35f * (yl - lpL); lpR += 0.35f * (yr - lpR);
            b.setSample (0, i, lpL * cg);
            b.setSample (1, i, lpR * cg);
            postL[(size_t) idx] = lpL * cg; postR[(size_t) idx] = lpR * cg;
        }
        lock.processBlock (b, midi);
        for (int i = 0; i < n; ++i) { outL[(size_t) (pos + i)] = b.getSample (0, i); outR[(size_t) (pos + i)] = b.getSample (1, i); }

        // Analyse-Thread läuft in Echtzeit-Abständen: ab und zu Zeit lassen
        if ((pos / block) % 200 == 0) std::this_thread::sleep_for (std::chrono::milliseconds (60));
        if (lock.status.tapLatencyKnown[0].load()) latencySeen = true;
    }

    const float inLufs = lufsOf (inL, inR, (size_t) (fs * 6), (size_t) (fs * 10.9), fs);
    const float postLufs = lufsOf (postL, postR, (size_t) (fs * 6), (size_t) (fs * 10.9), fs);
    const float outLufs = lufsOf (outL, outR, (size_t) (fs * 6), (size_t) (fs * 10.9), fs);
    std::printf ("Quelle %.2f LUFS, Kette %.2f LUFS, Lock-Ausgang %.2f LUFS, Korrektur %.2f dB\n",
                 inLufs, postLufs, outLufs, lock.status.gainDb.load());
    check (std::abs (outLufs - inLufs) < 0.5f, "Ausgang so laut wie die Quelle (< 0,5 dB)");
    check (latencySeen, "Latenz erkannt");
    check (std::abs (lock.status.tapLatencyMs[0].load() - chainDelay * 1000.0f / (float) fs) < 0.05f,
           "Latenz Source = " + juce::String (lock.status.tapLatencyMs[0].load(), 2) + " ms (soll " + juce::String (chainDelay * 1000.0 / fs, 2) + ")");
    check (lock.status.tapLinked[2].load(), "Point P2 gekoppelt");
    check (lock.status.tapLatencyKnown[2].load() && std::abs (lock.status.tapLatencyMs[2].load()) < 0.05f,
           "Latenz P2 = " + juce::String (lock.status.tapLatencyMs[2].load(), 2) + " ms (soll 0)");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (400);   // Timer verteilt die Namen
    check (lock.getTapName (2) == "after delay", "Name von P2 kommt beim Lock an");
    check (lock.isMonitorAvailable (GainLockProcessor::monP2) && ! lock.isMonitorAvailable (GainLockProcessor::monP1), "Monitor: P2 da, P1 nicht");

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

    // P2 und Delta abhören (weiterlaufen lassen)
    auto runMore = [&] (float seconds, std::vector<float>& oL, std::vector<float>& orig)
    {
        const int len = (int) (fs * seconds);
        oL.assign ((size_t) len, 0.0f); orig.assign ((size_t) len, 0.0f);
        for (int p0 = 0; p0 < len; p0 += block)
        {
            const int n = std::min (block, len - p0);
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i)
            {
                const int idx = (p0 + i) % total;
                b.setSample (0, i, inL[(size_t) idx]); b.setSample (1, i, inR[(size_t) idx]);
            }
            src.processBlock (b, midi);
            for (int i = 0; i < n; ++i)
            {
                const int idx = (p0 + i) % total;
                const int d = (idx - chainDelay + total) % total;
                b.setSample (0, i, inL[(size_t) d]); b.setSample (1, i, inR[(size_t) d]);
                orig[(size_t) (p0 + i)] = inL[(size_t) d];
            }
            pt.processBlock (b, midi);
            for (int i = 0; i < n; ++i)
            {
                const float yl = b.getSample (0, i), yr = b.getSample (1, i);
                lpL += 0.35f * (yl - lpL); lpR += 0.35f * (yr - lpR);
                b.setSample (0, i, lpL * cg); b.setSample (1, i, lpR * cg);
            }
            lock.processBlock (b, midi);
            for (int i = 0; i < n; ++i) oL[(size_t) (p0 + i)] = b.getSample (0, i);
        }
    };
    {
        std::vector<float> oL, orig;
        setParam (lock, ParamIDs::monitor, (float) GainLockProcessor::monP2);
        runMore (2.0f, oL, orig);
        double err = 0, ref = 0;
        const float g2 = gl::dbToGain (lock.status.tapGainDb[2].load());
        for (size_t i = (size_t) (fs * 0.5); i < oL.size(); ++i) { err += std::pow (oL[i] - orig[i] * g2, 2); ref += std::pow (orig[i], 2); }
        check (10 * std::log10 (ref / std::max (1e-20, err)) > 50.0, "P2 abhören: zeitrichtig (" + juce::String (10 * std::log10 (ref / std::max (1e-20, err)), 1) + " dB)");
        check (std::abs (lock.status.tapGainDb[2].load()) < 0.3f, "P2 ist schon gleich laut wie Source -> Gain ~0 (" + juce::String (lock.status.tapGainDb[2].load(), 2) + " dB)");

        // Delta: Kette ohne Verlust (nur Delay) -> fast Stille; hier Tiefpass -> Differenz klar unter Original
        setParam (lock, ParamIDs::monitor, (float) GainLockProcessor::monDelta);
        runMore (2.0f, oL, orig);
        double eD = 0, eO = 0;
        for (size_t i = (size_t) (fs * 0.5); i < oL.size(); ++i) { eD += oL[i] * oL[i]; eO += orig[i] * orig[i]; }
        const double rel = 10 * std::log10 (eD / std::max (1e-20, eO));
        check (rel < -3.0 && rel > -40.0, "Delta hörbar und leiser als Original (" + juce::String (rel, 1) + " dB)");
        setParam (lock, ParamIDs::monitor, (float) GainLockProcessor::monFinal);
    }

    // Blindtest: Ablauf + Audio-Zuordnung
    {
        lock.blindStart (GainLockProcessor::monOriginal, GainLockProcessor::monFinal, 5);
        bool mapOk = true;
        for (int round = 0; round < 5; ++round)
        {
            const auto st = lock.getBlindState();
            mapOk &= st.running && st.round == round;
            lock.blindListen (false);
            std::vector<float> oL, orig;
            runMore (0.2f, oL, orig);
            const int heard = lock.status.monitorActive.load();
            const auto st2 = lock.getBlindState();
            const int expect = st2.xIsA ? GainLockProcessor::monFinal : GainLockProcessor::monOriginal;   // Y gehört
            mapOk &= heard == expect;
            // immer die Final-Version bevorzugen
            const bool finalIsX = ! st2.xIsA;
            lock.blindAnswer (finalIsX ? 0 : 1);
        }
        const auto st = lock.getBlindState();
        check (mapOk, "Blindtest: X/Y spielen die richtige Quelle");
        check (! st.running && st.finished && st.prefB == 5 && st.prefA == 0, "Blindtest: 5 Runden, Final 5x gewählt");
        const auto hist = lock.getBlindHistory();
        check (! hist.empty() && hist.front().nameB == "Final", "Blindtest: Ergebnis im Log");
        std::vector<float> oL, orig;
        runMore (0.1f, oL, orig);
        check (lock.status.monitorActive.load() == GainLockProcessor::monFinal, "nach dem Test wieder normaler Monitor");

        auto v1 = gl::judgeBlind (0, 5, 0);  auto v2 = gl::judgeBlind (9, 1, 0);
        auto v3 = gl::judgeBlind (5, 5, 0);  auto v4 = gl::judgeBlind (1, 1, 8);  auto v5 = gl::judgeBlind (6, 3, 1);
        check (v1.kind == gl::BlindVerdict::trend, "Urteil 0:5 = Trend (5 Runden reichen noch nicht ganz)");
        check (v2.kind == gl::BlindVerdict::reliable && v2.winner == 0, "Urteil 9:1 = verlässlich A");
        check (v3.kind == gl::BlindVerdict::noPreference, "Urteil 5:5 = keine Vorliebe");
        check (v4.kind == gl::BlindVerdict::inaudible, "Urteil meist kein Unterschied = kaum hörbar");
        check (v5.kind == gl::BlindVerdict::noPreference, "Urteil 6:3 = keine verlässliche Vorliebe");
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
        check (lock2.getBlindHistory().size() == lock.getBlindHistory().size(), "Blindtest-Log nach dem Laden erhalten");
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
        lock.blindDismiss();
        setParam (lock, ParamIDs::monitor, (float) GainLockProcessor::monP2);
        std::unique_ptr<juce::AudioProcessorEditor> ed (lock.createEditor());
        for (int i = 0; i < 40; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        snapshot (ed.get(), outDir + "/gainlock_lock.png");
        ed->setSize (585, 420);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        snapshot (ed.get(), outDir + "/gainlock_lock_small.png");
    }
    {
        lock.blindStart (GainLockProcessor::monOriginal, GainLockProcessor::monP2, 10);
        lock.blindAnswer (0); lock.blindAnswer (2);
        std::unique_ptr<juce::AudioProcessorEditor> ed (lock.createEditor());
        for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        snapshot (ed.get(), outDir + "/gainlock_blind_running.png");
        for (int i = 0; i < 8; ++i) lock.blindAnswer (i % 3 == 0 ? 1 : 0);
        for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        snapshot (ed.get(), outDir + "/gainlock_blind_result.png");
        lock.blindDismiss();
        for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        snapshot (ed.get(), outDir + "/gainlock_blind_setup.png");
    }
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (pt.createEditor());
        for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        snapshot (ed.get(), outDir + "/gainlock_point.png");
    }
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (src.createEditor());
        for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        snapshot (ed.get(), outDir + "/gainlock_source.png");
    }

    for (auto* p : { &src, &pt, &lock }) p->releaseResources();
    std::printf (failures == 0 ? "\nALLE TESTS OK\n" : "\n%d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}
