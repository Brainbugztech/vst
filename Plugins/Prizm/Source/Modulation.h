#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace prizm
{
// Modulation: zwei LFOs und ein Envelope Follower, die per Slot (Quelle -> Ziel, Tiefe)
// auf die stufenlosen Regler wirken. Die Modulation liegt obendrauf, der Regler selbst
// (und die DAW-Automation) bleibt unverändert.

inline constexpr int kNumModSlots = 8;
inline constexpr int kNumModTargets = 11;

enum ModSource { srcOff = 0, srcLfo1 = 1, srcLfo2 = 2, srcEnv = 3 };

// Reihenfolge = Reihenfolge der Regler im Editor
inline constexpr std::array<const char*, kNumModTargets> kModTargetIds {
    "strength", "color", "clear", "mix", "output", "rangeLow", "rangeHigh", "gate", "shift", "formant", "tune"
};
inline constexpr std::array<const char*, kNumModTargets> kModTargetNames {
    "Strength", "Color", "Clear", "Mix", "Output", "Range Low", "Range High", "Gate", "Shift", "Formant", "Tune"
};

inline constexpr std::array<const char*, 8> kLfoShapeNames { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Random", "Custom" };
inline constexpr int kShapeCustom = 7;

// Lauf-Modus der LFOs (wie in Serum)
enum LfoMode { lfoFree = 0, lfoRetrig = 1, lfoEnvelope = 2 };
inline constexpr std::array<const char*, 3> kLfoModeNames { "Free", "Retrig", "Envelope" };
// Was Retrig/Envelope auslöst (gilt für beide LFOs)
enum TrigSource { trigAudio = 0, trigMidi = 1, trigBoth = 2 };
inline constexpr std::array<const char*, 3> kTrigSourceNames { "Audio", "MIDI", "Audio + MIDI" };

// Gezeichnete LFO-Kurve: Punkte (x 0..1, y -1..1), jedes Segment mit eigener Biegung (-1..1).
// Bearbeitet wird auf dem Message-Thread, der Audio-Thread liest eine fertige Tabelle
// (doppelt gepuffert, Umschalten über einen atomaren Index).
class LfoCurve
{
public:
    struct Point
    {
        float x = 0.0f, y = 0.0f;
        float curve = 0.0f; // Biegung des Segments ZUM NÄCHSTEN Punkt
    };

    static constexpr int kTableSize = 512;

    LfoCurve() { setPoints (defaultPoints()); }

    static std::vector<Point> defaultPoints()
    {
        return { { 0.0f, -1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
    }

    // Biegung: 0 = gerade, > 0 = langsam anfangen, < 0 = schnell anfangen
    static float bend (float t, float curve)
    {
        if (std::abs (curve) < 0.001f)
            return t;
        const float a = curve * 7.0f;
        return (std::exp (a * t) - 1.0f) / (std::exp (a) - 1.0f);
    }

    const std::vector<Point>& getPoints() const noexcept { return points; }

    void setPoints (std::vector<Point> newPoints)
    {
        if (newPoints.size() < 2)
            newPoints = defaultPoints();
        std::sort (newPoints.begin(), newPoints.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });
        newPoints.front().x = 0.0f;
        newPoints.back().x = 1.0f;
        for (auto& p : newPoints)
        {
            p.x = juce::jlimit (0.0f, 1.0f, p.x);
            p.y = juce::jlimit (-1.0f, 1.0f, p.y);
            p.curve = juce::jlimit (-1.0f, 1.0f, p.curve);
        }
        points = std::move (newPoints);
        rebuild();
    }

    // Wert bei Phase 0..1 direkt aus den Punkten (für die Anzeige)
    float evaluatePoints (float x) const { return evaluate (points, x); }

    static float evaluate (const std::vector<Point>& points, float x)
    {
        if (points.empty())
            return 0.0f;
        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            const auto& a = points[i];
            const auto& b = points[i + 1];
            if (x <= b.x || i + 2 == points.size())
            {
                const float w = b.x - a.x;
                const float t = w > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, (x - a.x) / w) : 1.0f;
                return a.y + (b.y - a.y) * bend (t, a.curve);
            }
        }
        return points.back().y;
    }

    // Audio-Thread
    float evaluate (double phase) const noexcept
    {
        const auto& table = tables[(size_t) active.load (std::memory_order_acquire)];
        const double pos = juce::jlimit (0.0, 1.0, phase) * (kTableSize - 1);
        const int i = juce::jmin ((int) pos, kTableSize - 2);
        const float frac = (float) (pos - i);
        return table[(size_t) i] + (table[(size_t) i + 1] - table[(size_t) i]) * frac;
    }

    // Speichern als Text: "x,y,c;x,y,c;..."
    juce::String toString() const
    {
        juce::StringArray parts;
        for (const auto& p : points)
            parts.add (juce::String (p.x, 4) + "," + juce::String (p.y, 4) + "," + juce::String (p.curve, 4));
        return parts.joinIntoString (";");
    }

    static std::vector<Point> parse (const juce::String& text)
    {
        std::vector<Point> result;
        for (const auto& part : juce::StringArray::fromTokens (text, ";", {}))
        {
            const auto v = juce::StringArray::fromTokens (part, ",", {});
            if (v.size() >= 2)
                result.push_back ({ v[0].getFloatValue(), v[1].getFloatValue(), v.size() > 2 ? v[2].getFloatValue() : 0.0f });
        }
        return result;
    }

    // Standardform als Punkte, damit man sie weiterzeichnen kann
    static std::vector<Point> pointsForShape (int shape)
    {
        switch (shape)
        {
            case 0: return { { 0.0f, 0.0f, -0.32f }, { 0.25f, 1.0f, 0.32f }, { 0.5f, 0.0f, -0.32f }, { 0.75f, -1.0f, 0.32f }, { 1.0f, 0.0f, 0.0f } };
            case 2: return { { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
            case 3: return { { 0.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
            case 4: return { { 0.0f, 1.0f, 0.0f }, { 0.499f, 1.0f, 0.0f }, { 0.501f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
            default: return defaultPoints();
        }
    }

private:
    void rebuild()
    {
        const int target = 1 - active.load (std::memory_order_relaxed);
        auto& table = tables[(size_t) target];
        for (int i = 0; i < kTableSize; ++i)
            table[(size_t) i] = evaluatePoints ((float) i / (float) (kTableSize - 1));
        active.store (target, std::memory_order_release);
    }

    std::vector<Point> points;
    std::array<std::array<float, kTableSize>, 2> tables {};
    std::atomic<int> active { 0 };
};

struct SyncDivision
{
    const char* name;
    double beats; // Länge eines LFO-Zyklus in Vierteln
};
inline constexpr std::array<SyncDivision, 13> kSyncDivisions {{
    { "4 Bars", 16.0 }, { "2 Bars", 8.0 }, { "1 Bar", 4.0 }, { "1/2", 2.0 },
    { "1/4.", 1.5 }, { "1/4", 1.0 }, { "1/4T", 2.0 / 3.0 },
    { "1/8.", 0.75 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 },
    { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 },
}};

struct Transport
{
    bool playing = false;
    bool hasPpq = false;
    double ppq = 0.0;   // Position in Vierteln am Anfang des Abschnitts
    double bpm = 120.0;
};

struct LfoSettings
{
    int shape = 0;
    bool sync = true;
    float rateHz = 1.0f;
    int division = 5; // 1/4
    int mode = lfoFree;
};

// Wert einer LFO-Form bei Phase 0..1, Ergebnis -1..1
inline float lfoShape (int shape, double phase, float rndPrev, float rndNext)
{
    const float p = (float) phase;
    switch (shape)
    {
        case 0: return std::sin (juce::MathConstants<float>::twoPi * p);
        case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);            // Dreieck, beginnt unten
        case 2: return 2.0f * p - 1.0f;                               // Säge auf
        case 3: return 1.0f - 2.0f * p;                               // Säge ab
        case 4: return p < 0.5f ? 1.0f : -1.0f;                       // Rechteck
        case 5: return rndNext;                                       // Zufall stufig
        case 6:                                                       // Zufall weich
        {
            const float t = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * p);
            return rndPrev + (rndNext - rndPrev) * t;
        }
        default: return 0.0f;
    }
}

class Lfo
{
public:
    explicit Lfo (int64_t seed) : rng (seed) { reset(); }

    void reset()
    {
        phase = 0.0;
        cycle = 0;
        finished = false;
        rndPrev = rng.nextFloat() * 2.0f - 1.0f;
        rndNext = rng.nextFloat() * 2.0f - 1.0f;
    }

    // Wert für einen Abschnitt von numSamples ab der aktuellen Position, danach weiterlaufen.
    // trigger: Anschlag/Note in diesem Abschnitt (wirkt bei Retrig und Envelope).
    float process (const LfoSettings& s, int numSamples, double sampleRate, const Transport& t, const LfoCurve* curve = nullptr,
                   bool trigger = false)
    {
        const double beatsPerCycle = kSyncDivisions[(size_t) juce::jlimit (0, (int) kSyncDivisions.size() - 1, s.division)].beats;

        if (s.mode != lfoEnvelope)
            finished = false;
        if (trigger && s.mode != lfoFree)
        {
            phase = 0.0;
            finished = false;
            nextRandom (cycle + 1);
        }

        // Nur Free läuft im Songtakt fest an der Songposition, Retrig/Envelope starten beim Trigger
        const bool locked = s.mode == lfoFree && s.sync && t.playing && t.hasPpq;
        if (locked)
        {
            // fest am Takt der DAW: Phase direkt aus der Songposition
            const double cycles = t.ppq / beatsPerCycle;
            const auto newCycle = (int64_t) std::floor (cycles);
            if (newCycle != cycle)
                nextRandom (newCycle);
            phase = cycles - std::floor (cycles);
        }

        const float value = (s.shape == kShapeCustom && curve != nullptr) ? curve->evaluate (phase)
                                                                          : lfoShape (s.shape, phase, rndPrev, rndNext);

        if (! locked && ! finished)
        {
            const double hz = s.sync ? (t.bpm / 60.0) / beatsPerCycle : (double) s.rateHz;
            phase += hz * (double) numSamples / sampleRate;
            if (phase >= 1.0 && s.mode == lfoEnvelope)
            {
                // einmal durch, dann am Ende stehen bleiben
                phase = 1.0;
                finished = true;
            }
            else if (phase >= 1.0)
            {
                const auto wraps = (int64_t) std::floor (phase);
                phase -= (double) wraps;
                nextRandom (cycle + wraps);
            }
        }
        lastValue = value;
        return value;
    }

    double getPhase() const noexcept { return phase; }
    bool isFinished() const noexcept { return finished; }
    float getValue() const noexcept { return lastValue; }

private:
    void nextRandom (int64_t newCycle)
    {
        cycle = newCycle;
        rndPrev = rndNext;
        rndNext = rng.nextFloat() * 2.0f - 1.0f;
    }

    juce::Random rng;
    double phase = 0.0;
    int64_t cycle = 0;
    float rndPrev = 0.0f, rndNext = 0.0f, lastValue = 0.0f;
    bool finished = false;
};

// Anschlags-Erkennung für Retrig/Envelope: schnelle Hüllkurve springt deutlich über die langsame
class OnsetDetector
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        fast = slow = 0.0f;
        holdoff = 0;
    }

    bool process (const float* const* channels, int numChannels, int numSamples)
    {
        const float fastRel = 1.0f - std::exp (-1.0f / ((float) fs * 0.005f));
        const float slowAtt = 1.0f - std::exp (-1.0f / ((float) fs * 0.030f));
        const float slowRel = 1.0f - std::exp (-1.0f / ((float) fs * 0.200f));
        bool hit = false;
        for (int n = 0; n < numSamples; ++n)
        {
            float x = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                x = juce::jmax (x, std::abs (channels[ch][n]));
            fast = x > fast ? x : fast + (x - fast) * fastRel;
            slow += (fast - slow) * (fast > slow ? slowAtt : slowRel);
            if (holdoff > 0)
                --holdoff;
            else if (fast > 0.003f && fast > slow * 2.0f + 1.0e-4f) // über -50 dB und gut 6 dB über dem Schnitt
            {
                hit = true;
                holdoff = (int) (fs * 0.07); // höchstens ein Anschlag pro 70 ms
            }
        }
        return hit;
    }

private:
    double fs = 48000.0;
    float fast = 0.0f, slow = 0.0f;
    int holdoff = 0;
};

// Envelope Follower: Lautstärke des Eingangs als 0..1 (linear in dB, -60 dB .. 0 dB, plus Empfindlichkeit)
class EnvFollower
{
public:
    void prepare (double sampleRate) { fs = sampleRate; peak = target = value = 0.0f; }

    float process (const float* const* channels, int numChannels, int numSamples, float attackMs, float releaseMs, float sensitivity01)
    {
        // 1) schneller Spitzenwert (1 ms hoch, 15 ms runter) -> dB -> 0..1
        // 2) Attack/Release wirken auf diesen 0..1-Wert, so bedeuten die Zeiten das, was man hört
        const float peakAtt = 1.0f - std::exp (-1.0f / ((float) fs * 0.001f));
        const float peakRel = 1.0f - std::exp (-1.0f / ((float) fs * 0.015f));
        const float att = 1.0f - std::exp (-1.0f / ((float) fs * juce::jmax (0.1f, attackMs) * 0.001f));
        const float rel = 1.0f - std::exp (-1.0f / ((float) fs * juce::jmax (1.0f, releaseMs) * 0.001f));
        const float offset = 60.0f + sensitivity01 * 36.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            float x = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                x = juce::jmax (x, std::abs (channels[ch][n]));
            peak += (x - peak) * (x > peak ? peakAtt : peakRel);
            if ((n & 7) == 0) // dB nur alle 8 Samples neu
                target = juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (peak + 1.0e-6f) + offset) / 60.0f);
            value += (target - value) * (target > value ? att : rel);
        }
        return value;
    }

    float getValue() const noexcept { return value; }

private:
    double fs = 48000.0;
    float peak = 0.0f, target = 0.0f, value = 0.0f;
};
} // namespace prizm
