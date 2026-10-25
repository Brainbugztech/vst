#pragma once

#include <utility>
#include <vector>

namespace prizm
{
// Werkspresets. Werte in Parametereinheiten (Prozent, Hz, Index).
// Nicht genannte Parameter gehen auf ihren Standard zurück. Die Tonart (Key, Scale, Custom,
// Tune, MIDI KEY, Sidechain) bleibt immer stehen, damit man im Song durchblättern kann.
//
// Spickzettel für die Indizes:
//   mode: 0 = Zero Latency, 1 = High Quality       stereo: 0 = Stereo, 1 = Mid, 2 = Side
//   lfoNShape: 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square, 5 S&H, 6 Smooth Random, 7 Custom (Standard)
//   lfoNMode: 0 Free, 1 Retrig, 2 Envelope      lfoTrig: 0 Audio, 1 MIDI, 2 Audio + MIDI (Standard)
//   lfoNDiv: 0 4 Bars, 1 2 Bars, 2 1 Bar, 3 1/2, 4 1/4., 5 1/4, 6 1/4T, 7 1/8., 8 1/8, 9 1/8T, 10 1/16, 11 1/16T, 12 1/32
//   modSrcI: 1 LFO 1, 2 LFO 2, 3 ENV
//   modDstI: 0 Strength, 1 Color, 2 Clear, 3 Mix, 4 Output, 5 Range Low, 6 Range High, 7 Gate, 8 Shift, 9 Formant, 10 Tune
//   Kurven: "x,y,Biegung;..." (x 0..1, y -1..1, Biegung -1..1: < 0 schnell anfangen, > 0 langsam anfangen)
struct FactoryPreset
{
    enum class Map { reset, majorToMinor, minorToMajor };

    const char* group;
    const char* name;
    const char* hint;
    std::vector<std::pair<const char*, float>> values;
    Map map = Map::reset;
    const char* curve1 = nullptr;
    const char* curve2 = nullptr;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        // --- Basics ---------------------------------------------------------------------------------
        { "BASICS", "Init", "Grundeinstellung.", {} },
        { "BASICS", "Live Safe", "Ohne Latenz, sanft und anschlagsfreundlich, für Live und Recording.",
          { { "mode", 0 }, { "strength", 80 }, { "color", 50 }, { "clear", 50 } } },
        { "BASICS", "Pad Sweetener", "Flächen und Keys sitzen in der Tonart, ohne an Körper zu verlieren.",
          { { "mode", 1 }, { "strength", 90 }, { "color", 80 }, { "clear", 20 }, { "rangeLow", 120 } } },
        { "BASICS", "Bass Keeper", "Alles über dem Bass geht in die Tonart, Kick und Sub bleiben sauber.",
          { { "mode", 0 }, { "strength", 100 }, { "color", 70 }, { "clear", 40 }, { "rangeLow", 220 } } },

        // --- Color Bass -----------------------------------------------------------------------------
        { "COLOR BASS", "CB Chord Growl", "Growl wird zum Akkord, die Resonanz zuckt im 1/8-Takt. Tipp: SCALE auf Minor Chord.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 130 }, { "clear", 35 }, { "rangeLow", 110 }, { "gate", 35 },
            { "lfo1Shape", 7 }, { "lfo1Div", 8 }, { "modSrc0", 1 }, { "modDst0", 1 }, { "modAmt0", 25 } },
          FactoryPreset::Map::reset, "0,1,0;0.05,1,-0.6;1,-1,0" },
        { "COLOR BASS", "CB Hit Wobble", "Wobble in Achtel-Triolen, der bei jedem Bass-Anschlag von vorn startet (Retrig). Sitzt immer gleich auf dem Groove.",
          { { "mode", 1 }, { "strength", 70 }, { "color", 100 }, { "clear", 30 }, { "rangeLow", 90 },
            { "lfo1Shape", 0 }, { "lfo1Div", 9 }, { "lfo1Mode", 1 },
            { "modSrc0", 1 }, { "modDst0", 1 }, { "modAmt0", 40 },
            { "modSrc1", 1 }, { "modDst1", 0 }, { "modAmt1", 30 } } },
        { "COLOR BASS", "CB Wobble 1/8", "Klassischer Wobble: Stimmung und Resonanz pendeln im 1/8-Takt. Tipp: SCALE auf Minor Chord.",
          { { "mode", 1 }, { "strength", 60 }, { "color", 90 }, { "clear", 30 }, { "rangeLow", 90 },
            { "lfo1Shape", 0 }, { "lfo1Div", 8 },
            { "modSrc0", 1 }, { "modDst0", 0 }, { "modAmt0", 45 },
            { "modSrc1", 1 }, { "modDst1", 1 }, { "modAmt1", 35 } } },
        { "COLOR BASS", "CB Triplet Stabs", "Harmonische Stabs in Achtel-Triolen: der Akkord wird rhythmisch rein- und rausgeschnitten.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 110 }, { "clear", 60 }, { "mix", 50 }, { "rangeLow", 100 },
            { "lfo1Shape", 4 }, { "lfo1Div", 9 }, { "modSrc0", 1 }, { "modDst0", 3 }, { "modAmt0", 60 } } },
        { "COLOR BASS", "CB Neuro Shift", "Schiefe Bassanteile gleiten im Viertel-Takt in die Tonart. Klingt nach gestimmtem Neuro-Bass.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 90 }, { "clear", 40 }, { "shift", 50 }, { "formant", 100 }, { "rangeLow", 80 },
            { "lfo1Shape", 3 }, { "lfo1Div", 5 }, { "modSrc0", 1 }, { "modDst0", 8 }, { "modAmt0", 50 } } },
        { "COLOR BASS", "CB Vowel Talk", "Der Bass spricht: Resonanz im Dreieck, lautere Stellen werden stärker gestimmt.",
          { { "mode", 1 }, { "strength", 60 }, { "color", 100 }, { "clear", 30 }, { "rangeLow", 100 },
            { "lfo1Shape", 1 }, { "lfo1Div", 5 }, { "envAttack", 5 }, { "envRelease", 180 },
            { "modSrc0", 1 }, { "modDst0", 1 }, { "modAmt0", 45 },
            { "modSrc1", 3 }, { "modDst1", 0 }, { "modAmt1", 40 } } },
        { "COLOR BASS", "CB Halftime Pump", "Jeder Schlag startet trocken und zieht dann in den Akkord, wie ein Sidechain-Pumpen.",
          { { "mode", 1 }, { "strength", 50 }, { "color", 120 }, { "clear", 50 }, { "rangeLow", 100 },
            { "lfo1Shape", 7 }, { "lfo1Div", 5 }, { "modSrc0", 1 }, { "modDst0", 0 }, { "modAmt0", 50 } },
          FactoryPreset::Map::reset, "0,-1,-0.5;0.6,1,0;1,1,0" },
        { "COLOR BASS", "CB Shimmer Chords", "Lange, schimmernde Akkord-Fahnen hinter dem Bass, sanft in Bewegung.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 175 }, { "clear", 30 }, { "mix", 55 }, { "rangeLow", 160 },
            { "lfo2Shape", 6 }, { "lfo2Div", 1 }, { "modSrc0", 2 }, { "modDst0", 1 }, { "modAmt0", 15 } } },
        { "COLOR BASS", "CB Riser Tune", "Für Build-ups: über 4 Takte wird der Sound immer stärker gestimmt und tonaler.",
          { { "mode", 1 }, { "strength", 50 }, { "color", 80 }, { "clear", 40 }, { "rangeLow", 90 },
            { "lfo2Shape", 2 }, { "lfo2Div", 0 },
            { "modSrc0", 2 }, { "modDst0", 0 }, { "modAmt0", 50 },
            { "modSrc1", 2 }, { "modDst1", 1 }, { "modAmt1", 40 } } },

        // --- Movement -------------------------------------------------------------------------------
        { "MOVEMENT", "Hit Bloom", "Jeder Anschlag startet trocken und blüht dann über eine Viertel in den Akkord auf (Envelope).",
          { { "mode", 1 }, { "strength", 50 }, { "color", 120 }, { "clear", 40 }, { "rangeLow", 90 },
            { "lfo1Div", 5 }, { "lfo1Mode", 2 }, { "modSrc0", 1 }, { "modDst0", 0 }, { "modAmt0", 50 },
            { "modSrc1", 1 }, { "modDst1", 1 }, { "modAmt1", 20 } },
          FactoryPreset::Map::reset, "0,-1,0.45;1,1,0" },
        { "MOVEMENT", "Harmonic Gate 1/16", "Der Tonart-Effekt springt im 1/16-Takt an und aus. Gut auf Pads und Noise.",
          { { "mode", 1 }, { "strength", 50 }, { "color", 100 }, { "clear", 50 },
            { "lfo1Shape", 4 }, { "lfo1Div", 10 }, { "modSrc0", 1 }, { "modDst0", 0 }, { "modAmt0", 50 } } },
        { "MOVEMENT", "Breathing Pad", "Die Fläche atmet: Resonanz über einen Takt, sanfter Mix.",
          { { "mode", 1 }, { "strength", 90 }, { "color", 90 }, { "clear", 20 }, { "mix", 85 },
            { "lfo2Shape", 0 }, { "lfo2Div", 2 }, { "modSrc0", 2 }, { "modDst0", 1 }, { "modAmt0", 30 } } },
        { "MOVEMENT", "Tape Warble", "Leichtes Leiern wie bei altem Band: die Stimmung schwankt zufällig um wenige Cent.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 60 }, { "shift", 100 }, { "formant", 100 },
            { "lfo1Shape", 6 }, { "lfo1Sync", 0 }, { "lfo1Rate", 0.35f }, { "modSrc0", 1 }, { "modDst0", 10 }, { "modAmt0", 6 } } },
        { "MOVEMENT", "Dynamic Tuning", "Je lauter, desto stärker gestimmt. Leise Stellen bleiben natürlich.",
          { { "mode", 1 }, { "strength", 40 }, { "color", 90 }, { "clear", 30 }, { "envAttack", 5 }, { "envRelease", 250 },
            { "modSrc0", 3 }, { "modDst0", 0 }, { "modAmt0", 50 } } },
        { "MOVEMENT", "Range Sweep", "Der gestimmte Frequenzbereich wandert über zwei Takte wie ein Filter durchs Signal.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 110 }, { "clear", 30 }, { "rangeLow", 150 },
            { "lfo2Shape", 1 }, { "lfo2Div", 1 }, { "modSrc0", 2 }, { "modDst0", 5 }, { "modAmt0", 30 } } },

        // --- Creative -------------------------------------------------------------------------------
        { "CREATIVE", "Noise to Chord", "Macht aus Rauschen, Risern und Texturen den Akkord.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 140 }, { "clear", 10 }, { "gate", 40 } } },
        { "CREATIVE", "Shimmer Tail", "Lange, farbige gestimmte Fahne hinter dem Original.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 180 }, { "clear", 30 }, { "mix", 50 }, { "rangeLow", 200 } } },
        { "CREATIVE", "Wide Pad", "Nur die Stereo-Seiten gehen in die Tonart, die Mitte bleibt unberührt.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 120 }, { "clear", 20 }, { "stereo", 2 } } },

        // --- Drums & Vocals -------------------------------------------------------------------------
        { "DRUMS & VOCALS", "Drum Tuner", "Stimmt Drum-Loops auf den Song, die Anschläge bleiben knackig.",
          { { "mode", 0 }, { "strength", 70 }, { "color", 60 }, { "clear", 85 }, { "rangeLow", 40 }, { "rangeHigh", 9000 } } },
        { "DRUMS & VOCALS", "Vocal Choir", "Zieht Stimmen in die Tonart, die Stimme bleibt natürlich.",
          { { "mode", 1 }, { "strength", 80 }, { "color", 110 }, { "clear", 40 }, { "shift", 60 }, { "formant", 100 }, { "mix", 70 } } },
        { "DRUMS & VOCALS", "Snap Hard", "Jede schiefe Note wird auf die Tonart gezogen.",
          { { "mode", 1 }, { "strength", 100 }, { "color", 100 }, { "clear", 30 }, { "shift", 100 }, { "formant", 100 }, { "gate", 30 } } },

        // --- Pitchmap -------------------------------------------------------------------------------
        { "PITCHMAP", "Major to Minor", "Macht aus Dur Moll (bezogen auf KEY).",
          { { "mode", 1 }, { "strength", 40 }, { "color", 40 }, { "clear", 50 }, { "formant", 100 } }, FactoryPreset::Map::majorToMinor },
        { "PITCHMAP", "Minor to Major", "Macht aus Moll Dur (bezogen auf KEY).",
          { { "mode", 1 }, { "strength", 40 }, { "color", 40 }, { "clear", 50 }, { "formant", 100 } }, FactoryPreset::Map::minorToMajor },
    };
    return presets;
}
} // namespace prizm
