// Offline-Test und Lautstärke-Check für Brainbugz Booth EQ (nicht Teil des Plugins).
// Läuft mit dem echten Prozessor bei 44,1 / 48 / 96 kHz.
// Prüft: Nulltest bitgenau, Kill-Tiefe, Anhebung, Pegelanstieg bei Extremstellungen, Klickfreiheit,
// NaN-Schutz, Reset (Transport), Blockgrößen, Rechenzeit, Zustand. Rendert die GUI als PNG.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <chrono>
#include <cstdio>
#include <functional>

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

void setParam (BoothEQProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

void neutral (BoothEQProcessor& p)
{
    for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::high }) setParam (p, id, 0.5f);
    for (auto* id : { ParamIDs::killLow, ParamIDs::killMid }) setParam (p, id, 0.0f);
    setParam (p, ParamIDs::curve, 0.0f);
    setParam (p, ParamIDs::xlow, (float) beq::kDefaultXLow);
    setParam (p, ParamIDs::xhigh, (float) beq::kDefaultXHigh);
}

double toDb (double g) { return 20.0 * std::log10 (juce::jmax (1.0e-12, g)); }

// Verarbeitet ein Signal blockweise; atBlock(k, samplePos) darf Parameter ändern
juce::AudioBuffer<float> run (BoothEQProcessor& p, const juce::AudioBuffer<float>& in, int block,
                              const std::function<void (int, int)>& atBlock = {})
{
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    const int n = in.getNumSamples();
    for (int pos = 0, k = 0; pos < n; pos += block, ++k)
    {
        if (atBlock) atBlock (k, pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), pos, juce::jmin (block, n - pos));
        p.processBlock (view, midi);
    }
    return out;
}

// Frequenzgang über Impulsantwort
std::vector<float> magnitudeDb (BoothEQProcessor& p, double fs)
{
    constexpr int order = 17, n = 1 << order;
    p.prepareToPlay (fs, 512);
    juce::AudioBuffer<float> imp (2, n);
    imp.clear();
    imp.setSample (0, 0, 1.0f); imp.setSample (1, 0, 1.0f);
    auto out = run (p, imp, 512);
    std::vector<float> fft ((size_t) n * 2, 0.0f);
    std::copy (out.getReadPointer (0), out.getReadPointer (0) + n, fft.begin());
    juce::dsp::FFT (order).performFrequencyOnlyForwardTransform (fft.data());
    std::vector<float> db ((size_t) n / 2);
    for (size_t i = 0; i < db.size(); ++i) db[i] = juce::Decibels::gainToDecibels (fft[i], -300.0f);
    return db;
}
float at (const std::vector<float>& db, double f, double fs) { return db[(size_t) std::lround (f / fs * (db.size() * 2))]; }

// Testsignale (Stereo)
juce::AudioBuffer<float> sine (double fs, double sec, double f, float amp)
{
    const int n = (int) (fs * sec);
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c) b.setSample (c, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * f * i / fs));
    return b;
}
juce::AudioBuffer<float> sweep (double fs, double sec, float amp)
{
    const int n = (int) (fs * sec);
    juce::AudioBuffer<float> b (2, n);
    const double f0 = 20.0, f1 = juce::jmin (20000.0, fs * 0.45), k = std::log (f1 / f0);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs, ph = juce::MathConstants<double>::twoPi * f0 * sec / k * (std::exp (t / sec * k) - 1.0);
        for (int c = 0; c < 2; ++c) b.setSample (c, i, amp * (float) std::sin (ph));
    }
    return b;
}
juce::AudioBuffer<float> noise (double fs, double sec, float amp, bool pink, int seed)
{
    const int n = (int) (fs * sec);
    juce::AudioBuffer<float> b (2, n);
    juce::Random rnd (seed);
    for (int c = 0; c < 2; ++c)
    {
        double b0 = 0, b1 = 0, b2 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double w = rnd.nextDouble() * 2.0 - 1.0;
            double v = w;
            if (pink)
            {
                b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
                v = (b0 + b1 + b2 + w * 0.1848) * 0.25;
            }
            b.setSample (c, i, (float) v);
        }
    }
    b.applyGain (amp / juce::jmax (1.0e-9f, b.getMagnitude (0, n)));
    return b;
}
// Hitech-ähnlich: Kick 150 BPM + rollender Bass + Hats, hart auf 0 dBFS geklippt (Master)
juce::AudioBuffer<float> hitech (double fs, double sec, int seed)
{
    const int n = (int) (fs * sec);
    juce::AudioBuffer<float> b (2, n);
    juce::Random rnd (seed);
    const double beat = 60.0 / 150.0, tau = juce::MathConstants<double>::twoPi;
    double kickPh = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs, tb = std::fmod (t, beat), t16 = std::fmod (t, beat / 4.0);
        kickPh += tau * (50.0 + 160.0 * std::exp (-tb * 35.0)) / fs;
        const double kick = 1.6 * std::sin (kickPh) * std::exp (-tb * 6.0);
        const double bass = (tb > beat * 0.25 ? 0.8 : 0.0) * std::sin (tau * 55.0 * t) * std::exp (-t16 * 18.0)
                          + 0.25 * std::sin (tau * 110.0 * t + 0.3) * (tb > beat * 0.25 ? 1.0 : 0.0);
        const double lead = 0.35 * std::sin (tau * 660.0 * t + 2.0 * std::sin (tau * 3.0 * t));
        const double hat = (t16 < 0.02 ? 0.5 : 0.0) * (rnd.nextDouble() - 0.5);
        for (int c = 0; c < 2; ++c)
            b.setSample (c, i, (float) juce::jlimit (-1.0, 1.0, kick + bass + lead + hat + (c ? 0.02 : -0.02) * (rnd.nextDouble() - 0.5)));
    }
    return b;
}

float peak (const juce::AudioBuffer<float>& b) { return b.getMagnitude (0, b.getNumSamples()); }
// größter Kurzzeit-RMS (50 ms)
double maxRms (const juce::AudioBuffer<float>& b, double fs)
{
    const int w = (int) (fs * 0.05);
    double best = 0.0;
    for (int s = 0; s + w <= b.getNumSamples(); s += w / 2)
        best = juce::jmax (best, (double) juce::jmax (b.getRMSLevel (0, s, w), b.getRMSLevel (1, s, w)));
    return best;
}
bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : ".";

    double worstExcessPeak = -100.0, worstRise = -100.0, worstExcessRms = -100.0, worstRaw = -100.0;
    juce::String worstCase;
    int totalCapHits = 0;

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("\n===== %.1f kHz =====\n", fs / 1000.0);
        const juce::String sr = " @" + juce::String (fs / 1000.0, 1) + "k";

        // 1) Nulltest: neutral = bitgenau, auch mit geklipptem Master
        {
            BoothEQProcessor p;
            p.prepareToPlay (fs, 512);
            auto in = hitech (fs, 3.0, 1);
            auto out = run (p, in, 512);
            float d = 0.0f;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < in.getNumSamples(); ++i) d = juce::jmax (d, std::abs (out.getSample (c, i) - in.getSample (c, i)));
            check (d == 0.0f && p.getEngine().isBypassing(), "Nulltest neutral: bitgenau (Abweichung " + juce::String (d) + ")" + sr);

            // Kill drücken und loslassen, Regler drehen und zurück: danach wieder bitgenau
            const int back = (int) (fs * 1.5);
            auto out2 = run (p, in, 512, [&] (int, int pos)
            {
                if (pos >= (int) (fs * 0.5) && pos < (int) (fs * 0.5) + 512) { setParam (p, ParamIDs::killLow, 1.0f); setParam (p, ParamIDs::high, 0.8f); }
                if (pos >= (int) (fs * 1.0) && pos < (int) (fs * 1.0) + 512) { setParam (p, ParamIDs::killLow, 0.0f); setParam (p, ParamIDs::high, 0.5f); }
            });
            int exactFrom = -1;
            for (int s = (int) fs; s < in.getNumSamples() && exactFrom < 0; s += 256)
            {
                bool same = true;
                for (int c = 0; c < 2 && same; ++c)
                    for (int i = s; i < in.getNumSamples(); ++i)
                        if (out2.getSample (c, i) != in.getSample (c, i)) { same = false; break; }
                if (same) exactFrom = s;
            }
            check (exactFrom > 0 && exactFrom <= back, "nach Kill/Drehen zurück auf neutral wieder bitgenau nach "
                                                        + juce::String ((exactFrom - fs) / fs * 1000.0, 0) + " ms" + sr);
        }

        // 2) Frequenzgang: neutral flach, Anhebung +6, Kill-Tiefe
        {
            BoothEQProcessor p;
            auto db = magnitudeDb (p, fs);
            float worst = 0.0f;
            for (size_t i = 1; i < db.size() * 9 / 10; ++i) worst = juce::jmax (worst, std::abs (db[i]));
            check (worst == 0.0f, "neutral: Frequenzgang exakt 0 dB" + sr);

            for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::high }) setParam (p, id, 1.0f);
            db = magnitudeDb (p, fs);
            float lo = 1e9f, hi = -1e9f;
            for (double f = 25.0; f < 18000.0; f *= 1.03) { lo = juce::jmin (lo, at (db, f, fs)); hi = juce::jmax (hi, at (db, f, fs)); }
            check (std::abs (lo - 6.0f) < 0.02f && std::abs (hi - 6.0f) < 0.02f, "alle +6: überall +6,0 dB (" + juce::String (lo, 2) + " … " + juce::String (hi, 2) + ")" + sr);

            neutral (p);
            setParam (p, ParamIDs::killLow, 1.0f);
            db = magnitudeDb (p, fs);
            check (at (db, 70.0, fs) < -60.0f, "Low-Kill: 30 Hz " + juce::String (at (db, 30.0, fs), 1) + " / 50 Hz " + juce::String (at (db, 50.0, fs), 1)
                                                + " / 70 Hz " + juce::String (at (db, 70.0, fs), 1) + " dB, 1 kHz " + juce::String (at (db, 1000.0, fs), 2) + " dB" + sr);

            neutral (p);
            setParam (p, ParamIDs::killMid, 1.0f);
            db = magnitudeDb (p, fs);
            check (at (db, 670.0, fs) < -60.0f, "Mid-Kill: 400 Hz " + juce::String (at (db, 400.0, fs), 1) + " / 670 Hz " + juce::String (at (db, 670.0, fs), 1)
                                                 + " / 1 kHz " + juce::String (at (db, 1000.0, fs), 1) + " dB, 50 Hz " + juce::String (at (db, 50.0, fs), 2)
                                                 + " / 13 kHz " + juce::String (at (db, 13000.0, fs), 2) + " dB" + sr);

            neutral (p);
            setParam (p, ParamIDs::high, 0.0f);
            db = magnitudeDb (p, fs);
            check (at (db, 9000.0, fs) < -60.0f, "High-Knopf links (ISO): 9 kHz " + juce::String (at (db, 9000.0, fs), 1) + " / 13 kHz "
                                                  + juce::String (at (db, 13000.0, fs), 1) + " dB" + sr);

            neutral (p);
            setParam (p, ParamIDs::curve, 1.0f);
            for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::high }) setParam (p, id, 0.0f);
            db = magnitudeDb (p, fs);
            check (std::abs (at (db, 1000.0, fs) + 26.0f) < 0.05f, "Kurve EQ, alle links: " + juce::String (at (db, 1000.0, fs), 2) + " dB (Soll -26)" + sr);
        }

        // 3) Lautstärke-Check: Signale x Extremstellungen
        struct Setting { const char* name; std::function<void (BoothEQProcessor&)> set; double boostDb; };
        const std::vector<Setting> settings = {
            { "neutral", [] (BoothEQProcessor&) {}, 0.0 },
            { "alle +6", [] (BoothEQProcessor& p) { for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::high }) setParam (p, id, 1.0f); }, 6.0 },
            { "alle links ISO", [] (BoothEQProcessor& p) { for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::high }) setParam (p, id, 0.0f); }, 0.0 },
            { "Low +6, Rest Kill", [] (BoothEQProcessor& p) { setParam (p, ParamIDs::low, 1.0f); setParam (p, ParamIDs::mid, 0.0f); setParam (p, ParamIDs::high, 0.0f); }, 6.0 },
            { "Mid +6, Rest Kill", [] (BoothEQProcessor& p) { setParam (p, ParamIDs::mid, 1.0f); setParam (p, ParamIDs::low, 0.0f); setParam (p, ParamIDs::high, 0.0f); }, 6.0 },
            { "High +6, Rest Kill", [] (BoothEQProcessor& p) { setParam (p, ParamIDs::high, 1.0f); setParam (p, ParamIDs::low, 0.0f); setParam (p, ParamIDs::mid, 0.0f); }, 6.0 },
            { "Low-Kill gehalten", [] (BoothEQProcessor& p) { setParam (p, ParamIDs::killLow, 1.0f); }, 0.0 },
            { "Mid-Kill gehalten", [] (BoothEQProcessor& p) { setParam (p, ParamIDs::killMid, 1.0f); }, 0.0 },
            { "beide Kills + High +6", [] (BoothEQProcessor& p) { setParam (p, ParamIDs::killLow, 1.0f); setParam (p, ParamIDs::killMid, 1.0f); setParam (p, ParamIDs::high, 1.0f); }, 6.0 },
            { "Kurve EQ, alle links", [] (BoothEQProcessor& p) { setParam (p, ParamIDs::curve, 1.0f); for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::high }) setParam (p, id, 0.0f); }, 0.0 },
            { "Trennung extrem (80 Hz/6 kHz), Mid +6", [] (BoothEQProcessor& p) { setParam (p, ParamIDs::xlow, 80.0f); setParam (p, ParamIDs::xhigh, 6000.0f); setParam (p, ParamIDs::mid, 1.0f); }, 6.0 },
        };
        struct Sig { const char* name; juce::AudioBuffer<float> buf; };
        std::vector<Sig> sigs;
        sigs.push_back ({ "Sinus 50 Hz", sine (fs, 2.0, 50.0, 0.9f) });
        sigs.push_back ({ "Sinus 1 kHz", sine (fs, 2.0, 1000.0, 0.9f) });
        sigs.push_back ({ "Sinus 12 kHz", sine (fs, 2.0, 12000.0, 0.9f) });
        sigs.push_back ({ "Sweep 20 Hz-20 kHz", sweep (fs, 4.0, 0.9f) });
        sigs.push_back ({ "weißes Rauschen", noise (fs, 2.0, 0.9f, false, 2) });
        sigs.push_back ({ "rosa Rauschen", noise (fs, 2.0, 0.9f, true, 3) });
        sigs.push_back ({ "Hitech-Master 0 dBFS", hitech (fs, 4.0, 4) });

        for (auto& st : settings)
        {
            double setRise = -100.0, setExcess = -100.0, setRms = -100.0, rawExcess = -100.0;
            int hitsMusic = 0;
            double grMusic = 0.0;
            juce::String setWorst;
            for (auto& sg : sigs)
            {
                for (int capOn = 1; capOn >= 0; --capOn)
                {
                    BoothEQProcessor p;
                    st.set (p);
                    p.prepareToPlay (fs, 256);
                    p.getEngine().setCapEnabled (capOn == 1);
                    p.getEngine().clearCapHits();
                    auto out = run (p, sg.buf, 256);
                    if (! allFinite (out)) check (false, juce::String ("NaN/Inf bei ") + st.name + " / " + sg.name + sr);
                    const double rise = toDb (peak (out) / peak (sg.buf));
                    if (capOn == 0) { rawExcess = juce::jmax (rawExcess, rise - st.boostDb); continue; }
                    const double rmsRise = toDb (maxRms (out, fs) / maxRms (sg.buf, fs));
                    totalCapHits += p.getEngine().capHitCount();
                    if (juce::String (sg.name).startsWith ("Hitech"))
                    {
                        hitsMusic += p.getEngine().capHitCount();
                        grMusic = juce::jmax (grMusic, p.getEngine().maxCapReductionDb());
                    }
                    if (rise > setRise) { setRise = rise; setWorst = sg.name; }
                    setExcess = juce::jmax (setExcess, rise - st.boostDb);
                    setRms = juce::jmax (setRms, rmsRise - st.boostDb);
                }
            }
            std::printf ("  %-38s Peak %+6.2f dB (%s) | über Einstellung: Peak %+5.2f (ohne Deckel %+5.2f) RMS %+5.2f | Deckel bei Musik: %d Samples, max. %.1f dB\n",
                         st.name, setRise, setWorst.toRawUTF8(), setExcess, rawExcess, setRms, hitsMusic, grMusic);
            if (setExcess > worstExcessPeak) { worstExcessPeak = setExcess; worstCase = juce::String (st.name) + sr; }
            worstRise = juce::jmax (worstRise, setRise);
            worstExcessRms = juce::jmax (worstExcessRms, setRms);
            worstRaw = juce::jmax (worstRaw, rawExcess);
        }

        // 4) Bewegung: Regler hart hin und her, Kills schnell tippen, Kurve umschalten, Trennfrequenz fahren
        {
            BoothEQProcessor p;
            p.prepareToPlay (fs, 128);
            auto in = hitech (fs, 4.0, 5);
            juce::Random rnd (9);
            p.getEngine().clearCapHits();
            auto out = run (p, in, 128, [&] (int k, int)
            {
                if (k % 3 == 0) setParam (p, ParamIDs::low, rnd.nextBool() ? 1.0f : 0.0f);
                if (k % 5 == 0) setParam (p, ParamIDs::mid, rnd.nextFloat());
                if (k % 7 == 0) setParam (p, ParamIDs::high, rnd.nextBool() ? 1.0f : 0.0f);
                if (k % 4 == 0) setParam (p, ParamIDs::killLow, rnd.nextBool() ? 1.0f : 0.0f);
                if (k % 6 == 0) setParam (p, ParamIDs::killMid, rnd.nextBool() ? 1.0f : 0.0f);
                if (k % 50 == 0) setParam (p, ParamIDs::curve, rnd.nextBool() ? 1.0f : 0.0f);
                if (k % 11 == 0) setParam (p, ParamIDs::xlow, 80.0f + 320.0f * rnd.nextFloat());
                if (k % 13 == 0) setParam (p, ParamIDs::xhigh, 1200.0f + 4800.0f * rnd.nextFloat());
            });
            const double rise = toDb (peak (out) / peak (in));
            totalCapHits += p.getEngine().capHitCount();
            {
                BoothEQProcessor q;
                q.prepareToPlay (fs, 128);
                q.getEngine().setCapEnabled (false);
                juce::Random rnd2 (9);
                auto raw = run (q, in, 128, [&] (int k, int)
                {
                    if (k % 3 == 0) setParam (q, ParamIDs::low, rnd2.nextBool() ? 1.0f : 0.0f);
                    if (k % 5 == 0) setParam (q, ParamIDs::mid, rnd2.nextFloat());
                    if (k % 7 == 0) setParam (q, ParamIDs::high, rnd2.nextBool() ? 1.0f : 0.0f);
                    if (k % 4 == 0) setParam (q, ParamIDs::killLow, rnd2.nextBool() ? 1.0f : 0.0f);
                    if (k % 6 == 0) setParam (q, ParamIDs::killMid, rnd2.nextBool() ? 1.0f : 0.0f);
                    if (k % 50 == 0) setParam (q, ParamIDs::curve, rnd2.nextBool() ? 1.0f : 0.0f);
                    if (k % 11 == 0) setParam (q, ParamIDs::xlow, 80.0f + 320.0f * rnd2.nextFloat());
                    if (k % 13 == 0) setParam (q, ParamIDs::xhigh, 1200.0f + 4800.0f * rnd2.nextFloat());
                });
                const double rawRise = toDb (peak (raw) / peak (in));
                std::printf ("    wildes Drehen ohne Deckel: Peak %+.2f dB über Eingang\n", rawRise);
                worstRaw = juce::jmax (worstRaw, rawRise - 6.0);
            }
            check (allFinite (out) && rise <= 6.0 + 3.0, "wildes Drehen/Tippen: Peak " + juce::String (rise, 2) + " dB über Eingang (Grenze +9), Deckel-Eingriffe "
                                                          + juce::String (p.getEngine().capHitCount()) + sr);
            worstRise = juce::jmax (worstRise, rise);
            worstExcessPeak = juce::jmax (worstExcessPeak, rise - 6.0);
        }

        // 5) Klickfreiheit der Kill-Taster (Sinus mitten im Band, Taster bei Nulldurchgang-fernem Punkt)
        for (auto cfg : { std::tuple { ParamIDs::killLow, 60.0 }, std::tuple { ParamIDs::killMid, 700.0 } })
        {
            const char* id = std::get<0> (cfg);
            const double f = std::get<1> (cfg);
            BoothEQProcessor p;
            p.prepareToPlay (fs, 64);
            auto in = sine (fs, 1.0, f, 0.8f);
            const int on = (int) (fs * 0.3) + 17, off = (int) (fs * 0.6) + 33;
            juce::AudioBuffer<float> out (in);
            juce::MidiBuffer midi;
            for (int pos = 0; pos < in.getNumSamples(); )
            {
                int len = juce::jmin (64, in.getNumSamples() - pos);
                if (pos < on && pos + len > on) len = on - pos;
                if (pos < off && pos + len > off) len = off - pos;
                if (pos == on) setParam (p, id, 1.0f);
                if (pos == off) setParam (p, id, 0.0f);
                juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, len);
                p.processBlock (view, midi);
                pos += len;
            }
            // Klick = breitbandiger Hochton-Splatter. Hochpass (2x 2. Ordnung) ab 4 kHz (bzw. 12x Testton),
            // Spitze relativ zum Ton. Vergleiche: hartes Umschalten und idealer 5-ms-Fade (Untergrenze).
            auto hfPeakDb = [&] (const juce::AudioBuffer<float>& b)
            {
                const double hpF = juce::jmax (4000.0, f * 12.0);
                auto co = juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, (float) hpF);
                juce::dsp::IIR::Filter<float> h1, h2;
                h1.coefficients = co; h2.coefficients = co;
                h1.reset(); h2.reset();
                double pk = 0.0;
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    const float v = h2.processSample (h1.processSample (b.getSample (0, i)));
                    if (i > (int) (fs * 0.1)) pk = juce::jmax (pk, (double) std::abs (v));
                }
                return toDb (pk / 0.8);
            };
            juce::AudioBuffer<float> hard (in);
            {
                // hartes Umschalten: Ausgang springt auf die statische Kill-Antwort
                BoothEQProcessor pk;
                setParam (pk, id, 1.0f);
                pk.prepareToPlay (fs, 64);
                auto killed = run (pk, in, 64);
                for (int i = on; i < off; ++i)
                    for (int c = 0; c < 2; ++c) hard.setSample (c, i, killed.getSample (c, i));
            }
            juce::AudioBuffer<float> ideal (in);
            {
                // idealer Fade: Crossfade (S-Kurve, 5 ms) zwischen ungekilltem und statisch gekilltem Signal
                BoothEQProcessor pk;
                setParam (pk, id, 1.0f);
                pk.prepareToPlay (fs, 64);
                auto killed = run (pk, in, 64);
                const int fl = juce::roundToInt (beq::kKillFadeSec * fs);
                for (int i = 0; i < in.getNumSamples(); ++i)
                {
                    double a = 0.0;
                    if (i >= on && i < off) a = juce::jmin (1.0, (i - on) / (double) fl);
                    if (i >= off) a = juce::jmax (0.0, 1.0 - (i - off) / (double) fl);
                    a = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * a);
                    for (int c = 0; c < 2; ++c) ideal.setSample (c, i, (float) ((1.0 - a) * in.getSample (c, i) + a * killed.getSample (c, i)));
                }
            }
            const double ours = hfPeakDb (out), ref = hfPeakDb (hard), best = hfPeakDb (ideal);
            check (ours < -60.0 && ours <= best + 3.0, juce::String (id) + " klickfrei: Splatter ab 4 kHz " + juce::String (ours, 1) + " dB (idealer Fade "
                                  + juce::String (best, 1) + " dB, hartes Umschalten " + juce::String (ref, 1) + " dB)" + sr);
        }

        // 6) Transport Stopp/Start: Host ruft reset() mitten im Signal -> kein Knall, keine NaN
        {
            BoothEQProcessor p;
            setParam (p, ParamIDs::low, 1.0f);
            p.prepareToPlay (fs, 256);
            auto in = hitech (fs, 2.0, 6);
            auto out = run (p, in, 256, [&] (int k, int) { if (k == 100) p.reset(); });
            const double rise = toDb (peak (out) / peak (in));
            BoothEQProcessor ref;
            setParam (ref, ParamIDs::low, 1.0f);
            ref.prepareToPlay (fs, 256);
            auto plain = run (ref, in, 256);
            float d = 0.0f;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < in.getNumSamples(); ++i) d = juce::jmax (d, std::abs (out.getSample (c, i) - plain.getSample (c, i)));
            check (allFinite (out) && d == 0.0f, "Transport Stopp/Start (Host-reset mitten im Signal): kein Sprung, identisch zu ohne reset, Peak "
                                                  + juce::String (rise, 2) + " dB bei Low +6" + sr);
            auto out2 = run (p, in, 256, [&] (int k, int) { if (k == 100) p.prepareToPlay (fs, 256); });
            const double rise2 = toDb (peak (out2) / peak (in));
            check (allFinite (out2) && rise2 <= 6.0 + beq::kCapHeadroomDb + 0.01, "Neu-Initialisierung (prepare) mitten im Signal: Peak " + juce::String (rise2, 2)
                                                                                   + " dB bei Low +6 (Grenze Anhebung + Deckel)" + sr);
        }
    }

    // 7) NaN/Inf am Eingang
    {
        BoothEQProcessor p;
        setParam (p, ParamIDs::mid, 0.2f);
        p.prepareToPlay (48000.0, 256);
        auto in = sine (48000.0, 1.0, 440.0, 0.5f);
        in.setSample (0, 1000, std::numeric_limits<float>::quiet_NaN());
        in.setSample (1, 2000, std::numeric_limits<float>::infinity());
        auto out = run (p, in, 256);
        check (allFinite (out) && peak (out) <= 0.5f, "NaN/Inf am Eingang: Ausgang endlich, Peak " + juce::String (peak (out), 3));
    }

    // 8) Mono, Blockgrößen
    {
        BoothEQProcessor p;
        p.setPlayConfigDetails (1, 1, 48000.0, 4096);
        setParam (p, ParamIDs::killLow, 1.0f);
        p.prepareToPlay (48000.0, 4096);
        juce::MidiBuffer midi;
        bool ok = true;
        int pos = 0;
        for (int blk : { 0, 1, 7, 13, 64, 4096, 333, 1 })
        {
            juce::AudioBuffer<float> b (1, juce::jmax (1, blk));
            for (int i = 0; i < blk; ++i, ++pos) b.setSample (0, i, 0.5f * std::sin ((float) pos * 0.0123f));
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 1, 0, blk);
            p.processBlock (view, midi);
            ok = ok && allFinite (view);
        }
        check (ok, "Mono + Blockgrößen 0/1/7/13/64/4096: keine NaN");
    }

    // 9) Rechenzeit + Latenz
    {
        BoothEQProcessor p;
        setParam (p, ParamIDs::low, 0.2f);
        setParam (p, ParamIDs::high, 0.7f);
        p.prepareToPlay (48000.0, 128);
        juce::AudioBuffer<float> b (2, 128);
        juce::MidiBuffer midi;
        juce::Random rnd (3);
        const int blocks = (int) (60.0 * 48000.0 / 128);
        double procSec = 0.0;
        for (int k = 0; k < blocks; ++k)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) b.setSample (c, i, rnd.nextFloat() - 0.5f);
            if (k % 400 == 0) setParam (p, ParamIDs::mid, rnd.nextFloat());
            const auto t0 = std::chrono::steady_clock::now();
            p.processBlock (b, midi);
            procSec += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        }
        check (procSec < 1.2, "CPU: 60 s Stereo (EQ aktiv) in " + juce::String (procSec * 1000.0, 1) + " ms = " + juce::String (procSec / 60.0 * 100.0, 3) + " % eines Kerns");
        check (p.getLatencySamples() == 0, "Latenz 0 Samples");
    }

    // 10) Zustand speichern/laden (Kill-Taster laden nie gedrückt)
    {
        BoothEQProcessor a, b;
        setParam (a, ParamIDs::high, 0.8f); setParam (a, ParamIDs::low, 0.15f); setParam (a, ParamIDs::xlow, 220.0f); setParam (a, ParamIDs::killMid, 1.0f);
        juce::MemoryBlock mb; a.getStateInformation (mb); b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (b.apvts.getRawParameterValue (ParamIDs::low)->load() - 0.15f) < 1e-4f
               && std::abs (b.apvts.getRawParameterValue (ParamIDs::xlow)->load() - 220.0f) < 0.5f
               && b.apvts.getRawParameterValue (ParamIDs::killMid)->load() < 0.5f, "Zustand speichern/laden, Kill-Taster laden losgelassen");

        // Parameter-Reihenfolge wie XL
        juce::StringArray ids;
        for (auto* prm : a.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) ids.add (r->getParameterID());
        check (ids.joinIntoString (",") == "low,mid,high,killLow,killMid,curve,xlow,xhigh", "Parameter-Reihenfolge: " + ids.joinIntoString (", "));
    }

    // 11) GUI-Bilder
    {
        BoothEQProcessor a;
        setParam (a, ParamIDs::low, 0.30f); setParam (a, ParamIDs::high, 0.66f); setParam (a, ParamIDs::killMid, 1.0f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        for (float sc : { 1.0f, 2.0f })
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, sc);
            juce::File f (outDir + "/boothEq" + (sc > 1 ? "_2x" : "") + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("Bild: %s\n", f.getFullPathName().toRawUTF8());
        }
    }

    std::printf ("\n===== LAUTSTÄRKE-CHECK ZUSAMMENFASSUNG =====\n");
    std::printf ("Max. Peak-Anstieg absolut: %+.2f dB\n", worstRise);
    std::printf ("Max. Peak über eingestellter Anhebung: %+.2f dB (%s)\n", worstExcessPeak, worstCase.toRawUTF8());
    std::printf ("Max. Peak über eingestellter Anhebung OHNE Deckel (rein vom EQ): %+.2f dB\n", worstRaw);
    std::printf ("Max. Kurzzeit-RMS über eingestellter Anhebung: %+.2f dB\n", worstExcessRms);
    std::printf ("Pegeldeckel-Eingriffe gesamt: %d\n", totalCapHits);

    std::printf (failures == 0 ? "\nALLES OK\n" : "\n%d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}
