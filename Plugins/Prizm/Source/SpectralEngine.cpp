#include "SpectralEngine.h"

#include <algorithm>
#include <cmath>

namespace prizm
{
namespace
{
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
    constexpr float kPi = juce::MathConstants<float>::pi;

    inline float wrapPhase (float x) { return x - kTwoPi * std::round (x / kTwoPi); }

    inline int noteOfHz (float hz) { return (int) std::lround (69.0f + 12.0f * std::log2 (hz / 440.0f)); }
} // namespace

void SpectralEngine::prepare (double sampleRate)
{
    fs = sampleRate;
    fftOrder = (fs > 50000.0) ? 13 : 12; // ~11 Hz Bins auch bei 88,2/96 kHz
    fftSize = 1 << fftOrder;
    hop = fftSize / 4;
    numBins = fftSize / 2 + 1;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    model.prepare (fs);

    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) fftSize);
    olaScale = 1.0f / 1.5f; // Hann * Hann bei 75 % Überlappung summiert sich zu 1,5
    gainAttack = 1.0f - std::exp (-(float) hop / ((float) fs * 0.050f));
    gainRelease = 1.0f - std::exp (-(float) hop / ((float) fs * 0.035f));

    prevMixMag.assign ((size_t) numBins, 0.0f);
    mixMag.assign ((size_t) numBins, 0.0f);
    mixBin.assign ((size_t) numBins, 0.0f);
    logPrefix.assign ((size_t) numBins + 1, 0.0f);
    envelope.assign ((size_t) numBins, 0.0f);
    grouper.peaks.reserve (512);
    peaks.reserve ((size_t) numBins);
    regionPeak.assign ((size_t) numBins, -1);

    for (auto& c : channels)
    {
        c.input.assign ((size_t) fftSize, 0.0f);
        c.output.assign ((size_t) fftSize, 0.0f);
        c.frame.assign ((size_t) fftSize * 2, 0.0f);
        for (auto* v : { &c.mag, &c.phase, &c.lastPhase, &c.trueBin, &c.outMag, &c.outBin, &c.outPhase,
                         &c.best, &c.synthPhase, &c.heldMag, &c.heldBin, &c.heldPhase, &c.gain, &c.gainSmooth })
            v->assign ((size_t) numBins, 0.0f);
        c.gainNote.assign ((size_t) numBins, -1);
    }
    reset();
}

void SpectralEngine::reset()
{
    for (auto& c : channels)
        for (auto* v : { &c.input, &c.output, &c.lastPhase, &c.synthPhase, &c.heldMag, &c.heldPhase, &c.gainSmooth })
            std::fill (v->begin(), v->end(), 0.0f);
    std::fill (prevMixMag.begin(), prevMixMag.end(), 0.0f);
    noteLevel.fill (0.0f);
    noteGate.fill (1.0f);
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

    // Transienten über spektralen Fluss
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
    const float transientAmount = total > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, (flux / total - 0.25f) * 2.5f) : 0.0f;

    // Gemeinsame Filterkurve mit derselben Glättung wie Zero Latency
    refScale = 440.0f / juce::jlimit (400.0f, 480.0f, p.a4);
    const float qTarget = ResponseModel::qFromColor (p.color);
    qSmoothed += (qTarget - qSmoothed) * (float) (1.0 - std::exp (-(double) hop / (fs * 0.05)));
    model.update (p.mask, qSmoothed, p.weight);

    // Tonhöhen verschieben (SHIFT / Noten-Map): Noten gemeinsam für beide Kanäle erkennen
    const bool moving = p.shift > 0.001f || p.mapActive() || std::any_of (p.map.begin(), p.map.end(), [] (int8_t m) { return m == kMapMute; });
    if (moving)
    {
        buildPitchPlan (p);
        for (int ch = 0; ch < numActiveChannels; ++ch)
            applyPitchPlan (channels[(size_t) ch], juce::jlimit (0.0f, 1.0f, p.formant));
    }
    else
    {
        for (int ch = 0; ch < numActiveChannels; ++ch)
            passThrough (channels[(size_t) ch]);
    }

    // Verstärkung pro Bin aus der Filterkurve (beim zugehörigen Teilton) + Gate pro Note
    noteSum.fill (0.0f);
    for (int ch = 0; ch < numActiveChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];
        computeGains (c);
        for (int j = 1; j < numBins; ++j)
        {
            const int note = c.gainNote[(size_t) j];
            if (note >= 0 && note < 128)
            {
                const float m = c.outMag[(size_t) j] * c.gain[(size_t) j];
                noteSum[(size_t) note] += m * m;
            }
        }
    }

    const float fall = std::exp (-(float) hop / ((float) fs * 0.08f));
    float loudest = 0.0f;
    for (size_t n = 0; n < 128; ++n)
    {
        const float lvl = std::sqrt (noteSum[n]);
        noteLevel[n] = lvl > noteLevel[n] ? lvl : noteLevel[n] * fall;
        loudest = std::max (loudest, noteLevel[n]);
    }
    const float gateCoef = 1.0f - std::exp (-(float) hop / ((float) fs * 0.03f));
    for (size_t n = 0; n < 128; ++n)
        noteGate[n] += (gateGain (noteLevel[n], loudest, p.gate) - noteGate[n]) * gateCoef;

    const float clearAmount = juce::jlimit (0.0f, 1.0f, p.clear) * transientAmount;
    for (int ch = 0; ch < numActiveChannels; ++ch)
        synthesise (channels[(size_t) ch], clearAmount);

    pitchEnergyFresh = true;
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
        const float dev = wrapPhase (ph - c.lastPhase[(size_t) k] - hopPhase * (float) k);
        c.lastPhase[(size_t) k] = ph;
        c.trueBin[(size_t) k] = (float) k + dev / hopPhase;
    }
}

void SpectralEngine::passThrough (Channel& c)
{
    // Spektrum unverändert weitergeben (Phasen bleiben die des Originals)
    std::copy (c.mag.begin(), c.mag.end(), c.outMag.begin());
    std::copy (c.trueBin.begin(), c.trueBin.end(), c.outBin.begin());
    std::copy (c.phase.begin(), c.phase.end(), c.outPhase.begin());
    std::copy (c.phase.begin(), c.phase.end(), c.synthPhase.begin());
}

void SpectralEngine::buildPitchPlan (const EngineParams& p)
{
    // Summe beider Kanäle: gemeinsame Peaks, gemeinsame Noten, gleiche Verschiebung links/rechts
    for (int k = 0; k < numBins; ++k)
    {
        float m = 0.0f, weighted = 0.0f;
        for (int ch = 0; ch < numActiveChannels; ++ch)
        {
            const auto& c = channels[(size_t) ch];
            m += c.mag[(size_t) k];
            weighted += c.mag[(size_t) k] * c.trueBin[(size_t) k];
        }
        mixMag[(size_t) k] = m;
        mixBin[(size_t) k] = m > 1.0e-12f ? weighted / m : (float) k;
    }

    findRegions (mixMag);

    const float binHz = (float) (fs / fftSize);
    grouper.peaks.clear();
    for (int pk : peaks)
    {
        NoteGrouper::Peak gp;
        gp.hz = mixBin[(size_t) pk] * binHz;
        gp.mag = mixMag[(size_t) pk];
        grouper.peaks.push_back (gp);
    }
    grouper.run (p, refScale);

    regionRatio.resize (peaks.size());
    regionGain.resize (peaks.size());
    for (size_t i = 0; i < peaks.size(); ++i)
    {
        regionRatio[i] = grouper.peaks[i].ratio;
        regionGain[i] = grouper.peaks[i].gain;
    }
}

void SpectralEngine::applyPitchPlan (Channel& c, float formant)
{
    const float hopPhase = kTwoPi * (float) hop / (float) fftSize;

    std::fill (c.outMag.begin(), c.outMag.end(), 0.0f);
    std::fill (c.best.begin(), c.best.end(), 0.0f);
    for (int j = 0; j < numBins; ++j)
    {
        c.outBin[(size_t) j] = (float) j;
        c.outPhase[(size_t) j] = wrapPhase (c.synthPhase[(size_t) j] + hopPhase * (float) j);
    }

    if (peaks.empty())
    {
        passThrough (c);
        return;
    }

    // Spektrale Hüllkurve (Klangfarbe): log-Betrag der Peaks, dazwischen linear verbunden,
    // dann über ca. eine Achtel-Oktave geglättet. Folgt den Obertonspitzen, nicht den Lücken.
    if (formant > 0.001f)
    {
        const auto logAt = [&c] (int k) { return std::log (c.mag[(size_t) k] + 1.0e-9f); };
        int prevK = 0;
        float prevV = logAt (peaks.front());
        for (size_t i = 0; i <= peaks.size(); ++i)
        {
            const int k = i < peaks.size() ? peaks[i] : numBins - 1;
            const float v = i < peaks.size() ? logAt (k) : prevV;
            for (int j = prevK; j <= k; ++j)
            {
                const float t = k > prevK ? (float) (j - prevK) / (float) (k - prevK) : 0.0f;
                envelope[(size_t) j] = prevV + (v - prevV) * t;
            }
            prevK = k;
            prevV = v;
        }
        logPrefix[0] = 0.0f;
        for (int k = 0; k < numBins; ++k)
            logPrefix[(size_t) k + 1] = logPrefix[(size_t) k] + envelope[(size_t) k];
        for (int k = 0; k < numBins; ++k)
        {
            const int w = std::max (3, (int) ((float) k * 0.06f));
            const int lo = std::max (0, k - w), hi = std::min (numBins - 1, k + w);
            c.best[(size_t) k] = (logPrefix[(size_t) hi + 1] - logPrefix[(size_t) lo]) / (float) (hi - lo + 1);
        }
        std::copy (c.best.begin(), c.best.end(), envelope.begin());
        std::fill (c.best.begin(), c.best.end(), 0.0f);
    }

    // Regionen (Teilton + Tal) als Ganzes verschieben, Phasen an den Peak gekoppelt
    int k = 0;
    while (k < numBins)
    {
        const auto region = (size_t) regionPeak[(size_t) k];
        const int p = peaks[region];
        const float ratio = regionRatio[region];
        const float gain = regionGain[region];
        const float peakBin = c.trueBin[(size_t) p];

        const int jp = juce::jlimit (0, numBins - 1, (int) std::lround ((float) p * ratio));
        const int offset = jp - p;
        const float peakPhase = wrapPhase (c.synthPhase[(size_t) jp] + hopPhase * peakBin * ratio);

        int k2 = k;
        while (k2 < numBins && regionPeak[(size_t) k2] == (int) region)
        {
            const int j = k2 + offset;
            if (j >= 1 && j < numBins && gain > 0.0f)
            {
                float m = c.mag[(size_t) k2] * gain;
                if (formant > 0.001f && offset != 0)
                {
                    // Klangfarbe stehen lassen: Betrag an die Hüllkurve am Zielort anpassen
                    const float corr = std::exp ((envelope[(size_t) j] - envelope[(size_t) k2]) * formant);
                    m *= juce::jlimit (0.1f, 10.0f, corr);
                }
                c.outMag[(size_t) j] += m;
                if (m > c.best[(size_t) j])
                {
                    c.best[(size_t) j] = m;
                    c.outBin[(size_t) j] = c.trueBin[(size_t) k2] * ratio;
                    c.outPhase[(size_t) j] = wrapPhase (peakPhase + (c.phase[(size_t) k2] - c.phase[(size_t) p]));
                }
            }
            ++k2;
        }
        k = k2;
    }

    std::copy (c.outPhase.begin(), c.outPhase.end(), c.synthPhase.begin());
}

void SpectralEngine::findRegions (const std::vector<float>& mags)
{
    float maxMag = 0.0f;
    for (int k = 0; k < numBins; ++k)
        maxMag = std::max (maxMag, mags[(size_t) k]);

    peaks.clear();
    const float floorMag = maxMag * 1.0e-6f;
    for (int k = 2; k < numBins - 2; ++k)
    {
        const float m = mags[(size_t) k];
        if (m > floorMag && m > mags[(size_t) k - 1] && m >= mags[(size_t) k + 1]
            && m > mags[(size_t) k - 2] && m >= mags[(size_t) k + 2])
            peaks.push_back (k);
    }
    if (peaks.empty())
        return;

    int start = 0;
    for (size_t i = 0; i < peaks.size(); ++i)
    {
        int end = numBins - 1;
        if (i + 1 < peaks.size())
        {
            end = peaks[i];
            for (int k = peaks[i]; k <= peaks[i + 1]; ++k)
                if (mags[(size_t) k] < mags[(size_t) end])
                    end = k;
        }
        for (int k = start; k <= end; ++k)
            regionPeak[(size_t) k] = (int) i;
        start = end + 1;
    }
}

void SpectralEngine::smoothGains (Channel& c)
{
    // Anlaufen etwa wie ein Resonator: ca. 2 Frames, Abfallen etwas schneller
    for (int j = 0; j < numBins; ++j)
    {
        auto& gs = c.gainSmooth[(size_t) j];
        const float g = c.gain[(size_t) j];
        gs += (g - gs) * (g > gs ? gainAttack : gainRelease);
        c.gain[(size_t) j] = gs;
    }
}

void SpectralEngine::computeGains (Channel& c)
{
    // Ein Teilton = ein Peak mit seinem Tal. Alle Bins eines Teiltons bekommen die Verstärkung
    // der Filterkurve bei dessen gemessener Frequenz. So entstehen zwischen Teiltönen keine
    // künstlichen Töne, und ein schiefer Teilton wird als Ganzes gedämpft wie im Filter.
    const float binHz = (float) (fs / fftSize);
    findRegions (c.outMag);

    if (peaks.empty())
    {
        for (int j = 0; j < numBins; ++j)
        {
            const float hz = std::max (1.0f, (float) j * binHz * refScale);
            c.gain[(size_t) j] = model.magnitudeAt (hz);
            c.gainNote[(size_t) j] = noteOfHz (hz);
        }
        smoothGains (c);
        return;
    }

    for (int j = 0; j < numBins; ++j)
    {
        const float own = c.outBin[(size_t) j];
        const float ownHz = std::max (1.0f, own * binHz * refScale);
        if (std::abs (own - (float) j) <= 2.5f)
        {
            // Bin liegt in der Hauptkeule seines Teiltons: dessen gemessene Frequenz zählt
            c.gain[(size_t) j] = model.magnitudeAt (ownHz);
            c.gainNote[(size_t) j] = noteOfHz (ownHz);
        }
        else
        {
            // Nebenkeule / Überlagerung: nie lauter als der Teilton, zu dem das Bin gehört
            const int pk = peaks[(size_t) regionPeak[(size_t) j]];
            const float peakHz = std::max (1.0f, c.outBin[(size_t) pk] * binHz * refScale);
            c.gain[(size_t) j] = std::min (model.magnitudeAt (ownHz), model.magnitudeAt (peakHz));
            c.gainNote[(size_t) j] = noteOfHz (peakHz);
        }
    }
    smoothGains (c);
}

void SpectralEngine::synthesise (Channel& c, float clearAmount)
{
    const float hopPhase = kTwoPi * (float) hop / (float) fftSize;
    const float binHz = (float) (fs / fftSize);
    const float q = qSmoothed;
    const bool reportEnergy = (&c == &channels[0]);

    for (int j = 0; j < numBins; ++j)
    {
        const float binPos = c.outBin[(size_t) j];
        const float hz = std::max (1.0f, binPos * binHz);
        const int note = c.gainNote[(size_t) j];
        const float gate = (note >= 0 && note < 128) ? noteGate[(size_t) note] : 1.0f;

        float m = c.outMag[(size_t) j] * c.gain[(size_t) j] * gate;
        float ph = c.outPhase[(size_t) j];
        float bin = binPos;

        // Ausklingen wie ein Resonator gleicher Güte: Hüllkurve exp(-pi f t / Q)
        const float heldHz = std::max (1.0f, c.heldBin[(size_t) j] * binHz);
        const float decay = std::exp (-kPi * heldHz * (float) hop / (q * (float) fs)) * (1.0f - clearAmount);
        const float held = c.heldMag[(size_t) j] * decay;
        if (held > m)
        {
            m = held;
            bin = c.heldBin[(size_t) j];
            ph = wrapPhase (c.heldPhase[(size_t) j] + hopPhase * bin);
        }
        c.heldMag[(size_t) j] = m;
        c.heldBin[(size_t) j] = bin;
        c.heldPhase[(size_t) j] = ph;

        if (reportEnergy && m > 1.0e-6f && hz > 40.0f && hz < 5000.0f)
            pitchEnergy[(size_t) (((note % 12) + 12) % 12)] += m * m;

        c.frame[(size_t) (2 * j)] = m * std::cos (ph);
        c.frame[(size_t) (2 * j + 1)] = m * std::sin (ph);
    }
    c.frame[1] = 0.0f;
    c.frame[(size_t) (2 * (numBins - 1) + 1)] = 0.0f;

    fft->performRealOnlyInverseTransform (c.frame.data());

    for (int i = 0; i < fftSize; ++i)
        c.output[(size_t) ((writePos + i) % fftSize)] += c.frame[(size_t) i] * window[(size_t) i] * olaScale;
}
} // namespace prizm
