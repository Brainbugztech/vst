#pragma once

// Brainfuzz – 40 Werkspresets in Gruppen (ohne JUCE)
// Jedes Preset setzt nur, was vom Init abweicht. Der Ausgangspegel (Pegel-Abgleich) kommt aus
// PresetLevels.h, das tools/level_match.cpp aus echten Renderings berechnet.

#include "ParamSpec.h"
#include <utility>

namespace bfz
{

struct FactoryPreset
{
    std::string name, group;
    std::vector<std::pair<std::string, float>> set;
};

// kleine Helfer zum Schreiben
struct PB
{
    FactoryPreset p;
    PB (const char* name, const char* group) { p.name = name; p.group = group; }
    PB& v (const std::string& id, float x) { p.set.push_back ({ id, x }); return *this; }
    PB& b (int band, const char* key, float x) { return v ("b" + std::to_string (band) + "_" + key, x); }
    // Drive fuer alle vier Baender
    PB& drives (float a, float b2, float c, float d) { return b (1, "drive", a).b (2, "drive", b2).b (3, "drive", c).b (4, "drive", d); }
    PB& spreads (float a, float b2, float c, float d) { return b (1, "spread", a).b (2, "spread", b2).b (3, "spread", c).b (4, "spread", d); }
    PB& widths (float a, float b2, float c, float d) { return b (1, "width", a).b (2, "width", b2).b (3, "width", c).b (4, "width", d); }
    PB& xo (float a, float b2, float c) { return v ("xover1", a).v ("xover2", b2).v ("xover3", c); }
    PB& sat (int type, float bite) { return v ("sattype", (float) type).v ("bite", bite); }
    PB& xtype (int t, int slope) { return v ("xtype", (float) t).v ("slope", (float) slope); }
    PB& crush (int band, float bits, float down, float mix, int pos = 1, int target = 0)
    {
        return b (band, "crush_on", 1).b (band, "crush_bits", bits).b (band, "crush_down", down).b (band, "crush_mix", mix)
              .b (band, "crush_pos", (float) pos).b (band, "crush_target", (float) target);
    }
    PB& chorus (int band, int voices, float rate, float depth, float mix, int pos = 1, int target = 0)
    {
        return b (band, "chorus_on", 1).b (band, "chorus_voices", (float) (voices - 1)).b (band, "chorus_rate", rate)
              .b (band, "chorus_depth", depth).b (band, "chorus_mix", mix).b (band, "chorus_pos", (float) pos).b (band, "chorus_target", (float) target);
    }
    PB& verb (int band, float size, float len, float mix, int target = 0, float pre = 10, float lo = 150, float hi = 9000, int pos = 1)
    {
        return b (band, "verb_on", 1).b (band, "verb_size", size).b (band, "verb_length", len).b (band, "verb_mix", mix)
              .b (band, "verb_target", (float) target).b (band, "verb_predelay", pre).b (band, "verb_lowcut", lo)
              .b (band, "verb_highcut", hi).b (band, "verb_pos", (float) pos);
    }
};

enum { tSplat = 0, tFuzz, tFold, tDiode, tCrunch, tTube, tTape };
enum { xBw = 0, xLr, xCh1, xCh2, xEll, xBes, xLeg, xGau };
enum { s12 = 0, s24, s48 };
enum { tgBand = 0, tgWidth, tgBoth };

inline std::vector<FactoryPreset> factoryPresetsRaw()
{
    std::vector<FactoryPreset> v;
    auto add = [&] (PB& b) { v.push_back (b.p); };

    // ------------------------------------------------ Basics
    { PB b ("Init", "Basics"); add (b); }
    { PB b ("Clean Split", "Basics"); b.drives (0, 0, 0, 0).spreads (0, 0, 0, 0); add (b); }
    { PB b ("Gentle Width", "Basics"); b.sat (tTape, 30).drives (0, 3, 4, 5).spreads (0, 15, 45, 70).widths (60, 110, 140, 160).v ("monobass", 1); add (b); }
    { PB b ("Warm Glue", "Basics"); b.sat (tTape, 55).drives (5, 6, 5, 4).spreads (0, 0, 10, 20).v ("satblend", 70); add (b); }

    // ------------------------------------------------ Crossover Types
    { PB b ("Butterworth Bite", "Crossover Types"); b.xtype (xBw, s24).sat (tFuzz, 50).drives (4, 14, 14, 8); add (b); }
    { PB b ("Linkwitz Clean Fuzz", "Crossover Types"); b.xtype (xLr, s48).sat (tFuzz, 40).drives (0, 15, 15, 10).v ("tighthp", 1); add (b); }
    { PB b ("Chebyshev Edge", "Crossover Types"); b.xtype (xCh1, s24).sat (tCrunch, 60).drives (6, 12, 16, 10); add (b); }
    { PB b ("Chebyshev II Notch", "Crossover Types"); b.xtype (xCh2, s48).sat (tDiode, 50).drives (3, 14, 12, 9).xo (150, 1100, 6000); add (b); }
    { PB b ("Elliptic Razor", "Crossover Types"); b.xtype (xEll, s48).sat (tSplat, 45).drives (0, 10, 18, 12).v ("tighthp", 1).v ("tightlp", 1); add (b); }
    { PB b ("Bessel Smear", "Crossover Types"); b.xtype (xBes, s12).sat (tTube, 50).drives (8, 10, 10, 8).spreads (0, 20, 30, 40); add (b); }
    { PB b ("Legendre Steep", "Crossover Types"); b.xtype (xLeg, s48).sat (tFuzz, 65).drives (2, 16, 18, 6).xo (100, 700, 4000); add (b); }
    { PB b ("Gaussian Melt", "Crossover Types"); b.xtype (xGau, s24).sat (tTape, 70).drives (8, 12, 12, 10).spreads (0, 30, 40, 50); add (b); }

    // ------------------------------------------------ Saturation
    { PB b ("Splat Attack", "Saturation"); b.sat (tSplat, 70).drives (6, 18, 20, 14).v ("tighthp", 1); add (b); }
    { PB b ("Classic Fuzz", "Saturation"); b.sat (tFuzz, 50).drives (8, 18, 18, 12); add (b); }
    { PB b ("Fold Metal", "Saturation"); b.sat (tFold, 60).drives (0, 12, 15, 10).spreads (0, 10, 30, 50); add (b); }
    { PB b ("Diode Grit", "Saturation"); b.sat (tDiode, 70).drives (6, 15, 15, 10); add (b); }
    { PB b ("Crunch Box", "Saturation"); b.sat (tCrunch, 50).drives (8, 14, 12, 8); add (b); }
    { PB b ("Tube Glow", "Saturation"); b.sat (tTube, 45).drives (6, 10, 9, 6).v ("satblend", 80); add (b); }
    { PB b ("Tape Warmth", "Saturation"); b.sat (tTape, 40).drives (8, 8, 6, 4).spreads (0, 0, 15, 25); add (b); }

    // ------------------------------------------------ Bass
    { PB b ("Sub Safe Fuzz", "Bass"); b.xo (110, 700, 3500).sat (tFuzz, 55).drives (0, 20, 18, 10).spreads (0, 0, 20, 30)
          .v ("tighthp", 1).v ("tightslope", 1).v ("monobass", 1).v ("monofreq", 150); add (b); }
    { PB b ("Reese Grinder", "Bass"); b.xo (90, 500, 2500).sat (tFold, 45).drives (0, 16, 20, 12).spreads (0, 20, 50, 60)
          .chorus (3, 3, 0.4f, 60, 40).v ("tighthp", 1).v ("monobass", 1); add (b); }
    { PB b ("808 Crunch", "Bass"); b.xo (80, 400, 3000).sat (tCrunch, 70).drives (10, 18, 12, 6).v ("tighthp", 1).v ("monobass", 1); add (b); }
    { PB b ("Psy Bass Bite", "Bass"); b.xo (120, 600, 4000).sat (tDiode, 60).drives (2, 16, 12, 4).v ("tighthp", 1).v ("tightlp", 1)
          .v ("monobass", 1).v ("monofreq", 160); add (b); }
    { PB b ("Hitech Bass Tight", "Bass"); b.xtype (xLr, s48).xo (130, 900, 5000).sat (tSplat, 40).drives (0, 14, 16, 8)
          .v ("tighthp", 1).v ("tightlp", 1).v ("tightslope", 1).v ("monobass", 1).v ("monofreq", 180); add (b); }

    // ------------------------------------------------ Drums
    { PB b ("Drum Bus Smash", "Drums"); b.sat (tCrunch, 60).drives (8, 12, 14, 10).spreads (0, 0, 15, 25).v ("satblend", 75); add (b); }
    { PB b ("Hat Shimmer", "Drums"); b.xo (200, 2000, 8000).sat (tTape, 50).drives (0, 0, 8, 14).spreads (0, 0, 40, 80)
          .chorus (4, 2, 0.3f, 40, 35, 1, tgWidth).v ("tighthp", 1); add (b); }
    { PB b ("Snare Crack", "Drums"); b.xo (150, 900, 4500).sat (tSplat, 55).drives (0, 14, 18, 8).verb (3, 30, 0.6f, 20, tgBand, 5, 400, 8000); add (b); }
    { PB b ("Kick Clean Top Dirt", "Drums"); b.xo (140, 1200, 6000).sat (tFuzz, 60).drives (0, 8, 18, 12).v ("tighthp", 1).v ("monobass", 1); add (b); }
    { PB b ("Room Crush", "Drums"); b.sat (tCrunch, 40).drives (4, 8, 10, 8).crush (3, 10, 3, 50).crush (4, 9, 2, 40)
          .verb (2, 35, 0.9f, 25, tgBoth, 8); add (b); }

    // ------------------------------------------------ Synth & Vocals
    { PB b ("Lead Fuzz Wide", "Synth & Vocals"); b.sat (tFuzz, 60).drives (4, 16, 18, 12).spreads (0, 30, 60, 70).widths (80, 120, 150, 160); add (b); }
    { PB b ("Pad Haze", "Synth & Vocals"); b.sat (tTape, 60).drives (4, 8, 8, 6).spreads (0, 30, 50, 70)
          .chorus (2, 3, 0.25f, 60, 40).verb (4, 70, 3.5f, 30, tgWidth, 30); add (b); }
    { PB b ("Vocal Air", "Synth & Vocals"); b.xo (180, 1500, 7000).sat (tTube, 35).drives (0, 4, 8, 10).spreads (0, 0, 25, 60)
          .v ("tightlp", 1).verb (4, 40, 1.2f, 15, tgWidth, 20, 2000, 16000); add (b); }
    { PB b ("Pluck Shine", "Synth & Vocals"); b.sat (tDiode, 40).drives (2, 8, 12, 12).spreads (0, 10, 40, 60).chorus (3, 2, 0.8f, 30, 30); add (b); }
    { PB b ("Acid Squelch", "Synth & Vocals"); b.xo (120, 900, 3500).sat (tFold, 70).drives (4, 20, 22, 8).v ("satblend", 85).v ("tightlp", 1); add (b); }

    // ------------------------------------------------ FX Combos
    { PB b ("Lo-Fi Crush Verb", "FX Combos"); b.sat (tTape, 50).drives (4, 8, 10, 8).crush (2, 8, 4, 70).crush (3, 7, 6, 70).crush (4, 6, 8, 60)
          .verb (3, 55, 2.2f, 30, tgBand, 15).verb (4, 55, 2.2f, 30, tgBand, 15); add (b); }
    { PB b ("Space Highs", "FX Combos"); b.sat (tFuzz, 40).drives (4, 10, 10, 10).spreads (0, 0, 30, 60)
          .verb (4, 90, 6.0f, 45, tgWidth, 60, 1500, 14000).verb (3, 70, 3.5f, 25, tgWidth, 40, 800, 10000); add (b); }
    { PB b ("Chorus Mids", "FX Combos"); b.sat (tTube, 40).drives (4, 10, 10, 6).chorus (2, 3, 0.5f, 55, 50).chorus (3, 3, 0.7f, 55, 50); add (b); }
    { PB b ("Bitwide", "FX Combos"); b.sat (tSplat, 30).drives (0, 8, 10, 10).spreads (0, 30, 60, 80)
          .crush (3, 6, 5, 80, 1, tgWidth).crush (4, 5, 8, 80, 1, tgWidth); add (b); }
    { PB b ("Total Meltdown", "FX Combos"); b.sat (tFold, 80).drives (14, 24, 26, 20).spreads (10, 40, 60, 80)
          .crush (2, 6, 6, 50, 0).crush (3, 5, 10, 50, 0).chorus (2, 3, 1.5f, 70, 40).verb (3, 60, 2.5f, 25, tgBoth, 20).verb (4, 60, 2.5f, 25, tgBoth, 20); add (b); }
    { PB b ("Mono Safe Galaxy", "FX Combos"); b.sat (tTape, 50).drives (2, 6, 8, 8).spreads (0, 40, 70, 90)
          .chorus (2, 3, 0.3f, 50, 45, 1, tgWidth).chorus (3, 3, 0.4f, 50, 45, 1, tgWidth)
          .verb (3, 80, 4.5f, 45, tgWidth, 40, 500, 12000).verb (4, 80, 4.5f, 45, tgWidth, 40, 500, 12000).v ("monobass", 1); add (b); }
    return v;
}

} // namespace bfz
