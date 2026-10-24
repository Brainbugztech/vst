#include "SpectralEngine.h"

#include <algorithm>
#include <cmath>

namespace prizm
{
namespace
{
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

    inline float wrapPhase (float x)
    {
        return x - kTwoPi * std::round (x / kTwoPi);
    }
} // namespace

void SpectralEngine::prepare (double sampleRate)
{
    fs = sampleRate;
    fftOrder = (fs > 50000.0) ? 13 : 12; // keep ~11 Hz bins at 88.2/96 kHz too
    fftSize = 1 << fftOrder;
    hop = fftSize / 4;
    numBins = fftSize / 2 + 1;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) fftSize);

    // Hann analysis * Hann synthesis at 75 % overlap sums to 1.5.
    olaScale = 1.0f / 1.5f;

    prefix.assign ((size_t) numBins + 1, 0.0f);
    prevMixMag.assign ((size_t) numBins, 0.0f);

    for (auto& c : channels)
    {
        c.input.assign ((size_t) fftSize, 0.0f);
        c.output.assign ((size_t) fftSize, 0.0f);
        c.frame.assign ((size_t) fftSize * 2, 0.0f);
        for (auto* v : { &c.mag, &c.phase, &c.lastPhase, &c.trueBin, &c.outMag, &c.outBin,
                         &c.bestMag, &c.synthPhase, &c.tailMag, &c.tailBin })
            v->assign ((size_t) numBins, 0.0f);
    }

    reset();
}

void SpectralEngine::reset()
{
    for (auto& c : channels)
    {
        std::fill (c.input.begin(), c.input.end(), 0.0f);
        std::fill (c.output.begin(), c.output.end(), 0.0f);
        std::fill (c.lastPhase.begin(), c.lastPhase.end(), 0.0f);
        std::fill (c.synthPhase.begin(), c.synthPhase.end(), 0.0f);
        std::fill (c.tailMag.begin(), c.tailMag.end(), 0.0f);
    }
    std::fill (prevMixMag.begin(), prevMixMag.end(), 0.0f);
    writePos = 0;
    hopCounter = 0;
}

void SpectralEngine::process (juce::AudioBuffer<float>& buffer, int numSamples, const EngineParams& p)
{
    numActiveChannels = std::min (buffer.getNumChannels(), kMaxChannels);
    if (numActiveChannels == 0 || fft == nullptr)
        return;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < numActiveChannels; ++ch)
        {
            auto& c = channels[(size_t) ch];
            c.input[(size_t) writePos] = buffer.getSample (ch, n);
            buffer.setSample (ch, n, c.output[(size_t) writePos]);
            c.output[(size_t) writePos] = 0.0f;
        }

        writePos = (writePos + 1) % fftSize;

        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            processFrame (p);
        }
    }
}

void SpectralEngine::processFrame (const EngineParams& p)
{
    for (int ch = 0; ch < numActiveChannels; ++ch)
        analyse (channels[(size_t) ch]);

    // Spectral flux on the summed magnitudes -> transient amount 0..1.
    float flux = 0.0f, total = 0.0f;
    for (int k = 0; k < numBins; ++k)
    {
        float m = 0.0f;
        for (int ch = 0; ch < numActiveChannels; ++ch)
            m += channels[(size_t) ch].mag[(size_t) k];

        const float d = m - prevMixMag[(size_t) k];
        if (d > 0.0f)
            flux += d;
        total += m;
        prevMixMag[(size_t) k] = m;
    }

    const float transientAmount = (total > 1.0e-6f)
                                    ? juce::jlimit (0.0f, 1.0f, (flux / total - 0.25f) * 2.5f)
                                    : 0.0f;

    for (int ch = 0; ch < numActiveChannels; ++ch)
        synthesise (channels[(size_t) ch], p, transientAmount);
}

void SpectralEngine::analyse (Channel& c)
{
    for (int i = 0; i < fftSize; ++i)
        c.frame[(size_t) i] = c.input[(size_t) ((writePos + i) % fftSize)] * window[(size_t) i];
    std::fill (c.frame.begin() + fftSize, c.frame.end(), 0.0f);

    fft->performRealOnlyForwardTransform (c.frame.data(), true);

    const float hopPhase = kTwoPi * (float) hop / (float) fftSize;

    for (int k = 0; k < numBins; ++k)
    {
        const float re = c.frame[(size_t) (2 * k)];
        const float im = c.frame[(size_t) (2 * k + 1)];
        const float ph = std::atan2 (im, re);

        c.mag[(size_t) k] = std::sqrt (re * re + im * im);
        c.phase[(size_t) k] = ph;

        const float deviation = wrapPhase (ph - c.lastPhase[(size_t) k] - hopPhase * (float) k);
        c.lastPhase[(size_t) k] = ph;
        c.trueBin[(size_t) k] = (float) k + deviation / hopPhase;
    }
}

void SpectralEngine::synthesise (Channel& c, const EngineParams& p, float transientAmount)
{
    const float strength = juce::jlimit (0.0f, 1.0f, p.strength);
    const float color = juce::jlimit (0.0f, 2.0f, p.color);
    const float a = juce::jlimit (0.0f, 1.0f, p.clear) * transientAmount;
    const float binHz = (float) (fs / fftSize);
    const float maxHz = (float) (0.45 * fs);

    std::fill (c.outMag.begin(), c.outMag.end(), 0.0f);
    std::fill (c.bestMag.begin(), c.bestMag.end(), 0.0f);
    for (int k = 0; k < numBins; ++k)
        c.outBin[(size_t) k] = (float) k;

    // 1) Move every bin to the nearest in-key pitch (scaled by Strength).
    c.outMag[0] = c.mag[0];
    for (int k = 1; k < numBins; ++k)
    {
        const float m = c.mag[(size_t) k];
        if (m < 1.0e-9f)
            continue;

        const float tb = c.trueBin[(size_t) k];
        const float hz = tb * binHz;
        float ratio = 1.0f;

        if (hz > 25.0f && hz < maxHz)
        {
            const float midi = hzToMidi (hz);
            const float target = midi + strength * ((float) nearestAllowedNote (p.mask, midi) - midi);
            ratio = std::pow (2.0f, (target - midi) / 12.0f);
        }

        const int j = (int) std::lround ((float) k * ratio);
        if (j < 1 || j >= numBins)
            continue;

        c.outMag[(size_t) j] += m;
        if (m > c.bestMag[(size_t) j])
        {
            c.bestMag[(size_t) j] = m;
            c.outBin[(size_t) j] = tb * ratio;
        }
    }

    // 2) Color 0..100 %: sharpen peaks against their neighbourhood, keep the energy.
    const float sharpen = std::min (color, 1.0f);
    if (sharpen > 0.01f)
    {
        double before = 0.0, after = 0.0;
        prefix[0] = 0.0f;
        for (int j = 0; j < numBins; ++j)
        {
            prefix[(size_t) j + 1] = prefix[(size_t) j] + c.outMag[(size_t) j];
            before += (double) c.outMag[(size_t) j] * c.outMag[(size_t) j];
        }

        const float exponent = sharpen * 0.9f;
        for (int j = 0; j < numBins; ++j)
        {
            const int lo = std::max (0, j - 8), hi = std::min (numBins - 1, j + 8);
            const float mean = (prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (float) (hi - lo + 1);
            const float factor = std::min (6.0f, std::pow (c.outMag[(size_t) j] / (mean + 1.0e-9f), exponent));
            c.bestMag[(size_t) j] = c.outMag[(size_t) j] * factor; // bestMag reused as scratch
            after += (double) c.bestMag[(size_t) j] * c.bestMag[(size_t) j];
        }

        const float norm = (after > 1.0e-20) ? (float) std::sqrt (before / after) : 0.0f;
        for (int j = 0; j < numBins; ++j)
            c.outMag[(size_t) j] = c.bestMag[(size_t) j] * norm;
    }

    // 3) Color above 100 %: spectral sustain tail.
    if (color > 1.0f)
    {
        const float seconds = 0.05f * std::pow (60.0f, color - 1.0f); // up to 3 s
        const float decay = std::exp (-(float) hop / ((float) fs * seconds)) * (1.0f - a);

        for (int j = 0; j < numBins; ++j)
        {
            const float held = c.tailMag[(size_t) j] * decay;
            if (held > c.outMag[(size_t) j])
            {
                c.outMag[(size_t) j] = held;
                c.outBin[(size_t) j] = c.tailBin[(size_t) j];
            }
            c.tailMag[(size_t) j] = c.outMag[(size_t) j];
            c.tailBin[(size_t) j] = c.outBin[(size_t) j];
        }
    }
    else
    {
        std::fill (c.tailMag.begin(), c.tailMag.end(), 0.0f);
    }

    // 4) Resynthesis with phase accumulation; Clear blends in the original on transients.
    const float hopPhase = kTwoPi * (float) hop / (float) fftSize;
    for (int j = 0; j < numBins; ++j)
    {
        float m = c.outMag[(size_t) j];
        float ph = wrapPhase (c.synthPhase[(size_t) j] + hopPhase * c.outBin[(size_t) j]);

        if (a > 0.0f)
        {
            m = m * (1.0f - a) + c.mag[(size_t) j] * a;
            if (a > 0.35f)
                ph = c.phase[(size_t) j]; // phase reset keeps the attack sharp
        }

        c.synthPhase[(size_t) j] = ph;

        if (&c == &channels[0] && m > 1.0e-6f)
        {
            const float hz = c.outBin[(size_t) j] * binHz;
            if (hz > 40.0f && hz < 5000.0f)
            {
                const int pc = ((int) std::lround (hzToMidi (hz)) % 12 + 12) % 12;
                pitchEnergy[(size_t) pc] += m * m;
            }
        }
        c.frame[(size_t) (2 * j)] = m * std::cos (ph);
        c.frame[(size_t) (2 * j + 1)] = m * std::sin (ph);
    }
    c.frame[1] = 0.0f;
    c.frame[(size_t) (2 * (numBins - 1) + 1)] = 0.0f;

    if (&c == &channels[0])
        pitchEnergyFresh = true;

    fft->performRealOnlyInverseTransform (c.frame.data());

    for (int i = 0; i < fftSize; ++i)
        c.output[(size_t) ((writePos + i) % fftSize)] += c.frame[(size_t) i] * window[(size_t) i] * olaScale;
}
} // namespace prizm
