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
    std::vector<double> marks;              // Pitch-Marken (Sample-Position, fein)
    std::vector<double> periods;            // Periode je Marke
    std::vector<unsigned char> voiced;      // stimmhaft je Marke
    int numFrames = 0;
    std::vector<float> refl;                // numFrames * lpcOrder, Reflexionskoeffizienten k1..kp
    std::vector<float> raw;                 // Originalstimme (für die Grain Engine)
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
    a->refl.assign ((size_t) a->numFrames * lpcOrder, 0.0f);
    a->raw = x;
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
        juce::ignoreUnused (e);
        for (int m = 0; m <= p; ++m) coef[(size_t) f][(size_t) m] = aa[m];
        for (int m = 1; m <= p; ++m) a->refl[(size_t) f * lpcOrder + (size_t) (m - 1)] = (float) kk[m];
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
    auto f0At = [&] (double pos) -> double
    {
        // linear zwischen Frames, 0 = stimmlos
        const double fp = pos / f0Hop;
        const int i0 = juce::jlimit (0, frames - 1, (int) fp);
        const int i1 = juce::jmin (frames - 1, i0 + 1);
        const double a0 = f0[(size_t) i0], a1 = f0[(size_t) i1];
        if (a0 <= 0) return 0.0;
        if (a1 <= 0) return a0;
        const double t = fp - i0;
        return a0 + (a1 - a0) * t;
    };
    // tiefpassgefiltertes Signal (bis ca. 1 kHz) für den Periodenvergleich
    std::vector<float> lp ((size_t) n);
    {
        juce::IIRFilter f1, f2;
        const auto c = juce::IIRCoefficients::makeLowPass (sr, 1000.0, 0.7071);
        f1.setCoefficients (c); f2.setCoefficients (c);
        for (int i = 0; i < n; ++i) lp[(size_t) i] = f2.processSingleSampleRaw (f1.processSingleSampleRaw (x[(size_t) i]));
    }
    auto ncc = [&] (double posA, double posB, int len) -> double
    {
        // normierte Kreuzkorrelation zweier Abschnitte der Länge len um posA und posB
        double sab = 0, saa = 0, sbb = 0;
        const int ia = (int) posA - len / 2, ib = (int) posB - len / 2;
        for (int k = 0; k < len; ++k)
        {
            const int pa = ia + k, pb = ib + k;
            if (pa < 0 || pb < 0 || pa >= n || pb >= n) continue;
            const double va = lp[(size_t) pa], vb = lp[(size_t) pb];
            sab += va * vb; saa += va * va; sbb += vb * vb;
        }
        return saa > 0 && sbb > 0 ? sab / std::sqrt (saa * sbb) : 0.0;
    };

    // Pitch-Marken: im stimmhaften Bereich Periode für Periode per Korrelation weiterverfolgen
    // (gleichmäßige Abstände = kein Knarzen), Einstieg auf der stärksten Anregungsspitze
    auto refine = [&] (double from, double period) -> double
    {
        const double pred = from + period;
        const int r = juce::jmax (2, (int) (period / 7.0));
        const int len = (int) (period * 1.5);
        const int cnt = juce::jmin (63, 2 * r + 1);
        double sc[64], best = -2.0;
        int bi = 0;
        for (int k = 0; k < cnt; ++k)
        {
            sc[k] = ncc (from, pred + (k - r), len);
            if (sc[k] > best) { best = sc[k]; bi = k; }
        }
        if (best < 0.4) return pred;
        double off = bi - r;
        if (bi > 0 && bi < cnt - 1)
        {
            const double y0 = sc[bi - 1], y1 = sc[bi], y2 = sc[bi + 1];
            const double den = y0 - 2 * y1 + y2;
            if (std::abs (den) > 1e-9) off += juce::jlimit (-0.5, 0.5, 0.5 * (y0 - y2) / den);
        }
        return pred + off;
    };
    const double unvoicedPeriod = 120.0;
    double t = 0.0;
    while (t < n)
    {
        if (f0At (t) <= 0)
        {
            a->marks.push_back (t);
            a->periods.push_back (unvoicedPeriod);
            a->voiced.push_back (0);
            t += unvoicedPeriod;
            continue;
        }
        const double p0 = juce::jlimit (40.0, 400.0, sr / f0At (t));
        int bestK = (int) t;
        float bestV = -1.0f;
        for (int k = (int) t; k < juce::jmin (n, (int) (t + p0)); ++k)
        {
            const float v = std::abs (a->residual[(size_t) k]);
            if (v > bestV) { bestV = v; bestK = k; }
        }
        double m = bestK;
        while (m < n)
        {
            const double fq = f0At (m);
            if (fq <= 0) break;
            const double P = juce::jlimit (40.0, 400.0, sr / fq);
            a->marks.push_back (m);
            a->voiced.push_back (1);
            const double nx = (m + P < n) ? refine (m, P) : m + P;
            const double d = nx - m;
            a->periods.push_back (d > 30.0 && d < 420.0 ? d : P);
            m = m + a->periods.back();
        }
        t = m;
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
// Jedes Korn = Anregung (2 Perioden) durch das Mundraum-Filter seines Frames geschickt, inkl. Nachklingen.
// Tonhöhe: Abstand der Körner. Formant: Korn wird gestaucht/gedehnt (wie klassische Formant-Shifter),
// Tonhöhe bleibt, weil der Abstand gleich bleibt. Das Nachklingen füllt beim Runterpitchen die Lücken.

class VoiceSynth
{
public:
    VoiceSynth()
    {
        acc.assign (accSize, 0.0f);
        grain.assign (maxGrainLen, 0.0f);
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
        tailLeft = 2400;
    }

    void stop() { active = false; }
    bool isActive() const { return active; }
    double getPosition() const { return srcPos; }
    double getLength() const { return src != nullptr ? (double) src->length() : 0.0; }

    // speed: 0..4 (0 = Position eingefroren), pitch: Faktor, formant: 0.5..2
    void render (float* out, int num, double speed, double pitch, double formant)
    {
        if (! active || src == nullptr)
        {
            std::fill (out, out + num, 0.0f);
            return;
        }
        const auto& S = *src;
        const double len = (double) S.length();
        const double ahead = 900.0; // Körner so weit im Voraus planen (Kornanfang liegt bis P/formant vor der Mitte)
        for (int i = 0; i < num; ++i)
        {
            while (tg <= (double) t + ahead && srcPos < len)
                scheduleGrain (S, speed, pitch, formant);
            const size_t idx = (size_t) (t & (accSize - 1));
            out[i] = juce::jlimit (-4.0f, 4.0f, acc[idx]);
            acc[idx] = 0.0f;
            ++t;
            if (srcPos < len) srcPos += speed;
            else if (--tailLeft <= 0) { active = false; std::fill (out + i + 1, out + num, 0.0f); return; }
        }
    }

private:
    void scheduleGrain (const VoiceAnalysis& S, double speed, double pitch, double formant)
    {
        const double sp = juce::jlimit (0.0, (double) S.length() - 1.0, srcPos + (tg - (double) t) * speed);
        auto it = std::lower_bound (S.marks.begin(), S.marks.end(), sp);
        int mi = (int) (it - S.marks.begin());
        if (mi >= (int) S.marks.size()) mi = (int) S.marks.size() - 1;
        if (mi > 0 && std::abs (S.marks[(size_t) mi - 1] - sp) < std::abs (S.marks[(size_t) mi] - sp)) --mi;
        if (mi < 0) { tg += 120.0; return; }
        const bool v = S.voiced[(size_t) mi] != 0;
        if (v && speed < 0.35)
        {
            const int j = mi + (int) (rng() % 3) - 1;
            if (j >= 0 && j < (int) S.marks.size() && S.voiced[(size_t) j]) mi = j;
        }
        const double P = juce::jmax (16.0, S.periods[(size_t) mi]);
        double centre = S.marks[(size_t) mi];
        double hop = P, weight = 1.0;
        if (v)
        {
            hop = P / pitch;
            weight = std::sqrt (1.0 / pitch);
        }
        else
            centre = sp + (double) ((int) (rng() % (unsigned) (2 * (int) P)) - (int) P);
        addGrain (S, centre, P, tg, weight, formant);
        tg += juce::jmax (8.0, hop);
    }

    static inline double cubic (const float* x, int n, double pos)
    {
        // Catmull-Rom: deutlich weniger Höhenverlust als linear
        const int i1 = (int) std::floor (pos);
        if (i1 < 1 || i1 + 2 >= n)
        {
            if (i1 < 0 || i1 + 1 >= n) return 0.0;
            const double f = pos - i1;
            return x[i1] + f * (x[i1 + 1] - x[i1]);
        }
        const double f = pos - i1;
        const double y0 = x[i1 - 1], y1 = x[i1], y2 = x[i1 + 1], y3 = x[i1 + 2];
        const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    void addGrain (const VoiceAnalysis& S, double centre, double P, double at, double weight, double formant)
    {
        const float* r = S.residual.data();
        const int n = (int) S.residual.size();
        const double fmt = juce::jlimit (0.5, 2.0, formant);
        const bool direct = std::abs (fmt - 1.0) < 1e-3;
        // Ohne Formant: Korn direkt im Ausgangsraster berechnen (nur eine Interpolation)
        const double outStart0 = at - P / fmt;
        const double phase = direct ? std::ceil (outStart0) - outStart0 : 0.0;
        const int body = (int) std::ceil (2.0 * P);
        const int len = juce::jmin (maxGrainLen, body + tailLen);
        const double start = centre - P + phase;
        const int fr = juce::jlimit (0, S.numFrames - 1, (int) std::lround (centre / frameHop));
        const float* k = &S.refl[(size_t) fr * lpcOrder];
        double gst[lpcOrder + 1] {};
        double de = 0.0;
        for (int i = 0; i < len; ++i)
        {
            double e = 0.0;
            const double u = (i + phase - P) / P;
            if (u < 1.0)
                e = cubic (r, n, start + i) * (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * u));
            double f = e;
            for (int m = lpcOrder; m >= 1; --m)
            {
                f -= k[m - 1] * gst[m - 1];
                gst[m] = k[m - 1] * f + gst[m - 1];
            }
            gst[0] = f;
            de = f + preEmph * de;
            float g = (float) de;
            if (i >= len - fadeLen) g *= (float) (len - i) / (float) fadeLen;
            grain[(size_t) i] = g;
        }
        const double gain = weight * std::sqrt (fmt);
        if (direct)
        {
            const int j0 = (int) std::ceil (outStart0);
            for (int i = 0; i < len; ++i)
            {
                const int j = j0 + i;
                if (j < t) continue;
                acc[(size_t) (j & (accSize - 1))] += (float) (grain[(size_t) i] * gain);
            }
            return;
        }
        if (fmt > 1.1)
        {
            // Tiefpass gegen Aliasing beim Stauchen (Butterworth 2. Ordnung bei 0.9 * Nyquist / fmt)
            const double fc = 0.45 / fmt; // relativ zur Samplerate
            const double w0 = juce::MathConstants<double>::twoPi * fc, cw = std::cos (w0), al = std::sin (w0) / (2.0 * 0.7071);
            const double b0 = (1 - cw) / 2, b1 = 1 - cw, b2 = (1 - cw) / 2, a0 = 1 + al, a1 = -2 * cw, a2 = 1 - al;
            double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
            for (int i = 0; i < len; ++i)
            {
                const double x0 = grain[(size_t) i];
                const double y0 = (b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
                x2 = x1; x1 = x0; y2 = y1; y1 = y0;
                grain[(size_t) i] = (float) y0;
            }
        }
        // gestaucht/gedehnt (Formant) ins Ausgangssignal legen, Mitte (Anregung) bei 'at'
        const double outEnd = outStart0 + (len - 1) / fmt;
        const int j0 = juce::jmax ((int) t, (int) std::ceil (outStart0));
        const int j1 = (int) std::floor (outEnd);
        for (int j = j0; j <= j1; ++j)
        {
            const double gp = (j - outStart0) * fmt;
            acc[(size_t) (j & (accSize - 1))] += (float) (cubic (grain.data(), len, gp) * gain);
        }
    }

    static constexpr int accSize = 8192;
    static constexpr int tailLen = 480;   // 20 ms Nachklingen
    static constexpr int fadeLen = 96;
    static constexpr int maxGrainLen = 2 * 420 + tailLen + 8;
    std::shared_ptr<const VoiceAnalysis> src;
    std::vector<float> acc, grain;
    juce::int64 t = 0;
    double tg = 0.0, srcPos = 0.0;
    bool active = false;
    int tailLeft = 0;
    std::minstd_rand rng { 1234 };
};

} // namespace tts::dsp
