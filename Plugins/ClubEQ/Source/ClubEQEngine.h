#pragma once

// Club EQ: 3-Band-Mixer-EQ nach Vorbild Pioneer DJM-900NXS / NXS2.
//
// Aufbau wie ein Mixer-EQ mit Bandaufteilung:
//   Linkwitz-Riley 24 dB/Okt. bei f1 (LOW|MID) und f2 (MID|HI),
//   das LOW-Band läuft zusätzlich durch einen Allpass bei f2, damit alle drei Bänder phasengleich sind.
//   Summe bei 0 dB = Allpass: Frequenzgang exakt flach (0,000 dB), kein Gain, keine Färbung, null Latenz.
//
// Werte (DJM-900NXS-Datenblatt): jedes Band -26 dB … +6 dB, Messpunkte LOW 70 Hz, MID 1 kHz, HI 13 kHz.
// Die Trennfrequenzen liegen standardmäßig im geometrischen Mittel dieser Punkte (265 Hz, 3,6 kHz).
// Mittenraste wie am Mixer: ein kleiner Bereich um 12 Uhr ist exakt 0 dB.
// ISO-Modus (Extra, wie die Isolator-Kurve der neueren DJMs): ganz links = komplett weg (-inf).

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <complex>

namespace ceq
{
inline constexpr float kMinDb = -26.0f;   // DJM-900NXS: -26 dB
inline constexpr float kMaxDb = 6.0f;     // DJM-900NXS: +6 dB
inline constexpr float kDetent = 0.02f;   // halbe Breite der Mittenraste (Knopfweg 0…1)
inline constexpr double kDefaultXLow = 265.0;
inline constexpr double kDefaultXHigh = 3600.0;

// Die Nachbarbänder streuen bei 24 dB/Okt. etwas in das abgesenkte Band hinein. Damit am Messpunkt
// (70 Hz / 1 kHz / 13 kHz) ganz links wirklich -26 dB herauskommen wie im Datenblatt, liegt der interne
// Boden je Band etwas tiefer (mit den Standard-Trennfrequenzen ausgerechnet, im Test nachgemessen).
enum Band { bandLow = 0, bandMid = 1, bandHigh = 2 };
inline constexpr float kFloorDb[3] = { -26.84f, -27.92f, -26.33f };

// Knopfposition 0…1 (0,5 = 12 Uhr) -> Weg ab Mitte, mit Raste. Ergebnis -1…+1, 0 in der Raste.
inline float travel (float pos) noexcept
{
    const float d = pos - 0.5f;
    const float a = std::abs (d);
    if (a <= kDetent) return 0.0f;
    const float t = juce::jmin (1.0f, (a - kDetent) / (0.5f - kDetent));
    return d < 0.0f ? -t : t;
}

// Knopfposition -> linearer Faktor.
// Anheben: linear in dB bis +6. Absenken: Audio-Kurve wie ein Log-Poti, die meisten dB liegen im letzten Drittel.
// Nennwert in dB, wie er am Regler steht (= Pegel am Messpunkt des Bands)
inline float positionToDb (float pos, bool iso) noexcept
{
    const float t = travel (pos);
    if (t >= 0.0f) return kMaxDb * t;
    const float c = -t;   // 0…1 Richtung links
    if (iso)
    {
        const float g = std::pow (1.0f - c, 2.2f);   // bis ganz weg (-inf)
        return g <= 0.0f ? -std::numeric_limits<float>::infinity() : juce::Decibels::gainToDecibels (g, -1000.0f);
    }
    return kMinDb * std::pow (c, 1.5f);
}

// Interner Faktor für das Band (Absenkung auf den Messpunkt kalibriert)
inline float positionToGain (float pos, bool iso, int band = -1) noexcept
{
    const float t = travel (pos);
    if (t >= 0.0f) return juce::Decibels::decibelsToGain (kMaxDb * t);
    if (iso) return std::pow (1.0f + t, 2.2f);
    const float scale = band >= 0 ? kFloorDb[band] / kMinDb : 1.0f;
    return juce::Decibels::decibelsToGain (positionToDb (pos, false) * scale);
}

inline juce::String formatDb (float pos, bool iso)
{
    const float db = positionToDb (pos, iso);
    if (! std::isfinite (db) || db < -99.0f) return juce::String::fromUTF8 ("-\xe2\x88\x9e dB");
    if (std::abs (db) < 0.05f) return "0.0 dB";
    return (db > 0 ? "+" : "") + juce::String (db, 1) + " dB";
}

// Analytischer Frequenzgang (für die Kurvenanzeige)
inline float responseDb (double f, double f1, double f2, float gL, float gM, float gH) noexcept
{
    using C = std::complex<double>;
    auto lr = [] (double fr, double fc, C& lp, C& hp, C& ap)
    {
        const C s (0.0, fr / fc);
        const C d = s * s + std::sqrt (2.0) * s + 1.0;
        lp = 1.0 / (d * d);
        hp = (s * s * s * s) / (d * d);
        ap = (s * s - std::sqrt (2.0) * s + 1.0) / d;
    };
    C lp1, hp1, ap1, lp2, hp2, ap2;
    lr (f, f1, lp1, hp1, ap1);
    lr (f, f2, lp2, hp2, ap2);
    const C h = (double) gL * lp1 * ap2 + (double) gM * hp1 * lp2 + (double) gH * hp1 * hp2;
    return (float) (20.0 * std::log10 (juce::jmax (1.0e-9, std::abs (h))));
}

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        fs = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), (juce::uint32) juce::jmax (1, numChannels) };
        for (auto* f : { &split1, &split2, &allpass2 })
            f->prepare (spec);
        allpass2.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        curX1 = curX2 = -1.0;
        setCrossovers (xTarget1, xTarget2, true);

        const double rampSec = 0.015;   // 15 ms: klickfrei, trotzdem knackig für Drops
        for (auto* g : { &gLow, &gMid, &gHigh })
            g->reset (sampleRate, rampSec);
        gLow.setCurrentAndTargetValue (tLow);
        gMid.setCurrentAndTargetValue (tMid);
        gHigh.setCurrentAndTargetValue (tHigh);
        reset();
    }

    void reset()
    {
        for (auto* f : { &split1, &split2, &allpass2 })
            f->reset();
    }

    void setGains (float low, float mid, float high, bool snap = false)
    {
        tLow = low; tMid = mid; tHigh = high;
        gLow.setTargetValue (low); gMid.setTargetValue (mid); gHigh.setTargetValue (high);
        if (snap)
        {
            gLow.setCurrentAndTargetValue (low);
            gMid.setCurrentAndTargetValue (mid);
            gHigh.setCurrentAndTargetValue (high);
        }
    }

    void setCrossovers (double x1, double x2, bool snap = false)
    {
        const double nyq = fs * 0.45;
        x1 = juce::jlimit (40.0, juce::jmin (1000.0, nyq), x1);
        x2 = juce::jlimit (juce::jmax (x1 * 2.0, 1000.0), juce::jmax (1000.0, nyq), x2);
        xTarget1 = x1; xTarget2 = x2;
        if (snap || curX1 < 0.0)
            applyCrossovers (x1, x2);
    }

    // Verarbeitet in-place. Trennfrequenz-Änderungen gleiten pro Block (selten, nur beim Drehen der Expertenwerte).
    void process (float* const* ch, int numCh, int n) noexcept
    {
        if (std::abs (curX1 - xTarget1) > 1.0e-3 || std::abs (curX2 - xTarget2) > 1.0e-3)
        {
            const double k = 0.35;
            applyCrossovers (curX1 * std::pow (xTarget1 / curX1, k), curX2 * std::pow (xTarget2 / curX2, k));
            if (std::abs (std::log (curX1 / xTarget1)) < 1.0e-3 && std::abs (std::log (curX2 / xTarget2)) < 1.0e-3)
                applyCrossovers (xTarget1, xTarget2);
        }

        numCh = juce::jmin (numCh, 2);
        const bool ramp = gLow.isSmoothing() || gMid.isSmoothing() || gHigh.isSmoothing();

        if (! ramp)
        {
            const double l = gLow.getCurrentValue(), m = gMid.getCurrentValue(), h = gHigh.getCurrentValue();
            for (int c = 0; c < numCh; ++c)
            {
                float* x = ch[c];
                for (int i = 0; i < n; ++i)
                    x[i] = (float) tick (c, (double) x[i], l, m, h);
            }
        }
        else
        {
            for (int i = 0; i < n; ++i)
            {
                const double l = gLow.getNextValue(), m = gMid.getNextValue(), h = gHigh.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    ch[c][i] = (float) tick (c, (double) ch[c][i], l, m, h);
            }
        }

        for (auto* f : { &split1, &split2, &allpass2 })
            f->snapToZero();
    }

    double crossLow() const noexcept { return curX1; }
    double crossHigh() const noexcept { return curX2; }

private:
    inline double tick (int c, double x, double l, double m, double h) noexcept
    {
        double lo, rest, mid, hi;
        split1.processSample (c, x, lo, rest);      // LOW | (MID+HI), Summe = Allpass f1
        split2.processSample (c, rest, mid, hi);    // MID | HI,       Summe = Allpass f2
        lo = allpass2.processSample (c, lo);         // LOW phasengleich zu MID+HI
        return l * lo + m * mid + h * hi;
    }

    void applyCrossovers (double x1, double x2)
    {
        curX1 = x1; curX2 = x2;
        split1.setCutoffFrequency (x1);
        split2.setCutoffFrequency (x2);
        allpass2.setCutoffFrequency (x2);
    }

    double fs = 48000.0;
    juce::dsp::LinkwitzRileyFilter<double> split1, split2, allpass2;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> gLow, gMid, gHigh;
    float tLow = 1.0f, tMid = 1.0f, tHigh = 1.0f;
    double xTarget1 = kDefaultXLow, xTarget2 = kDefaultXHigh, curX1 = -1.0, curX2 = -1.0;
};
} // namespace ceq
