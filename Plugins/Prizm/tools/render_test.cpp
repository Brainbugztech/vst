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

    // 2) HQ mode snaps an out-of-key sine (450 Hz, between A and A#) to A = 440 Hz.
    {
        auto p = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        auto out = render (*p, makeTone ({ 450.0f }, 2.0, 0.3f));
        const float f = peakFrequency (out, 16000);
        check (std::abs (f - 440.0f) < 3.0f, "HQ snaps 450 Hz sine to " + juce::String (f, 1) + " Hz (target 440)");

        auto p2 = makeProcessor (highQuality, 100.0f, 0.0f, 0.0f);
        auto out2 = render (*p2, makeTone ({ 285.0f }, 2.0, 0.3f)); // between C# and D, C# not in C major -> D 293.7
        const float f2 = peakFrequency (out2, 16000);
        check (std::abs (f2 - 293.66f) < 3.0f, "HQ snaps 285 Hz sine to " + juce::String (f2, 1) + " Hz (target D 293.7)");

        auto p3 = makeProcessor (highQuality, 50.0f, 0.0f, 0.0f);
        auto out3 = render (*p3, makeTone ({ 450.0f }, 2.0, 0.3f));
        const float f3 = peakFrequency (out3, 16000);
        check (f3 > 441.0f && f3 < 449.0f, "HQ Strength 50 % pulls halfway: " + juce::String (f3, 1) + " Hz");
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
