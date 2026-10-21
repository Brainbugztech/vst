#pragma once

#include "BleepGenerator.h"
#include "Arpeggiator.h"   // SplitMix, mixKey

// Charaktere fuer GENERATE: 8 typische Bleep-Arten. Jeder Charakter wuerfelt seine Werte in
// einem eigenen Bereich, so klingen auch zwei Slots vom selben Typ verschieden.
// Slot 1 bleibt nah an deinen Reglern, Slot 2-4 bekommen je einen anderen Charakter.
// "Variation" bestimmt, wie weit die Slots von deinen Reglern Richtung Charakter wandern.

namespace bleep
{

inline constexpr int kNumCharacters = 14;

inline const char* characterName (int c)
{
    static const char* names[kNumCharacters] = { "Blip", "Chirp", "Warble", "Zap", "Chatter", "Metal", "Bloop", "Droid",
                                                 "Chip-Arp", "Ringmod", "Throw", "Stretch", "Knatter", "Long Tone" };
    return names[juce::jlimit (0, kNumCharacters - 1, c)];
}

inline GenParams makeCharacter (int c, SplitMix& rng)
{
    auto R  = [&rng] (double a, double b) { return (float) (a + (b - a) * rng.uniform()); };
    auto RI = [&rng] (int a, int b) { return a + rng.below (b - a + 1); };
    auto sign = [&rng] { return rng.uniform() < 0.5 ? -1.0f : 1.0f; };

    GenParams p;
    p.attack = 1.0f; p.sustain = 0.0f; p.release = 30.0f;
    p.pd = 0.0f; p.lfoPitch = 0.0f; p.lfoPd = 0.0f; p.noise = 0.0f;
    p.fltCut = 18000.0f; p.fltEnv = 0.0f; p.ratchets = 1; p.modFine = 0.0f;
    p.chipArp = 0; p.pitchSteps = false; p.pwmDepth = 0.0f; p.crushBits = 0.0f; p.crushDown = 1;
    p.ringRatio = 0.0f; p.ringMix = 0.0f; p.gateRate = 0.0f; p.gateDepth = 0.0f;
    p.echoMs = 0.0f; p.echoFb = 0.0f; p.stretch = 0.0f;

    switch (c)
    {
        case 0: // Blip: kurz, rund, kleiner Pitch-Fall
            p.shape = R (0.0, 1.2); p.sqOct = RI (-1, 1); p.pd = R (0.0, 0.3); p.modOct = RI (-1, 1);
            p.penvAmt = R (5, 18); p.penvDecay = R (10, 35); p.decay = R (50, 110); p.drive = R (0.1, 0.3);
            break;
        case 1: // Chirp: langer Sweep hoch oder runter
            p.shape = R (0.5, 1.5); p.sqOct = RI (-1, 1); p.pd = R (0.0, 0.25); p.modOct = RI (-2, 0);
            p.penvAmt = sign() * R (18, 40); p.penvDecay = R (80, 260); p.decay = R (150, 320); p.drive = R (0.1, 0.3);
            break;
        case 2: // Warble: wackelnder Ton durch schnellen LFO
            p.shape = R (1.0, 2.2); p.sqOct = RI (-1, 0); p.pd = R (0.1, 0.4); p.modOct = -3;
            p.lfoRate = R (6, 22); p.lfoPitch = R (2, 7); p.penvAmt = R (0, 8); p.penvDecay = 40.0f;
            p.decay = R (150, 300); p.drive = R (0.2, 0.4);
            break;
        case 3: // Zap: Laser-Schuss, Saw mit Rauschen und Filter-Sweep
            p.shape = R (2.5, 3.0); p.sqOct = RI (0, 1); p.penvAmt = R (30, 48); p.penvDecay = R (15, 50);
            p.noise = R (0.2, 0.5); p.fltCut = R (600, 2000); p.fltRes = R (0.5, 0.85); p.fltEnv = R (0.5, 0.9);
            p.fltDecay = R (30, 90); p.decay = R (80, 160); p.drive = R (0.3, 0.6); p.modOct = -3;
            break;
        case 4: // Chatter: schnelle Wiederholungen mit Tonschritt ("bi-bi-bip")
        {
            static constexpr float stepsList[] = { -12.0f, -7.0f, -5.0f, -3.0f, 2.0f, 3.0f, 5.0f, 7.0f, 12.0f };
            p.shape = R (0.0, 2.0); p.sqOct = RI (-1, 1); p.ratchets = RI (3, 6); p.ratchetMs = R (35, 70);
            p.ratchetStep = stepsList[rng.below (9)]; p.decay = R (30, 60); p.penvAmt = R (0, 12); p.penvDecay = 20.0f;
            p.pd = R (0.0, 0.3); p.modOct = RI (-1, 1); p.drive = R (0.1, 0.3);
            break;
        }
        case 5: // Metal: Phasen-FM mit krummem Verhaeltnis, glockig bis blechern
            p.shape = R (1.0, 2.2); p.sqOct = RI (-1, 0); p.modOct = RI (0, 2); p.modFine = R (0.3, 11.0);
            p.pd = R (0.45, 0.8); p.penvAmt = R (0, 12); p.penvDecay = 30.0f; p.decay = R (120, 260);
            p.lfoRate = R (3, 12); p.lfoPd = R (-0.3, 0.3); p.drive = R (0.2, 0.4);
            break;
        case 6: // Bloop: tiefer Ton mit resonantem Filter-Sweep
            p.shape = R (2.0, 3.0); p.sqOct = RI (-2, -1); p.fltCut = R (150, 450); p.fltRes = R (0.7, 0.92);
            p.fltEnv = R (0.5, 0.85); p.fltDecay = R (60, 180); p.penvAmt = -R (0, 12); p.penvDecay = 60.0f;
            p.decay = R (140, 280); p.drive = 0.4f; p.modOct = -3;
            break;
        case 7: // Droid: R2-artiges Plappern
            p.shape = R (0.0, 0.6); p.sqOct = RI (0, 1); p.lfoRate = R (15, 40); p.lfoPitch = R (4, 12);
            p.penvAmt = sign() * R (10, 30); p.penvDecay = R (40, 150); p.ratchets = RI (1, 3);
            p.ratchetMs = R (60, 110); p.ratchetStep = sign() * R (2, 9); p.decay = R (180, 400);
            p.pd = R (0.0, 0.2); p.modOct = RI (-1, 1); p.drive = R (0.1, 0.25);
            break;
        case 8: // Chip-Arp: Pulswelle, Akkord-Arpeggio im Ton, Stufen-Tonhoehe, PWM
            p.shape = R (1.9, 2.1); p.pw = R (0.12, 0.5); p.sqOct = RI (-1, 1); p.chipArp = RI (1, 6);
            p.chipArpMs = R (16, 45); p.pitchSteps = true; p.pwmDepth = R (0.0, 0.3); p.lfoRate = R (2, 8);
            p.penvAmt = rng.uniform() < 0.5 ? 0.0f : R (5, 12); p.penvDecay = 20.0f; p.decay = R (120, 450);
            p.crushBits = rng.uniform() < 0.5 ? R (5, 8) : 0.0f; p.drive = 0.15f; p.modOct = -3;
            break;
        case 9: // Ringmod: klirrend und unharmonisch
            p.shape = R (0.0, 2.0); p.sqOct = RI (-1, 1); p.ringRatio = R (0.3, 5.0); p.ringMix = R (0.6, 1.0);
            p.penvAmt = sign() * R (0, 24); p.penvDecay = R (20, 120); p.decay = R (80, 300);
            p.lfoRate = R (2, 30); p.lfoPitch = R (0, 2); p.drive = R (0.2, 0.4); p.modOct = -3;
            break;
        case 10: // Throw: kurzer Ton mit einzelnem Delay-Echo
            p.shape = R (0.0, 2.5); p.sqOct = RI (-1, 1); p.penvAmt = R (6, 24); p.penvDecay = R (10, 40);
            p.decay = R (30, 80); p.echoMs = R (60, 180); p.echoFb = R (0.35, 0.7); p.drive = R (0.1, 0.3);
            p.modOct = RI (-2, 0); p.pd = R (0.0, 0.3);
            break;
        case 11: // Stretch: verzieht sich, wird zum Ende langsamer und tiefer
            p.shape = R (0.5, 2.5); p.sqOct = RI (-1, 1); p.penvAmt = sign() * R (5, 20); p.penvDecay = R (100, 300);
            p.decay = R (200, 450); p.stretch = R (0.4, 0.9); p.lfoRate = R (4, 9); p.lfoPitch = R (0.3, 1.5);
            p.drive = R (0.2, 0.4); p.modOct = -3; p.pd = R (0.1, 0.4);
            break;
        case 12: // Knatter: schnell zerhackt, digital
            p.shape = R (1.5, 3.0); p.sqOct = RI (-1, 1); p.gateRate = R (25, 90); p.gateDepth = R (0.7, 1.0);
            p.penvAmt = sign() * R (0, 30); p.penvDecay = R (30, 150); p.decay = R (100, 350);
            p.crushBits = rng.uniform() < 0.4 ? R (4, 7) : 0.0f; p.crushDown = RI (1, 4); p.drive = 0.3f; p.modOct = -3;
            break;
        default: // Long Tone: laengerer Ton mit Vibrato, gehalten
            p.shape = R (0.0, 2.0); p.sqOct = RI (-1, 0); p.penvAmt = R (0, 7); p.penvDecay = 50.0f;
            p.decay = R (300, 600); p.sustain = R (0.3, 0.6); p.release = R (150, 350);
            p.lfoRate = R (4, 7); p.lfoPitch = R (0.2, 0.8); p.pwmDepth = R (0.0, 0.25); p.drive = 0.2f; p.modOct = -3;
            break;
    }

    // Wuerze: zufaellige Zusatz-Bausteine, damit sich Charaktere mischen
    if (rng.uniform() < 0.25 && p.echoMs <= 0.0f) { p.echoMs = R (70, 200); p.echoFb = R (0.3, 0.6); }
    if (rng.uniform() < 0.2 && p.crushBits <= 0.0f) { p.crushBits = R (5, 9); p.crushDown = RI (1, 3); }
    if (rng.uniform() < 0.15 && p.ringMix <= 0.0f) { p.ringRatio = R (0.5, 4.0); p.ringMix = R (0.3, 0.7); }
    if (rng.uniform() < 0.12 && p.gateRate <= 0.0f) { p.gateRate = R (30, 80); p.gateDepth = R (0.5, 0.9); }
    if (rng.uniform() < 0.3) p.pitchSteps = true;
    if (rng.uniform() < 0.1 && p.stretch <= 0.0f) p.stretch = R (0.3, 0.7);
    return p;
}

// Mischt zwei Einstellungen: t = 0 -> a, t = 1 -> b
inline GenParams blend (const GenParams& a, const GenParams& b, float t)
{
    auto L = [t] (float x, float y) { return x + (y - x) * t; };
    auto Li = [t] (int x, int y) { return juce::roundToInt ((float) x + (float) (y - x) * t); };
    auto Lexp = [t] (float x, float y) { return std::exp (std::log (juce::jmax (0.01f, x)) * (1.0f - t) + std::log (juce::jmax (0.01f, y)) * t); };
    GenParams p = a;
    p.sqOct = Li (a.sqOct, b.sqOct);       p.shape = L (a.shape, b.shape);       p.pw = L (a.pw, b.pw);
    p.modOct = Li (a.modOct, b.modOct);    p.modFine = L (a.modFine, b.modFine); p.pd = L (a.pd, b.pd);
    p.pitchLink = t < 0.5f ? a.pitchLink : b.pitchLink;
    p.penvAmt = L (a.penvAmt, b.penvAmt);  p.penvDecay = Lexp (a.penvDecay, b.penvDecay);
    p.ratchets = Li (a.ratchets, b.ratchets); p.ratchetMs = L (a.ratchetMs, b.ratchetMs); p.ratchetStep = L (a.ratchetStep, b.ratchetStep);
    p.lfoRate = Lexp (a.lfoRate, b.lfoRate); p.lfoPitch = L (a.lfoPitch, b.lfoPitch); p.lfoPd = L (a.lfoPd, b.lfoPd);
    p.noise = L (a.noise, b.noise);        p.fltCut = Lexp (a.fltCut, b.fltCut);  p.fltRes = L (a.fltRes, b.fltRes);
    p.fltEnv = L (a.fltEnv, b.fltEnv);     p.fltDecay = Lexp (a.fltDecay, b.fltDecay); p.drive = L (a.drive, b.drive);
    p.chipArp = t < 0.5f ? a.chipArp : b.chipArp; p.chipArpMs = L (a.chipArpMs, b.chipArpMs);
    p.pitchSteps = t < 0.5f ? a.pitchSteps : b.pitchSteps; p.pwmDepth = L (a.pwmDepth, b.pwmDepth);
    p.crushBits = t < 0.5f ? a.crushBits : b.crushBits; p.crushDown = t < 0.5f ? a.crushDown : b.crushDown;
    p.ringRatio = t < 0.5f ? a.ringRatio : b.ringRatio; p.ringMix = L (a.ringMix, b.ringMix);
    p.gateRate = t < 0.5f ? a.gateRate : b.gateRate; p.gateDepth = L (a.gateDepth, b.gateDepth);
    p.echoMs = t < 0.5f ? a.echoMs : b.echoMs; p.echoFb = L (a.echoFb, b.echoFb); p.stretch = L (a.stretch, b.stretch);
    p.attack = Lexp (a.attack, b.attack);  p.decay = Lexp (a.decay, b.decay);
    p.sustain = L (a.sustain, b.sustain);  p.release = Lexp (a.release, b.release);
    return p;
}

// Kleine Zufallsabweichung um eine Einstellung herum
inline GenParams jitter (GenParams p, float amount, SplitMix& rng)
{
    auto r = [&rng] { return (float) (rng.uniform() * 2.0 - 1.0); };
    const float v = juce::jlimit (0.0f, 1.0f, amount);
    p.shape     = juce::jlimit (0.0f, 3.0f, p.shape + r() * 0.6f * v);
    p.pw        = juce::jlimit (0.05f, 0.95f, p.pw + r() * 0.3f * v);
    p.pd        = juce::jlimit (0.0f, 1.0f, p.pd + r() * 0.3f * v);
    p.penvAmt   = juce::jlimit (-48.0f, 48.0f, p.penvAmt + r() * 12.0f * v);
    p.penvDecay = juce::jlimit (1.0f, 500.0f, p.penvDecay * std::exp2 (r() * 1.2f * v));
    p.lfoRate   = juce::jlimit (0.1f, 500.0f, p.lfoRate * std::exp2 (r() * 1.5f * v));
    p.fltCut    = juce::jlimit (30.0f, 18000.0f, p.fltCut * std::exp2 (r() * 1.5f * v));
    p.decay     = juce::jlimit (5.0f, 2000.0f, p.decay * std::exp2 (r() * 0.8f * v));
    return p;
}

// Erzeugt die Einstellung fuer einen Slot (immer gleich bei gleichem Seed)
inline GenParams slotParams (const GenParams& base, float variation, int seed, int slot, int* characterOut = nullptr)
{
    SplitMix pick (mixKey ((uint64_t) seed, 0x0C4Aull, 0));
    // drei verschiedene Charaktere fuer Slot 2-4
    int order[kNumCharacters];
    for (int i = 0; i < kNumCharacters; ++i)
        order[i] = i;
    for (int i = kNumCharacters - 1; i > 0; --i)
        std::swap (order[i], order[pick.below (i + 1)]);

    SplitMix rng (mixKey ((uint64_t) seed, (uint64_t) slot, 0x5107ull));
    const float v = juce::jlimit (0.0f, 1.0f, variation);
    if (slot == 0)
    {
        if (characterOut != nullptr) *characterOut = -1;
        return jitter (base, v * 0.3f, rng);
    }

    const int c = order[(slot - 1) % kNumCharacters];
    if (characterOut != nullptr) *characterOut = c;
    auto target = makeCharacter (c, rng);
    target.sqGain = base.sqGain;
    return jitter (blend (base, target, v), 0.25f, rng);
}

} // namespace bleep
