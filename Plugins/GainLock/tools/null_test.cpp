// Nulltest: Source -> Lock ohne etwas dazwischen. Muss bit-genau durchreichen,
// auch wenn der Lock vor der Source startet, die Source kurz aussetzt und die Blockgrößen wechseln.
// Aufruf: nulltest <speed 0-2> <wechselnde Blockgrößen 0/1>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <random>
#include <thread>

static void setParam (GainLockProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double fs = 44100.0;
    const int speed = argc > 1 ? std::atoi (argv[1]) : 1;
    const bool varBlocks = argc > 2 ? std::atoi (argv[2]) != 0 : true;

    GainLockProcessor src, lock;
    setParam (src, ParamIDs::role, 0.0f);
    setParam (lock, ParamIDs::role, 2.0f);
    setParam (lock, ParamIDs::speed, (float) speed);
    src.prepareToPlay (fs, 512);
    lock.prepareToPlay (fs, 512);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    // Psytrance-artig: Kick 145 BPM, rollender Bass, Lead mit Lautstärke-Verlauf, Rauschen
    const int total = (int) (fs * 40);
    std::mt19937 rng (5); std::normal_distribution<float> nd (0, 1);
    std::uniform_int_distribution<int> bsz (1, 1024);
    const double beat = 60.0 / 145.0;
    juce::MidiBuffer midi;
    double sumG = 0; float minG = 1e9, maxG = -1e9; int cnt = 0;
    double maxDiff = 0, diffE = 0, inE = 0;
    float bassPh = 0;
    for (int pos = 0; pos < total;)
    {
        const int n = varBlocks ? std::min (bsz (rng), total - pos) : std::min (512, total - pos);
        juce::AudioBuffer<float> b (2, n), orig (2, n);
        for (int i = 0; i < n; ++i)
        {
            const double t = (pos + i) / fs;
            const double tb = std::fmod (t, beat);
            const double sub = std::fmod (t, beat / 4.0);
            const float kick = (float) (0.8 * std::sin (2 * gl::kPi * (45 + 120 * std::exp (-tb * 40)) * tb) * std::exp (-tb * 9));
            bassPh += (float) (2 * gl::kPi * 49 / fs);
            const float bass = (float) (tb > 0.12 ? 0.35 * std::sin (bassPh) * std::exp (-sub * 18) : 0.0);
            const float env = (float) (0.5 + 0.5 * std::sin (2 * gl::kPi * t / 16.0));
            const float lead = 0.15f * env * (float) std::sin (2 * gl::kPi * 880 * t + 3 * std::sin (2 * gl::kPi * 5 * t));
            const float hat = (std::fmod (t + beat / 2, beat) < 0.03 ? 0.2f : 0.0f) * nd (rng);
            b.setSample (0, i, kick + bass + lead + hat);
            b.setSample (1, i, kick + bass - 0.7f * lead + hat * 0.8f);
        }
        orig.makeCopyOf (b);
        // Szenario: Lock läuft zuerst allein, Source kommt nach 2,03 s dazu, setzt bei 20 s kurz aus
        const bool srcOn = pos > fs * 2.03 && ! (pos > fs * 20 && pos < fs * 20.7);
        if (srcOn) src.processBlock (b, midi);
        lock.processBlock (b, midi);
        if (pos > fs * 25)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const double d = b.getSample (c, i) - orig.getSample (c, i);
                    maxDiff = std::max (maxDiff, std::abs (d));
                    diffE += d * d; inE += (double) orig.getSample (c, i) * orig.getSample (c, i);
                }
            const float g = lock.status.gainDb.load();
            sumG += g; minG = std::min (minG, g); maxG = std::max (maxG, g); ++cnt;
        }
        pos += n;
        if ((pos / 4096) % 40 == 0) std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    std::printf ("speed %d varBlocks %d: gain mean %+.3f min %+.3f max %+.3f dB | diff max %.2e, rel %.1f dB | latKnown %d lat %.2f ms\n",
                 speed, (int) varBlocks, sumG / cnt, minG, maxG, maxDiff,
                 10 * std::log10 (std::max (1e-30, diffE) / inE), (int) lock.status.tapLatencyKnown[0].load(), lock.status.tapLatencyMs[0].load());
    src.releaseResources(); lock.releaseResources();
    const bool pass = maxDiff == 0.0;
    std::printf (pass ? "[ OK ] bit-genau\n" : "[FAIL] nicht bit-genau\n");
    return pass ? 0 : 1;
}
