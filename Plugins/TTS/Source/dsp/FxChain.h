#pragma once

// Effekte auf der Ausgabe (Host-Samplerate, Stereo): 3-Band-EQ -> Delay -> Reverb

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace tts::dsp
{
enum class EqType { lowCut = 0, lowShelf, bell, highShelf, highCut };

struct EqBand
{
    bool on = true;
    EqType type = EqType::bell;
    float freq = 1000.0f, gainDb = 0.0f, q = 0.7f;
};

struct FxParams
{
    bool eqOn = true;
    EqBand band[3];

    bool dlOn = false, dlPing = true;
    float dlTimeMs = 375.0f;      // bereits aus Sync/ms berechnet
    float dlFeedback = 0.35f, dlLow = 150.0f, dlHigh = 8000.0f, dlMix = 0.25f;

    bool rvOn = true;
    float rvSize = 0.6f, rvDamp = 0.45f, rvPreMs = 20.0f, rvWidth = 1.0f, rvLow = 200.0f, rvMix = 0.15f;
};

inline juce::dsp::IIR::Coefficients<float>::Ptr makeEqCoefficients (double sr, const EqBand& b)
{
    using C = juce::dsp::IIR::Coefficients<float>;
    const double f = juce::jlimit (20.0, sr * 0.45, (double) b.freq);
    const float q = juce::jlimit (0.1f, 18.0f, b.q);
    const float g = juce::Decibels::decibelsToGain (b.gainDb);
    switch (b.type)
    {
        case EqType::lowCut:    return C::makeHighPass (sr, f, q);
        case EqType::lowShelf:  return C::makeLowShelf (sr, f, q, g);
        case EqType::highShelf: return C::makeHighShelf (sr, f, q, g);
        case EqType::highCut:   return C::makeLowPass (sr, f, q);
        case EqType::bell:
        default:                return C::makePeakFilter (sr, f, q, g);
    }
}

class FxChain
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate > 0 ? sampleRate : 48000.0;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlock), 1 };
        for (auto& b : eq) for (auto& f : b) { f.prepare (spec); f.reset(); f.coefficients = new juce::dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0); }
        for (auto& c : cache) c = {};
        cacheValid = false;
        const int maxDelay = (int) (sr * 2.6) + 8;
        dl.setSize (2, maxDelay, false, true, true);
        dl.clear();
        dlWrite = 0;
        dlTime.reset (sr, 0.2);
        dlTime.setCurrentAndTargetValue ((float) (sr * 0.375));
        for (auto* f : { &fbLowL, &fbLowR, &fbHighL, &fbHighR, &rvLowL, &rvLowR }) { f->prepare (spec); f->reset(); }
        fbCacheLow = fbCacheHigh = rvCacheLow = -1.0f;
        pre.setSize (2, (int) (sr * 0.3) + 8, false, true, true);
        pre.clear();
        preWrite = 0;
        reverb.setSampleRate (sr);
        reverb.reset();
        wetL.setSize (1, juce::jmax (1, maxBlock));
        wetR.setSize (1, juce::jmax (1, maxBlock));
        smDlMix.reset (sr, 0.05); smRvMix.reset (sr, 0.05);
    }

    void reset()
    {
        for (auto& b : eq) for (auto& f : b) f.reset();
        dl.clear();
        pre.clear();
        reverb.reset();
        for (auto* f : { &fbLowL, &fbLowR, &fbHighL, &fbHighR, &rvLowL, &rvLowR }) f->reset();
    }

    void process (float* L, float* R, int num, const FxParams& p)
    {
        if (wetL.getNumSamples() < num) { wetL.setSize (1, num); wetR.setSize (1, num); }

        // ---- EQ
        if (p.eqOn)
        {
            for (int b = 0; b < 3; ++b)
            {
                const auto& band = p.band[b];
                if (! band.on) continue;
                if (! cacheValid || differs (cache[b], band))
                {
                    auto c = makeEqCoefficients (sr, band);
                    *eq[b][0].coefficients = *c;
                    *eq[b][1].coefficients = *c;
                    cache[b] = band;
                }
                for (int i = 0; i < num; ++i)
                {
                    L[i] = eq[b][0].processSample (L[i]);
                    R[i] = eq[b][1].processSample (R[i]);
                }
            }
            cacheValid = true;
        }

        // ---- Delay
        smDlMix.setTargetValue (p.dlOn ? p.dlMix : 0.0f);
        if (p.dlOn || smDlMix.isSmoothing() || smDlMix.getCurrentValue() > 0.0001f)
        {
            if (std::abs (p.dlLow - fbCacheLow) > 0.5f)
            {
                fbCacheLow = p.dlLow;
                auto c = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, juce::jlimit (20.0f, 5000.0f, p.dlLow));
                *fbLowL.coefficients = *c; *fbLowR.coefficients = *c;
            }
            if (std::abs (p.dlHigh - fbCacheHigh) > 0.5f)
            {
                fbCacheHigh = p.dlHigh;
                auto c = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (sr, juce::jlimit (500.0f, (float) (sr * 0.45), p.dlHigh));
                *fbHighL.coefficients = *c; *fbHighR.coefficients = *c;
            }
            dlTime.setTargetValue ((float) juce::jlimit (1.0, sr * 2.5, sr * p.dlTimeMs * 0.001));
            const int len = dl.getNumSamples();
            auto* bl = dl.getWritePointer (0);
            auto* br = dl.getWritePointer (1);
            const float fb = juce::jlimit (0.0f, 0.95f, p.dlFeedback);
            for (int i = 0; i < num; ++i)
            {
                const float d = dlTime.getNextValue();
                float rp = (float) dlWrite - d;
                while (rp < 0) rp += (float) len;
                const int i0 = (int) rp, i1 = (i0 + 1) % len;
                const float fr = rp - (float) i0;
                float yl = bl[i0] + fr * (bl[i1] - bl[i0]);
                float yr = br[i0] + fr * (br[i1] - br[i0]);
                const float inL = p.dlOn ? L[i] : 0.0f, inR = p.dlOn ? R[i] : 0.0f;
                const float fl = fbHighL.processSample (fbLowL.processSample (yl)) * fb;
                const float fr2 = fbHighR.processSample (fbLowR.processSample (yr)) * fb;
                if (p.dlPing)
                {
                    // Eingang (Mono) links, Echos springen hin und her
                    bl[dlWrite] = 0.5f * (inL + inR) + fr2;
                    br[dlWrite] = fl;
                }
                else
                {
                    bl[dlWrite] = inL + fl;
                    br[dlWrite] = inR + fr2;
                }
                dlWrite = (dlWrite + 1) % len;
                const float m = smDlMix.getNextValue();
                L[i] = L[i] * (1.0f - 0.5f * m) + yl * m;
                R[i] = R[i] * (1.0f - 0.5f * m) + yr * m;
            }
        }

        // ---- Reverb (Pre-Delay -> Freeverb -> Low Cut auf dem Hall)
        smRvMix.setTargetValue (p.rvOn ? p.rvMix : 0.0f);
        if (p.rvOn || smRvMix.isSmoothing() || smRvMix.getCurrentValue() > 0.0001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit (0.0f, 1.0f, 0.3f + 0.69f * p.rvSize);
            rp.damping = juce::jlimit (0.0f, 1.0f, p.rvDamp);
            rp.width = juce::jlimit (0.0f, 1.0f, p.rvWidth);
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            reverb.setParameters (rp);
            if (std::abs (p.rvLow - rvCacheLow) > 0.5f)
            {
                rvCacheLow = p.rvLow;
                auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, juce::jlimit (20.0f, 2000.0f, p.rvLow), 0.7071f);
                *rvLowL.coefficients = *c; *rvLowR.coefficients = *c;
            }
            const int plen = pre.getNumSamples();
            const int pd = juce::jlimit (0, plen - 2, (int) (p.rvPreMs * 0.001 * sr));
            auto* pl = pre.getWritePointer (0);
            auto* pr = pre.getWritePointer (1);
            auto* wl = wetL.getWritePointer (0);
            auto* wr = wetR.getWritePointer (0);
            for (int i = 0; i < num; ++i)
            {
                pl[preWrite] = p.rvOn ? L[i] : 0.0f;
                pr[preWrite] = p.rvOn ? R[i] : 0.0f;
                int rd = preWrite - pd;
                if (rd < 0) rd += plen;
                wl[i] = pl[rd];
                wr[i] = pr[rd];
                preWrite = (preWrite + 1) % plen;
            }
            reverb.processStereo (wl, wr, num);
            for (int i = 0; i < num; ++i)
            {
                const float m = smRvMix.getNextValue();
                const float a = rvLowL.processSample (wl[i]), b = rvLowR.processSample (wr[i]);
                // Freeverb ist leise: etwas anheben
                L[i] = L[i] * (1.0f - 0.35f * m) + a * m * 2.2f;
                R[i] = R[i] * (1.0f - 0.35f * m) + b * m * 2.2f;
            }
        }
    }

private:
    static bool differs (const EqBand& a, const EqBand& b)
    {
        return a.on != b.on || a.type != b.type || std::abs (a.freq - b.freq) > 0.01f
            || std::abs (a.gainDb - b.gainDb) > 0.001f || std::abs (a.q - b.q) > 0.0001f;
    }

    double sr = 48000.0;
    juce::dsp::IIR::Filter<float> eq[3][2];
    EqBand cache[3];
    bool cacheValid = false;

    juce::AudioBuffer<float> dl;
    int dlWrite = 0;
    juce::SmoothedValue<float> dlTime;
    juce::dsp::IIR::Filter<float> fbLowL, fbLowR, fbHighL, fbHighR;
    float fbCacheLow = -1, fbCacheHigh = -1;

    juce::AudioBuffer<float> pre, wetL, wetR;
    int preWrite = 0;
    juce::Reverb reverb;
    juce::dsp::IIR::Filter<float> rvLowL, rvLowR;
    float rvCacheLow = -1;
    juce::SmoothedValue<float> smDlMix, smRvMix;
};
} // namespace tts::dsp
