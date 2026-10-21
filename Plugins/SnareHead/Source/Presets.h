// SnareHead factory presets
#pragma once
#include "SnareEngine.h"
#include <vector>
#include <string>

namespace snarehead {

struct Preset {
    std::string name;
    bool xtremeNoisy; // shows the "XTREME NOISY" sticker
    Params p;
};

inline std::vector<Preset> factoryPresets() {
    std::vector<Preset> v;
    auto add = [&](const char* name, bool xn, auto fn) { Params p; fn(p); v.push_back({name, xn, p}); };

    // ---------------- ringing psy / hitech snares (in the style of the reference pack)
    add("Android 750", false, [](Params& p) {
        p.tune = 800; p.spread = 1.0f; p.metal = 0; p.punch = 0.35f;
        p.noiseLevel = 0.75f; p.noiseTone = 0.8f; p.hold = 300; p.release = 120;
        p.eqFreq = 760; p.eqGain = 6; p.eqQ = 4; p.verbMix = 0; p.width = 0; p.volume = 3.7f; });
    add("Android 1350", false, [](Params& p) {
        p.tune = 1320; p.spread = 0.65f; p.punch = 0.3f;
        p.noiseLevel = 0.4f; p.noiseTone = 0.78f; p.hold = 170; p.release = 12;
        p.eqFreq = 1350; p.eqGain = 5; p.eqQ = 5; p.verbMix = 0; p.width = 0.8f; p.volume = 3.0f; });
    add("Hitech Tick", false, [](Params& p) {
        p.tune = 1450; p.spread = 0.4f; p.punch = 0.8f; p.noiseLevel = 0.6f; p.noiseTone = 0.9f;
        p.hold = 45; p.release = 15; p.eqFreq = 1450; p.eqGain = 4; p.eqQ = 3;
        p.verbSize = 0.3f; p.verbMix = 0.15f; p.width = 0.6f; p.volume = 4.4f; });
    add("Psy Ring", false, [](Params& p) {
        p.tune = 880; p.spread = 0.55f; p.metal = 0.15f; p.punch = 0.4f; p.noiseLevel = 0.45f; p.noiseTone = 0.65f;
        p.hold = 220; p.release = 80; p.eqLink = true; p.eqGain = 9; p.eqQ = 6;
        p.verbSize = 0.5f; p.verbMix = 0.25f; p.width = 0.5f; p.volume = 4.7f; });
    add("Night Shift", false, [](Params& p) {
        p.tune = 720; p.spread = 0.8f; p.punch = 0.3f; p.noiseLevel = 0.5f; p.noiseTone = 0.5f;
        p.hold = 380; p.release = 200; p.eqFreq = 720; p.eqGain = 5; p.eqQ = 3;
        p.verbSize = 0.8f; p.verbMix = 0.35f; p.verbSync = 8; p.verbCut = 0.15f; p.width = 0.6f; p.volume = 2.6f; });
    add("Glass Box", false, [](Params& p) {
        p.tune = 1180; p.spread = 0.3f; p.metal = 0.25f; p.punch = 0.5f; p.noiseLevel = 0.3f; p.noiseTone = 0.7f;
        p.hold = 200; p.release = 10; p.eqFreq = 1180; p.eqGain = 12; p.eqQ = 8; p.verbMix = 0.1f; p.width = 0.7f; p.volume = 4.4f; });
    add("Gate 80s", false, [](Params& p) {
        p.tune = 950; p.spread = 0.7f; p.punch = 0.6f; p.noiseLevel = 0.7f; p.noiseTone = 0.6f;
        p.hold = 90; p.release = 40; p.eqFreq = 950; p.eqGain = 4; p.eqQ = 2;
        p.verbSize = 0.9f; p.verbMix = 0.55f; p.verbGate = 280; p.verbCut = 0.05f; p.width = 0.7f; p.volume = 3.4f; });
    add("Neon Rim", false, [](Params& p) {
        p.tune = 1550; p.spread = 0.25f; p.metal = 0.35f; p.punch = 1.0f; p.noiseLevel = 0.4f; p.noiseTone = 0.8f;
        p.hold = 60; p.release = 30; p.eqFreq = 1550; p.eqGain = 6; p.eqQ = 5; p.verbMix = 0.12f; p.width = 0.5f; p.volume = 5.7f; });
    add("Short Circuit", false, [](Params& p) {
        p.tune = 1050; p.spread = 0.9f; p.punch = 0.5f; p.noiseLevel = 0.6f; p.noiseTone = 0.75f;
        p.hold = 120; p.release = 5; p.noisy = 0.25f; p.eqFreq = 1050; p.eqGain = 5; p.eqQ = 4; p.width = 0.7f; p.volume = 2.3f; });
    add("Float Ring", false, [](Params& p) {
        p.tune = 760; p.spread = 0.45f; p.punch = 0.25f; p.noiseLevel = 0.35f; p.noiseTone = 0.6f;
        p.hold = 500; p.release = 300; p.eqFreq = 760; p.eqGain = 8; p.eqQ = 5;
        p.verbSize = 0.7f; p.verbMix = 0.4f; p.width = 0.9f; p.volume = -2.0f; });
    add("Tight Tin", false, [](Params& p) {
        p.tune = 1250; p.spread = 0.5f; p.metal = 0.4f; p.punch = 0.6f; p.noiseLevel = 0.55f; p.noiseTone = 0.7f;
        p.hold = 80; p.release = 20; p.noisy = 0.15f; p.eqFreq = 1250; p.eqGain = 4; p.eqQ = 4; p.width = 0.5f; p.volume = 6.0f; });
    add("Wide Ring", false, [](Params& p) {
        p.tune = 990; p.spread = 0.75f; p.punch = 0.4f; p.noiseLevel = 0.5f; p.noiseTone = 0.7f;
        p.hold = 260; p.release = 60; p.eqFreq = 990; p.eqGain = 6; p.eqQ = 4;
        p.verbSize = 0.5f; p.verbMix = 0.2f; p.width = 1.0f; p.volume = -0.8f; });

    // ---------------- industrial / XTREME NOISY
    add("Tin Can", true, [](Params& p) {
        p.tune = 1100; p.spread = 0.3f; p.metal = 0.6f; p.punch = 0.6f; p.noiseLevel = 0.4f; p.noiseTone = 0.6f;
        p.hold = 140; p.release = 50; p.noisy = 0.55f; p.eqFreq = 1100; p.eqGain = 5; p.eqQ = 4; p.width = 0.5f; p.volume = 6.0f; });
    add("Steel Drum Factory", true, [](Params& p) {
        p.tune = 820; p.spread = 0.2f; p.metal = 0.45f; p.punch = 0.5f; p.noiseLevel = 0.35f; p.noiseTone = 0.5f;
        p.hold = 350; p.release = 250; p.noisy = 0.3f; p.eqLink = true; p.eqGain = 6; p.eqQ = 5;
        p.verbSize = 0.6f; p.verbMix = 0.3f; p.width = 0.6f; p.volume = 4.4f; });
    add("Rust Plate", true, [](Params& p) {
        p.tune = 960; p.spread = 0.6f; p.metal = 0.8f; p.punch = 0.4f; p.noiseLevel = 0.5f; p.noiseTone = 0.55f;
        p.hold = 200; p.release = 300; p.noisy = 0.45f; p.eqFreq = 960; p.eqGain = 4; p.eqQ = 3;
        p.verbSize = 0.75f; p.verbMix = 0.45f; p.width = 0.8f; p.volume = 2.6f; });
    add("Pipe Hit", true, [](Params& p) {
        p.tune = 1400; p.spread = 0.15f; p.metal = 0.7f; p.punch = 0.9f; p.noiseLevel = 0.3f; p.noiseTone = 0.7f;
        p.hold = 100; p.release = 180; p.noisy = 0.35f; p.eqFreq = 1400; p.eqGain = 6; p.eqQ = 6;
        p.verbSize = 0.4f; p.verbMix = 0.15f; p.width = 0.5f; p.volume = 6.0f; });
    add("Clang Machine", true, [](Params& p) {
        p.tune = 1240; p.spread = 0.5f; p.metal = 1.0f; p.punch = 0.7f; p.noiseLevel = 0.45f; p.noiseTone = 0.65f;
        p.hold = 160; p.release = 60; p.noisy = 0.7f; p.eqFreq = 1240; p.eqGain = 3; p.eqQ = 3; p.width = 0.8f; p.volume = 5.1f; });
    add("Bunker Hall", true, [](Params& p) {
        p.tune = 870; p.spread = 0.7f; p.metal = 0.3f; p.punch = 0.5f; p.noiseLevel = 0.55f; p.noiseTone = 0.6f;
        p.hold = 150; p.release = 80; p.noisy = 0.3f; p.eqFreq = 870; p.eqGain = 5; p.eqQ = 4;
        p.verbSize = 1.0f; p.verbMix = 0.65f; p.width = 0.8f; p.volume = 4.8f; });
    add("Scrap Metal", true, [](Params& p) {
        p.tune = 1600; p.spread = 0.6f; p.metal = 0.9f; p.punch = 0.8f; p.noiseLevel = 0.6f; p.noiseTone = 0.7f;
        p.hold = 110; p.release = 30; p.noisy = 1.0f; p.eqFreq = 1600; p.eqGain = 3; p.eqQ = 3; p.width = 0.9f; p.volume = 4.8f; });
    add("Foundry Gate", true, [](Params& p) {
        p.tune = 740; p.spread = 0.5f; p.metal = 0.55f; p.punch = 0.6f; p.noiseLevel = 0.5f; p.noiseTone = 0.55f;
        p.hold = 120; p.release = 60; p.noisy = 0.5f; p.eqFreq = 740; p.eqGain = 5; p.eqQ = 4;
        p.verbSize = 0.95f; p.verbMix = 0.6f; p.verbSync = 7; p.verbCut = 0.03f; p.width = 0.8f; p.volume = 5.4f; });
    return v;
}

} // namespace snarehead
