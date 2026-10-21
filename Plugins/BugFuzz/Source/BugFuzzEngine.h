#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <algorithm>

// Brainbugz BugFuzz DSP (Quadrafuzz-Prinzip)
// - 4x Oversampling, alles Nichtlineare läuft auf der hohen Rate
// - Linkwitz-Riley-Weiche (24 dB/Okt.) in 4 Bänder, phasengleich per Allpass-Ausgleich
// - jedes Band: eigener Drive, asymmetrischer Fuzz, DC-Blocker, Anti-Fizz-Tiefpass
// - "Krass"-Bereich (Drive > 50 %): härtere, asymmetrische Klippung bis zum Gated-Fuzz
// - OCTA: Gleichrichter-Oktave auf den oberen drei Bändern
// - Auto-Gain: Pegel des Fuzz-Signals folgt dem sauberen Signal
// - Dry/Wet wird im Oversampling-Bereich gemischt, dadurch sind beide Wege zeitgleich

namespace bb
{

struct FuzzParams
{
    float drive   = 0.45f;                          // 0..1   (über 0.5 = Krass)
    float mix     = 1.0f;                           // 0..1
    float outDb   = 0.0f;                           // -24..+12
    std::array<float, 4> band { 0.55f, 0.8f, 0.85f, 0.6f }; // Band-Drive 0..1
    std::array<float, 3> xover { 160.0f, 900.0f, 4200.0f };  // Weichen in Hz
    bool  octa     = false;
    bool  autoGain = true;
};

class BugFuzzEngine
{
public:
    static constexpr int kNumBands = 4;
    static constexpr int kOsFactorLog2 = 3; // 8x

    static constexpr float kXMin[3] { 40.0f, 200.0f, 1200.0f };
    static constexpr float kXMax[3] { 600.0f, 3500.0f, 14000.0f };

    // Sorgt dafür, dass die Weichen aufsteigend und mindestens eine halbe Oktave auseinander liegen
    static std::array<float, 3> sanitiseXover (std::array<float, 3> x)
    {
        for (int i = 0; i < 3; ++i) x[(size_t) i] = std::clamp (x[(size_t) i], kXMin[i], kXMax[i]);
        x[1] = std::max (x[1], x[0] * 1.5f);
        x[2] = std::max (x[2], x[1] * 1.5f);
        return x;
    }

    // Verstärkung eines Bandes in dB
    static float bandGainDb (float drive, float bandAmt)
    {
        const float krass = std::max (0.0f, drive - 0.5f) * 2.0f;
        return bandAmt * (drive * 40.0f + krass * 14.0f);
    }

    // schnelle tanh-Näherung (Padé), auf -1..1 begrenzt
    static inline float fastTanh (float x)
    {
        x = std::clamp (x, -4.9f, 4.9f);
        const float x2 = x * x;
        const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
        const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
        return std::clamp (num / den, -1.0f, 1.0f);
    }

    // Fuzz-Kennlinie mit vorberechneten Werten für einen Drive-Wert
    struct Shaper
    {
        float krass = 0.0f, bias = 0.06f, kp = 1.0f, kn = 0.85f, ceil = 1.0f, hardMix = 0.0f, rest = 0.0f;

        explicit Shaper (float drive = 0.45f)
        {
            krass   = std::max (0.0f, drive - 0.5f) * 2.0f;
            bias    = 0.06f + 0.1f * krass;          // leichte Asymmetrie wie beim Transistor-Fuzz
            kp      = 1.0f + 0.6f * krass;           // positive Seite steiler ...
            kn      = 0.85f + 0.3f * krass;          // ... negative weicher
            ceil    = 1.0f - 0.15f * krass;          // positive Seite etwas niedriger gedeckelt
            hardMix = krass * 0.5f;                  // im Krass-Bereich Richtung Rechteck
            rest    = 0.0f;
            rest    = raw (bias);                    // Ruhepunkt, damit Stille still bleibt
        }

        inline float raw (float a) const
        {
            float y = a > 0.0f ? fastTanh (a * kp) * ceil : fastTanh (a * kn);
            if (hardMix > 0.0f)
                y += (fastTanh (a * 4.0f) * (a > 0.0f ? ceil : 1.0f) - y) * hardMix;
            return y;
        }

        inline float operator() (float x) const { return raw (x + bias) - rest; }
    };

    // Kennlinie (auch für die Anzeige): x = verstärktes Signal
    static float shape (float x, float drive) { return Shaper (drive) (x); }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        maxBlockSize = std::max (1, maxBlock);
        os = std::make_unique<juce::dsp::Oversampling<float>> (
            2, kOsFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        os->initProcessing ((size_t) maxBlockSize);
        osFs = fs * (double) (1 << kOsFactorLog2);

        juce::dsp::ProcessSpec spec { osFs, (juce::uint32) (maxBlockSize << kOsFactorLog2), 2 };
        for (auto& f : xo) { f.prepare (spec); f.setType (juce::dsp::LinkwitzRileyFilterType::lowpass); }
        for (auto& f : ap) { f.prepare (spec); f.setType (juce::dsp::LinkwitzRileyFilterType::allpass); }
        reset();
    }

    void reset()
    {
        if (os) os->reset();
        for (auto& f : xo) f.reset();
        for (auto& f : ap) f.reset();
        for (auto& c : st) c = {};
        rmsDry = rmsWet = 1e-6;
        agGain = 1.0;
        initialised = false;
        for (auto& l : bandLevel) l = 0.0f;
    }

    int getLatencySamples() const { return os ? juce::roundToInt (os->getLatencyInSamples()) : 0; }

    void process (juce::AudioBuffer<float>& buffer, const FuzzParams& p)
    {
        const int n = buffer.getNumSamples();
        if (n == 0 || ! os) return;

        // Weichen setzen
        const auto x = sanitiseXover (p.xover);
        if (x != lastX)
        {
            lastX = x;
            xo[0].setCutoffFrequency (x[0]);
            xo[1].setCutoffFrequency (x[1]);
            xo[2].setCutoffFrequency (x[2]);
            ap[0].setCutoffFrequency (x[1]); // Low   <- Allpass f2
            ap[1].setCutoffFrequency (x[2]); // Low   <- Allpass f3
            ap[2].setCutoffFrequency (x[2]); // LoMid <- Allpass f3
        }

        // Zielwerte
        std::array<double, kNumBands> tGain {};
        for (int b = 0; b < kNumBands; ++b)
            tGain[(size_t) b] = std::pow (10.0, bandGainDb (p.drive, p.band[(size_t) b]) / 20.0);
        const double tMix = p.mix, tOut = std::pow (10.0, p.outDb / 20.0);
        const double tOcta = p.octa ? 1.0 : 0.0;

        if (! initialised)
        {
            gSm = tGain; mixSm = tMix; outSm = tOut; octaSm = tOcta;
            initialised = true;
        }

        // Anti-Fizz-Tiefpässe pro Band (relativ zur oberen Bandgrenze), Höhen-Band bei 13 kHz
        const double lpHz[kNumBands] { std::min (18000.0, x[0] * 8.0), std::min (18000.0, x[1] * 7.0),
                                       std::min (18000.0, x[2] * 4.0), 13000.0 };
        double kLp[kNumBands];
        for (int b = 0; b < kNumBands; ++b) kLp[b] = 1.0 - std::exp (-2.0 * pi * lpHz[b] / osFs);
        const double kDc = 1.0 - std::exp (-2.0 * pi * 12.0 / osFs);
        const double kSm = 1.0 - std::exp (-1.0 / (osFs * 0.02));
        const double kRms = 1.0 - std::exp (-1.0 / (osFs * 0.3));
        const double kLvl = 1.0 - std::exp (-1.0 / (osFs * 0.06));
        const Shaper shaper (p.drive);

        juce::dsp::AudioBlock<float> block (buffer);
        auto up = os->processSamplesUp (block);
        const int m = (int) up.getNumSamples();
        float* ch[2] { up.getChannelPointer (0), up.getChannelPointer (1) };

        double lvl[kNumBands] { bandLevel[0], bandLevel[1], bandLevel[2], bandLevel[3] };

        for (int i = 0; i < m; ++i)
        {
            for (int b = 0; b < kNumBands; ++b) gSm[(size_t) b] += (tGain[(size_t) b] - gSm[(size_t) b]) * kSm;
            mixSm  += (tMix - mixSm) * kSm;
            outSm  += (tOut - outSm) * kSm;
            octaSm += (tOcta - octaSm) * kSm;

            double dryS = 0.0, wetS = 0.0;
            double out[2];

            for (int c = 0; c < 2; ++c)
            {
                const float in = ch[c][i];
                float band[kNumBands];
                float l1, h1, l2, h2, l3, h3;
                xo[0].processSample (c, in, l1, h1);
                band[0] = ap[1].processSample (c, ap[0].processSample (c, l1));
                xo[1].processSample (c, h1, l2, h2);
                band[1] = ap[2].processSample (c, l2);
                xo[2].processSample (c, h2, l3, h3);
                band[2] = l3;
                band[3] = h3;

                auto& s = st[(size_t) c];
                double dry = 0.0, wet = 0.0;
                for (int b = 0; b < kNumBands; ++b)
                {
                    dry += band[b];
                    double y = shaper ((float) (band[b] * gSm[(size_t) b]));
                    if (b > 0 && octaSm > 0.001)
                    {
                        // Gleichrichter-Oktave, Gleichanteil macht der DC-Blocker weg
                        const double oc = std::abs (y) * 1.6 - 0.5;
                        y += (oc - y) * octaSm * (b == 1 ? 0.45 : 0.7);
                    }
                    // DC-Blocker
                    s.dc[(size_t) b] += (y - s.dc[(size_t) b]) * kDc;
                    y -= s.dc[(size_t) b];
                    // Anti-Fizz
                    s.lp[(size_t) b] += (y - s.lp[(size_t) b]) * kLp[b];
                    y = s.lp[(size_t) b];
                    // leiser bei wenig Band-Drive, damit Band-Regler auch die Balance ändern
                    const double bandMix = std::clamp ((double) p.band[(size_t) b] * 4.0, 0.0, 1.0);
                    y = band[b] + (y * 0.35 - band[b]) * bandMix;
                    lvl[b] += (std::abs (y) - lvl[b]) * kLvl;
                    wet += y;
                }
                dryS += dry * dry;
                wetS += wet * wet;
                out[c] = wet;
                ch[c][i] = (float) dry; // Dry hier zwischenspeichern
            }

            // Auto-Gain: Wet auf den Pegel von Dry ziehen (langsam, begrenzt)
            rmsDry += (dryS - rmsDry) * kRms;
            rmsWet += (wetS - rmsWet) * kRms;
            double ag = 1.0;
            if (p.autoGain)
                ag = std::clamp (std::sqrt ((rmsDry + 1e-9) / (rmsWet + 1e-9)), 0.06, 4.0);
            agGain += (ag - agGain) * kSm * 0.25;

            for (int c = 0; c < 2; ++c)
            {
                const double dry = ch[c][i];
                const double v = dry + (out[c] * agGain - dry) * mixSm;
                ch[c][i] = (float) (v * outSm);
            }
        }

        for (int b = 0; b < kNumBands; ++b) bandLevel[(size_t) b] = (float) lvl[b];
        os->processSamplesDown (block);

        // Sicherheits-Begrenzer auf der normalen Rate (greift nur über 0.95)
        for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float av = std::abs (d[i]);
                if (av > 0.95f) d[i] = std::copysign (0.95f + 0.05f * std::tanh ((av - 0.95f) / 0.05f), d[i]);
            }
        }
    }

    // Für die Anzeige: geglätteter Pegel je Band
    std::array<float, kNumBands> bandLevel {};

private:
    static constexpr double pi = 3.14159265358979323846;

    struct ChanState
    {
        std::array<double, kNumBands> dc {}, lp {};
    };

    double fs = 48000.0, osFs = 192000.0;
    int maxBlockSize = 512;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> xo, ap;
    std::array<ChanState, 2> st {};
    std::array<float, 3> lastX { -1.0f, -1.0f, -1.0f };
    std::array<double, kNumBands> gSm {};
    double mixSm = 1.0, outSm = 1.0, octaSm = 0.0;
    double rmsDry = 1e-6, rmsWet = 1e-6, agGain = 1.0;
    bool initialised = false;
};

} // namespace bb
