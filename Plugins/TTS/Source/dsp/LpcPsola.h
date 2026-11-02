#pragma once

// Echtzeit-Stimme: LPC-PSOLA
//  Analyse (einmal nach Generate): Tonhöhe + Pitch-Marken, LPC-Hüllkurve (Mundraum) alle 5 ms, Restsignal (Anregung)
//  Synthese (live, 24 kHz): PSOLA auf dem Restsignal (Tonhöhe + Tempo), danach Filter mit der Hüllkurve
//  (Formant = Hüllkurve verschoben). Weil die Hüllkurve getrennt bleibt, klingt Runterpitchen voll statt knarzig,
//  und alle Regler lassen sich in Echtzeit bewegen und automatisieren.

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

namespace tts::dsp
{
constexpr int lpcOrder = 24;
constexpr int envBins = 129;   // 0..Nyquist
constexpr int frameHop = 120;  // 5 ms bei 24 kHz
constexpr float preEmph = 0.9f;

// ---------------------------------------------------------------- Levinson-Durbin

// r[0..p] -> a[1..p] (A(z) = 1 + sum a_i z^-i), k[1..p] Reflexionskoeffizienten, Rückgabe: Restfehler
inline double levinson (const double* r, double* a, double* k, int p)
{
    double err = r[0];
    if (err <= 1e-12)
    {
        for (int i = 0; i <= p; ++i) { a[i] = 0.0; k[i] = 0.0; }
        a[0] = 1.0;
        return 1e-12;
    }
    double tmp[lpcOrder + 2];
    a[0] = 1.0;
    for (int i = 1; i <= p; ++i) a[i] = 0.0;
    for (int m = 1; m <= p; ++m)
    {
        double acc = r[m];
        for (int i = 1; i < m; ++i) acc += a[i] * r[m - i];
        double km = -acc / err;
        km = juce::jlimit (-0.9995, 0.9995, km);
        k[m] = km;
        for (int i = 1; i < m; ++i) tmp[i] = a[i] + km * a[m - i];
        for (int i = 1; i < m; ++i) a[i] = tmp[i];
        a[m] = km;
        err *= (1.0 - km * km);
        if (err <= 1e-12) err = 1e-12;
    }
    return err;
}

// ---------------------------------------------------------------- Analyse

struct VoiceAnalysis
{
    int sampleRate = 24000;
    std::vector<float> residual;            // Anregung (vorverzerrt)
    std::vector<int> marks;                 // Pitch-Marken (Sample-Position)
    std::vector<int> periods;               // Periode je Marke
    std::vector<unsigned char> voiced;      // stimmhaft je Marke
    int numFrames = 0;
    std::vector<float> env;                 // numFrames * envBins, Leistungs-Hüllkurve
    std::vector<float> peaks;               // für die Anzeige (Betrag, 0..1), 1 Wert je 120 Samples
    size_t length() const { return residual.size(); }
};

namespace detail
{
inline std::vector<float> findF0 (const std::vector<float>& x, int sr, int hop)
{
    const int n = (int) x.size();
    const int win = sr / 25;
    const int minLag = sr / 420, maxLag = sr / 60;
    const int frames = n / hop + 1;
    std::vector<float> f0 ((size_t) frames, 0.0f);
    std::vector<double> d ((size_t) maxLag + 2);
    for (int f = 0; f < frames; ++f)
    {
        const int start = f * hop - win / 2;
        double energy = 0.0;
        for (int i = 0; i < win; ++i)
        {
            const int k = start + i;
            const float v = (k >= 0 && k < n) ? x[(size_t) k] : 0.0f;
            energy += (double) v * v;
        }
        if (energy / win < 2e-6) continue;
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            double s = 0.0;
            for (int i = 0; i < win - maxLag; ++i)
            {
                const int k = start + i;
                const float a = (k >= 0 && k < n) ? x[(size_t) k] : 0.0f;
                const float b = (k + lag >= 0 && k + lag < n) ? x[(size_t) (k + lag)] : 0.0f;
                s += (double) (a - b) * (a - b);
            }
            d[(size_t) lag] = s;
        }
        double run = 0.0;
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            run += d[(size_t) lag];
            d[(size_t) lag] = run > 0 ? d[(size_t) lag] * lag / run : 1.0;
        }
        int best = -1;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            if (d[(size_t) lag] < 0.2)
            {
                while (lag + 1 <= maxLag && d[(size_t) lag + 1] < d[(size_t) lag]) ++lag;
                best = lag;
                break;
            }
        }
        if (best > 0)
        {
            // parabolische Verfeinerung
            double lagF = best;
            if (best > 1 && best < maxLag)
            {
                const double y0 = d[(size_t) best - 1], y1 = d[(size_t) best], y2 = d[(size_t) best + 1];
                const double den = y0 - 2 * y1 + y2;
                if (std::abs (den) > 1e-12) lagF += 0.5 * (y0 - y2) / den;
            }
            f0[(size_t) f] = (float) (sr / lagF);
        }
    }
    // Median über 5 Frames + Oktavsprünge korrigieren
    std::vector<float> out = f0;
    for (int f = 0; f < frames; ++f)
    {
        if (f0[(size_t) f] <= 0) continue;
        float w[5];
        int c = 0;
        for (int k = f - 2; k <= f + 2; ++k)
            if (k >= 0 && k < frames && f0[(size_t) k] > 0) w[c++] = f0[(size_t) k];
        std::sort (w, w + c);
        const float med = w[c / 2];
        float v = f0[(size_t) f];
        if (v > med * 1.7f) v *= 0.5f;
        else if (v < med * 0.6f) v *= 2.0f;
        out[(size_t) f] = v;
        // einzelne stimmhafte Frames zwischen stimmlosen sind meist Fehler
        if (c <= 1) out[(size_t) f] = 0.0f;
    }
    return out;
}
} // namespace detail

inline std::shared_ptr<VoiceAnalysis> analyseVoice (const std::vector<float>& x, int sr, const std::atomic<bool>* cancel = nullptr)
{
    auto a = std::make_shared<VoiceAnalysis>();
    a->sampleRate = sr;
    const int n = (int) x.size();
    if (n == 0) return a;

    // Vorverzerrung
    std::vector<float> y ((size_t) n);
    float prev = 0.0f;
    for (int i = 0; i < n; ++i) { y[(size_t) i] = x[(size_t) i] - preEmph * prev; prev = x[(size_t) i]; }

    // LPC je Frame
    const int p = lpcOrder;
    const int win = 600;
    a->numFrames = n / frameHop + 1;
    a->env.assign ((size_t) a->numFrames * envBins, 0.0f);
    std::vector<std::array<double, lpcOrder + 1>> coef ((size_t) a->numFrames);
    std::vector<double> w ((size_t) win);
    for (int i = 0; i < win; ++i) w[(size_t) i] = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i + 0.5) / win);
    // Lag-Fenster (glättet die Hüllkurve etwas, stabiler)
    double lagW[lpcOrder + 1];
    for (int m = 0; m <= p; ++m) { const double t = 2.0 * juce::MathConstants<double>::pi * 50.0 * m / sr; lagW[m] = std::exp (-0.5 * t * t); }
    std::vector<double> buf ((size_t) win);

    for (int f = 0; f < a->numFrames; ++f)
    {
        if (cancel != nullptr && (f & 255) == 0 && cancel->load()) return {};
        const int start = f * frameHop - win / 2;
        for (int i = 0; i < win; ++i)
        {
            const int k = start + i;
            buf[(size_t) i] = (k >= 0 && k < n) ? y[(size_t) k] * w[(size_t) i] : 0.0;
        }
        double r[lpcOrder + 1];
        for (int m = 0; m <= p; ++m)
        {
            double s = 0.0;
            for (int i = m; i < win; ++i) s += buf[(size_t) i] * buf[(size_t) (i - m)];
            r[m] = s * lagW[m];
        }
        r[0] = r[0] * 1.0001 + 1e-9;
        double aa[lpcOrder + 1], kk[lpcOrder + 1];
        const double e = levinson (r, aa, kk, p);
        for (int m = 0; m <= p; ++m) coef[(size_t) f][(size_t) m] = aa[m];
        // Leistungs-Hüllkurve E / |A|^2 (pro Sample normiert)
        const double norm = e / win;
        for (int b = 0; b < envBins; ++b)
        {
            double re = 0.0, im = 0.0;
            for (int m = 0; m <= p; ++m)
            {
                const double ang = juce::MathConstants<double>::pi * b * m / (envBins - 1);
                re += aa[m] * std::cos (ang);
                im -= aa[m] * std::sin (ang);
            }
            const double mag2 = juce::jmax (1e-12, re * re + im * im);
            a->env[(size_t) f * envBins + (size_t) b] = (float) (norm / mag2);
        }
    }

    // Restsignal: inverse Filterung mit den Frame-Koeffizienten (blockweise, Übergang über 2 Frames gleitend)
    a->residual.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const double fpos = (double) i / frameHop;
        const int f0 = juce::jlimit (0, a->numFrames - 1, (int) fpos);
        const int f1 = juce::jmin (a->numFrames - 1, f0 + 1);
        const double t = fpos - f0;
        double acc = y[(size_t) i];
        for (int m = 1; m <= p; ++m)
        {
            const int k = i - m;
            if (k < 0) break;
            const double c = coef[(size_t) f0][(size_t) m] * (1.0 - t) + coef[(size_t) f1][(size_t) m] * t;
            acc += c * y[(size_t) k];
        }
        a->residual[(size_t) i] = (float) acc;
    }

    // Tonhöhe + Pitch-Marken (auf die Anregungsspitzen eingerastet)
    const int f0Hop = 120;
    auto f0 = detail::findF0 (x, sr, f0Hop);
    const int frames = (int) f0.size();
    const int unvoicedPeriod = 120;
    int t = 0;
    while (t < n)
    {
        const int f = juce::jlimit (0, frames - 1, t / f0Hop);
        const float freq = f0[(size_t) f];
        if (freq > 0)
        {
            const int period = juce::jlimit (40, 400, (int) std::lround (sr / freq));
            int bestK = t;
            float bestV = -1.0f;
            const int rad = period / 4;
            for (int k = juce::jmax (0, t - rad); k <= juce::jmin (n - 1, t + rad); ++k)
            {
                const float v = std::abs (a->residual[(size_t) k]);
                if (v > bestV) { bestV = v; bestK = k; }
            }
            if (! a->marks.empty() && bestK < a->marks.back() + period * 2 / 3) bestK = a->marks.back() + period;
            if (bestK >= n) break;
            a->marks.push_back (bestK);
            a->periods.push_back (period);
            a->voiced.push_back (1);
            t = bestK + period;
        }
        else
        {
            a->marks.push_back (t);
            a->periods.push_back (unvoicedPeriod);
            a->voiced.push_back (0);
            t += unvoicedPeriod;
        }
    }
    // Perioden aus den tatsächlichen Abständen (glatter)
    for (size_t i = 0; i + 1 < a->marks.size(); ++i)
        if (a->voiced[i] && a->voiced[i + 1])
        {
            const int d = a->marks[i + 1] - a->marks[i];
            if (d > 30 && d < 420) a->periods[i] = d;
        }

    // Anzeige-Spitzen
    const int pk = (n + frameHop - 1) / frameHop;
    a->peaks.assign ((size_t) pk, 0.0f);
    float mx = 1e-6f;
    for (int i = 0; i < n; ++i) mx = juce::jmax (mx, std::abs (x[(size_t) i]));
    for (int i = 0; i < n; ++i)
    {
        auto& v = a->peaks[(size_t) (i / frameHop)];
        v = juce::jmax (v, std::abs (x[(size_t) i]) / mx);
    }
    return a;
}

// ---------------------------------------------------------------- Synthese (Echtzeit, 24 kHz)

class VoiceSynth
{
public:
    VoiceSynth()
    {
        for (int b = 0; b < envBins; ++b)
        {
            const double wb = (b == 0 || b == envBins - 1) ? 0.5 : 1.0;
            for (int m = 0; m <= lpcOrder; ++m)
                cosTab[b][m] = wb * std::cos (juce::MathConstants<double>::pi * b * m / (envBins - 1));
        }
        acc.assign (accSize, 0.0f);
    }

    void setSource (std::shared_ptr<const VoiceAnalysis> s) { src = std::move (s); stop(); }
    const std::shared_ptr<const VoiceAnalysis>& getSource() const { return src; }

    void start (double pos = 0.0)
    {
        std::fill (acc.begin(), acc.end(), 0.0f);
        t = 0;
        tg = 0.0;
        srcPos = pos;
        active = src != nullptr && src->length() > 0;
        for (auto& v : g) v = 0.0;
        deemph = 0.0;
        coefCountdown = 0;
        tailLeft = 2400;
    }

    void stop() { active = false; }
    bool isActive() const { return active; }
    double getPosition() const { return srcPos; }
    double getLength() const { return src != nullptr ? (double) src->length() : 0.0; }

    // speed: 0.001..4, pitch: Faktor, formant: 0.5..2
    void render (float* out, int num, double speed, double pitch, double formant)
    {
        if (! active || src == nullptr)
        {
            std::fill (out, out + num, 0.0f);
            return;
        }
        const auto& S = *src;
        const double len = (double) S.length();
        const double maxP = 420.0;
        for (int i = 0; i < num; ++i)
        {
            // Grains vorausplanen
            while (tg <= (double) t + maxP && srcPos < len)
                scheduleGrain (S, speed, pitch);
            // Filter-Koeffizienten regelmäßig erneuern
            if (--coefCountdown <= 0)
            {
                updateFilter (S, formant);
                coefCountdown = 48;
            }
            const size_t idx = (size_t) (t & (accSize - 1));
            const double e = acc[idx];
            acc[idx] = 0.0f;
            ++t;
            // Gitterfilter (Allpol) mit Reflexionskoeffizienten
            double f = e * gain;
            for (int m = lpcOrder; m >= 1; --m)
            {
                f -= k[m] * g[m - 1];
                g[m] = k[m] * f + g[m - 1];
            }
            g[0] = f;
            // Entzerrung der Vorverzerrung
            deemph = f + preEmph * deemph;
            if (! std::isfinite (deemph)) { deemph = 0.0; for (auto& v : g) v = 0.0; }
            out[i] = (float) juce::jlimit (-4.0, 4.0, deemph);
            if (srcPos < len) srcPos += speed;
            else if (--tailLeft <= 0) { active = false; std::fill (out + i + 1, out + num, 0.0f); return; }
        }
    }

private:
    void scheduleGrain (const VoiceAnalysis& S, double speed, double pitch)
    {
        // Quellposition zum Zeitpunkt des Grains
        const double sp = juce::jlimit (0.0, (double) S.length() - 1.0, srcPos + (tg - (double) t) * speed);
        auto it = std::lower_bound (S.marks.begin(), S.marks.end(), (int) sp);
        int mi = (int) (it - S.marks.begin());
        if (mi >= (int) S.marks.size()) mi = (int) S.marks.size() - 1;
        if (mi > 0 && std::abs (S.marks[(size_t) mi - 1] - sp) < std::abs (S.marks[(size_t) mi] - sp)) --mi;
        if (mi < 0) { tg += 120.0; return; }
        const bool v = S.voiced[(size_t) mi] != 0;
        // sehr langsam: leicht streuen, damit es nicht maschinell brummt
        if (v && speed < 0.35)
        {
            const int j = mi + (int) (rng() % 3) - 1;
            if (j >= 0 && j < (int) S.marks.size() && S.voiced[(size_t) j]) mi = j;
        }
        const int P = juce::jmax (16, S.periods[(size_t) mi]);
        double centre = (double) S.marks[(size_t) mi];
        double weight = 1.0;
        double hop = P;
        if (v)
        {
            hop = (double) P / pitch;
            weight = std::sqrt (1.0 / pitch);
        }
        else
        {
            // stimmlos: zufällige Stelle in der Nähe -> Rauschen statt Brummen
            centre = sp + (double) ((int) (rng() % (unsigned) (2 * P)) - P);
            hop = P;
        }
        addGrain (S, centre, P, tg, weight);
        tg += juce::jmax (8.0, hop);
    }

    void addGrain (const VoiceAnalysis& S, double centre, int P, double at, double weight)
    {
        const auto& r = S.residual;
        const int n = (int) r.size();
        const int j0 = (int) std::ceil (at - P), j1 = (int) std::floor (at + P);
        const double invP = 1.0 / P;
        for (int j = juce::jmax ((int) t, j0); j <= j1; ++j)
        {
            const double u = (double) j - at;
            const double sPos = centre + u;
            const int s0 = (int) std::floor (sPos);
            if (s0 < 0 || s0 + 1 >= n) continue;
            const double fr = sPos - s0;
            const double val = r[(size_t) s0] + fr * (r[(size_t) s0 + 1] - r[(size_t) s0]);
            const double w = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * u * invP);
            acc[(size_t) (j & (accSize - 1))] += (float) (val * w * weight);
        }
    }

    void updateFilter (const VoiceAnalysis& S, double formant)
    {
        const double fpos = juce::jlimit (0.0, (double) (S.numFrames - 1), srcPos / frameHop);
        const int f0 = (int) fpos;
        const int f1 = juce::jmin (S.numFrames - 1, f0 + 1);
        const double ft = fpos - f0;
        const float* e0 = &S.env[(size_t) f0 * envBins];
        const float* e1 = &S.env[(size_t) f1 * envBins];
        double env[envBins];
        for (int b = 0; b < envBins; ++b) env[b] = e0[b] * (1.0 - ft) + e1[b] * ft;

        double r0[lpcOrder + 1], aTmp[lpcOrder + 1], kTmp[lpcOrder + 1];
        autocorr (env, r0);
        const double eOrig = levinson (r0, aTmp, kTmp, lpcOrder);
        if (std::abs (formant - 1.0) < 1e-3)
        {
            for (int m = 1; m <= lpcOrder; ++m) k[m] = kTmp[m];
            gain = 1.0;
            return;
        }
        double warped[envBins];
        for (int b = 0; b < envBins; ++b)
        {
            const double sb = b / formant;
            if (sb >= envBins - 1) { warped[b] = env[envBins - 1]; continue; }
            const int i0 = (int) sb;
            const double fr = sb - i0;
            warped[b] = env[i0] * (1.0 - fr) + env[i0 + 1] * fr;
        }
        double rw[lpcOrder + 1];
        autocorr (warped, rw);
        const double eW = levinson (rw, aTmp, kTmp, lpcOrder);
        for (int m = 1; m <= lpcOrder; ++m) k[m] = kTmp[m];
        gain = juce::jlimit (0.05, 20.0, std::sqrt (eW / juce::jmax (1e-15, eOrig)));
    }

    void autocorr (const double* env, double* r) const
    {
        for (int m = 0; m <= lpcOrder; ++m)
        {
            double s = 0.0;
            for (int b = 0; b < envBins; ++b) s += env[b] * cosTab[b][m];
            r[m] = s;
        }
        r[0] *= 1.0001;
    }

    static constexpr int accSize = 4096;
    std::shared_ptr<const VoiceAnalysis> src;
    std::vector<float> acc;
    juce::int64 t = 0;
    double tg = 0.0, srcPos = 0.0;
    bool active = false;
    int tailLeft = 0;
    double k[lpcOrder + 1] {}, g[lpcOrder + 1] {};
    double gain = 1.0, deemph = 0.0;
    int coefCountdown = 0;
    double cosTab[envBins][lpcOrder + 1];
    std::minstd_rand rng { 1234 };
};
} // namespace tts::dsp
