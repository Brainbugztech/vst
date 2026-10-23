#pragma once

// Automatisch erzeugt von tools/level_match.cpp – nicht von Hand bearbeiten.
// Output-Pegel pro Werkspreset, damit alle Presets so laut sind wie das Original.

#include <string>

namespace bfz
{

inline float presetLevelDb (const std::string& name)
{
    if (name == "Init") return 0.6f;
    if (name == "Clean Split") return 0.0f;
    if (name == "Gentle Width") return -0.2f;
    if (name == "Warm Glue") return -0.0f;
    if (name == "Butterworth Bite") return 1.2f;
    if (name == "Linkwitz Clean Fuzz") return -4.7f;
    if (name == "Chebyshev Edge") return -1.0f;
    if (name == "Chebyshev II Notch") return 1.5f;
    if (name == "Elliptic Razor") return -1.8f;
    if (name == "Bessel Smear") return -0.0f;
    if (name == "Legendre Steep") return 2.1f;
    if (name == "Gaussian Melt") return 1.1f;
    if (name == "Splat Attack") return -4.1f;
    if (name == "Classic Fuzz") return 1.0f;
    if (name == "Fold Metal") return 1.1f;
    if (name == "Diode Grit") return 0.7f;
    if (name == "Crunch Box") return -0.6f;
    if (name == "Tube Glow") return 0.0f;
    if (name == "Tape Warmth") return -0.2f;
    if (name == "Sub Safe Fuzz") return -4.6f;
    if (name == "Reese Grinder") return -7.2f;
    if (name == "808 Crunch") return -5.2f;
    if (name == "Psy Bass Bite") return -4.4f;
    if (name == "Hitech Bass Tight") return -0.8f;
    if (name == "Drum Bus Smash") return -0.5f;
    if (name == "Hat Shimmer") return -0.1f;
    if (name == "Snare Crack") return -0.4f;
    if (name == "Kick Clean Top Dirt") return -1.9f;
    if (name == "Room Crush") return 0.3f;
    if (name == "Lead Fuzz Wide") return 0.7f;
    if (name == "Pad Haze") return 1.1f;
    if (name == "Vocal Air") return -0.1f;
    if (name == "Pluck Shine") return 0.6f;
    if (name == "Acid Squelch") return -3.1f;
    if (name == "Lo-Fi Crush Verb") return -0.0f;
    if (name == "Space Highs") return 0.3f;
    if (name == "Chorus Mids") return 1.6f;
    if (name == "Bitwide") return -0.6f;
    if (name == "Total Meltdown") return 6.2f;
    if (name == "Mono Safe Galaxy") return -0.1f;
    return 0.0f;
}

} // namespace bfz
