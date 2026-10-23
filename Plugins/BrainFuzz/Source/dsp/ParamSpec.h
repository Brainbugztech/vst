#pragma once

// Brainfuzz – zentrale Parameter-Tabelle (ohne JUCE)
// Plugin, Presets und Test-Tools benutzen dieselbe Liste und dieselbe Umrechnung in EngineParams.

#include "BrainFuzzEngine.h"
#include <string>
#include <vector>

namespace bfz
{

enum class PType { flt, boolean, choice };

// Gruppen fuer die Preset-Sperren
enum PGroup { gMain = 0, gInput, gOutput, gFx, gTight };

struct PSpec
{
    std::string id, name;
    PType type = PType::flt;
    float min = 0, max = 1, def = 0, centre = 0; // centre > 0: logarithmische Skala um diesen Wert
    std::vector<std::string> choices;
    std::string unit;
    int group = gMain;
    float step = 0.0f;
};

// Globale Parameter
enum G
{
    pX1, pX2, pX3, pXType, pSlope, pSatType, pBite, pSatBlend, pSpreadBlend, pMix, pIn, pOut, pLimit,
    pTightHp, pTightLp, pTightSlope, pMCrush, pMChorus, pMVerb, pMonoBass, pMonoFreq, pBypass, kNumGlobal
};

// Parameter pro Band
enum B
{
    bDrive, bLevel, bWidth, bSpread, bSolo, bMute, bBypass,
    bCrushOn, bCrushPos, bCrushTarget, bCrushBits, bCrushDown, bCrushMix,
    bChorusOn, bChorusPos, bChorusTarget, bChorusVoices, bChorusRate, bChorusDepth, bChorusMix,
    bVerbOn, bVerbPos, bVerbTarget, bVerbSize, bVerbLength, bVerbWidth, bVerbLow, bVerbHigh, bVerbPre, bVerbMix,
    kNumBand
};

static constexpr int kNumParams = kNumGlobal + 4 * kNumBand;
inline constexpr int bandIndex (int band, int p) { return kNumGlobal + band * kNumBand + p; }

inline const char* bandName (int b)
{
    static const char* n[] { "Low", "Lo-Mid", "Hi-Mid", "High" };
    return n[std::clamp (b, 0, 3)];
}

inline const std::vector<PSpec>& paramSpecs()
{
    static const std::vector<PSpec> specs = []
    {
        std::vector<PSpec> v ((size_t) kNumParams);
        auto F = [&] (int i, const char* id, const char* name, float mn, float mx, float def, const char* unit,
                      float centre = 0.0f, int group = gMain, float step = 0.0f)
        {
            PSpec s; s.id = id; s.name = name; s.type = PType::flt; s.min = mn; s.max = mx; s.def = def;
            s.unit = unit; s.centre = centre; s.group = group; s.step = step;
            v[(size_t) i] = s;
        };
        auto Bo = [&] (int i, const char* id, const char* name, bool def, int group = gMain)
        {
            PSpec s; s.id = id; s.name = name; s.type = PType::boolean; s.min = 0; s.max = 1; s.def = def ? 1.0f : 0.0f; s.group = group;
            v[(size_t) i] = s;
        };
        auto C = [&] (int i, const char* id, const char* name, std::vector<std::string> ch, int def, int group = gMain)
        {
            PSpec s; s.id = id; s.name = name; s.type = PType::choice; s.choices = std::move (ch); s.min = 0;
            s.max = (float) s.choices.size() - 1; s.def = (float) def; s.group = group;
            v[(size_t) i] = s;
        };

        F (pX1, "xover1", "Crossover 1", BrainFuzzEngine::kXMin[0], BrainFuzzEngine::kXMax[0], 120.0f, "Hz", 170.0f);
        F (pX2, "xover2", "Crossover 2", BrainFuzzEngine::kXMin[1], BrainFuzzEngine::kXMax[1], 900.0f, "Hz", 950.0f);
        F (pX3, "xover3", "Crossover 3", BrainFuzzEngine::kXMin[2], BrainFuzzEngine::kXMax[2], 5000.0f, "Hz", 4000.0f);
        std::vector<std::string> xt;
        for (int t = 0; t < 8; ++t) xt.push_back (xoverTypeName (t));
        C (pXType, "xtype", "Crossover Type", xt, 1);
        C (pSlope, "slope", "Crossover Slope", { "12 dB", "24 dB", "48 dB" }, 1);
        std::vector<std::string> st;
        for (int t = 0; t < 7; ++t) st.push_back (satTypeName (t));
        C (pSatType, "sattype", "Saturation Type", st, 1);
        F (pBite, "bite", "Bite", 0, 100, 50, "%");
        F (pSatBlend, "satblend", "Sat", 0, 100, 100, "%");
        F (pSpreadBlend, "spreadblend", "Spread", 0, 200, 100, "%");
        F (pMix, "mix", "Mix", 0, 100, 100, "%");
        F (pIn, "input", "Input", -24, 24, 0, "dB", 0, gInput);
        F (pOut, "output", "Output", -24, 24, 0, "dB", 0, gOutput);
        Bo (pLimit, "limit", "Limit", true, gOutput);
        Bo (pTightHp, "tighthp", "Tight HP", false, gTight);
        Bo (pTightLp, "tightlp", "Tight LP", false, gTight);
        C (pTightSlope, "tightslope", "Tight Slope", { "24 dB", "48 dB" }, 0, gTight);
        F (pMCrush, "mcrush", "Master Crush", 0, 200, 100, "%", 0, gFx);
        F (pMChorus, "mchorus", "Master Chorus", 0, 200, 100, "%", 0, gFx);
        F (pMVerb, "mverb", "Master Reverb", 0, 200, 100, "%", 0, gFx);
        Bo (pMonoBass, "monobass", "Mono Bass", false);
        F (pMonoFreq, "monofreq", "Mono Bass Freq", 20, 500, 120, "Hz", 120.0f);
        Bo (pBypass, "bypass", "Bypass", false);

        static const float driveDef[4] { 6.0f, 12.0f, 12.0f, 9.0f };
        static const float spreadDef[4] { 0.0f, 0.0f, 25.0f, 45.0f };
        for (int b = 0; b < 4; ++b)
        {
            const std::string pre = "b" + std::to_string (b + 1) + "_";
            const std::string nm = std::string (bandName (b)) + " ";
            auto id = [&] (const char* s) { return pre + s; };
            auto name = [&] (const char* s) { return nm + s; };
            auto I = [&] (int p) { return bandIndex (b, p); };
            const std::vector<std::string> posC { "Pre", "Post" }, tgtC { "Band", "Width", "Both" };

            F (I (bDrive), id ("drive").c_str(), name ("Drive").c_str(), 0, 36, driveDef[b], "dB");
            F (I (bLevel), id ("level").c_str(), name ("Level").c_str(), -24, 12, 0, "dB");
            F (I (bWidth), id ("width").c_str(), name ("Width").c_str(), 0, 200, 100, "%");
            F (I (bSpread), id ("spread").c_str(), name ("Spread").c_str(), 0, 100, spreadDef[b], "%");
            Bo (I (bSolo), id ("solo").c_str(), name ("Solo").c_str(), false);
            Bo (I (bMute), id ("mute").c_str(), name ("Mute").c_str(), false);
            Bo (I (bBypass), id ("bypass").c_str(), name ("Bypass").c_str(), false);

            Bo (I (bCrushOn), id ("crush_on").c_str(), name ("Crush On").c_str(), false, gFx);
            C (I (bCrushPos), id ("crush_pos").c_str(), name ("Crush Position").c_str(), posC, 1, gFx);
            C (I (bCrushTarget), id ("crush_target").c_str(), name ("Crush Target").c_str(), tgtC, 0, gFx);
            F (I (bCrushBits), id ("crush_bits").c_str(), name ("Crush Bits").c_str(), 1, 24, 8, "bit", 0, gFx);
            F (I (bCrushDown), id ("crush_down").c_str(), name ("Crush Downsample").c_str(), 1, 40, 4, "x", 6.0f, gFx);
            F (I (bCrushMix), id ("crush_mix").c_str(), name ("Crush Mix").c_str(), 0, 100, 100, "%", 0, gFx);

            Bo (I (bChorusOn), id ("chorus_on").c_str(), name ("Chorus On").c_str(), false, gFx);
            C (I (bChorusPos), id ("chorus_pos").c_str(), name ("Chorus Position").c_str(), posC, 1, gFx);
            C (I (bChorusTarget), id ("chorus_target").c_str(), name ("Chorus Target").c_str(), tgtC, 0, gFx);
            C (I (bChorusVoices), id ("chorus_voices").c_str(), name ("Chorus Voices").c_str(), { "1", "2", "3" }, 1, gFx);
            F (I (bChorusRate), id ("chorus_rate").c_str(), name ("Chorus Rate").c_str(), 0.05f, 8.0f, 0.6f, "Hz", 0.8f, gFx);
            F (I (bChorusDepth), id ("chorus_depth").c_str(), name ("Chorus Depth").c_str(), 0, 100, 50, "%", 0, gFx);
            F (I (bChorusMix), id ("chorus_mix").c_str(), name ("Chorus Mix").c_str(), 0, 100, 50, "%", 0, gFx);

            Bo (I (bVerbOn), id ("verb_on").c_str(), name ("Reverb On").c_str(), false, gFx);
            C (I (bVerbPos), id ("verb_pos").c_str(), name ("Reverb Position").c_str(), posC, 1, gFx);
            C (I (bVerbTarget), id ("verb_target").c_str(), name ("Reverb Target").c_str(), tgtC, 0, gFx);
            F (I (bVerbSize), id ("verb_size").c_str(), name ("Reverb Size").c_str(), 0, 100, 50, "%", 0, gFx);
            F (I (bVerbLength), id ("verb_length").c_str(), name ("Reverb Length").c_str(), 0.1f, 12.0f, 1.8f, "s", 1.5f, gFx);
            F (I (bVerbWidth), id ("verb_width").c_str(), name ("Reverb Width").c_str(), 0, 100, 100, "%", 0, gFx);
            F (I (bVerbLow), id ("verb_lowcut").c_str(), name ("Reverb Low Cut").c_str(), 20, 2000, 150, "Hz", 180.0f, gFx);
            F (I (bVerbHigh), id ("verb_highcut").c_str(), name ("Reverb High Cut").c_str(), 1000, 20000, 9000, "Hz", 6000.0f, gFx);
            F (I (bVerbPre), id ("verb_predelay").c_str(), name ("Reverb Pre-Delay").c_str(), 0, 250, 10, "ms", 40.0f, gFx);
            F (I (bVerbMix), id ("verb_mix").c_str(), name ("Reverb Mix").c_str(), 0, 100, 30, "%", 0, gFx);
        }
        return v;
    }();
    return specs;
}

inline int paramIndex (const std::string& id)
{
    const auto& s = paramSpecs();
    for (size_t i = 0; i < s.size(); ++i) if (s[i].id == id) return (int) i;
    return -1;
}

inline std::vector<float> defaultValues()
{
    std::vector<float> v;
    for (const auto& s : paramSpecs()) v.push_back (s.def);
    return v;
}

// Werte (in Einheiten wie angezeigt) -> Engine-Parameter
inline EngineParams toEngineParams (const float* v)
{
    EngineParams p;
    auto pc = [] (float x) { return x * 0.01f; };
    auto on = [] (float x) { return x > 0.5f; };
    auto ci = [] (float x) { return (int) std::lround (x); };
    p.xover = { v[pX1], v[pX2], v[pX3] };
    p.xoverType = ci (v[pXType]);
    p.slope = ci (v[pSlope]);
    p.satType = ci (v[pSatType]);
    p.bite = pc (v[pBite]);
    p.satBlend = pc (v[pSatBlend]);
    p.spreadBlend = pc (v[pSpreadBlend]);
    p.mix = pc (v[pMix]);
    p.inDb = v[pIn];
    p.outDb = v[pOut];
    p.limit = on (v[pLimit]);
    p.tightHp = on (v[pTightHp]);
    p.tightLp = on (v[pTightLp]);
    p.tightSlope = ci (v[pTightSlope]);
    p.masterCrush = pc (v[pMCrush]);
    p.masterChorus = pc (v[pMChorus]);
    p.masterVerb = pc (v[pMVerb]);
    p.monoBass = on (v[pMonoBass]);
    p.monoFreq = v[pMonoFreq];
    p.bypass = on (v[pBypass]);
    for (int b = 0; b < 4; ++b)
    {
        auto V = [&] (int q) { return v[bandIndex (b, q)]; };
        auto& bp = p.band[(size_t) b];
        bp.driveDb = V (bDrive);
        bp.levelDb = V (bLevel);
        bp.width = pc (V (bWidth));
        bp.spread = pc (V (bSpread));
        bp.solo = on (V (bSolo));
        bp.mute = on (V (bMute));
        bp.bypass = on (V (bBypass));
        bp.crushSlot = { on (V (bCrushOn)), ci (V (bCrushPos)), ci (V (bCrushTarget)) };
        bp.crush = { V (bCrushBits), V (bCrushDown), pc (V (bCrushMix)) };
        bp.chorusSlot = { on (V (bChorusOn)), ci (V (bChorusPos)), ci (V (bChorusTarget)) };
        bp.chorus = { ci (V (bChorusVoices)) + 1, V (bChorusRate), pc (V (bChorusDepth)), pc (V (bChorusMix)) };
        bp.verbSlot = { on (V (bVerbOn)), ci (V (bVerbPos)), ci (V (bVerbTarget)) };
        bp.verb.size = pc (V (bVerbSize));
        bp.verb.length = V (bVerbLength);
        bp.verb.width = pc (V (bVerbWidth));
        bp.verb.lowCut = V (bVerbLow);
        bp.verb.highCut = V (bVerbHigh);
        bp.verb.predelay = V (bVerbPre);
        bp.verb.mix = pc (V (bVerbMix));
    }
    return p;
}

} // namespace bfz
