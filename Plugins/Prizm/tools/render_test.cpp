// Offline test for Brainbugz Prizm: runs the real processor on synthetic signals,
// checks the results and writes WAV files for listening. Not part of the plugin.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "../Source/PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <random>

using namespace prizm;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (! ok)
        ++failures;
}

void setParam (PrizmAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

juce::AudioBuffer<float> render (PrizmAudioProcessor& p, const juce::AudioBuffer<float>& in, juce::MidiBuffer* firstBlockMidi = nullptr)
{
    juce::AudioBuffer<float> out (in);
    for (int start = 0; start < out.getNumSamples(); start += kBlock)
    {
        const int n = std::min (kBlock, out.getNumSamples() - start);
        juce::AudioBuffer<float> block (out.getArrayOfWritePointers(), out.getNumChannels(), start, n);
        juce::MidiBuffer midi;
        if (start == 0 && firstBlockMidi != nullptr)
            midi = *firstBlockMidi;
        p.processBlock (block, midi);
    }
    return out;
}

juce::AudioBuffer<float> makeTone (std::initializer_list<float> freqs, double seconds, float amp)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * kSampleRate));
    b.clear();
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        float s = 0.0f;
        for (float f : freqs)
            s += std::sin (juce::MathConstants<float>::twoPi * f * (float) i / (float) kSampleRate);
        b.setSample (0, i, s * amp);
        b.setSample (1, i, s * amp);
    }
    return b;
}

// Rich sawtooth cluster: dissonant "pad".
juce::AudioBuffer<float> makeSawPad (std::initializer_list<float> freqs, double seconds, float amp)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * kSampleRate));
    b.clear();
    std::vector<float> phase (freqs.size(), 0.0f);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        float s = 0.0f;
        size_t k = 0;
        for (float f : freqs)
        {
            phase[k] += f / (float) kSampleRate;
            phase[k] -= std::floor (phase[k]);
            s += 2.0f * phase[k] - 1.0f;
            ++k;
        }
        const float env = std::min (1.0f, (float) i / 2400.0f);
        b.setSample (0, i, s * amp * env);
        b.setSample (1, i, s * amp * env);
    }
    return b;
}

juce::AudioBuffer<float> makeNoise (double seconds, float amp)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * kSampleRate));
    std::mt19937 rng (42);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, d (rng) * amp);
    return b;
}

// Synthetic drum-ish loop: clicks + decaying noise + low thump, to test Clear.
juce::AudioBuffer<float> makeHits (double seconds)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * kSampleRate));
    b.clear();
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    const int period = (int) (0.25 * kSampleRate);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const int t = i % period;
        const float tt = (float) t / (float) kSampleRate;
        const float thump = std::sin (juce::MathConstants<float>::twoPi * (55.0f + 80.0f * std::exp (-tt * 40.0f)) * tt) * std::exp (-tt * 12.0f);
        const float snap = d (rng) * std::exp (-tt * 60.0f) * 0.5f;
        const float s = 0.6f * thump + snap;
        b.setSample (0, i, s);
        b.setSample (1, i, s);
    }
    return b;
}

float peakFrequency (const juce::AudioBuffer<float>& b, int startSample)
{
    constexpr int order = 15;
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) n * 2, 0.0f);
    for (int i = 0; i < n && startSample + i < b.getNumSamples(); ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n);
        data[(size_t) i] = b.getSample (0, startSample + i) * w;
    }
    fft.performFrequencyOnlyForwardTransform (data.data(), true);
    int best = 1;
    for (int k = 1; k < n / 2; ++k)
        if (data[(size_t) k] > data[(size_t) best])
            best = k;
    return (float) best * (float) kSampleRate / (float) n;
}

// Fraction of spectral energy (40 Hz .. 8 kHz) that lies within +-25 cents of an allowed note.
float inKeyEnergyRatio (const juce::AudioBuffer<float>& b, int startSample, PitchMask mask)
{
    constexpr int order = 15;
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) n * 2, 0.0f);
    for (int i = 0; i < n && startSample + i < b.getNumSamples(); ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n);
        data[(size_t) i] = b.getSample (0, startSample + i) * w;
    }
    fft.performFrequencyOnlyForwardTransform (data.data(), true);

    double inKey = 0.0, total = 0.0;
    for (int k = 1; k < n / 2; ++k)
    {
        const float hz = (float) k * (float) kSampleRate / (float) n;
        if (hz < 40.0f || hz > 8000.0f)
            continue;
        const double e = (double) data[(size_t) k] * data[(size_t) k];
        const float midi = hzToMidi (hz);
        const int note = (int) std::lround (midi);
        total += e;
        if (isAllowed (mask, note) && std::abs (midi - (float) note) < 0.25f)
            inKey += e;
    }
    return total > 0.0 ? (float) (inKey / total) : 0.0f;
}

struct Stats { float peak = 0.0f, rms = 0.0f; bool finite = true; };

Stats stats (const juce::AudioBuffer<float>& b, int start = 0)
{
    Stats s;
    double sum = 0.0;
    int count = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = start; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (ch, i);
            if (! std::isfinite (v))
                s.finite = false;
            s.peak = std::max (s.peak, std::abs (v));
            sum += (double) v * v;
            ++count;
        }
    s.rms = count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    return s;
}

void writeWav (const juce::AudioBuffer<float>& b, const juce::String& name)
{
    auto dir = juce::File::getCurrentWorkingDirectory().getChildFile ("renders");
    dir.createDirectory();
    auto file = dir.getChildFile (name + ".wav");
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (auto* writer = wav.createWriterFor (stream.get(), kSampleRate, (unsigned int) b.getNumChannels(), 24, {}, 0))
    {
        stream.release();
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        delete writer;
    }
}


// Magnitude spectrum (32k FFT, Hann) of channel 0 from startSample.
std::vector<float> spectrum (const juce::AudioBuffer<float>& b, int startSample)
{
    constexpr int order = 15;
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) n * 2, 0.0f);
    for (int i = 0; i < n && startSample + i < b.getNumSamples(); ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n);
        data[(size_t) i] = b.getSample (0, startSample + i) * w;
    }
    fft.performFrequencyOnlyForwardTransform (data.data(), true);
    data.resize ((size_t) n / 2);
    return data;
}

float binHz() { return (float) kSampleRate / 32768.0f; }

// Energy per pitch class (40 Hz .. 5 kHz), normalised to unit length.
std::array<double, 12> pitchClassVector (const std::vector<float>& spec)
{
    std::array<double, 12> v {};
    for (size_t k = 1; k < spec.size(); ++k)
    {
        const float hz = (float) k * binHz();
        if (hz < 40.0f || hz > 5000.0f)
            continue;
        const int pc = (((int) std::lround (hzToMidi (hz))) % 12 + 12) % 12;
        v[(size_t) pc] += (double) spec[k] * spec[k];
    }
    double norm = 0.0;
    for (auto x : v) norm += x * x;
    norm = std::sqrt (norm) + 1e-30;
    for (auto& x : v) x /= norm;
    return v;
}

double cosine (const std::array<double, 12>& a, const std::array<double, 12>& b)
{
    double d = 0.0;
    for (size_t i = 0; i < 12; ++i) d += a[i] * b[i];
    return d;
}

// Levels in third-octave bands 63 Hz .. 8 kHz (dB).
std::vector<double> thirdOctaves (const std::vector<float>& spec)
{
    std::vector<double> out;
    for (double fc = 63.0; fc <= 8000.0; fc *= std::pow (2.0, 1.0 / 3.0))
    {
        const double lo = fc / std::pow (2.0, 1.0 / 6.0), hi = fc * std::pow (2.0, 1.0 / 6.0);
        double e = 1e-20;
        for (size_t k = 1; k < spec.size(); ++k)
        {
            const double hz = (double) k * binHz();
            if (hz >= lo && hz < hi)
                e += (double) spec[k] * spec[k];
        }
        out.push_back (10.0 * std::log10 (e));
    }
    return out;
}

// Energy around one frequency (+- width Hz), in dB.
double toneLevel (const std::vector<float>& spec, float hz, float width = 6.0f)
{
    double e = 1e-20;
    for (size_t k = 1; k < spec.size(); ++k)
        if (std::abs ((float) k * binHz() - hz) <= width)
            e += (double) spec[k] * spec[k];
    return 10.0 * std::log10 (e);
}

// Akkordfolge mit Grundton-Bass, additiv (8 Obertöne), jede Stufe 1 s. detune in Cent.
juce::AudioBuffer<float> makeProgression (const std::vector<std::vector<int>>& chords, int bassNote, double seconds, float detuneCents = 0.0f)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * kSampleRate));
    b.clear();
    const float scale = std::pow (2.0f, detuneCents / 1200.0f);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const auto& chord = chords[(size_t) ((i / (int) kSampleRate) % (int) chords.size())];
        const float t = (float) i / (float) kSampleRate;
        float s = 0.0f;
        auto voice = [&] (int note, float amp)
        {
            const float f = midiToHz ((float) note) * scale;
            for (int h = 1; h <= 8; ++h)
                if (f * (float) h < 8000.0f)
                    s += amp / (float) h * std::sin (juce::MathConstants<float>::twoPi * f * (float) h * t);
        };
        for (int n : chord)
            voice (n, 0.05f);
        voice (bassNote + (chord[0] - chords[0][0]), 0.08f); // Bass folgt dem Akkordgrundton
        b.setSample (0, i, s);
        b.setSample (1, i, s);
    }
    return b;
}

// Harmonischer Ton (additiv), Amplitude pro Oberton aus einer Funktion.
template <typename AmpFn>
juce::AudioBuffer<float> makeHarmonic (std::initializer_list<float> f0s, int harmonics, double seconds, AmpFn amp)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * kSampleRate));
    b.clear();
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = (float) i / (float) kSampleRate;
        float s = 0.0f;
        for (float f0 : f0s)
            for (int h = 1; h <= harmonics; ++h)
                s += amp (f0, h) * std::sin (juce::MathConstants<float>::twoPi * f0 * (float) h * t + 0.7f * (float) h);
        b.setSample (0, i, s);
        b.setSample (1, i, s);
    }
    return b;
}

std::unique_ptr<PrizmAudioProcessor> makeProcessor (int mode, float strength, float color, float clear)
{
    auto p = std::make_unique<PrizmAudioProcessor>();
    p->setPlayConfigDetails (2, 2, kSampleRate, kBlock);
    setParam (*p, ParamIDs::mode, (float) mode);
    setParam (*p, ParamIDs::key, 0.0f);   // C
    setParam (*p, ParamIDs::scale, 0.0f); // Major
    setParam (*p, ParamIDs::strength, strength);
    setParam (*p, ParamIDs::color, color);
    setParam (*p, ParamIDs::clear, clear);
    setParam (*p, ParamIDs::mix, 100.0f);
    setParam (*p, ParamIDs::output, 0.0f);
    p->prepareToPlay (kSampleRate, kBlock);
    return p;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const PitchMask cMajor = rotateToKey (kScales[0].mask, 0);

    // 1) HQ mode, Strength 0, Color 0, Clear 0 must reproduce the input (delayed by the latency).
    {
        auto p = makeProcessor (highQuality, 0.0f, 0.0f, 0.0f);
        auto in = makeSawPad ({ 110.0f, 163.0f, 247.0f }, 2.0, 0.15f);
        auto out = render (*p, in);
        const int lat = p->getLatencySamples();
        double err = 0.0, ref = 0.0;
        for (int i = lat + 8192; i < in.getNumSamples(); ++i)
        {
            const float d = out.getSample (0, i) - in.getSample (0, i - lat);
            err += (double) d * d;
            ref += (double) in.getSample (0, i - lat) * in.getSample (0, i - lat);
        }
        const double snr = 10.0 * std::log10 (ref / std::max (err, 1.0e-20));
        check (lat == 4096, "HQ latency reported = " + juce::String (lat) + " samples");
        check (snr > 30.0, "HQ neutral settings reproduce the input (SNR " + juce::String (snr, 1) + " dB)");
    }

    // 2) HQ + SHIFT moves an out-of-key sine (450 Hz, between A and A#) to A = 440 Hz.
    {
        auto p = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        setParam (*p, ParamIDs::shift, 100.0f);
        auto out = render (*p, makeTone ({ 450.0f }, 2.0, 0.3f));
        const float f = peakFrequency (out, 16000);
        check (std::abs (f - 440.0f) < 3.0f, "HQ snaps 450 Hz sine to " + juce::String (f, 1) + " Hz (target 440)");

        auto p2 = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        setParam (*p2, ParamIDs::shift, 100.0f);
        auto out2 = render (*p2, makeTone ({ 285.0f }, 2.0, 0.3f)); // between C# and D, C# not in C major -> D 293.7
        const float f2 = peakFrequency (out2, 16000);
        check (std::abs (f2 - 293.66f) < 3.0f, "HQ snaps 285 Hz sine to " + juce::String (f2, 1) + " Hz (target D 293.7)");

        auto p3 = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        setParam (*p3, ParamIDs::shift, 50.0f);
        auto out3 = render (*p3, makeTone ({ 450.0f }, 2.0, 0.3f));
        const float f3 = peakFrequency (out3, 16000);
        check (f3 > 441.0f && f3 < 449.0f, "HQ Shift 50 % pulls halfway: " + juce::String (f3, 1) + " Hz");
    }

    // 3) Both modes raise the share of in-key energy on noise and on a dissonant saw pad.
    for (int mode : { zeroLatency, highQuality })
    {
        const juce::String name = mode == zeroLatency ? "ZeroLatency" : "HighQuality";

        auto noise = makeNoise (3.0, 0.2f);
        auto p = makeProcessor (mode, 100.0f, 60.0f, 0.0f);
        auto outNoise = render (*p, noise);
        const float before = inKeyEnergyRatio (noise, 48000, cMajor);
        const float after = inKeyEnergyRatio (outNoise, 48000, cMajor);
        check (after > before * 1.8f, name + ": noise in-key energy " + juce::String (before * 100, 1) + " % -> " + juce::String (after * 100, 1) + " %");

        auto pad = makeSawPad ({ 146.0f, 185.0f, 207.0f, 311.0f }, 4.0, 0.12f); // ~D, F#, G#, D# : clashes with C major
        auto p2 = makeProcessor (mode, 100.0f, 60.0f, 30.0f);
        auto outPad = render (*p2, pad);
        const float padBefore = inKeyEnergyRatio (pad, 48000, cMajor);
        const float padAfter = inKeyEnergyRatio (outPad, 48000, cMajor);
        check (padAfter > padBefore * 1.5f, name + ": dissonant pad in-key energy " + juce::String (padBefore * 100, 1) + " % -> " + juce::String (padAfter * 100, 1) + " %");

        const auto sIn = stats (pad, 48000), sOut = stats (outPad, 48000);
        const float levelDb = 20.0f * std::log10 (sOut.rms / sIn.rms);
        check (std::abs (levelDb) < 6.0f, name + ": output level vs input " + juce::String (levelDb, 1) + " dB");

        writeWav (pad, "pad_input");
        writeWav (outPad, "pad_" + name);
        writeWav (outNoise, "noise_" + name);
    }

    // 3b) The key display energy sits on in-key notes.
    for (int mode : { zeroLatency, highQuality })
    {
        auto p = makeProcessor (mode, 100.0f, 60.0f, 0.0f);
        render (*p, makeNoise (2.0, 0.2f));
        float inKey = 0.0f, outKey = 0.0f;
        for (int pc = 0; pc < 12; ++pc)
            (isAllowed (cMajor, pc) ? inKey : outKey) += p->getPitchEnergy (pc);
        check (inKey > outKey * 5.0f, juce::String (mode == zeroLatency ? "ZeroLatency" : "HighQuality")
                                          + ": key display energy in key " + juce::String (100.0f * inKey / (inKey + outKey + 1e-20f), 1) + " %");
    }

    // 3c) Both modes must sound the same at the same settings (same filter curve).
    {
        struct Case { const char* name; juce::AudioBuffer<float> in; float color; };
        std::vector<Case> cases;
        cases.push_back ({ "noise", makeNoise (4.0, 0.2f), 60.0f });
        cases.push_back ({ "pad", makeSawPad ({ 146.0f, 185.0f, 207.0f, 311.0f }, 4.0, 0.12f), 60.0f });
        cases.push_back ({ "pad soft (Color 0)", makeSawPad ({ 146.0f, 185.0f, 207.0f, 311.0f }, 4.0, 0.12f), 0.0f });
        cases.push_back ({ "pad tonal (Color 120)", makeSawPad ({ 146.0f, 185.0f, 207.0f, 311.0f }, 4.0, 0.12f), 120.0f });
        cases.push_back ({ "hits", makeHits (4.0), 60.0f });

        for (auto& c : cases)
        {
            auto pz = makeProcessor (zeroLatency, 100.0f, c.color, 30.0f);
            auto ph = makeProcessor (highQuality, 100.0f, c.color, 30.0f);
            auto oz = render (*pz, c.in);
            auto oh = render (*ph, c.in);
            const int lat = ph->getLatencySamples();
            const auto sz = spectrum (oz, 48000), sh = spectrum (oh, 48000 + lat);
            const double cs = cosine (pitchClassVector (sz), pitchClassVector (sh));
            const auto tz = thirdOctaves (sz), th = thirdOctaves (sh);
            double envDiff = 0.0;
            int bands = 0;
            for (size_t i = 0; i < tz.size(); ++i)
                if (tz[i] > -60.0 || th[i] > -60.0) { envDiff += std::abs (tz[i] - th[i]); ++bands; }
            envDiff /= std::max (1, bands);
            const float lz = stats (oz, 48000).rms, lh = stats (oh, 48000 + lat).rms;
            const double levelDiff = 20.0 * std::log10 (lh / lz);
            // Drums haben keine feste Tonhöhe (Kick-Glide), dort zählen nur Klangkurve und Pegel.
            const bool pitched = juce::String (c.name) != "hits";
            check ((! pitched || cs > 0.97) && envDiff < 3.0 && std::abs (levelDiff) < 1.5,
                   juce::String ("Modes agree on ") + c.name + ": note match " + juce::String (cs * 100.0, 1)
                       + " %, tone curve diff " + juce::String (envDiff, 2) + " dB, level diff " + juce::String (levelDiff, 2) + " dB");
            writeWav (oz, juce::String ("compare_") + juce::String (c.name).retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789") + "_zl");
            writeWav (oh, juce::String ("compare_") + juce::String (c.name).retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789") + "_hq");
        }
    }

    // 3c2) SHIFT keeps a snapped tone clean (phase locking): energy at 440 Hz vs everything else.
    {
        auto p = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        setParam (*p, ParamIDs::shift, 100.0f);
        auto out = render (*p, makeTone ({ 450.0f }, 3.0, 0.3f));
        const auto sp = spectrum (out, 48000 + p->getLatencySamples());
        double tone = 0.0, rest = 1e-20;
        for (size_t k = 1; k < sp.size(); ++k)
        {
            const float hz = (float) k * binHz();
            (std::abs (hz - 440.0f) < 10.0f ? tone : rest) += (double) sp[k] * sp[k];
        }
        const double purity = 10.0 * std::log10 (tone / rest);
        check (purity > 30.0, "SHIFT output stays clean: tone " + juce::String (purity, 1) + " dB above artefacts");
    }

    // 3c3) Mode switch ZL -> HQ -> ZL, with the host re-preparing on the latency change
    //      (many DAWs do that), must still filter right away without touching the notes.
    {
        auto p = makeProcessor (zeroLatency, 100.0f, 60.0f, 0.0f);
        auto pad = makeSawPad ({ 146.0f, 185.0f, 207.0f, 311.0f }, 3.0, 0.12f);
        render (*p, pad);
        setParam (*p, ParamIDs::mode, (float) highQuality);
        render (*p, pad);
        p->prepareToPlay (kSampleRate, kBlock); // Host startet neu wegen geänderter Latenz
        setParam (*p, ParamIDs::mode, (float) zeroLatency);
        p->prepareToPlay (kSampleRate, kBlock);
        auto out = render (*p, pad);
        const float ratio = inKeyEnergyRatio (out, 48000, cMajor);
        const float fresh = inKeyEnergyRatio (render (*makeProcessor (zeroLatency, 100.0f, 60.0f, 0.0f), pad), 48000, cMajor);
        check (std::abs (ratio - fresh) < 0.03f && stats (out, 48000).rms > 0.01f,
               "Switching back to Zero Latency works immediately (in key " + juce::String (ratio * 100.0f, 1) + " %, fresh instance "
                   + juce::String (fresh * 100.0f, 1) + " %)");
    }

    // 3d) Range: below RANGE LOW the signal stays untouched (an out-of-key 104 Hz bass survives).
    for (int mode : { zeroLatency, highQuality })
    {
        const juce::String name = mode == zeroLatency ? "ZeroLatency" : "HighQuality";
        auto in = makeTone ({ 104.0f, 450.0f }, 3.0, 0.25f); // both out of C major
        auto pFull = makeProcessor (mode, 100.0f, 60.0f, 0.0f);
        auto pCut = makeProcessor (mode, 100.0f, 60.0f, 0.0f);
        setParam (*pCut, ParamIDs::rangeLow, 300.0f);
        auto oFull = render (*pFull, in);
        auto oCut = render (*pCut, in);
        const int lat = pFull->getLatencySamples();
        const auto sIn = spectrum (in, 48000), sFull = spectrum (oFull, 48000 + lat), sCut = spectrum (oCut, 48000 + lat);
        const double bassIn = toneLevel (sIn, 104.0f), bassFull = toneLevel (sFull, 104.0f), bassCut = toneLevel (sCut, 104.0f);
        check (std::abs (bassCut - bassIn) < 1.0 && bassFull < bassIn - 6.0,
               name + ": Range Low 300 Hz keeps the 104 Hz bass (" + juce::String (bassCut - bassIn, 2) + " dB, full range: "
                   + juce::String (bassFull - bassIn, 1) + " dB)");
    }

    // 3e) Gate: quiet noise around a loud in-key tone gets pushed down.
    for (int mode : { zeroLatency, highQuality })
    {
        const juce::String name = mode == zeroLatency ? "ZeroLatency" : "HighQuality";
        auto in = makeTone ({ 261.63f }, 3.0, 0.3f); // C4
        auto noise = makeNoise (3.0, 0.02f);
        for (int ch = 0; ch < 2; ++ch)
            in.addFrom (ch, 0, noise, ch, 0, in.getNumSamples());
        auto p0 = makeProcessor (mode, 100.0f, 60.0f, 0.0f);
        auto p1 = makeProcessor (mode, 100.0f, 60.0f, 0.0f);
        setParam (*p1, ParamIDs::gate, 100.0f);
        auto o0 = render (*p0, in);
        auto o1 = render (*p1, in);
        const int lat = p0->getLatencySamples();
        auto ratio = [] (const std::vector<float>& sp)
        {
            double tone = 0.0, rest = 1e-20;
            for (size_t k = 1; k < sp.size(); ++k)
            {
                const float hz = (float) k * binHz();
                if (hz < 40.0f || hz > 8000.0f) continue;
                (std::abs (hz - 261.63f) < 8.0f ? tone : rest) += (double) sp[k] * sp[k];
            }
            return 10.0 * std::log10 (tone / rest);
        };
        const double r0 = ratio (spectrum (o0, 48000 + lat)), r1 = ratio (spectrum (o1, 48000 + lat));
        check (r1 > r0 + 6.0, name + ": Gate 100 % lifts the tone above the noise by " + juce::String (r1 - r0, 1) + " dB");
    }

    // 3f) LEARN: Tonart und Stimmton aus dem Eingang erkennen.
    {
        struct LearnCase { const char* name; std::vector<std::vector<int>> chords; int bass; float detune; int key; bool minor; };
        const std::vector<LearnCase> cases {
            { "D major (D G A D)",            { { 62, 66, 69 }, { 67, 71, 74 }, { 69, 73, 76 }, { 62, 66, 69 } }, 38, 0.0f,  2, false },
            { "E minor (Em Am Bm Em)",        { { 64, 67, 71 }, { 69, 72, 76 }, { 71, 74, 78 }, { 64, 67, 71 } }, 40, 0.0f,  4, true },
            { "F major +30 cent (F Bb C F)",  { { 65, 69, 72 }, { 70, 74, 77 }, { 72, 76, 79 }, { 65, 69, 72 } }, 41, 30.0f, 5, false },
            { "G minor -40 cent (Gm Cm D Gm)",{ { 67, 70, 74 }, { 72, 75, 79 }, { 74, 78, 81 }, { 67, 70, 74 } }, 43, -40.0f, 7, true },
        };
        for (const auto& c : cases)
        {
            auto p = makeProcessor (zeroLatency, 100.0f, 60.0f, 30.0f);
            p->startLearn();
            render (*p, makeProgression (c.chords, c.bass, 10.0, c.detune));
            KeyAnalyzer::Result r;
            const bool got = p->applyLearnResult (r);
            const float expectedA4 = 440.0f * std::pow (2.0f, c.detune / 1200.0f);
            const int keyParam = p->apvts.getParameter (ParamIDs::key)->convertFrom0to1 (p->apvts.getParameter (ParamIDs::key)->getValue()) + 0.5f;
            const float tune = p->apvts.getRawParameterValue (ParamIDs::tune)->load();
            check (got && r.key == c.key && r.minor == c.minor && std::abs (r.a4 - expectedA4) < 1.0f && keyParam == c.key && std::abs (tune - r.a4) < 0.1f,
                   juce::String ("Learn ") + c.name + ": " + (got ? juce::String (kNoteNames[(size_t) r.key]) + (r.minor ? " minor" : " major") : juce::String ("no result"))
                       + ", A4 " + juce::String (r.a4, 1) + " Hz (expected " + juce::String (expectedA4, 1) + "), confidence " + juce::String (r.confidence, 2));
        }
    }

    // 3g) TUNE: mit A4 = 450 Hz landet ein 460-Hz-Ton (SHIFT) auf 450 Hz, und beide Modi stimmen überein.
    {
        auto p = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        setParam (*p, ParamIDs::shift, 100.0f);
        setParam (*p, ParamIDs::tune, 450.0f);
        auto out = render (*p, makeTone ({ 460.0f }, 2.0, 0.3f));
        const float f = peakFrequency (out, 16000);
        check (std::abs (f - 450.0f) < 3.0f, "Tune 450 Hz: SHIFT moves 460 Hz to " + juce::String (f, 1) + " Hz (target 450)");

        // Gefiltertes Rauschen: die Resonanz von A muss mit Tune 450 bei 450 Hz liegen, nicht bei 440 Hz
        for (int mode : { zeroLatency, highQuality })
        {
            auto pt = makeProcessor (mode, 100.0f, 130.0f, 0.0f);
            setParam (*pt, ParamIDs::tune, 450.0f);
            auto o = render (*pt, makeNoise (4.0, 0.2f));
            const auto sp = spectrum (o, 48000 + pt->getLatencySamples());
            const double at450 = toneLevel (sp, 450.0f, 2.0f), at440 = toneLevel (sp, 440.0f, 2.0f);
            check (at450 > at440 + 6.0, juce::String (mode == zeroLatency ? "ZeroLatency" : "HighQuality")
                                            + ": Tune 450 Hz puts the A resonance at 450 Hz (+" + juce::String (at450 - at440, 1) + " dB vs. 440 Hz)");
        }
    }

    // 3h) PITCHMAP: Noten werden mit allen Obertönen gemeinsam verschoben.
    {
        // 450-Hz-Ton mit 8 Obertönen, chromatisch einrasten: der 5. Oberton muss bei 5 x 440 = 2200 Hz landen
        // (gemeinsam verschoben), nicht bei 2217,5 Hz (C#7, wenn jeder Teilton einzeln einrastet).
        auto p = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        setParam (*p, ParamIDs::scale, 13.0f); // Chromatic
        setParam (*p, ParamIDs::shift, 100.0f);
        auto out = render (*p, makeHarmonic ({ 450.0f }, 8, 3.0, [] (float, int h) { return 0.12f / (float) h; }));
        const auto sp = spectrum (out, 48000 + p->getLatencySamples());
        const double grouped = toneLevel (sp, 2200.0f, 5.0f), single = toneLevel (sp, 2217.5f, 5.0f);
        const double g7 = toneLevel (sp, 3080.0f, 5.0f), s7 = toneLevel (sp, 3136.0f, 5.0f);
        check (grouped > single + 10.0 && g7 > s7 + 10.0, "SHIFT moves a note with all its harmonics (5th at 2200 Hz: +"
                                                             + juce::String (grouped - single, 1) + " dB, 7th at 3080 Hz: +" + juce::String (g7 - s7, 1) + " dB)");
    }
    {
        // C-Dur-Akkord, Map E -> Eb: das gespielte E (329,6 Hz) wird Eb (311,1 Hz),
        // der 5. Oberton des C (1308 Hz) bleibt stehen.
        auto chord = makeHarmonic ({ 261.63f, 329.63f, 392.0f }, 6, 3.0, [] (float, int h) { return 0.06f / (float) h; });
        auto p = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        setParam (*p, ParamIDs::mapId (4).toRawUTF8(), 4.0f); // E -> Eb (Index = Ziel + 1)
        auto out = render (*p, chord);
        const int lat = p->getLatencySamples();
        const auto sIn = spectrum (chord, 48000), sOut = spectrum (out, 48000 + lat);
        const double eDrop = toneLevel (sIn, 329.63f, 4.0f) - toneLevel (sOut, 329.63f, 4.0f);
        const double ebGain = toneLevel (sOut, 311.13f, 4.0f) - toneLevel (sIn, 311.13f, 4.0f);
        const double c5 = toneLevel (sOut, 1308.1f, 4.0f) - toneLevel (sIn, 1308.1f, 4.0f);
        const double cRoot = toneLevel (sOut, 261.63f, 4.0f) - toneLevel (sIn, 261.63f, 4.0f);
        check (eDrop > 20.0 && ebGain > 20.0 && std::abs (c5) < 4.0 && std::abs (cRoot) < 3.0,
               "Map E->Eb: E down " + juce::String (eDrop, 1) + " dB, Eb up " + juce::String (ebGain, 1) + " dB, C stays ("
                   + juce::String (cRoot, 1) + " dB), C's 5th harmonic stays (" + juce::String (c5, 1) + " dB)");
        writeWav (chord, "map_input_cmajor");
        writeWav (out, "map_E_to_Eb");
    }
    {
        // Robustheit: Map E->Eb funktioniert auch mit Rauschen darunter
        auto chord = makeHarmonic ({ 261.63f, 329.63f, 392.0f }, 6, 3.0, [] (float, int h) { return 0.06f / (float) h; });
        auto noise = makeNoise (3.0, 0.01f);
        for (int ch = 0; ch < 2; ++ch)
            chord.addFrom (ch, 0, noise, ch, 0, chord.getNumSamples());
        auto p = makeProcessor (highQuality, 100.0f, 60.0f, 0.0f);
        setParam (*p, ParamIDs::mapId (4).toRawUTF8(), 4.0f);
        auto out = render (*p, chord);
        const auto sIn = spectrum (chord, 48000), sOut = spectrum (out, 48000 + p->getLatencySamples());
        const double eDrop = toneLevel (sIn, 329.63f, 4.0f) - toneLevel (sOut, 329.63f, 4.0f);
        const double ebGain = toneLevel (sOut, 311.13f, 4.0f) - toneLevel (sIn, 311.13f, 4.0f);
        check (eDrop > 12.0 && ebGain > 12.0, "Map E->Eb with noise underneath: E down " + juce::String (eDrop, 1) + " dB, Eb up " + juce::String (ebGain, 1) + " dB");
    }
    for (int mode : { highQuality, zeroLatency })
    {
        // Map G -> Mute
        auto chord = makeHarmonic ({ 261.63f, 329.63f, 392.0f }, 6, 3.0, [] (float, int h) { return 0.06f / (float) h; });
        auto p = makeProcessor (mode, 100.0f, 60.0f, 0.0f);
        setParam (*p, ParamIDs::mapId (7).toRawUTF8(), 13.0f);
        auto out = render (*p, chord);
        const int lat = p->getLatencySamples();
        const auto sIn = spectrum (chord, 48000), sOut = spectrum (out, 48000 + lat);
        const double gDrop = toneLevel (sIn, 392.0f, 4.0f) - toneLevel (sOut, 392.0f, 4.0f);
        const double cKeep = toneLevel (sOut, 261.63f, 4.0f) - toneLevel (sIn, 261.63f, 4.0f);
        check (gDrop > (mode == highQuality ? 20.0 : 10.0) && std::abs (cKeep) < 4.0,
               juce::String (mode == zeroLatency ? "ZeroLatency" : "HighQuality") + ": Map G->Mute takes G down by " + juce::String (gDrop, 1)
                   + " dB, C stays (" + juce::String (cKeep, 1) + " dB)");
    }
    {
        // Formant: Ton 220 Hz mit Klangfarben-Spitze bei 1000 Hz, Map A -> C# (+4 Halbtöne).
        auto vowel = makeHarmonic ({ 220.0f }, 20, 3.0, [] (float f0, int h) {
            const float f = f0 * (float) h;
            return 0.15f * (0.03f + 1.0f / (1.0f + std::pow ((f - 1000.0f) / 160.0f, 2.0f)));
        });
        auto peakHarmonic = [] (const std::vector<float>& sp)
        {
            float bestHz = 0.0f, best = 0.0f;
            for (size_t k = 1; k < sp.size(); ++k)
            {
                const float hz = (float) k * binHz();
                if (hz > 400.0f && hz < 3000.0f && sp[k] > best) { best = sp[k]; bestHz = hz; }
            }
            return bestHz;
        };
        float peaks[2] {};
        for (int i = 0; i < 2; ++i)
        {
            auto p = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
            setParam (*p, ParamIDs::scale, 13.0f);
            setParam (*p, ParamIDs::mapId (9).toRawUTF8(), 2.0f); // A -> C#
            setParam (*p, ParamIDs::formant, i == 0 ? 100.0f : 0.0f);
            auto out = render (*p, vowel);
            peaks[i] = peakHarmonic (spectrum (out, 48000 + p->getLatencySamples()));
            writeWav (out, i == 0 ? "formant_on" : "formant_off");
        }
        check (peaks[0] < 1150.0f && peaks[1] > 1200.0f, "Formant keeps the tone colour when moving +4 semitones (peak at "
                                                              + juce::String (peaks[0], 0) + " Hz with Formant, " + juce::String (peaks[1], 0) + " Hz without, source 1000 Hz)");
    }

    // 4) Clear keeps transients: compare attack peaks on the hit loop with Clear 0 vs 100.
    for (int mode : { zeroLatency, highQuality })
    {
        const juce::String name = mode == zeroLatency ? "ZeroLatency" : "HighQuality";
        auto hits = makeHits (3.0);
        auto p0 = makeProcessor (mode, 100.0f, 60.0f, 0.0f);
        auto p1 = makeProcessor (mode, 100.0f, 60.0f, 100.0f);
        auto out0 = render (*p0, hits);
        auto out1 = render (*p1, hits);
        const int lat = p0->getLatencySamples();

        // crest factor in the 2nd half
        auto crest = [] (const juce::AudioBuffer<float>& b, int start)
        {
            auto s = stats (b, start);
            return s.rms > 0 ? s.peak / s.rms : 0.0f;
        };
        const float c0 = crest (out0, 72000 + lat), c1 = crest (out1, 72000 + lat), cIn = crest (hits, 72000);
        check (c1 > c0, name + ": Clear raises punch (crest in " + juce::String (cIn, 2) + ", clear 0: " + juce::String (c0, 2) + ", clear 100: " + juce::String (c1, 2) + ")");
        writeWav (hits, "hits_input");
        writeWav (out0, "hits_" + name + "_clear0");
        writeWav (out1, "hits_" + name + "_clear100");
    }

    // 5) Color above 100 % leaves a tail after the input stops.
    for (int mode : { zeroLatency, highQuality })
    {
        const juce::String name = mode == zeroLatency ? "ZeroLatency" : "HighQuality";
        auto in = makeSawPad ({ 110.0f, 220.0f }, 3.0, 0.2f);
        for (int ch = 0; ch < 2; ++ch)
            in.clear (ch, 48000, in.getNumSamples() - 48000); // 1 s of sound, then silence

        auto pShort = makeProcessor (mode, 100.0f, 50.0f, 0.0f);
        auto pLong = makeProcessor (mode, 100.0f, 190.0f, 0.0f);
        auto outShort = render (*pShort, in);
        auto outLong = render (*pLong, in);
        const int lat = pShort->getLatencySamples();
        juce::AudioBuffer<float> tailShort (2, 24000), tailLong (2, 24000);
        for (int ch = 0; ch < 2; ++ch)
        {
            tailShort.copyFrom (ch, 0, outShort, ch, 48000 + lat + 12000, 24000);
            tailLong.copyFrom (ch, 0, outLong, ch, 48000 + lat + 12000, 24000);
        }
        const float rs = stats (tailShort).rms, rl = stats (tailLong).rms;
        check (rl > rs * 4.0f && rl > 1.0e-4f, name + ": Color 190 % tail is longer (rms " + juce::String (rs, 6) + " vs " + juce::String (rl, 6) + ")");
        writeWav (outLong, "tail_" + name + "_color190");
    }

    // 6) MIDI key: a held A minor chord (A, C, E) becomes the key. 300 Hz is MIDI 62.4,
    //    the nearest chord tone is E4 (64, 1.6 semitones away) rather than C4 (2.4 away).
    {
        auto p = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        setParam (*p, ParamIDs::shift, 100.0f);
        setParam (*p, ParamIDs::midiKey, 1.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0); // A3
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); // C4
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0); // E4
        auto out = render (*p, makeTone ({ 300.0f }, 2.0, 0.3f), &midi);
        const float f = peakFrequency (out, 16000);
        check (p->getDisplayMask() == bits (0, 4, 9), "MIDI chord A-C-E sets the key mask");
        check (std::abs (f - 329.63f) < 3.0f, "MIDI key snaps 300 Hz to " + juce::String (f, 1) + " Hz (target E 329.6)");
    }

    // 7) Stress: extreme settings, loud input, silence, mode switches -> finite, no hard clipping.
    {
        std::mt19937 rng (3);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        bool allFinite = true;
        float maxPeak = 0.0f;
        for (int run = 0; run < 12; ++run)
        {
            auto p = makeProcessor (run % 2, u (rng) * 100.0f, u (rng) * 200.0f, u (rng) * 100.0f);
            setParam (*p, ParamIDs::key, (float) (run % 12));
            setParam (*p, ParamIDs::scale, (float) (run % (int) kScales.size()));
            setParam (*p, ParamIDs::output, 12.0f);
            setParam (*p, ParamIDs::shift, u (rng) * 100.0f);
            setParam (*p, ParamIDs::gate, u (rng) * 100.0f);
            setParam (*p, ParamIDs::rangeLow, 20.0f + u (rng) * 600.0f);
            setParam (*p, ParamIDs::rangeHigh, 2000.0f + u (rng) * 18000.0f);
            juce::AudioBuffer<float> in = makeNoise (1.0, 1.0f);
            in.applyGain (0, 12000, 3.0f); // clipping-level burst
            for (int ch = 0; ch < 2; ++ch)
                in.clear (ch, 24000, 12000); // silence gap
            auto out = render (*p, in);
            setParam (*p, ParamIDs::mode, (float) ((run + 1) % 2)); // switch mode mid-stream
            auto out2 = render (*p, in);
            for (auto* b : { &out, &out2 })
            {
                auto s = stats (*b);
                allFinite = allFinite && s.finite;
                maxPeak = std::max (maxPeak, s.peak);
            }
        }
        check (allFinite, "Stress test: no NaN/Inf");
        check (maxPeak <= 1.0f, "Stress test: output stays within 0 dBFS (peak " + juce::String (maxPeak, 3) + ")");
    }

    // 8b) CPU mit SHIFT + Noten-Map + Formant (High Quality, schlimmster Fall)
    {
        auto p = makeProcessor (highQuality, 100.0f, 100.0f, 30.0f);
        setParam (*p, ParamIDs::shift, 100.0f);
        setParam (*p, ParamIDs::mapId (4).toRawUTF8(), 4.0f);
        auto in = makeProgression ({ { 60, 64, 67 }, { 65, 69, 72 }, { 67, 71, 74 } }, 36, 10.0);
        auto noise = makeNoise (10.0, 0.02f);
        for (int ch = 0; ch < 2; ++ch)
            in.addFrom (ch, 0, noise, ch, 0, in.getNumSamples());
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        render (*p, in);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("[INFO] High Quality + Shift + Map + Formant: 10 s stereo in %.0f ms (%.1f %% of one core)\n", ms, ms / 100.0);
    }

    // 8) CPU: time to process 10 s of stereo audio in each mode.
    for (int mode : { zeroLatency, highQuality })
    {
        auto p = makeProcessor (mode, 100.0f, 100.0f, 30.0f);
        setParam (*p, ParamIDs::scale, 13.0f); // Chromatic = most bands
        auto in = makeNoise (10.0, 0.2f);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        render (*p, in);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("[INFO] %s: 10 s stereo processed in %.0f ms (%.1f %% of one core, worst-case scale)\n",
                     mode == zeroLatency ? "Zero Latency" : "High Quality", ms, ms / 100.0);
    }

    // 9) Optional: render the editor to a PNG (set PRIZM_UI_SNAPSHOT=1).
    if (juce::SystemStats::getEnvironmentVariable ("PRIZM_UI_SNAPSHOT", {}) == "1")
    {
        auto p = makeProcessor (zeroLatency, 85.0f, 140.0f, 30.0f);
        setParam (*p, ParamIDs::key, 9.0f);   // A
        setParam (*p, ParamIDs::scale, 1.0f); // Minor
        setParam (*p, ParamIDs::mode, (float) highQuality);
        setParam (*p, ParamIDs::mapId (0).toRawUTF8(), 2.0f);  // C -> C#
        setParam (*p, ParamIDs::mapId (7).toRawUTF8(), 13.0f); // G off
        auto noise = makeNoise (1.0, 0.2f);
        render (*p, noise); // so the key strip shows live energy
        std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditor());
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("renders/ui.png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (image, os);
        std::printf ("[INFO] UI snapshot written\n");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
