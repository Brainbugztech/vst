#pragma once

#include <atomic>
#include <cmath>
#include <array>
#include <algorithm>
#include <random>

// Brainbugz Phazer DSP
// Kette resonanter 2-Pol-Allpässe (RBJ), deren Frequenzen per LFO (tempo-synchron oder frei)
// und Hüllkurve moduliert werden. Spacing verteilt die Notches übers Spektrum,
// Feedback mit tanh-Sättigung im Loop, Grit sättigt den Ausgang.

namespace bb
{

struct PhazerParams
{
    bool  sync      = true;
    int   div       = 3;      // Index in kDivBeats
    float rate      = 0.5f;   // Hz (frei)
    int   shape     = 0;      // 0 Sine,1 Tri,2 SawUp,3 SawDown,4 Square,5 Stairs,6 S&H,7 Random
    float smooth    = 0.0f;   // 0..1
    int   steps     = 8;      // Stufen für Stairs
    float phase     = 0.0f;   // Grad
    float center    = 900.0f; // Hz
    float depth     = 0.85f;  // 0..1  -> ±4.5 Oktaven
    float feedback  = 0.7f;   // -0.98..0.98
    int   notches   = 4;      // Index in kNotches
    float sharp     = 2.0f;   // Q
    float spacing   = 1.25f;  // Frequenzverhältnis benachbarter Notches
    float spread    = 45.0f;  // Grad Stereo-Versatz
    float grit      = 0.35f;  // 0..1
    float env       = 0.0f;   // -1..1
    float mix       = 0.5f;   // 0..1
    float outDb     = 0.0f;   // dB
};

static constexpr std::array<double, 15> kDivBeats { 32.0, 16.0, 8.0, 4.0, 2.0, 3.0, 1.0, 1.5, 2.0 / 3.0,
                                                    0.5, 0.75, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
static constexpr std::array<const char*, 15> kDivNames { "8/1", "4/1", "2/1", "1/1", "1/2", "1/2.", "1/4", "1/4.", "1/4T",
                                                         "1/8", "1/8.", "1/8T", "1/16", "1/16T", "1/32" };
static constexpr std::array<int, 8> kNotches { 1, 2, 3, 4, 6, 8, 12, 16 };
static constexpr std::array<const char*, 8> kShapeNames { "Sine", "Tri", "Saw Up", "Saw Down", "Square", "Stairs", "S&H", "Random" };

class PhazerEngine
{
public:
    static constexpr int kMaxSections = 16;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset();
    }

    void reset()
    {
        for (auto& c : chans)
        {
            c.s1.fill (0.0); c.s2.fill (0.0); c.coef.fill (0.0);
            c.fb = 0.0; c.env = 0.0; c.lfoSm = 0.0; c.prevPh = 0.0;
            c.sh = 0.0; c.rnA = 0.0; c.rnB = randBi();
        }
        freePh = 0.0;
        counter = 0;
        initialised = false;
    }

    // ppqStart: Song-Position am Blockanfang (Viertel), ppqPerSample: Viertel pro Sample
    void process (float* const* io, int numChannels, int numSamples, const PhazerParams& p,
                  double ppqStart, double ppqPerSample)
    {
        const int nCh = std::min (numChannels, 2);
        if (nCh <= 0) return;

        if (! initialised)
        {
            mixS = p.mix; fbS = p.feedback; cS = p.center; dS = p.depth;
            initialised = true;
        }

        const int    sections = kNotches[(size_t) std::clamp (p.notches, 0, (int) kNotches.size() - 1)];
        const double tau  = 0.0004 + (double) p.smooth * p.smooth * 0.09;
        const double ks   = 1.0 - std::exp (-1.0 / (fs * tau));
        const double ka   = 1.0 - std::exp (-1.0 / (fs * 0.004));
        const double kr   = 1.0 - std::exp (-1.0 / (fs * 0.16));
        const double kp   = 1.0 - std::exp (-1.0 / (fs * 0.02));
        const double g    = 1.0 + p.grit * 8.0;
        const double dr   = 1.0 + p.grit * 3.0;
        const double dg   = (1.0 + p.grit) / dr;
        const double og   = std::pow (10.0, p.outDb / 20.0);
        const double divB = kDivBeats[(size_t) std::clamp (p.div, 0, (int) kDivBeats.size() - 1)];
        const double q    = std::max (0.1, (double) p.sharp);
        const double nyq  = fs * 0.45;
        steps = std::max (2, p.steps);

        double lastF[2] { 0, 0 }, lastL[2] { 0, 0 };

        for (int i = 0; i < numSamples; ++i)
        {
            double ph;
            if (p.sync)
                ph = (ppqStart + i * ppqPerSample) / divB + p.phase / 360.0;
            else
            {
                freePh += p.rate / fs;
                if (freePh > 1.0e6) freePh -= std::floor (freePh);
                ph = freePh + p.phase / 360.0;
            }

            mixS += (p.mix - mixS) * kp;
            fbS  += (p.feedback - fbS) * kp;
            cS   += (p.center - cS) * kp;
            dS   += (p.depth - dS) * kp;

            const bool updateCoefs = (counter & 7) == 0;
            ++counter;

            for (int ch = 0; ch < nCh; ++ch)
            {
                auto& c = chans[(size_t) ch];
                const double x = io[ch][i];

                double pc = ph + ch * p.spread / 360.0;
                pc -= std::floor (pc);
                if (pc < c.prevPh)
                {
                    c.sh = randBi();
                    c.rnA = c.rnB;
                    c.rnB = randBi();
                }
                c.prevPh = pc;

                const double raw = shape (p.shape, pc, c);
                c.lfoSm += (raw - c.lfoSm) * ks;
                const double lfo = c.lfoSm;

                const double ax = std::abs (x);
                c.env += (ax > c.env ? ka : kr) * (ax - c.env);

                const double oct = dS * 4.5 * lfo + p.env * 4.0 * std::min (1.0, c.env * 2.5);
                const double f = std::clamp (cS * std::pow (2.0, oct), 20.0, nyq);
                lastF[ch] = f; lastL[ch] = lfo;

                if (updateCoefs)
                {
                    for (int k = 0; k < sections; ++k)
                    {
                        const double fk = std::clamp (f * std::pow ((double) p.spacing, k - (sections - 1) * 0.5), 20.0, nyq);
                        const double w0 = 2.0 * pi * fk / fs;
                        const double al = std::sin (w0) / (2.0 * q);
                        const double a0 = 1.0 + al;
                        c.coef[(size_t) k * 2]     = (1.0 - al) / a0;       // b0 (= a2)
                        c.coef[(size_t) k * 2 + 1] = -2.0 * std::cos (w0) / a0; // b1 (= a1), b2 = 1
                    }
                }

                double u = x + fbS * c.fb;
                for (int k = 0; k < sections; ++k)
                {
                    const double b0 = c.coef[(size_t) k * 2], b1 = c.coef[(size_t) k * 2 + 1];
                    const double y = b0 * u + c.s1[(size_t) k];
                    c.s1[(size_t) k] = b1 * u - b1 * y + c.s2[(size_t) k];
                    c.s2[(size_t) k] = u - b0 * y;
                    u = y;
                }
                c.fb = std::tanh (u * g) / g;

                if (! std::isfinite (u))
                {
                    c.s1.fill (0.0); c.s2.fill (0.0); c.fb = 0.0; u = 0.0;
                }

                const double o = std::tanh ((x * (1.0 - mixS) + u * mixS) * dr) * dg * og;
                io[ch][i] = (float) o;
            }
        }

        if (nCh == 1) { lastF[1] = lastF[0]; lastL[1] = lastL[0]; }

        vizLfoL.store ((float) lastL[0]);
        vizLfoR.store ((float) lastL[1]);
        vizFreqL.store ((float) lastF[0]);
        vizFreqR.store ((float) lastF[1]);
        vizEnv.store ((float) std::max (chans[0].env, chans[1].env));
        vizBeats.store (ppqStart + numSamples * ppqPerSample);
    }

    // Werte für die Animation im Editor
    std::atomic<float>  vizLfoL { 0 }, vizLfoR { 0 }, vizFreqL { 900 }, vizFreqR { 900 }, vizEnv { 0 };
    std::atomic<double> vizBeats { 0 };

private:
    static constexpr double pi = 3.14159265358979323846;

    struct Channel
    {
        std::array<double, kMaxSections> s1 {}, s2 {};
        std::array<double, kMaxSections * 2> coef {};
        double fb = 0, env = 0, lfoSm = 0, prevPh = 0, sh = 0, rnA = 0, rnB = 0;
    };

    double randBi() { return dist (rng); }

    double shape (int s, double p, const Channel& c) const
    {
        switch (s)
        {
            case 0: return std::sin (2.0 * pi * p);
            case 1: return p < 0.5 ? -1.0 + 4.0 * p : 3.0 - 4.0 * p;
            case 2: return 2.0 * p - 1.0;
            case 3: return 1.0 - 2.0 * p;
            case 4: return p < 0.5 ? 1.0 : -1.0;
            case 5:
            {
                const double t = p < 0.5 ? -1.0 + 4.0 * p : 3.0 - 4.0 * p;
                const double n = steps - 1;
                return std::round ((t + 1.0) * 0.5 * n) / n * 2.0 - 1.0;
            }
            case 6: return c.sh;
            case 7:
            {
                const double t = 0.5 - 0.5 * std::cos (pi * p);
                return c.rnA + (c.rnB - c.rnA) * t;
            }
            default: return 0.0;
        }
    }

    double fs = 48000.0;
    std::array<Channel, 2> chans {};
    double freePh = 0.0;
    unsigned int counter = 0;
    int steps = 8;
    bool initialised = false;
    double mixS = 0.5, fbS = 0.0, cS = 900.0, dS = 0.85;

    std::mt19937 rng { 0xB8A1B6u };
    std::uniform_real_distribution<double> dist { -1.0, 1.0 };
};

} // namespace bb
