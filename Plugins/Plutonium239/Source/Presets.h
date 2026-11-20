#pragma once

// Werkspresets. Jedes Preset startet vom Init-Zustand (alle Parameter auf Standard)
// und setzt dann nur die genannten Werte. Auswahlparameter werden per Name gesetzt.

#include "Params.h"

namespace pluto
{
struct PV
{
    juce::String id;
    float v = 0.0f;
    juce::String choice;
    PV (juce::String i, float val) : id (std::move (i)), v (val) {}
    PV (juce::String i, const char* c) : id (std::move (i)), choice (c) {}
};

struct Preset
{
    juce::String name;
    std::vector<PV> values;
};

// Hilfen: Oszillator per Name ansprechen
inline int oscIdx (const char* n) { return oscNames().indexOf (n); }
inline juce::String O (const char* osc, const char* p) { return oid (oscIdx (osc), p); }
inline juce::String F (int f1, const char* p) { return fid (f1 - 1, p); }
inline juce::String L (int l1, const char* p) { return lid (l1 - 1, p); }

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> p
    {
        { "Init", {} },

        { "FM Bass", {
            { O ("C1", "wave"), "Sin" }, { O ("C1", "oct"), -1 }, { O ("C1", "d"), 600 }, { O ("C1", "s"), 0.7f }, { O ("C1", "r"), 120 },
            { O ("M1.1", "wave"), "Sin" }, { O ("M1.1", "oct"), -1 }, { O ("M1.1", "dest"), "C1" }, { O ("M1.1", "fmamt"), 0.45f },
            { O ("M1.1", "d"), 220 }, { O ("M1.1", "s"), 0.25f }, { O ("M1.1", "fb"), 0.35f },
            { F (1, "type"), "LP4" }, { F (1, "cut"), 1400 }, { F (1, "res"), 0.2f },
            { "m_voices", 1 }, { "m_glide", 40 } } },

        { "Psy Lead", {
            { O ("C1", "wave"), "Saw" }, { O ("C2", "wave"), "Saw" }, { O ("C2", "cent"), 14 }, { O ("C2", "fdest"), "Filter 1" },
            { O ("M1.1", "wave"), "Sin" }, { O ("M1.1", "oct"), 1 }, { O ("M1.1", "dest"), "C1" }, { O ("M1.1", "fmamt"), 0.3f },
            { F (1, "type"), "LP2" }, { F (1, "cut"), 2600 }, { F (1, "res"), 0.45f }, { F (1, "xpan"), 0.5f },
            { L (1, "sync"), 1 }, { L (1, "div"), "1/16" }, { L (1, "tA"), "F1 Cutoff" }, { L (1, "aA"), 0.25f },
            { "m_glide", 25 } } },

        { "Glass Bell", {
            { O ("C1", "wave"), "Sin" }, { O ("C1", "a"), 1 }, { O ("C1", "d"), 2800 }, { O ("C1", "s"), 0.0f }, { O ("C1", "r"), 1800 },
            { O ("M1.1", "wave"), "Sin" }, { O ("M1.1", "coarse"), 21.69f }, { O ("M1.1", "dest"), "C1" }, { O ("M1.1", "fmamt"), 0.5f },
            { O ("M1.1", "d"), 1500 }, { O ("M1.1", "s"), 0.0f }, { O ("M1.1", "r"), 1500 },
            { O ("C2", "wave"), "Sin" }, { O ("C2", "oct"), 1 }, { O ("C2", "fine"), 6.0f }, { O ("C2", "d"), 1800 }, { O ("C2", "s"), 0.0f },
            { O ("C2", "r"), 1500 }, { O ("C2", "lvl"), 0.35f }, { O ("C2", "fdest"), "Filter 1" },
            { O ("C1", "lvl"), 1.0f }, { F (1, "type"), "HP2" }, { F (1, "cut"), 120 }, { F (1, "xpan"), 0.6f }, { F (1, "lvl"), 1.0f }, { "m_voices", 8 } } },

        { "Growl WT", {
            { O ("C1", "wave"), "WT" }, { O ("C1", "wt"), "FM" }, { O ("C1", "wtpos"), 0.3f }, { O ("C1", "oct"), -1 },
            { O ("M1.1", "wave"), "Sin" }, { O ("M1.1", "oct"), -1 }, { O ("M1.1", "dest"), "C1" }, { O ("M1.1", "fmtype"), "+/-LogFM" },
            { O ("M1.1", "fmamt"), 0.15f },
            { F (1, "type"), "LP4" }, { F (1, "cut"), 1100 }, { F (1, "res"), 0.5f }, { F (1, "drive"), 0.5f },
            { L (1, "shape"), "Triangle" }, { L (1, "sync"), 1 }, { L (1, "div"), "1/4" }, { L (1, "tA"), "C1 WT Pos" }, { L (1, "aA"), 0.5f },
            { L (2, "sync"), 1 }, { L (2, "div"), "1/8" }, { L (2, "tA"), "F1 Cutoff" }, { L (2, "aA"), 0.3f },
            { "m_voices", 2 } } },

        { "Comb Metal", {
            { O ("C1", "wave"), "Saw" }, { O ("C1", "oct"), -1 }, { O ("C1", "d"), 700 }, { O ("C1", "s"), 0.3f }, { O ("C1", "r"), 300 },
            { O ("M1.1", "wave"), "SQR" }, { O ("M1.1", "coarse"), 7 }, { O ("M1.1", "dest"), "C1" }, { O ("M1.1", "fmamt"), 0.25f },
            { O ("C1", "lvl"), 1.0f }, { F (1, "type"), "Comb+" }, { F (1, "cut"), 220 }, { F (1, "res"), 0.85f }, { F (1, "lvl"), 1.0f } } },

        { "Formant Talk", {
            { O ("C1", "wave"), "Saw" }, { O ("C1", "oct"), -1 },
            { O ("C1", "lvl"), 1.0f }, { F (1, "type"), "Formant" }, { F (1, "cut"), 400 }, { F (1, "res"), 0.6f }, { F (1, "lvl"), 1.0f },
            { L (1, "shape"), "Smooth Rnd" }, { L (1, "sync"), 1 }, { L (1, "div"), "1/8" }, { L (1, "tA"), "F1 Cutoff" }, { L (1, "aA"), 0.5f },
            { "m_voices", 2 }, { "m_glide", 60 } } },

        { "Sync Screamer", {
            { O ("M2.1", "wave"), "Sin" }, { O ("M2.1", "oct"), -1 },
            { O ("C1", "wave"), "Saw" }, { O ("C1", "coarse"), 12 }, { O ("C1", "sync"), "M2.1" },
            { L (1, "rate"), 0.4f }, { L (1, "tA"), "C1 Pitch" }, { L (1, "aA"), 0.4f },
            { F (1, "type"), "LP2" }, { F (1, "cut"), 5000 }, { F (1, "res"), 0.2f }, { "m_voices", 1 } } },

        { "Hollow Pad", {
            { O ("C1", "wave"), "WT" }, { O ("C1", "wt"), "Vowel" }, { O ("C1", "a"), 900 }, { O ("C1", "r"), 1800 },
            { O ("C2", "wave"), "Tri" }, { O ("C2", "cent"), 8 }, { O ("C2", "a"), 1200 }, { O ("C2", "r"), 1800 }, { O ("C2", "lvl"), 0.5f },
            { O ("C2", "fdest"), "Filter 1" },
            { L (1, "rate"), 0.15f }, { L (1, "tA"), "C1 WT Pos" }, { L (1, "aA"), 0.6f },
            { O ("C1", "lvl"), 1.0f }, { F (1, "type"), "LP2" }, { F (1, "cut"), 2600 }, { F (1, "xpan"), 0.7f }, { F (1, "lvl"), 1.0f }, { "m_voices", 8 } } },

        { "Bit Pluck", {
            { O ("C1", "wave"), "WT" }, { O ("C1", "wt"), "Grit" }, { O ("C1", "wtpos"), 0.6f },
            { O ("C1", "d"), 260 }, { O ("C1", "s"), 0.0f }, { O ("C1", "r"), 200 },
            { O ("M1.1", "wave"), "Saw" }, { O ("M1.1", "dest"), "C1" }, { O ("M1.1", "fmtype"), "+LogFM" }, { O ("M1.1", "fmamt"), 0.2f },
            { O ("M1.1", "d"), 120 }, { O ("M1.1", "s"), 0.0f },
            { O ("C1", "lvl"), 1.0f }, { F (1, "type"), "LP2" }, { F (1, "cut"), 3200 }, { F (1, "res"), 0.35f }, { F (1, "xpan"), 0.4f } } },

        { "CZ Reso", {
            { O ("C1", "wave"), "Sin" }, { O ("C1", "pdmode"), "Reso Saw" }, { O ("C1", "pdamt"), 0.25f }, { O ("C1", "lvl"), 1.0f },
            { O ("C1", "d"), 900 }, { O ("C1", "s"), 0.5f }, { O ("C1", "r"), 250 },
            { O ("M1.1", "wave"), "Sin" }, { O ("M1.1", "oct"), -1 }, { O ("M1.1", "dest"), "C1" }, { O ("M1.1", "fmtype"), "+PD" },
            { O ("M1.1", "fmamt"), 0.6f }, { O ("M1.1", "d"), 500 }, { O ("M1.1", "s"), 0.2f },
            { F (1, "type"), "HP2" }, { F (1, "cut"), 60 }, { F (1, "lvl"), 1.0f }, { "m_voices", 4 } } },

        { "PD Bass", {
            { O ("C1", "wave"), "Sin" }, { O ("C1", "oct"), -1 }, { O ("C1", "pdmode"), "Bend" }, { O ("C1", "pdamt"), 0.7f }, { O ("C1", "lvl"), 1.0f },
            { O ("C1", "d"), 350 }, { O ("C1", "s"), 0.6f }, { O ("C1", "r"), 90 },
            { O ("M1.1", "wave"), "Sin" }, { O ("M1.1", "oct"), -1 }, { O ("M1.1", "dest"), "C1" }, { O ("M1.1", "fmtype"), "+/-PM" },
            { O ("M1.1", "fmamt"), 0.35f }, { O ("M1.1", "d"), 180 }, { O ("M1.1", "s"), 0.1f },
            { F (1, "type"), "LP4" }, { F (1, "cut"), 2200 }, { F (1, "res"), 0.15f }, { F (1, "lvl"), 1.0f },
            { "m_voices", 1 }, { "m_glide", 30 } } },
    };
    return p;
}

inline void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& pr)
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& v : pr.values)
    {
        auto* rp = state.getParameter (v.id);
        if (rp == nullptr) { jassertfalse; continue; }
        if (v.choice.isNotEmpty())
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (rp))
            {
                const int idx = c->choices.indexOf (v.choice);
                jassert (idx >= 0);
                if (idx >= 0) rp->setValueNotifyingHost (rp->convertTo0to1 ((float) idx));
            }
        }
        else
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.v));
    }
}
} // namespace pluto
