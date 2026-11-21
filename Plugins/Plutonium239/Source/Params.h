#pragma once

// Plutonium239: alle Parameter-IDs, Auswahllisten und das Parameter-Layout
// Aufbau wie das Reaktor-Original: 6 Modulatoren (M1.1-M2.3), 3 Carrier (C1-C3),
// 3 Filter, Mixer, Sync-Matrix, MIDI/Master. Neu: Wavetable, mehr Filtertypen, 4 LFOs.

#include <juce_audio_processors/juce_audio_processors.h>
#include "PhaseDist.h"

namespace pluto
{
inline constexpr int numOsc = 9;
inline constexpr int numMod = 6;      // 0-5 Modulatoren, 6-8 Carrier
inline constexpr int numFilt = 3;
inline constexpr int numLfo = 4;

inline const juce::StringArray& oscNames()
{
    static const juce::StringArray n { "M1.1", "M1.2", "M1.3", "M2.1", "M2.2", "M2.3", "C1", "C2", "C3" };
    return n;
}

inline bool isCarrier (int o) { return o >= numMod; }

inline juce::String oid (int o, const char* name) { return "o" + juce::String (o) + "_" + name; }
inline juce::String fid (int f, const char* name) { return "f" + juce::String (f) + "_" + name; }
inline juce::String lid (int l, const char* name) { return "l" + juce::String (l) + "_" + name; }

// ---------- Auswahllisten ----------
inline const juce::StringArray& waveNames()   { static const juce::StringArray a { "Sin", "Tri", "Saw", "SQR", "WT" }; return a; }
inline const juce::StringArray& fmTypeNames() { static const juce::StringArray a { "+/-LinFM", "+LinFM", "+/-LogFM", "+LogFM", "+/-PM", "+PM", "+/-PD", "+PD" }; return a; }
inline const juce::StringArray& keyModeNames(){ static const juce::StringArray a { "Key", "Fix", "Half" }; return a; }
inline const juce::StringArray& filterDestNames() { static const juce::StringArray a { "Filter 1", "Filter 2", "Filter 3", "Off" }; return a; }

inline const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray a { "HP2", "HP4", "LP2", "LP4", "BP2", "BP4", "Notch", "Comb+", "Comb-", "Formant" };
    return a;
}
inline const juce::StringArray& filterLongNames()
{
    static const juce::StringArray a { "Highpass", "Highpass", "Lowpass", "Lowpass", "Bandpass", "Bandpass", "Notch", "Comb +", "Comb -", "Formant" };
    return a;
}
enum FilterType { HP2, HP4, LP2, LP4, BP2, BP4, Notch, CombPlus, CombMinus, Formant };

inline constexpr int userTableIndex = 8;
inline const juce::StringArray& wavetableNames()
{
    static const juce::StringArray a { "Basic", "PWM", "Harmonic", "Vowel", "Sync", "FM", "Hollow", "Grit", "User" };
    return a;
}

inline const juce::StringArray& lfoShapeNames()
{
    static const juce::StringArray a { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Rnd" };
    return a;
}

inline const juce::StringArray& lfoDivNames()
{
    static const juce::StringArray a { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                       "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" };
    return a;
}
inline double lfoDivBeats (int i)
{
    static const double b[] { 16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 1.5, 0.75, 0.375, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return b[juce::jlimit (0, 13, i)];
}

// Ziele/Quellen-Liste fuer Destination und Sync: alle anderen Oszillatoren + Off
inline juce::StringArray otherOscNames (int self)
{
    juce::StringArray a;
    for (int i = 0; i < numOsc; ++i)
        if (i != self) a.add (oscNames()[i]);
    a.add ("Off");
    return a;
}
// Index in der Liste -> Oszillator (-1 = Off)
inline int otherOscIndexToOsc (int self, int idx)
{
    if (idx < 0 || idx >= numOsc - 1) return -1;
    return idx < self ? idx : idx + 1;
}

// LFO-Ziele
enum TargetKind { tOff, tPitch, tFm, tPwm, tWtPos, tPd, tLevel, tCut, tRes, tMix, tPan, tMasterPitch, tMasterVol };
struct Target { TargetKind kind; int index; };

inline const std::vector<Target>& targets()
{
    static const std::vector<Target> t = []
    {
        std::vector<Target> v;
        v.push_back ({ tOff, 0 });
        for (int o = 0; o < numOsc; ++o) v.push_back ({ tPitch, o });
        for (int o = 0; o < numMod; ++o) v.push_back ({ tFm, o });
        for (int o = 0; o < numOsc; ++o) v.push_back ({ tWtPos, o });
        for (int o = 0; o < numOsc; ++o) v.push_back ({ tPd, o });
        for (int o = 0; o < numOsc; ++o) v.push_back ({ tPwm, o });
        for (int o = 0; o < numOsc; ++o) v.push_back ({ tLevel, o });
        for (int f = 0; f < numFilt; ++f) v.push_back ({ tCut, f });
        for (int f = 0; f < numFilt; ++f) v.push_back ({ tRes, f });
        for (int f = 0; f < numFilt; ++f) v.push_back ({ tMix, f });
        for (int f = 0; f < numFilt; ++f) v.push_back ({ tPan, f });
        v.push_back ({ tMasterPitch, 0 });
        v.push_back ({ tMasterVol, 0 });
        return v;
    }();
    return t;
}

inline const juce::StringArray& targetNames()
{
    static const juce::StringArray a = []
    {
        juce::StringArray s;
        for (auto& t : targets())
        {
            const auto on = oscNames()[t.index];
            const auto fn = "F" + juce::String (t.index + 1);
            switch (t.kind)
            {
                case tOff:         s.add ("Off"); break;
                case tPitch:       s.add (on + " Pitch"); break;
                case tFm:          s.add (on + " FM Amt"); break;
                case tPwm:         s.add (on + " PWM"); break;
                case tWtPos:       s.add (on + " WT Pos"); break;
                case tPd:          s.add (on + " PD Amt"); break;
                case tLevel:       s.add (on + " Level"); break;
                case tCut:         s.add (fn + " Cutoff"); break;
                case tRes:         s.add (fn + " Res"); break;
                case tMix:         s.add (fn + " D/W"); break;
                case tPan:         s.add (fn + " Pan"); break;
                case tMasterPitch: s.add ("Master Pitch"); break;
                case tMasterVol:   s.add ("Master Vol"); break;
            }
        }
        return s;
    }();
    return a;
}
inline int targetIndex (const juce::String& name) { return juce::jmax (0, targetNames().indexOf (name)); }

// ---------- Layout ----------
inline juce::NormalisableRange<float> msRange (float maxMs)
{
    juce::NormalisableRange<float> r (0.0f, maxMs);
    r.setSkewForCentre (maxMs * 0.04f);
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    // Anzeige-Formate: 0 = Zahl (2 Stellen), 1 = ms/s, 2 = Hz/kHz, 3 = Hz (Rate), 4 = Zahl (1 Stelle), 5 = Halbtoene
    auto addF = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, int fmt = 0)
    {
        auto toStr = [fmt] (float v, int) -> String
        {
            switch (fmt)
            {
                case 1: return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (v, v < 10.0f ? 1 : 0) + " ms";
                case 2: return v >= 1000.0f ? String (v / 1000.0f, 2) + " k" : String (v, v < 100.0f ? 1 : 0);
                case 3: return String (v, v < 1.0f ? 2 : 1) + " Hz";
                case 4: return String (v, 1);
                case 5: return String (v, 2);
                default: return String (v, 2);
            }
        };
        auto fromStr = [fmt] (const String& t) -> float
        {
            const float v = t.getFloatValue();
            const auto lt = t.toLowerCase();
            if (fmt == 1 && lt.trim().endsWith ("s") && ! lt.contains ("ms")) return v * 1000.0f;
            if (fmt == 2 && lt.contains ("k")) return v * 1000.0f;
            return v;
        };
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (toStr).withValueFromStringFunction (fromStr)));
    };
    auto addI = [&] (const String& id, const String& name, int lo, int hi, int def)
    { layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto addC = [&] (const String& id, const String& name, const StringArray& c, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };
    auto addB = [&] (const String& id, const String& name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    for (int o = 0; o < numOsc; ++o)
    {
        const auto n = oscNames()[o] + " ";
        addC (oid (o, "wave"), n + "Wave", waveNames(), o == 6 ? 2 : 0);
        addC (oid (o, "wt"), n + "Wavetable", wavetableNames(), 0);
        addI (oid (o, "oct"), n + "Oct", -4, 4, 0);
        addI (oid (o, "semi"), n + "Semi", -12, 12, 0);
        addF (oid (o, "cent"), n + "Cent", { -100.0f, 100.0f, 0.1f }, 0.0f, 4);
        addF (oid (o, "pwm"), n + "PWM", { 0.0f, 1.0f }, 0.0f);
        addF (oid (o, "wtpos"), n + "WT Position", { 0.0f, 1.0f }, 0.5f);
        addC (oid (o, "pdmode"), n + "PD Mode", pdModeNames(), 0);
        addF (oid (o, "pdamt"), n + "PD Amount", { 0.0f, 1.0f }, 0.5f);
        addF (oid (o, "fine"), n + "Fine (Cent)", { -100.0f, 100.0f, 0.01f }, 0.0f, 4);
        addF (oid (o, "coarse"), n + "Coarse (stufenlos)", { -48.0f, 48.0f, 0.0f }, 0.0f, 4);
        addF (oid (o, "a"), n + "Attack", msRange (10000.0f), 2.0f, 1);
        addF (oid (o, "d"), n + "Decay", msRange (10000.0f), 400.0f, 1);
        addF (oid (o, "s"), n + "Sustain", { 0.0f, 1.0f }, 1.0f);
        addF (oid (o, "r"), n + "Release", msRange (10000.0f), 150.0f, 1);
        addF (oid (o, "lvl"), n + "Level", { 0.0f, 1.0f }, isCarrier (o) ? 0.8f : 1.0f);
        addC (oid (o, "key"), n + "Key Mode", keyModeNames(), 0);
        addC (oid (o, "sync"), n + "Sync", otherOscNames (o), numOsc - 1);

        if (! isCarrier (o))
        {
            addC (oid (o, "dest"), n + "Destination", otherOscNames (o), numOsc - 1);
            addC (oid (o, "fmtype"), n + "FM Type", fmTypeNames(), 0);
            addF (oid (o, "fmamt"), n + "FM Amount", { 0.0f, 1.0f }, 0.3f);
            addF (oid (o, "fmfine"), n + "FM Fine", { -1.0f, 1.0f }, 0.0f);
            addF (oid (o, "fb"), n + "Feedback", { 0.0f, 1.0f }, 0.0f);
        }
        else
        {
            addC (oid (o, "fdest"), n + "Filter Destination", filterDestNames(), o == 6 ? 0 : 3);
        }
    }

    for (int f = 0; f < numFilt; ++f)
    {
        const auto n = "Filter " + String (f + 1) + " ";
        NormalisableRange<float> cut (20.0f, 20000.0f);
        cut.setSkewForCentre (630.0f);
        addB (fid (f, "on"), n + "On", true);
        addC (fid (f, "type"), n + "Type", filterTypeNames(), 1);
        addF (fid (f, "cut"), n + "Cutoff", cut, 24.0f, 2);
        addF (fid (f, "res"), n + "Res", { 0.0f, 1.0f }, 0.0f);
        addF (fid (f, "mix"), n + "D/W", { 0.0f, 1.0f }, 1.0f);
        addF (fid (f, "drive"), n + "Drive", { 0.0f, 1.0f }, 0.0f);
        addF (fid (f, "pan"), n + "Pan", { -1.0f, 1.0f }, 0.0f);
        addF (fid (f, "xpan"), n + "X Pan", { 0.0f, 1.0f }, 0.0f);
        addF (fid (f, "lvl"), n + "Level", { 0.0f, 1.0f }, 0.8f);
    }

    for (int l = 0; l < numLfo; ++l)
    {
        const auto n = "LFO " + String (l + 1) + " ";
        NormalisableRange<float> rate (0.01f, 30.0f);
        rate.setSkewForCentre (2.0f);
        addC (lid (l, "shape"), n + "Shape", lfoShapeNames(), 0);
        addF (lid (l, "rate"), n + "Rate", rate, 1.0f, 3);
        addB (lid (l, "sync"), n + "Sync", false);
        addC (lid (l, "div"), n + "Division", lfoDivNames(), 4);
        addB (lid (l, "retrig"), n + "Retrig", false);
        addC (lid (l, "tA"), n + "Target A", targetNames(), 0);
        addF (lid (l, "aA"), n + "Amount A", { -1.0f, 1.0f }, 0.0f);
        addC (lid (l, "tB"), n + "Target B", targetNames(), 0);
        addF (lid (l, "aB"), n + "Amount B", { -1.0f, 1.0f }, 0.0f);
    }

    addF ("m_tune", "Tune", { -100.0f, 100.0f, 0.1f }, 0.0f, 4);
    addI ("m_oct", "Master Oct", -3, 3, 0);
    addI ("m_semi", "Master Semi", -12, 12, 0);
    addF ("m_cent", "Master Cent", { -100.0f, 100.0f, 0.1f }, 0.0f, 4);
    addF ("m_fine", "Master Fine", { -1.0f, 1.0f, 0.001f }, 0.0f);
    addF ("m_coarse", "Master Coarse", { -24.0f, 24.0f, 0.01f }, 0.0f);
    addF ("m_vol", "Master Volume", { 0.0f, 1.0f }, 0.7f);
    addI ("m_voices", "Voices", 1, 16, 4);
    addF ("m_glide", "Glide", msRange (2000.0f), 0.0f, 1);
    addI ("m_bend", "Bend Range", 0, 24, 2);
    addF ("m_vel", "Velocity", { 0.0f, 1.0f }, 0.5f);
    return layout;
}
} // namespace pluto
