#pragma once

#include <utility>
#include <vector>

namespace prizm
{
// Werkspresets. Werte in Parametereinheiten (Prozent, Hz, Index).
// Nicht genannte Parameter gehen auf ihren Standard zurück. Die Tonart (Key, Scale,
// Custom, Tune, MIDI KEY, Sidechain) bleibt immer stehen, damit man im Song durchblättern kann.
struct FactoryPreset
{
    enum class Map { reset, majorToMinor, minorToMajor };

    const char* name;
    const char* hint;
    std::vector<std::pair<const char*, float>> values;
    Map map = Map::reset;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    // mode: 0 = Zero Latency, 1 = High Quality; stereo: 0 = Stereo, 1 = Mid, 2 = Side
    static const std::vector<FactoryPreset> presets {
        { "Init", "Default settings", {} },
        { "Pad Sweetener", "Pads and keys sit in key without losing body",
          { { "mode", 1 }, { "strength", 90 }, { "color", 80 }, { "clear", 20 }, { "rangeLow", 120 } } },
        { "Noise to Chord", "Turns noise, risers and textures into the chord",
          { { "mode", 1 }, { "strength", 100 }, { "color", 140 }, { "clear", 10 }, { "gate", 40 } } },
        { "Drum Tuner", "Tunes drum loops to the song, keeps the punch",
          { { "mode", 0 }, { "strength", 70 }, { "color", 60 }, { "clear", 85 }, { "rangeLow", 40 }, { "rangeHigh", 9000 } } },
        { "Bass Keeper", "Everything above the bass goes in key, kick and sub stay clean",
          { { "mode", 0 }, { "strength", 100 }, { "color", 70 }, { "clear", 40 }, { "rangeLow", 220 } } },
        { "Vocal Choir", "Pulls vocals onto the key, keeps the voice natural",
          { { "mode", 1 }, { "strength", 80 }, { "color", 110 }, { "clear", 40 }, { "shift", 60 }, { "formant", 100 }, { "mix", 70 } } },
        { "Snap Hard", "Moves every out-of-key note onto the key",
          { { "mode", 1 }, { "strength", 100 }, { "color", 100 }, { "clear", 30 }, { "shift", 100 }, { "formant", 100 }, { "gate", 30 } } },
        { "Major to Minor", "Turns a major part into minor (relative to KEY)",
          { { "mode", 1 }, { "strength", 40 }, { "color", 40 }, { "clear", 50 }, { "formant", 100 } }, FactoryPreset::Map::majorToMinor },
        { "Minor to Major", "Turns a minor part into major (relative to KEY)",
          { { "mode", 1 }, { "strength", 40 }, { "color", 40 }, { "clear", 50 }, { "formant", 100 } }, FactoryPreset::Map::minorToMajor },
        { "Shimmer Tail", "Long, colourful tuned tail behind the original",
          { { "mode", 1 }, { "strength", 100 }, { "color", 180 }, { "clear", 30 }, { "mix", 50 }, { "rangeLow", 200 } } },
        { "Wide Pad", "Only the stereo sides go in key, the centre stays untouched",
          { { "mode", 1 }, { "strength", 100 }, { "color", 120 }, { "clear", 20 }, { "stereo", 2 } } },
        { "Live Safe", "Zero latency, gentle and transient-friendly for live use",
          { { "mode", 0 }, { "strength", 80 }, { "color", 50 }, { "clear", 50 } } },
    };
    return presets;
}
} // namespace prizm
