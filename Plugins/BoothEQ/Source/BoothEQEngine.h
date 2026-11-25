#pragma once

// Booth EQ: 3-Band-Live-EQ der Booth-Reihe (Weiterentwicklung des Club EQ).
//
// Klangziel (statischer Frequenzgang):
//   H(s) = gMid * LowShelf(gLow / gMid, f1) * HighShelf(gHigh / gMid, f2)
//   Shelves: Butterworth 8. Ordnung (48 dB/Okt. Übergang). Bei Kill liegt -6 dB genau auf der Trennfrequenz.
//
// Umsetzung (parallel, Partialbruchzerlegung):
//   Alle 16 Pole liegen fest (8 Pol-Paare: 4 bei f1, 4 bei f2). Jedes Pol-Paar ist ein eigenes
//   State-Variable-Filter (TPT), das NUR das Eingangssignal sieht. Der Ausgang ist eine gewichtete Summe:
//     y = d0 * x + Summe_k ( cBp_k * bp_k + cLp_k * lp_k )
//   Die Gewichte sind die Partialbruch-Residuen von H(s) und hängen nur von den Reglern ab.
//   Folgen:
//   - Die Filterzustände hängen nie von Reglern ab. Jede Regler- oder Kill-Bewegung ist mathematisch ein
//     Crossfade zwischen zwei gültigen EQ-Einstellungen: klickfrei, keine Überschwinger, keine Ausschläge.
//     (Eine Kaskade mit umgerechneten Koeffizienten erzeugt beim Mid-Kill bis zu 100 dB interne Anhebung,
//      die bei schnellen Bewegungen als Knall durchkommt.)
//   - Neutral: d0 = 1, alle Gewichte 0, Filter laufen warm weiter, Ausgang = Eingang bitgenau.
//   - Keine Latenz, minimum-phase.
//
// Warum keine Linkwitz-Riley-Weiche: Die Summe ist nur ein Allpass, die Phase dreht sich, sobald ein
// Regler leicht aus der Mitte ist, und bei limitierten Tracks steigen die Spitzen um mehrere dB.
//
// Live-Schutz:
//   - Kill-Taster: 5 ms Crossfade (S-Kurve) zwischen den Mischgewichten "ohne Kill" und "mit Kill".
//     Da alle Wege dieselben Filter teilen, ist das exakt ein idealer Crossfade der beiden Ausgänge.
//   - Kurvenwechsel 5 ms, Drehregler 15 ms. Gewichte pro Sample interpoliert.
//   - Pegeldeckel ohne Lookahead: Ausgang höchstens (größte eingestellte Anhebung + Spielraum) über der
//     Eingangs-Hüllkurve (Spitzenwert, 300 ms Rücklauf). Sicherheitsnetz.
//   - NaN/Inf am Eingang wird als Stille behandelt, Filterzustände werden bei Fehlern zurückgesetzt.

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>

namespace beq
{
inline constexpr float kMinDb = -26.0f;           // Kurve "EQ": ganz links -26 dB (wie DJM-900)
inline constexpr float kMaxDb = 6.0f;             // ganz rechts +6 dB
inline constexpr float kDetent = 0.02f;           // halbe Breite der Mittenraste (Knopfweg 0…1)
inline constexpr double kKillGain = 1.0e-5;       // Kill / ISO ganz links: -100 dB
inline constexpr double kDefaultXLow = 180.0;     // Kick + Bass samt Obertönen komplett im LOW-Band
inline constexpr double kDefaultXHigh = 2500.0;   // Leads im MID, Hats und Acid-Zischeln im HIGH
inline constexpr int kOrder = 8;                  // Butterworth-Ordnung der Shelves
inline constexpr int kSections = kOrder / 2;      // Pol-Paare je Shelf
inline constexpr int kPoles = 2 * kSections;      // Pol-Paare gesamt
inline constexpr double kKillFadeSec = 0.005;     // Regel 9: Zustandswechsel 2–5 ms
inline constexpr double kKnobGlideSec = 0.015;    // Drehregler
inline constexpr double kCapHeadroomDb = 3.0;     // Spielraum des Pegeldeckels über der eingestellten Anhebung
inline constexpr double kEnvReleaseSec = 0.300;
inline constexpr double kCapReleaseSec = 0.050;

// Feste Polfrequenz: Beim Kill liegt der -6-dB-Punkt genau auf der Trennfrequenz.
inline double poleScale() noexcept { return std::pow (1.0 / 3.0, 1.0 / (2.0 * kOrder)); }

// Knopfposition 0…1 (0,5 = 12 Uhr) -> Weg ab Mitte, mit Raste. Ergebnis -1…+1, 0 in der Raste.
inline float travel (float pos) noexcept
{
    const float d = pos - 0.5f;
    const float a = std::abs (d);
    if (a <= kDetent) return 0.0f;
    const float t = juce::jmin (1.0f, (a - kDetent) / (0.5f - kDetent));
    return d < 0.0f ? -t : t;
}

// Knopfposition -> dB, wie am Regler.
// Anheben: linear in dB bis +6. Absenken "ISO": bis ganz weg. Absenken "EQ": Log-Kurve bis -26 dB.
inline float positionToDb (float pos, bool iso) noexcept
{
    const float t = travel (pos);
    if (t >= 0.0f) return kMaxDb * t;
    const float c = -t;
    if (iso)
    {
        const float g = std::pow (1.0f - c, 2.2f);
        return g <= 0.0f ? -std::numeric_limits<float>::infinity() : juce::Decibels::gainToDecibels (g, -1000.0f);
    }
    return kMinDb * std::pow (c, 1.5f);
}

// Knopfposition -> linearer Faktor (in der Raste exakt 1.0)
inline double positionToGain (float pos, bool iso) noexcept
{
    const float t = travel (pos);
    if (t == 0.0f) return 1.0;
    if (t > 0.0f) return (double) juce::Decibels::decibelsToGain (kMaxDb * t);
    if (iso) return juce::jmax (kKillGain, (double) std::pow (1.0f + t, 2.2f));
    return (double) juce::Decibels::decibelsToGain (positionToDb (pos, false));
}

inline juce::String formatDb (float pos, bool iso)
{
    const float db = positionToDb (pos, iso);
    if (! std::isfinite (db) || db < -99.0f) return juce::String::fromUTF8 ("-\xe2\x88\x9e dB");
    if (std::abs (db) < 0.05f) return "0.0 dB";
    return (db > 0 ? "+" : "") + juce::String (db, 1) + " dB";
}

inline double sectionR (int m) noexcept   // Dämpfung R = sin((2m-1) pi / 2N), m = 1…N/2
{
    return std::sin ((2.0 * m - 1.0) * juce::MathConstants<double>::pi / (2.0 * kOrder));
}

// Analytischer Frequenzgang (Kurvenanzeige), Bandfaktoren inkl. Kill
inline float responseDb (double f, double f1, double f2, double gL, double gM, double gH) noexcept
{
    using C = std::complex<double>;
    const double m = juce::jmax (gM, kKillGain);
    const double r = std::pow (juce::jmax (gL, kKillGain) / m, 1.0 / kOrder);
    const double q = std::pow (juce::jmax (gH, kKillGain) / m, 1.0 / kOrder);
    const C s1 (0.0, f / (f1 / poleScale())), s2 (0.0, f / (f2 / poleScale()));
    C h (m, 0.0);
    for (int k = 1; k <= kSections; ++k)
    {
        const double R = sectionR (k);
        h *= (s1 * s1 + 2.0 * R * r * s1 + r * r) / (s1 * s1 + 2.0 * R * s1 + 1.0);
        h *= (q * q * s2 * s2 + 2.0 * R * q * s2 + 1.0) / (s2 * s2 + 2.0 * R * s2 + 1.0);
    }
    return (float) (20.0 * std::log10 (juce::jmax (1.0e-9, std::abs (h))));
}

// Übergang mit wählbarer Dauer; sCurve = weicher Anfang und weiches Ende (für Kills)
struct Ramp
{
    double cur = 1.0, target = 1.0, from = 1.0;
    int left = 0, total = 0;
    bool sCurve = false;

    void snap (double v) noexcept { cur = target = from = v; left = total = 0; }
    void setTarget (double v, int samples, bool s) noexcept
    {
        if (v == target) return;
        from = cur; target = v; sCurve = s;
        total = left = juce::jmax (1, samples);
    }
    bool moving() const noexcept { return left > 0; }
    void advance (int n) noexcept
    {
        if (left <= 0) return;
        left = juce::jmax (0, left - n);
        if (left == 0) { cur = target; return; }
        double t = 1.0 - (double) left / (double) total;
        if (sCurve) t = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * t);
        cur = from + (target - from) * t;
    }
};

// TPT-State-Variable-Filter (2. Ordnung), Zustände pro Kanal. Liefert Bandpass und Tiefpass.
struct Svf
{
    double g = 0.1, k = 1.4, h = 1.0;
    double s1[2] {}, s2[2] {};

    void setup (double tanG, double R) noexcept
    {
        g = tanG; k = 2.0 * R;
        h = 1.0 / (1.0 + k * g + g * g);
    }
    inline void tick (int c, double x, double& bp, double& lp) noexcept
    {
        const double hp = (x - (k + g) * s1[c] - s2[c]) * h;
        const double v1 = g * hp;
        bp = v1 + s1[c];
        s1[c] = bp + v1;
        const double v2 = g * bp;
        lp = v2 + s2[c];
        s2[c] = lp + v2;
    }
    void reset() noexcept { s1[0] = s1[1] = s2[0] = s2[1] = 0.0; }
    void flushDenormals() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            if (std::abs (s1[c]) < 1.0e-30) s1[c] = 0.0;
            if (std::abs (s2[c]) < 1.0e-30) s2[c] = 0.0;
        }
    }
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        knobSamples = juce::roundToInt (kKnobGlideSec * fs);
        killSamples = juce::roundToInt (kKillFadeSec * fs);
        envRelease = std::exp (-1.0 / (kEnvReleaseSec * fs));
        capRelease = std::exp (-1.0 / (kCapReleaseSec * fs));
        for (auto* r : { &knobL, &knobM, &knobH, &killL, &killM })
            r->snap (r->target);
        curX1 = curX2 = -1.0;
        setCrossovers (xTarget1, xTarget2, true);
        clearState();
        validMask = 0;
        refreshCorners (true);
        blend (killL.cur, killM.cur, 1.0, mixStatic);
        mixDirty = false;
        unity = atUnity();
    }

    // Host-reset (z. B. Transport Stopp/Start): Ein EQ hat keinen Nachhall. Die Filter bleiben warm,
    // damit ein reset mitten im laufenden Signal keinen Sprung erzeugt. Nur der Deckel wird gelöst.
    void reset() noexcept
    {
        capGain = 1.0;
    }

    void clearState() noexcept   // nur bei prepare (neue Samplerate)
    {
        for (auto& s : svf) s.reset();
        env = 0.0;
        capGain = 1.0;
    }

    // Reglerfaktoren; fast = Kurvenwechsel (5 ms statt 15 ms)
    void setKnobGains (double l, double m, double h, bool fast, bool snap = false)
    {
        if (snap) { knobL.snap (l); knobM.snap (m); knobH.snap (h); return; }
        const int n = fast ? killSamples : knobSamples;
        knobL.setTarget (l, n, fast); knobM.setTarget (m, n, fast); knobH.setTarget (h, n, fast);
    }

    void setKills (bool lowKill, bool midKill, bool snap = false)
    {
        const double l = lowKill ? 1.0 : 0.0, m = midKill ? 1.0 : 0.0;   // Überblendanteil "gekillt"
        if (snap) { killL.snap (l); killM.snap (m); mixDirty = true; return; }
        if (l != killL.target || m != killM.target) mixDirty = true;
        killL.setTarget (l, killSamples, true);
        killM.setTarget (m, killSamples, true);
    }

    void setCrossovers (double x1, double x2, bool snap = false)
    {
        const double nyq = fs * 0.45 * poleScale();
        x1 = juce::jlimit (40.0, juce::jmin (1000.0, nyq), x1);
        x2 = juce::jlimit (juce::jmax (x1 * 2.0, 1000.0), juce::jmax (1000.0, nyq), x2);
        xTarget1 = x1; xTarget2 = x2;
        if (snap || curX1 < 0.0)
        {
            curX1 = x1; curX2 = x2;
            setupFilters();
            mixDirty = true;
            filtersChanged = true;
        }
    }

    void setCapEnabled (bool on) noexcept { capEnabled = on; }   // nur für Messungen

    // Verarbeitet in-place (1 oder 2 Kanäle)
    void process (float* const* ch, int numCh, int n) noexcept
    {
        numCh = juce::jlimit (1, 2, numCh);
        glideCrossovers();

        constexpr int sub = 8;
        for (int start = 0; start < n; start += sub)
        {
            const int len = juce::jmin (sub, n - start);
            const bool knobsMoving = knobL.moving() || knobM.moving() || knobH.moving();
            const bool killsMoving = killL.moving() || killM.moving();
            const bool dynamic = knobsMoving || killsMoving || mixDirty;

            if (knobsMoving || mixDirty)
            {
                // Ecken (ohne/mit Kill) für das Ende dieses Abschnitts; Anfang = bisherige Werte
                for (auto* r : { &knobL, &knobM, &knobH }) r->advance (len);
                refreshCorners (knobsMoving);
                mixDirty = false;
            }
            const bool bypass = unity && ! dynamic;
            const double inv = 1.0 / (double) len;

            for (int i = 0; i < len; ++i)
            {
                const Mix* mix = &mixStatic;
                if (dynamic)
                {
                    // Kill-Überblendung pro Sample (S-Kurve), Ecken linear über den Abschnitt
                    killL.advance (1);
                    killM.advance (1);
                    blend (killL.cur, killM.cur, (double) (i + 1) * inv, mixTemp);
                    mix = &mixTemp;
                }

                double y[2] {}, pkIn = 0.0, pkOut = 0.0;
                bool fault = false;
                for (int c = 0; c < numCh; ++c)
                {
                    float& in = ch[c][start + i];
                    if (! std::isfinite (in)) in = 0.0f;
                    const double x = (double) in;
                    pkIn = juce::jmax (pkIn, std::abs (x));

                    double acc = mix->d0 * x;
                    for (size_t p = 0; p < (size_t) kPoles; ++p)   // Filter laufen immer (warm), auch im Bypass
                    {
                        double bp, lp;
                        svf[p].tick (c, x, bp, lp);
                        acc += mix->bp[p] * bp + mix->lp[p] * lp;
                    }
                    if (! (std::abs (acc) < 1.0e4)) { fault = true; acc = 0.0; }
                    y[c] = acc;
                    pkOut = juce::jmax (pkOut, std::abs (acc));
                }

                env = pkIn > env ? pkIn : env * envRelease;   // inkl. aktuellem Sample
                if (fault) recoverFromFault();
                if (bypass) continue;   // Ausgang = Eingang, bitgenau

                if (capEnabled)
                {
                    // Pegeldeckel: sofort runter, mit 50 ms zurück, nie über die Decke
                    const double ceiling = juce::jmax (env, 1.0e-4) * capFactor;
                    const double allowed = pkOut > ceiling ? ceiling / pkOut : 1.0;
                    const double released = 1.0 - (1.0 - capGain) * capRelease;
                    if (allowed < 1.0) ++capHits;
                    capGain = juce::jmin (allowed, released);
                    minCapGain = juce::jmin (minCapGain, capGain);
                }
                for (int c = 0; c < numCh; ++c)
                    ch[c][start + i] = (float) (y[c] * capGain);
            }

            if (dynamic)
            {
                blend (killL.cur, killM.cur, 1.0, mixStatic);   // Stand am Abschnittsende
                cornerPrev = cornerNow;                          // nächster Abschnitt startet hier
            }
        }

        for (auto& s : svf) s.flushDenormals();
        unity = atUnity() && capGain > 0.999999;
        if (unity) capGain = 1.0;
    }

    // für Anzeige und Test
    double bandGain (int band) const noexcept
    {
        auto k = [] (double a) { return 1.0 - a + a * kKillGain; };
        return band == 0 ? knobL.cur * k (killL.cur) : band == 1 ? knobM.cur * k (killM.cur) : knobH.cur;
    }
    int capHitCount() const noexcept { return capHits; }
    void clearCapHits() noexcept { capHits = 0; minCapGain = 1.0; }
    double maxCapReductionDb() const noexcept { return -20.0 * std::log10 (juce::jmax (1.0e-9, minCapGain)); }
    bool isBypassing() const noexcept { return unity; }

private:
    struct Mix
    {
        double d0 = 1.0;
        std::array<double, kPoles> bp {}, lp {};
    };

    bool anyMoving() const noexcept
    {
        return knobL.moving() || knobM.moving() || knobH.moving() || killL.moving() || killM.moving();
    }

    bool atUnity() const noexcept
    {
        return ! anyMoving() && ! mixDirty && knobL.cur == 1.0 && knobM.cur == 1.0 && knobH.cur == 1.0
            && killL.cur == 0.0 && killM.cur == 0.0;
    }

    // Welche Ecken (0 = ohne Kill, 1 = Low-Kill, 2 = Mid-Kill, 3 = beide) werden gebraucht?
    int neededMask() const noexcept
    {
        const bool l0 = killL.cur < 1.0 || killL.target < 1.0, l1 = killL.cur > 0.0 || killL.target > 0.0;
        const bool m0 = killM.cur < 1.0 || killM.target < 1.0, m1 = killM.cur > 0.0 || killM.target > 0.0;
        int mask = 0;
        if (l0 && m0) mask |= 1;
        if (l1 && m0) mask |= 2;
        if (l0 && m1) mask |= 4;
        if (l1 && m1) mask |= 8;
        return mask;
    }

    // Ecken-Gewichte für die aktuellen Reglerwerte. knobsMoved: Anfang = alter Stand (Interpolation).
    void refreshCorners (bool knobsMoved) noexcept
    {
        const double maxBoost = juce::jmax (juce::jmax (1.0, knobL.target, knobM.target),
                                            juce::jmax (knobH.target, knobL.cur, knobM.cur), knobH.cur);
        capFactor = maxBoost * juce::Decibels::decibelsToGain (kCapHeadroomDb);

        const int need = neededMask();
        for (int e = 0; e < 4; ++e)
        {
            if ((need & (1 << e)) == 0) { validMask &= ~(1 << e); continue; }
            const bool wasValid = (validMask & (1 << e)) != 0;
            if (wasValid && ! knobsMoved && ! filtersChanged)
            {
                cornerPrev[(size_t) e] = cornerNow[(size_t) e];
                continue;
            }
            const double gl = knobL.cur * ((e & 1) ? kKillGain : 1.0);
            const double gm = knobM.cur * ((e & 2) ? kKillGain : 1.0);
            cornerPrev[(size_t) e] = cornerNow[(size_t) e];
            residues (gl, gm, knobH.cur, cornerNow[(size_t) e]);
            if (! wasValid || filtersChanged) cornerPrev[(size_t) e] = cornerNow[(size_t) e];
            validMask |= (1 << e);
        }
        filtersChanged = false;
    }

    // Gesamtgewichte = Kill-Überblendung der Ecken; t = Position im Abschnitt (Ecken linear Anfang -> Ende)
    void blend (double aL, double aM, double t, Mix& out) const noexcept
    {
        const double w[4] = { (1.0 - aL) * (1.0 - aM), aL * (1.0 - aM), (1.0 - aL) * aM, aL * aM };
        out.d0 = 0.0;
        out.bp.fill (0.0);
        out.lp.fill (0.0);
        for (size_t e = 0; e < 4; ++e)
        {
            if (w[e] == 0.0 || (validMask & (1 << e)) == 0) continue;
            const Mix& p0 = cornerPrev[e];
            const Mix& p1 = cornerNow[e];
            const double u = w[e] * (1.0 - t), v = w[e] * t;
            out.d0 += u * p0.d0 + v * p1.d0;
            for (size_t p = 0; p < (size_t) kPoles; ++p)
            {
                out.bp[p] += u * p0.bp[p] + v * p1.bp[p];
                out.lp[p] += u * p0.lp[p] + v * p1.lp[p];
            }
        }
    }

    // Partialbruch-Gewichte für feste Bandfaktoren (keine Allokation)
    void residues (double gl, double gm, double gh, Mix& mx) const noexcept
    {
        using C = std::complex<double>;
        gl = juce::jmax (gl, kKillGain);
        gm = juce::jmax (gm, kKillGain);
        gh = juce::jmax (gh, kKillGain);

        if (gl == 1.0 && gm == 1.0 && gh == 1.0)
        {
            mx.d0 = 1.0;
            mx.bp.fill (0.0);
            mx.lp.fill (0.0);
            return;
        }

        const double r = std::pow (gl / gm, 1.0 / kOrder);
        const double q = std::pow (gh / gm, 1.0 / kOrder);
        // Zähler je Pol-Paar (Koeffizienten von s^2, s, 1) im normierten BLT-Bereich (Polradius = tan)
        std::array<double, kPoles> a2, a1, a0, gg, RR;
        for (int k = 0; k < kSections; ++k)
        {
            const double R = sectionR (k + 1);
            gg[(size_t) k] = g1; RR[(size_t) k] = R;
            a2[(size_t) k] = 1.0; a1[(size_t) k] = 2.0 * R * r * g1; a0[(size_t) k] = r * r * g1 * g1;
            const size_t j = (size_t) (k + kSections);
            gg[j] = g2; RR[j] = R;
            a2[j] = q * q; a1[j] = 2.0 * R * q * g2; a0[j] = g2 * g2;
        }

        mx.d0 = gm * std::pow (q, (double) (2 * kSections));   // = gh (Hochfrequenz-Grenzwert)
        for (int i = 0; i < kPoles; ++i)
        {
            const double g = gg[(size_t) i], R = RR[(size_t) i];
            const C p (-R * g, g * std::sqrt (1.0 - R * R));
            C num (gm, 0.0), den = p - std::conj (p);
            for (int j = 0; j < kPoles; ++j)
            {
                num *= a2[(size_t) j] * p * p + a1[(size_t) j] * p + a0[(size_t) j];
                if (j != i) den *= p * p + 2.0 * RR[(size_t) j] * gg[(size_t) j] * p + gg[(size_t) j] * gg[(size_t) j];
            }
            const C res = num / den;
            const double A = 2.0 * res.real(), B = -2.0 * (res * std::conj (p)).real();
            mx.bp[(size_t) i] = A / g;          // (A s + B) / D = (A/g) * bp + (B/g^2) * lp
            mx.lp[(size_t) i] = B / (g * g);
        }
    }

    void recoverFromFault() noexcept
    {
        for (auto& s : svf) s.reset();
        capGain = 1.0;
    }

    void setupFilters() noexcept
    {
        g1 = std::tan (juce::MathConstants<double>::pi * (curX1 / poleScale()) / fs);
        g2 = std::tan (juce::MathConstants<double>::pi * (curX2 / poleScale()) / fs);
        for (int k = 0; k < kSections; ++k)
        {
            svf[(size_t) k].setup (g1, sectionR (k + 1));
            svf[(size_t) (k + kSections)].setup (g2, sectionR (k + 1));
        }
    }

    void glideCrossovers() noexcept
    {
        if (curX1 == xTarget1 && curX2 == xTarget2) return;
        const double k = 0.35;
        curX1 *= std::pow (xTarget1 / curX1, k);
        curX2 *= std::pow (xTarget2 / curX2, k);
        if (std::abs (std::log (curX1 / xTarget1)) < 1.0e-3) curX1 = xTarget1;
        if (std::abs (std::log (curX2 / xTarget2)) < 1.0e-3) curX2 = xTarget2;
        setupFilters();
        mixDirty = true;
        filtersChanged = true;
    }

    double fs = 48000.0, g1 = 0.01, g2 = 0.1;
    int knobSamples = 720, killSamples = 240;
    std::array<Svf, kPoles> svf;
    Ramp knobL, knobM, knobH;
    Ramp killL { 0.0, 0.0, 0.0 }, killM { 0.0, 0.0, 0.0 };   // Überblendanteil 0 = offen, 1 = gekillt
    std::array<Mix, 4> cornerPrev, cornerNow;
    Mix mixStatic, mixTemp;
    int validMask = 0;
    bool filtersChanged = true;
    double xTarget1 = kDefaultXLow, xTarget2 = kDefaultXHigh, curX1 = -1.0, curX2 = -1.0;
    double env = 0.0, envRelease = 0.0, capGain = 1.0, capRelease = 0.0, capFactor = 1.4125;
    int capHits = 0;
    double minCapGain = 1.0;
    bool unity = true, mixDirty = true, capEnabled = true;
};
} // namespace beq
