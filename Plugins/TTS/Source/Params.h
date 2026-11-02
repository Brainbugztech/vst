#pragma once

// Alle automatisierbaren Parameter von Brainbugz TTS

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace ttsparams
{
// MAIN
inline constexpr const char* pitch = "pitch";
inline constexpr const char* speed = "speed";
inline constexpr const char* volume = "volume";
inline constexpr const char* formant = "formant";
// GRAIN
inline constexpr const char* grOn = "grOn";
inline constexpr const char* grMix = "grMix";
inline constexpr const char* grSize = "grSize";
inline constexpr const char* grFlux = "grFlux";
inline constexpr const char* grDensity = "grDensity";
inline constexpr const char* grSpray = "grSpray";
inline constexpr const char* grPitch = "grPitch";
inline constexpr const char* grWidth = "grWidth";
inline constexpr const char* grReverse = "grReverse";
inline constexpr const char* grFreeze = "grFreeze";
// FX
inline constexpr const char* eqOn = "eqOn";
inline juce::String eqId (int band, const char* what) { return "eq" + juce::String (band + 1) + what; } // On, Type, Freq, Gain, Q
inline constexpr const char* dlOn = "dlOn";
inline constexpr const char* dlSync = "dlSync";
inline constexpr const char* dlDiv = "dlDiv";
inline constexpr const char* dlTime = "dlTime";
inline constexpr const char* dlFeedback = "dlFeedback";
inline constexpr const char* dlPing = "dlPing";
inline constexpr const char* dlLow = "dlLow";
inline constexpr const char* dlHigh = "dlHigh";
inline constexpr const char* dlMix = "dlMix";
inline constexpr const char* rvOn = "rvOn";
inline constexpr const char* rvSize = "rvSize";
inline constexpr const char* rvDamp = "rvDamp";
inline constexpr const char* rvPre = "rvPre";
inline constexpr const char* rvWidth = "rvWidth";
inline constexpr const char* rvLow = "rvLow";
inline constexpr const char* rvMix = "rvMix";

inline const juce::StringArray& eqTypeNames()
{
    static const juce::StringArray s { "Low Cut", "Low Shelf", "Bell", "High Shelf", "High Cut" };
    return s;
}
inline const juce::StringArray& delayDivNames()
{
    static const juce::StringArray s { "1/32", "1/16 T", "1/16", "1/16 D", "1/8 T", "1/8", "1/8 D", "1/4 T", "1/4", "1/4 D", "1/2", "1/2 D", "1/1" };
    return s;
}
inline double delayDivBeats (int i)
{
    static const double b[] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 3.0, 4.0 };
    return b[juce::jlimit (0, 12, i)];
}

// ---- Wertebereiche
inline float speedFrom01 (float n) { return n < 0.5f ? std::pow (10.0f, -3.0f + 6.0f * n) : std::pow (4.0f, (n - 0.5f) * 2.0f); }
inline float speedTo01 (float v)
{
    v = juce::jlimit (0.001f, 4.0f, v);
    return v < 1.0f ? (std::log10 (v) + 3.0f) / 6.0f : 0.5f + std::log (v) / std::log (4.0f) * 0.5f;
}

inline juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float a, float b, float n) { return a * std::pow (b / a, n); },
             [] (float a, float b, float v) { return std::log (juce::jlimit (a, b, v) / a) / std::log (b / a); },
             [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
}

inline juce::String hzText (float v)
{
    if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (juce::roundToInt (v)) + " Hz";
}
inline juce::String signedText (float v, int decimals, const char* unit)
{
    if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -decimals)) return juce::String (0.0f, decimals) + unit;
    return (v > 0 ? "+" : "") + juce::String (v, decimals) + unit;
}
inline juce::String msText (float v)
{
    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
    return juce::String (juce::roundToInt (v)) + " ms";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto attr = [] (std::function<String (float)> f)
    { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };
    auto pct = [] (float v) { return String (roundToInt (v)) + "%"; };
    auto addF = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, std::function<String (float)> f)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, attr (f))); };
    auto addB = [&] (const String& id, const String& name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto addC = [&] (const String& id, const String& name, const StringArray& items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    // MAIN
    addF (pitch, "Pitch", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, [] (float v) { return signedText (v, 2, " st"); });
    NormalisableRange<float> speedRange (0.001f, 4.0f,
        [] (float, float, float n) { return speedFrom01 (n); },
        [] (float, float, float v) { return speedTo01 (v); },
        [] (float, float, float v) { return jlimit (0.001f, 4.0f, v); });
    addF (speed, "Speed", speedRange, 1.0f, [] (float v) { return String (v, v < 0.1f ? 3 : 2) + " x"; });
    addF (volume, "Volume", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), 0.0f, [] (float v) { return signedText (v, 1, " dB"); });
    NormalisableRange<float> formantRange (0.5f, 2.0f,
        [] (float, float, float n) { return std::pow (2.0f, (n - 0.5f) * 2.0f); },
        [] (float, float, float v) { return 0.5f + std::log2 (jlimit (0.5f, 2.0f, v)) * 0.5f; },
        [] (float, float, float v) { return jlimit (0.5f, 2.0f, v); });
    addF (formant, "Formant", formantRange, 1.0f, [] (float v) { return String (v, 2) + " x"; });

    // GRAIN
    addB (grOn, "Grain On", false);
    addF (grMix, "Grain Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, pct);
    addF (grSize, "Grain Size", logRange (10.0f, 500.0f), 80.0f, msText);
    addF (grFlux, "Grain Flux", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct);
    addF (grDensity, "Grain Density", logRange (1.0f, 16.0f), 4.0f, [] (float v) { return String (v, 1) + " x"; });
    addF (grSpray, "Grain Spray", NormalisableRange<float> (0.0f, 1000.0f, 0.1f, 0.4f), 50.0f, msText);
    addF (grPitch, "Grain Pitch", NormalisableRange<float> (0.0f, 24.0f, 0.01f, 0.6f), 0.0f, [] (float v) { return "+/- " + String (v, 1) + " st"; });
    addF (grWidth, "Grain Width", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct);
    addF (grReverse, "Grain Reverse", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct);
    addB (grFreeze, "Grain Freeze", false);

    // EQ
    addB (eqOn, "EQ On", true);
    const float defFreq[] = { 150.0f, 1500.0f, 7000.0f };
    const int defType[] = { 1, 2, 3 };
    const float defQ[] = { 0.7f, 1.0f, 0.7f };
    for (int b = 0; b < 3; ++b)
    {
        const String n = "EQ " + String (b + 1) + " ";
        addB (eqId (b, "On"), n + "On", true);
        addC (eqId (b, "Type"), n + "Type", eqTypeNames(), defType[b]);
        addF (eqId (b, "Freq"), n + "Freq", logRange (20.0f, 20000.0f), defFreq[b], hzText);
        addF (eqId (b, "Gain"), n + "Gain", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, [] (float v) { return signedText (v, 1, " dB"); });
        addF (eqId (b, "Q"), n + "Q", logRange (0.3f, 10.0f), defQ[b], [] (float v) { return "Q " + String (v, 2); });
    }

    // Delay
    addB (dlOn, "Delay On", false);
    addB (dlSync, "Delay Sync", true);
    addC (dlDiv, "Delay Time Sync", delayDivNames(), 6);
    addF (dlTime, "Delay Time", logRange (1.0f, 2000.0f), 375.0f, msText);
    addF (dlFeedback, "Delay Feedback", NormalisableRange<float> (0.0f, 95.0f, 0.1f), 35.0f, pct);
    addB (dlPing, "Delay Ping-Pong", true);
    addF (dlLow, "Delay Low Cut", logRange (20.0f, 2000.0f), 150.0f, hzText);
    addF (dlHigh, "Delay High Cut", logRange (1000.0f, 20000.0f), 8000.0f, hzText);
    addF (dlMix, "Delay Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, pct);

    // Reverb
    addB (rvOn, "Reverb On", true);
    addF (rvSize, "Reverb Size", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 55.0f, pct);
    addF (rvDamp, "Reverb Damping", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 45.0f, pct);
    addF (rvPre, "Reverb Pre-Delay", NormalisableRange<float> (0.0f, 250.0f, 0.1f), 20.0f, msText);
    addF (rvWidth, "Reverb Width", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct);
    addF (rvLow, "Reverb Low Cut", logRange (20.0f, 1000.0f), 200.0f, hzText);
    addF (rvMix, "Reverb Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 15.0f, pct);

    return { p.begin(), p.end() };
}
} // namespace ttsparams
