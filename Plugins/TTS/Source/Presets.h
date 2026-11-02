#pragma once

// Werkspresets für Brainbugz TTS (nur Sound-Einstellungen, nicht Text und Stimme)

#include <utility>
#include <vector>

namespace ttspresets
{
struct Preset
{
    const char* name;
    const char* group;
    std::vector<std::pair<const char*, float>> set; // Parameter-ID -> Wert (Choice/Bool: Index bzw. 0/1)
};

// EQ-Typen: 0 Low Cut, 1 Low Shelf, 2 Bell, 3 High Shelf, 4 High Cut
// Delay-Raster: 0 1/32, 1 1/16T, 2 1/16, 3 1/16D, 4 1/8T, 5 1/8, 6 1/8D, 7 1/4T, 8 1/4, 9 1/4D, 10 1/2, 11 1/2D, 12 1/1
inline const std::vector<Preset>& factory()
{
    static const std::vector<Preset> p = {
        { "Init", "Basics", {} },
        { "Clean Narrator", "Basics", { { "eq1Type", 0 }, { "eq1Freq", 80 }, { "eq3Gain", 2.5f }, { "eq3Freq", 8000 },
                                         { "rvSize", 30 }, { "rvMix", 10 } } },
        { "Deep Announcer", "Characters", { { "pitch", -5 }, { "formant", 0.92f }, { "eq1Gain", 3 }, { "eq1Freq", 140 },
                                             { "rvSize", 45 }, { "rvMix", 12 } } },
        { "Monster", "Characters", { { "pitch", -12 }, { "formant", 0.72f }, { "eq1Gain", 4 }, { "eq1Freq", 120 },
                                      { "eq3Gain", -3 }, { "rvSize", 70 }, { "rvMix", 20 } } },
        { "Chipmunk", "Characters", { { "pitch", 9 }, { "formant", 1.35f }, { "speed", 1.15f }, { "rvMix", 8 } } },
        { "Alien Transmission", "Characters", { { "pitch", -3 }, { "formant", 1.6f },
                                                 { "eq1Type", 0 }, { "eq1Freq", 300 }, { "eq2Freq", 2500 }, { "eq2Gain", 6 }, { "eq2Q", 2 },
                                                 { "eq3Type", 4 }, { "eq3Freq", 6000 },
                                                 { "dlOn", 1 }, { "dlDiv", 6 }, { "dlFeedback", 45 }, { "dlMix", 30 } } },
        { "Slow Motion", "Characters", { { "speed", 0.4f }, { "pitch", -7 }, { "formant", 0.9f }, { "rvSize", 65 }, { "rvMix", 25 } } },
        { "Lo-Fi Phone", "FX", { { "eq1Type", 0 }, { "eq1Freq", 400 }, { "eq1Q", 0.9f }, { "eq2Freq", 1800 }, { "eq2Gain", 5 },
                                  { "eq2Q", 1.2f }, { "eq3Type", 4 }, { "eq3Freq", 3400 }, { "rvOn", 0 } } },
        { "Big Hall", "FX", { { "rvSize", 90 }, { "rvDamp", 30 }, { "rvPre", 60 }, { "rvLow", 250 }, { "rvMix", 35 } } },
        { "Ping Pong Hype", "FX", { { "dlOn", 1 }, { "dlDiv", 6 }, { "dlFeedback", 50 }, { "dlMix", 35 }, { "dlPing", 1 },
                                     { "dlLow", 300 }, { "dlHigh", 6000 }, { "rvMix", 12 } } },
        { "Texture Stretch", "Texture", { { "speed", 0.15f }, { "grOn", 1 }, { "grMix", 85 }, { "grSize", 120 }, { "grFlux", 45 },
                                           { "grDensity", 6 }, { "grSpray", 80 }, { "grWidth", 70 }, { "rvMix", 25 } } },
        { "Choir Cloud", "Texture", { { "speed", 0.25f }, { "grOn", 1 }, { "grMix", 90 }, { "grSize", 300 }, { "grFlux", 60 },
                                       { "grDensity", 10 }, { "grSpray", 300 }, { "grPitch", 0.3f }, { "grOctave", 35 }, { "grWidth", 100 },
                                       { "rvSize", 85 }, { "rvMix", 35 } } },
        { "Glitch Stutter", "Texture", { { "grOn", 1 }, { "grMix", 80 }, { "grSize", 25 }, { "grFlux", 80 }, { "grDensity", 2 },
                                          { "grSpray", 150 }, { "grReverse", 40 }, { "grWidth", 80 }, { "grLfoRate", 4 }, { "grLfoDepth", 40 } } },
        { "Shimmer Drone", "Texture", { { "speed", 0.02f }, { "grOn", 1 }, { "grMix", 100 }, { "grSize", 400 }, { "grFlux", 50 },
                                         { "grDensity", 12 }, { "grSpray", 200 }, { "grOctave", 60 }, { "grDrift", 60 }, { "grWidth", 100 },
                                         { "rvSize", 95 }, { "rvMix", 45 } } },
        { "Robot Scanner", "Texture", { { "formant", 1.1f }, { "grOn", 1 }, { "grMix", 60 }, { "grSize", 40 }, { "grFlux", 0 },
                                         { "grDensity", 2 }, { "grLfoRate", 6 }, { "grLfoDepth", 60 },
                                         { "dlOn", 1 }, { "dlDiv", 2 }, { "dlFeedback", 30 }, { "dlMix", 20 } } },
        { "Whisper Space", "Texture", { { "formant", 1.15f }, { "grOn", 1 }, { "grMix", 50 }, { "grSize", 60 }, { "grFlux", 90 },
                                         { "grSpray", 400 }, { "grWidth", 100 }, { "grReverse", 20 }, { "rvSize", 80 }, { "rvMix", 40 } } },
    };
    return p;
}
} // namespace ttspresets
