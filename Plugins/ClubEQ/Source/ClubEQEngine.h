#pragma once

// Club EQ: 3-Band-Mixer-EQ nach Vorbild Pioneer DJM-900NXS / NXS2.
//
// Aufbau: Gesamtpegel = MID, davor ein steiler Tiefen-Shelf (LOW relativ zu MID) bei f1
// und ein steiler Höhen-Shelf (HI relativ zu MID) bei f2.
//   y = gMid * LowShelf(gLow / gMid, f1) * HighShelf(gHigh / gMid, f2) * x
// Die Shelves sind Butterworth-Shelves 4. Ordnung (24 dB/Okt. Übergang, ohne Überschwinger).
// Steht ein Shelf auf 1, sind Nullstellen und Pole identisch: der Filter ist exakt die Identität.
// Darum gilt bei 0 dB: Ausgang == Eingang, bitgenau, auch keine Phasendrehung, keine höheren Spitzen.
// (Eine klassische Linkwitz-Riley-Bandaufteilung summiert sich nur zu einem Allpass: Frequenzgang flach,
//  aber die Phase dreht sich, und bei limitierten Tracks steigen die Spitzen um mehrere dB.)
//
// Werte (DJM-900NXS-Datenblatt): jedes Band -26 dB … +6 dB, Messpunkte LOW 70 Hz, MID 1 kHz, HI 13 kHz.
// Die Trennfrequenzen liegen standardmäßig im geometrischen Mittel dieser Punkte (265 Hz, 3,6 kHz),
// definiert als Punkt, an dem der Übergang auf halbem Weg ist (bei Kill -6 dB wie bei einer Weiche).
// Mittenraste wie am Mixer: ein kleiner Bereich um 12 Uhr ist exakt 0 dB.
// ISO-Modus (Extra, wie die Isolator-Kurve der neueren DJMs): ganz links = komplett weg (-100 dB).

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <complex>

namespace ceq
{
inline constexpr float kMinDb = -26.0f;   // DJM-900NXS: -26 dB
inline constexpr float kMaxDb = 6.0f;     // DJM-900NXS: +6 dB
inline constexpr float kDetent = 0.02f;   // halbe Breite der Mittenraste (Knopfweg 0…1)
inline constexpr double kKillGain = 1.0e-5;   // ISO ganz links: -100 dB
inline constexpr double kDefaultXLow = 265.0;
inline constexpr double kDefaultXHigh = 3600.0;
inline constexpr int kOrder = 4;              // Butterworth-Ordnung der Shelves
inline constexpr int kSections = kOrder / 2;

enum Band { bandLow = 0, bandMid = 1, bandHigh = 2 };

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
// Anheben: linear in dB bis +6. Absenken: Audio-Kurve wie ein Log-Poti, die meisten dB liegen im letzten Drittel.
inline float positionToDb (float pos, bool iso) noexcept
{
    const float t = travel (pos);
    if (t >= 0.0f) return kMaxDb * t;
    const float c = -t;   // 0…1 Richtung links
    if (iso)
    {
        const float g = std::pow (1.0f - c, 2.2f);   // bis ganz weg
        return g <= 0.0f ? -std::numeric_limits<float>::infinity() : juce::Decibels::gainToDecibels (g, -1000.0f);
    }
    return kMinDb * std::pow (c, 1.5f);
}

// Knopfposition -> linearer Faktor (in der Raste exakt 1.0)
inline float positionToGain (float pos, bool iso, int = -1) noexcept
{
    const float t = travel (pos);
    if (t == 0.0f) return 1.0f;
    if (t > 0.0f) return juce::Decibels::decibelsToGain (kMaxDb * t);
    if (iso) return std::pow (1.0f + t, 2.2f);
    return juce::Decibels::decibelsToGain (positionToDb (pos, false));
}

inline juce::String formatDb (float pos, bool iso)
{
    const float db = positionToDb (pos, iso);
    if (! std::isfinite (db) || db < -99.0f) return juce::String::fromUTF8 ("-\xe2\x88\x9e dB");
    if (std::abs (db) < 0.05f) return "0.0 dB";
    return (db > 0 ? "+" : "") + juce::String (db, 1) + " dB";
}

// Frequenzskalierung des Shelfs: an der Trennfrequenz steht der Betrag genau auf halbem Weg (1+G)/2.
inline double shelfScale (double G) noexcept
{
    if (std::abs (G - 1.0) < 1.0e-9) return 1.0;
    const double A = 0.5 * (1.0 + G);
    const double x = (A * A - G * G) / (1.0 - A * A);   // = Omega^(2N) am Trennpunkt
    return std::pow (x, 1.0 / (2.0 * kOrder));
}

inline double sectionSigma (int m) noexcept   // sin((2m-1) pi / 2N), m = 1…N/2
{
    return std::sin ((2.0 * m - 1.0) * juce::MathConstants<double>::pi / (2.0 * kOrder));
}

// Analytischer Frequenzgang (für die Kurvenanzeige): Tiefen-Shelf G = gL/gM bei f1, Höhen-Shelf bei f2.
inline float responseDb (double f, double f1, double f2, float gL, float gM, float gH) noexcept
{
    using C = std::complex<double>;
    const double m = juce::jmax ((double) gM, kKillGain);
    auto lowShelf = [] (double fr, double fc, double G)
    {
        const double r = std::pow (G, 1.0 / kOrder);
        const C s (0.0, fr / fc * shelfScale (G));
        C h (1.0, 0.0);
        for (int k = 1; k <= kSections; ++k)
        {
            const double sg = sectionSigma (k);
            h *= (s * s + 2.0 * sg * r * s + r * r) / (s * s + 2.0 * sg * s + 1.0);
        }
        return h;
    };
    const double gl = juce::jmax ((double) gL, kKillGain) / m, gh = juce::jmax ((double) gH, kKillGain) / m;
    const C h = m * lowShelf (f, f1, gl) * (gh * lowShelf (f, f2, 1.0 / gh));
    return (float) (20.0 * std::log10 (juce::jmax (1.0e-9, std::abs (h))));
}

// Biquad (transponierte Direktform II, double), Koeffizienten werden blockweise nachgeführt
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1[2] {}, s2[2] {};

    inline double tick (int c, double x) noexcept
    {
        const double y = b0 * x + s1[c];
        s1[c] = b1 * x - a1 * y + s2[c];
        s2[c] = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { s1[0] = s1[1] = s2[0] = s2[1] = 0.0; }
    void snap() noexcept
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
    void prepare (double sampleRate, int, int)
    {
        fs = sampleRate;
        const double rampSec = 0.015;   // 15 ms: klickfrei, trotzdem knackig für Drops
        for (auto* g : { &gLow, &gMid, &gHigh })
            g->reset (sampleRate, rampSec);
        gLow.setCurrentAndTargetValue (tLow);
        gMid.setCurrentAndTargetValue (tMid);
        gHigh.setCurrentAndTargetValue (tHigh);
        curX1 = curX2 = -1.0;
        setCrossovers (xTarget1, xTarget2, true);
        updateCoefficients();
        reset();
        dirty = false;
        unity = atUnityTarget();
    }

    void reset() noexcept
    {
        for (auto& b : low) b.reset();
        for (auto& b : high) b.reset();
    }

    void setGains (float lowG, float midG, float highG, bool snap = false)
    {
        tLow = lowG; tMid = midG; tHigh = highG;
        gLow.setTargetValue (lowG); gMid.setTargetValue (midG); gHigh.setTargetValue (highG);
        if (snap)
        {
            gLow.setCurrentAndTargetValue (lowG);
            gMid.setCurrentAndTargetValue (midG);
            gHigh.setCurrentAndTargetValue (highG);
        }
    }

    void setCrossovers (double x1, double x2, bool snap = false)
    {
        const double nyq = fs * 0.45;
        x1 = juce::jlimit (40.0, juce::jmin (1000.0, nyq), x1);
        x2 = juce::jlimit (juce::jmax (x1 * 2.0, 1000.0), juce::jmax (1000.0, nyq), x2);
        xTarget1 = x1; xTarget2 = x2;
        if (snap || curX1 < 0.0)
        {
            curX1 = x1; curX2 = x2;
            tan1 = std::tan (juce::MathConstants<double>::pi * curX1 / fs);
            tan2 = std::tan (juce::MathConstants<double>::pi * curX2 / fs);
        }
    }

    // Verarbeitet in-place. Während Regler gleiten, werden die Koeffizienten alle 16 Samples nachgeführt.
    void process (float* const* ch, int numCh, int n) noexcept
    {
        numCh = juce::jmin (numCh, 2);
        glideCrossovers();

        constexpr int sub = 16;
        for (int start = 0; start < n; start += sub)
        {
            const int len = juce::jmin (sub, n - start);
            const bool ramp = gLow.isSmoothing() || gMid.isSmoothing() || gHigh.isSmoothing();
            if (ramp || dirty)
            {
                gLow.skip (len); gMid.skip (len); gHigh.skip (len);
                updateCoefficients();
                dirty = false;
            }

            if (unity && ! ramp)
                continue;   // alles auf 0 dB und Filter leer: Signal bleibt bitgenau unberührt

            for (int c = 0; c < numCh; ++c)
            {
                float* x = ch[c] + start;
                for (int i = 0; i < len; ++i)
                {
                    double y = (double) x[i];
                    for (auto& b : low) y = b.tick (c, y);
                    for (auto& b : high) y = b.tick (c, y);
                    x[i] = (float) (y * outGain);
                }
            }
        }

        for (auto& b : low) b.snap();
        for (auto& b : high) b.snap();
        if (atUnityTarget())
            unity = filtersEmpty();
        else
            unity = false;
    }

    double crossLow() const noexcept { return curX1; }
    double crossHigh() const noexcept { return curX2; }

private:
    bool atUnityTarget() const noexcept
    {
        return ! gLow.isSmoothing() && ! gMid.isSmoothing() && ! gHigh.isSmoothing()
            && gLow.getCurrentValue() == 1.0 && gMid.getCurrentValue() == 1.0 && gHigh.getCurrentValue() == 1.0
            && curX1 == xTarget1 && curX2 == xTarget2;
    }

    bool filtersEmpty() const noexcept
    {
        // Bei Shelf = 1 ist der Filter die Identität. Restzustände nach dem Zurückdrehen klingen ab;
        // sobald sie unter der float-Auflösung liegen, wird durchgereicht.
        for (auto* set : { &low, &high })
            for (auto& b : *set)
                for (int c = 0; c < 2; ++c)
                    if (std::abs (b.s1[c]) > 1.0e-12 || std::abs (b.s2[c]) > 1.0e-12)
                        return false;
        return true;
    }

    void glideCrossovers() noexcept
    {
        if (curX1 == xTarget1 && curX2 == xTarget2) return;
        const double k = 0.35;
        curX1 *= std::pow (xTarget1 / curX1, k);
        curX2 *= std::pow (xTarget2 / curX2, k);
        if (std::abs (std::log (curX1 / xTarget1)) < 1.0e-3) curX1 = xTarget1;
        if (std::abs (std::log (curX2 / xTarget2)) < 1.0e-3) curX2 = xTarget2;
        tan1 = std::tan (juce::MathConstants<double>::pi * curX1 / fs);
        tan2 = std::tan (juce::MathConstants<double>::pi * curX2 / fs);
        dirty = true;
    }

    // Tiefen-Shelf mit Faktor G bei tanFc, per Bilinear-Transformation (Trennfrequenz vorverzerrt)
    static void designLowShelf (std::array<Biquad, kSections>& out, double G, double tanFc, double extraGain) noexcept
    {
        const double r = std::pow (G, 1.0 / kOrder);
        const double K = tanFc / shelfScale (G);
        for (int m = 0; m < kSections; ++m)
        {
            const double sg = sectionSigma (m + 1);
            const double a0 = 1.0 + 2.0 * sg * K + K * K;
            const double a1 = -2.0 + 2.0 * K * K;
            const double a2 = 1.0 - 2.0 * sg * K + K * K;
            const double rK = r * K;
            const double b0 = 1.0 + 2.0 * sg * rK + rK * rK;
            const double b1 = -2.0 + 2.0 * rK * rK;
            const double b2 = 1.0 - 2.0 * sg * rK + rK * rK;
            const double gain = m == 0 ? extraGain : 1.0;
            auto& q = out[(size_t) m];
            q.b0 = gain * b0 / a0; q.b1 = gain * b1 / a0; q.b2 = gain * b2 / a0;
            q.a1 = a1 / a0;        q.a2 = a2 / a0;
        }
    }

    void updateCoefficients() noexcept
    {
        const double m = juce::jmax (gMid.getCurrentValue(), kKillGain);
        const double gl = juce::jmax (gLow.getCurrentValue(), kKillGain) / m;
        const double gh = juce::jmax (gHigh.getCurrentValue(), kKillGain) / m;
        designLowShelf (low, gl, tan1, 1.0);
        designLowShelf (high, 1.0 / gh, tan2, gh);   // Höhen-Shelf = gh * Tiefen-Shelf(1/gh)
        outGain = m;
    }

    double fs = 48000.0;
    std::array<Biquad, kSections> low, high;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> gLow, gMid, gHigh;
    float tLow = 1.0f, tMid = 1.0f, tHigh = 1.0f;
    double xTarget1 = kDefaultXLow, xTarget2 = kDefaultXHigh, curX1 = -1.0, curX2 = -1.0;
    double tan1 = 0.0, tan2 = 0.0, outGain = 1.0;
    bool dirty = true, unity = true;
};
} // namespace ceq
