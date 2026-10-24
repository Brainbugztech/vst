#pragma once

#include "Scales.h"

namespace prizm
{
// Snapshot of the controls the engines need, taken once per audio block.
struct EngineParams
{
    PitchMask mask = kAllPitches; // absolute pitch classes that are "in key"
    float strength = 1.0f;        // 0..1   how hard the sound is pulled into the key
    float color = 0.6f;           // 0..2   resonance / sharpening; above 1 adds a decay tail
    float clear = 0.3f;           // 0..1   transient protection, shorter tails on new hits
};
} // namespace prizm
