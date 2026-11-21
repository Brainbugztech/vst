#pragma once

// Werkspresets für BrainBugz Touch Me!
// 14 FX-Kategorien (linke Tastenspalte). Jedes Preset ist eine Kette aus bis zu 3 Effekten (Infiltrator-Stil).
// Pro Effekt: x0..x1 / y0..y1 = welcher Bereich auf dem Pad liegt (gleich = fester Wert),
// optional ein eigener LFO im Songtempo (lfoDiv/lfoDepth/lfoShape) und vertauschte Achsen (swap).
// px/py/mix: Startposition des Cursors und Dry/Wet beim Laden.

#include "FxEngine.h"
#include <vector>

namespace swarm
{
struct SlotDef
{
    int algo = A_NONE;
    float x0 = 0, x1 = 1, y0 = 0, y1 = 1;
    float p[4] { 0, 0, 0, 0 };
    bool forceSend = false;
    bool swap = false;                 // Pad-Y steuert die X-Achse des Effekts und umgekehrt
    int lfoDiv = -1;                   // Notenwert-Index (kDivBeats), -1 = kein LFO
    float lfoDepth = 0.0f;             // wie weit der LFO den Wert verschiebt (0..1)
    int lfoShape = 0;                  // 0 Sinus, 1 Dreieck, 2 Saw, 3 Rechteck, 4 Sample & Hold
    bool lfoOnY = false;               // LFO auf die Y-Achse des Effekts statt X
};

constexpr int kSlots = 3;

struct PresetDef
{
    const char* name;
    int cat;
    SlotDef s[kSlots];
    float px = 0.5f, py = 0.5f, mix = 1.0f;
};

enum Category { C_FILTER, C_ECHO, C_DRIVE, C_GATE, C_SPACE, C_MODFX, C_LFO, C_LOOPER, C_GRAIN, C_PITCH,
                C_RING, C_RESO, C_GLITCH, C_FREAK, C_NUM };

inline const char* catName (int c)
{
    static const char* n[C_NUM] = { "FILTER", "ECHO", "DRIVE", "GATE", "SPACE", "MOD FX", "LFO", "LOOPER",
                                    "GRAIN", "PITCH", "RING MOD", "RESONATOR", "GLITCH", "FREAK" };
    return c >= 0 && c < C_NUM ? n[c] : "";
}

namespace def
{
    inline SlotDef S (int algo, float x0, float x1, float y0, float y1,
                      float p0 = 0, float p1 = 0, float p2 = 0, float p3 = 0, bool send = false)
    {
        SlotDef s; s.algo = algo; s.x0 = x0; s.x1 = x1; s.y0 = y0; s.y1 = y1;
        s.p[0] = p0; s.p[1] = p1; s.p[2] = p2; s.p[3] = p3; s.forceSend = send; return s;
    }
    // LFO an einen Effekt hängen
    inline SlotDef M (SlotDef s, int div, float depth, int shape = 0, bool onY = false)
    { s.lfoDiv = div; s.lfoDepth = depth; s.lfoShape = shape; s.lfoOnY = onY; return s; }
    // Achsen tauschen
    inline SlotDef SW (SlotDef s) { s.swap = true; return s; }
    constexpr float D (int i) { return (i + 0.5f) / 12.0f; }          // fester Notenwert (Delay-Zeit)
    constexpr float R (int i) { return 1.0f - (i + 0.5f) / 12.0f; }   // feste Rate: rechts = schneller
    constexpr float ST (float st) { return (st + 12.0f) / 24.0f; }    // Halbtöne Pitch
    enum { SIN = 0, TRI = 1, SAW = 2, SQR = 3, SH = 4 };
    inline PresetDef P (const char* n, int c, SlotDef a, SlotDef b = {}, SlotDef cc = {}, float px = 0.5f, float py = 0.5f, float mix = 1.0f)
    {
        PresetDef d { n, c, { a, b, cc } }; d.px = px; d.py = py; d.mix = mix; return d;
    }
}

inline const std::vector<PresetDef>& presets()
{
    using namespace def;
    static const std::vector<PresetDef> p {
        // ---------- Startliste aus dem GUI-Kit (presets_default.json) ----------
        P ("Filter Sweep",       C_FILTER, S (A_LPF, 0, 1, 0, 1, 0.25f), {}, {}, 0.614f, 0.431f, 0.75f),
        P ("Acid Resonance",     C_FILTER, S (A_LPF, 0.05f, 0.9f, 0.55f, 1, 0.8f), S (A_SHAPER, 0.3f, 0.3f, 0, 0, 0.3f, 0.2f), {}, 0.3f, 0.85f, 0.85f),
        P ("Deep Lowpass",       C_FILTER, S (A_LPF, 0, 0.75f, 0, 0.7f), {}, {}, 0.14f, 0.24f, 0.7f),
        P ("Tape Echo",          C_ECHO,   S (A_DELAY, 0.2f, 0.9f, 0.1f, 0.85f, 0.45f, 0, 0.7f, 0.2f), {}, {}, 0.52f, 0.66f, 0.7f),
        P ("Dub Feedback",       C_ECHO,   S (A_DELAY, 0.3f, 1, 0.4f, 0.85f, 0.35f, 1, 0.3f, 0.3f), M (S (A_HPF, 0.15f, 0.15f, 0.3f, 0.3f), 11, 0.15f, TRI), {}, 0.73f, 0.82f, 0.8f),
        P ("Space Tunnel",       C_SPACE,  S (A_REVERB, 0.6f, 1, 0, 1, 0.5f, 1, 0.3f), M (S (A_FLANGE, 0.5f, 0.5f, 0.6f, 0.6f, 0.3f), 11, 0.4f, SIN), S (A_DELAY, D (7), D (7), 0.45f, 0.45f, 0.4f, 1, 0.4f, 0.3f), 0.7f, 0.74f, 0.85f),
        P ("Beat Chopper",       C_GATE,   S (A_SLICER, R (5), R (0), 0, 1, 0, 0.45f), {}, {}, 0.64f, 0.92f, 1.0f),
        P ("Pulse Gate",         C_GATE,   S (A_SLICER, R (8), R (2), 0, 1, 0.33f, 0.55f), M (S (A_BPF, 0.6f, 0.6f, 0.3f, 0.3f), 10, 0.25f, TRI), {}, 0.3f, 0.7f, 0.88f),

        // ---------- FILTER ----------
        P ("Low Pass Sweep",     C_FILTER, S (A_LPF, 0, 1, 0, 1, 0.2f)),
        P ("High Pass Sweep",    C_FILTER, S (A_HPF, 0, 1, 0, 1, 0.2f)),
        P ("Band Pass Scream",   C_FILTER, S (A_BPF, 0.1f, 1, 0.3f, 1, 0.5f)),
        P ("DJ Filter Reso",     C_FILTER, S (A_DJ, 0, 1, 0.3f, 0.95f, 0.3f)),
        P ("Acid Squelch",       C_FILTER, S (A_LPF, 0.1f, 0.8f, 0.7f, 1, 0.9f), S (A_SHAPER, 0.4f, 0.4f, 0.1f, 0.1f, 0.2f, 0.3f), S (A_DELAY, D (7), D (7), 0.35f, 0.35f, 0.5f, 1)),
        P ("Zipper Filter",      C_FILTER, M (S (A_LPF, 0, 1, 0.6f, 1, 0.4f), 2, 0.3f, SH)),
        P ("Squelch Zap",        C_FILTER, M (S (A_LPF, 0.2f, 0.9f, 0.8f, 1, 0.6f), 5, 0.4f, SAW), S (A_DELAY, D (5), D (5), 0.4f, 0.4f, 0.7f, 1)),
        P ("Vowel Morph",        C_FILTER, M (S (A_TALK, 0, 1, 0.4f, 0.95f, 0.5f), 5, 0.15f, TRI)),
        P ("Formant Grit",       C_FILTER, S (A_TALK, 0, 1, 0.5f, 0.9f, 0.3f), S (A_CRUSH, 0.45f, 0.45f, 0.5f, 0.5f, 0.4f)),
        P ("Telephone",          C_FILTER, S (A_BPF, 0.45f, 0.7f, 0.2f, 0.6f, 0.6f), S (A_CRUSH, 0.4f, 0.4f, 0.35f, 0.35f)),
        P ("Isolator",           C_FILTER, S (A_ISO, 0, 1, 0, 1), {}, {}, 0.5f, 0.0f),
        P ("Isolator Drive",     C_FILTER, S (A_ISO, 0, 1, 0, 1), S (A_SHAPER, 0.5f, 0.5f, 0.33f, 0.33f, 0.2f, 0.2f), {}, 0.5f, 0.0f),
        P ("Swirl Filter",       C_FILTER, S (A_BPF, 0.1f, 1, 0.4f, 0.95f, 0.3f), M (S (A_FLANGE, 0.4f, 0.4f, 0.75f, 0.75f, 0.4f), 10, 0.5f, TRI)),

        // ---------- ECHO ----------
        P ("Sync Delay",         C_ECHO, S (A_DELAY, 0, 1, 0.1f, 0.85f, 0.7f)),
        P ("Ping Pong Delay",    C_ECHO, S (A_DELAY, 0, 1, 0.1f, 0.85f, 0.7f, 1)),
        P ("Spiral Echo",        C_ECHO, S (A_FSHIFT, 0.5f, 0.7f, 0.3f, 0.85f, D (7), 0, 0, 0, true)),
        P ("Falling Echo",       C_ECHO, S (A_FSHIFT, 0.3f, 0.5f, 0.3f, 0.85f, D (5), 0, 0, 0, true)),
        P ("Wobble Tape",        C_ECHO, S (A_DELAY, 0.2f, 0.9f, 0.2f, 0.8f, 0.4f, 0, 1, 0.2f), S (A_SHAPER, 0.25f, 0.25f, 0, 0, 0.4f, 0.1f)),
        P ("Filter Echo Morph",  C_ECHO, S (A_DELAY, 0, 1, 0.3f, 0.85f, 0.8f, 1), M (S (A_BPF, 0.6f, 0.6f, 0.4f, 0.4f), 10, 0.45f, SIN)),
        P ("Reverse Echo",       C_ECHO, S (A_REVERSE, 0.3f, 1, 0.2f, 0.7f)),
        P ("Grain Echo",         C_ECHO, S (A_DELAY, 0, 1, 0.2f, 0.8f, 0.7f, 1), S (A_CLOUD, 0.4f, 0.4f, 0.3f, 0.3f, 0.5f, 0, 0.5f, 0, true)),
        P ("Crushed Echo",       C_ECHO, S (A_DELAY, 0, 0.7f, 0.4f, 0.85f, 0.8f, 1), M (S (A_CRUSH, 0.5f, 0.5f, 0.55f, 0.55f), 11, 0.3f, TRI)),
        P ("Slap Haas",          C_ECHO, S (A_DELAY, 0, 0.25f, 0, 0.4f, 0.8f), S (A_WIDTH, 0.8f, 0.8f, 0, 1)),
        P ("Octave Echo",        C_ECHO, S (A_OCTAVE, 0.85f, 0.85f, 0.5f, 0.5f, 0, 0, 0, 0, true), S (A_DELAY, 0, 1, 0.2f, 0.85f, 0.6f, 1)),

        // ---------- DRIVE ----------
        P ("Tube Drive",         C_DRIVE, S (A_SHAPER, 0, 1, 0, 0.2f, 0.3f, 0.1f)),
        P ("Morph Shaper",       C_DRIVE, S (A_SHAPER, 0, 1, 0, 1, 0.3f, 0.2f)),
        P ("Fold Lab",           C_DRIVE, M (S (A_SHAPER, 0.2f, 1, 0.25f, 0.45f, 0.2f, 0.2f), 8, 0.3f, TRI, true)),
        P ("Fuzz Wall",          C_DRIVE, S (A_SHAPER, 0.4f, 1, 0.6f, 0.75f, 0.1f, 0.3f), S (A_LPF, 0, 1, 0, 0.6f)),
        P ("Sub Drive",          C_DRIVE, S (A_OCTAVE, 0, 0.4f, 0.3f, 0.8f), S (A_SHAPER, 0.4f, 0.4f, 0, 0, 0.4f, 0)),
        P ("Acid Drive",         C_DRIVE, S (A_SHAPER, 0, 1, 0.1f, 0.4f, 0.3f, 0.2f), M (S (A_LPF, 0.5f, 0.5f, 0.85f, 0.85f, 0.4f), 5, 0.35f, SAW)),
        P ("Bit Crusher",        C_DRIVE, S (A_CRUSH, 0, 1, 0, 1)),
        P ("Decimator",          C_DRIVE, S (A_CRUSH, 0, 1, 0, 0.5f)),
        P ("8-Bit",              C_DRIVE, S (A_CRUSH, 0.3f, 0.8f, 0.6f, 0.85f, 0.3f)),
        P ("Crush Sweep",        C_DRIVE, M (S (A_CRUSH, 0, 1, 0.3f, 0.9f), 10, 0.35f, TRI)),
        P ("Hard Clip Gate",     C_DRIVE, S (A_SHAPER, 0.4f, 1, 1, 1, 0.2f, 0.3f), S (A_SLICER, R (5), R (0), 0, 1, 1, 0.5f)),
        P ("Ring Fuzz",          C_DRIVE, S (A_RING, 0.2f, 0.7f, 0.5f, 0.5f), S (A_SHAPER, 0.3f, 1, 0.6f, 0.9f, 0.1f, 0.3f)),

        // ---------- GATE ----------
        P ("Trance Gate",        C_GATE, S (A_SLICER, R (5), R (1), 0, 1, 0.33f, 0.5f), {}, {}, 0.66f, 0.9f),
        P ("Slicer",             C_GATE, S (A_SLICER, R (8), R (0), 0, 1, 0, 0.5f)),
        P ("Slicer Random",      C_GATE, S (A_SLICER, R (5), R (0), 0, 1, 1, 0.6f)),
        P ("Stutter Slicer",     C_GATE, S (A_SLICER, R (2), R (0), 0.5f, 1, 0, 0.3f)),
        P ("Sidechain Pump",     C_GATE, S (A_PUMP, R (11), R (5), 0, 1, 0.5f)),
        P ("Ducking",            C_GATE, S (A_PUMP, R (11), R (5), 0, 1, 0.9f)),
        P ("Auto Pan",           C_GATE, S (A_PAN, 0, 1, 0, 1)),
        P ("Square Pan",         C_GATE, S (A_PAN, 0, 1, 0, 1, 1)),
        P ("Gate Filter",        C_GATE, S (A_SLICER, R (8), R (0), 0, 1, 0.33f, 0.4f), M (S (A_LPF, 0.55f, 0.55f, 0.7f, 0.7f, 0.3f), 10, 0.35f, SIN)),
        P ("Chopped Space",      C_GATE, S (A_SLICER, R (8), R (0), 0, 1, 0, 0.35f), S (A_REVERB, 0.9f, 0.9f, 0.6f, 0.6f, 0.5f, 1, 0.3f)),
        P ("Wide Pump",          C_GATE, S (A_PUMP, R (11), R (5), 0, 1, 0.6f), S (A_WIDTH, 0.9f, 0.9f, 0.3f, 0.3f)),

        // ---------- SPACE ----------
        P ("Hall",               C_SPACE, S (A_REVERB, 0.4f, 1, 0, 1, 0.4f, 1, 0.2f)),
        P ("Dark Space",         C_SPACE, S (A_REVERB, 0.7f, 1, 0, 1, 0.9f, 1, 0.3f)),
        P ("Bright Plate",       C_SPACE, S (A_REVERB, 0.4f, 0.9f, 0, 1, 0.05f, 1, 0.5f)),
        P ("Freeze Reverb",      C_SPACE, S (A_REVERB, 0.6f, 1, 0.2f, 1, 0.4f, 1, 0.3f, 1)),
        P ("Shimmer",            C_SPACE, S (A_PITCH, ST (12), ST (12), 0.25f, 0.25f, 0.7f, 1, 0, 0, true), S (A_REVERB, 0.8f, 0.99f, 0.2f, 0.8f, 0.3f, 1, 0.4f)),
        P ("Cloud Verb",         C_SPACE, S (A_CLOUD, 0, 1, 0, 0.5f, 0.6f, 0, 0.6f, 0, true), S (A_REVERB, 0.9f, 0.9f, 0.6f, 0.6f, 0.4f, 1, 0.3f)),
        P ("Spiral Verb",        C_SPACE, S (A_FSHIFT, 0.5f, 0.62f, 0.4f, 0.4f, 0, 0, 0, 0, true), S (A_REVERB, 0.85f, 0.85f, 0.2f, 1, 0.5f, 1, 0.3f)),
        P ("Reverse Swell",      C_SPACE, S (A_REVERSE, 0.5f, 1, 0, 0.5f), S (A_REVERB, 0.9f, 0.9f, 0.6f, 0.6f, 0.4f, 1, 0.3f)),
        P ("Gate Reverb",        C_SPACE, S (A_REVERB, 0.85f, 0.85f, 0.3f, 1, 0.3f, 1, 0.3f), S (A_SLICER, R (8), R (0), 0.8f, 0.8f, 0, 0.4f)),
        P ("Pump Reverb",        C_SPACE, S (A_REVERB, 0.6f, 1, 0.3f, 1, 0.4f, 1, 0.3f), S (A_PUMP, R (8), R (8), 0.7f, 0.7f, 0.5f)),
        P ("Wide Room",          C_SPACE, S (A_REVERB, 0.2f, 0.7f, 0, 1, 0.5f, 1, 0.3f), S (A_WIDTH, 1, 1, 0.2f, 0.2f)),

        // ---------- MOD FX ----------
        P ("Jet Flanger",        C_MODFX, S (A_FLANGE, 0, 1, 0.75f, 0.97f, 0.3f)),
        P ("Flanger",            C_MODFX, S (A_FLANGE, 0, 1, 0, 0.9f, 0.5f)),
        P ("Negative Flanger",   C_MODFX, S (A_FLANGE, 0, 1, 0.3f, 0.95f, 0.5f, 1)),
        P ("Sync Flanger",       C_MODFX, S (A_FLANGE, 0, 1, 0.3f, 0.9f, 0.8f, 0, 1)),
        P ("Manual Phaser",      C_MODFX, S (A_PHASER, 0, 1, 0.2f, 1, 0.5f, 1)),
        P ("Phaser Deep",        C_MODFX, S (A_PHASER, 0, 1, 0.5f, 1, 1, 1)),
        P ("Slow Phaser",        C_MODFX, S (A_PHASER, 0, 0.3f, 0.3f, 0.9f, 0.5f, 1, 1)),
        P ("Barberpole",         C_MODFX, S (A_FSHIFT, 0.5f, 0.62f, 0.4f, 0.9f)),
        P ("Chorus",             C_MODFX, S (A_CHORUS, 0, 1, 0, 1)),
        P ("Ensemble Wide",      C_MODFX, S (A_CHORUS, 0.1f, 0.7f, 0.5f, 1, 1), S (A_WIDTH, 0.9f, 0.9f, 0.2f, 0.2f)),
        P ("Auto Wah",           C_MODFX, S (A_WAH, 0, 1, 0.2f, 1, 0.6f)),
        P ("Envelope Phaser",    C_MODFX, S (A_WAH, 0, 1, 0.2f, 0.9f, 0.5f, 1), S (A_PHASER, 0.4f, 0.4f, 0.6f, 0.6f, 0.5f, 1, 1)),

        // ---------- LFO ----------
        P ("Wobble LPF",         C_LFO, S (A_LFOF, 0, 1, 0, 1, 0.3f, 0, 0, 0.4f)),
        P ("Wobble Square",      C_LFO, S (A_LFOF, 0, 1, 0, 1, 0.3f, 0.75f, 0, 0.4f)),
        P ("Wobble Saw",         C_LFO, S (A_LFOF, 0, 1, 0, 1, 0.25f, 0.5f, 0, 0.6f)),
        P ("Jag Filter",         C_LFO, S (A_LFOF, 0.3f, 1, 0.2f, 1, 0.35f, 1, 0, 0.8f)),
        P ("S&H Band Pass",      C_LFO, S (A_LFOF, 0.3f, 1, 0.2f, 1, 0.4f, 1, 0.5f, 0.5f)),
        P ("LFO High Pass",      C_LFO, S (A_LFOF, 0, 1, 0, 1, 0.15f, 0, 1, 0.4f)),
        P ("Deep Wobble",        C_LFO, S (A_LFOF, 0, 0.6f, 0.4f, 1, 0.08f, 0, 0, 0.7f), S (A_SHAPER, 0.3f, 0.3f, 0, 0, 0.3f, 0)),
        P ("Wobble Drive",       C_LFO, S (A_LFOF, 0, 1, 0, 1, 0.2f, 0.25f, 0, 0.7f), S (A_SHAPER, 0.55f, 0.55f, 0.33f, 0.33f, 0.2f, 0.2f)),
        P ("Wobble Pan",         C_LFO, S (A_LFOF, 0, 1, 0, 1, 0.3f, 0, 0, 0.5f), S (A_PAN, 0.5f, 0.5f, 0.8f, 0.8f)),
        P ("Talk Wobble",        C_LFO, M (S (A_TALK, 0, 1, 0.5f, 0.95f, 0.5f), 5, 0.3f, TRI)),
        P ("Wah Wobble",         C_LFO, M (S (A_WAH, 0.3f, 0.3f, 0.3f, 1, 0.5f), 5, 0.4f, SIN), S (A_LFOF, 0, 1, 0, 0.3f, 0.5f, 0, 0, 0.3f)),

        // ---------- LOOPER ----------
        P ("Beat Repeat",        C_LOOPER, S (A_REPEAT, 0, 1, 0, 1, 0, 0, 0, 1)),
        P ("Stutter Roll",       C_LOOPER, S (A_REPEAT, R (5), R (0), 0, 1, 0, 0, 0, 1)),
        P ("Slice Looper",       C_LOOPER, S (A_REPEAT, R (4), R (0), 0, 1, 0, 0, 0.35f, 1)),
        P ("Break Looper",       C_LOOPER, S (A_REPEAT, 0, 1, 0, 0.5f)),
        P ("Loop Pitch",         C_LOOPER, S (A_REPEAT, 0, 1, 0, 1)),
        P ("Loop Reverse",       C_LOOPER, S (A_REPEAT, 0, 1, 0, 1, 0, 1, 0, 1)),
        P ("Vinyl Break",        C_LOOPER, S (A_TAPE, 0, 0.6f, 0.1f, 0.6f, 0.9f), {}, {}, 0.0f, 0.3f),
        P ("Tape Stop",          C_LOOPER, S (A_TAPE, 0, 1, 0.2f, 1, 0.4f), {}, {}, 0.0f, 0.5f),
        P ("Reverse Loop",       C_LOOPER, S (A_REVERSE, 0.3f, 0.8f, 0, 0.2f)),
        P ("Loop Filter Rise",   C_LOOPER, S (A_REPEAT, 0, 1, 0, 1, 0, 0, 0, 1), M (S (A_HPF, 0.3f, 0.3f, 0.6f, 0.6f), 11, 0.35f, SAW)),
        P ("Loop Shifter",       C_LOOPER, S (A_REPEAT, 0, 1, 0, 0, 0, 0, 0, 1), SW (S (A_FSHIFT, 0.5f, 0.75f, 0.3f, 0.3f))),
        P ("Loop Crush",         C_LOOPER, S (A_REPEAT, 0, 1, 0, 1, 0, 0, 0, 1), M (S (A_CRUSH, 0.4f, 0.4f, 0.5f, 0.5f), 10, 0.4f, TRI)),
        P ("Loop Space",         C_LOOPER, S (A_REPEAT, 0, 1, 0, 1, 0, 0, 0, 1), S (A_REVERB, 0.85f, 0.85f, 0.5f, 0.5f, 0.5f, 1, 0.3f)),

        // ---------- GRAIN ----------
        P ("Grain Shifter",      C_GRAIN, S (A_REPEAT, 0, 1, 0, 1, 1, 0, 0, 1)),
        P ("Grain Pitch",        C_GRAIN, S (A_REPEAT, 0, 1, 0, 1, 1)),
        P ("Beat Grain",         C_GRAIN, S (A_REPEAT, R (3), R (0), 0, 1, 0, 0, 0, 1)),
        P ("Reverse Grain",      C_GRAIN, S (A_REPEAT, 0, 1, 0, 1, 1, 1, 0, 1)),
        P ("Grain Cloud",        C_GRAIN, S (A_CLOUD, 0, 1, 0, 1, 0.4f, 0, 0.4f)),
        P ("Octave Cloud",       C_GRAIN, S (A_CLOUD, 0, 1, 0, 1, 0.5f, 1, 0.5f)),
        P ("Dense Cloud",        C_GRAIN, S (A_CLOUD, 0, 0.6f, 0, 0.6f, 1, 0, 0.3f), S (A_WIDTH, 0.9f, 0.9f, 0, 0)),
        P ("Frozen Cloud",       C_GRAIN, S (A_CLOUD, 0, 1, 0, 0.3f, 0.6f, 0, 0), S (A_REVERB, 0.95f, 0.95f, 0.5f, 0.5f, 0.4f, 1, 0.3f)),
        P ("Grain Shimmer",      C_GRAIN, S (A_CLOUD, 0, 1, 0.5f, 1, 0.5f, 1, 0.5f, 0, true), S (A_REVERB, 0.97f, 0.97f, 0.5f, 0.5f, 0.3f, 1, 0.4f)),
        P ("Grain Shift Spiral", C_GRAIN, S (A_REPEAT, 0, 1, 0, 0, 1, 0, 0, 1), SW (S (A_FSHIFT, 0.5f, 0.8f, 0.4f, 0.4f))),

        // ---------- PITCH ----------
        P ("Pitch Shifter",      C_PITCH, S (A_PITCH, 0, 1, 0, 0.3f, 0.5f, 0.5f)),
        P ("Pitch Up",           C_PITCH, S (A_PITCH, 0.5f, 1, 0, 0.3f, 0.5f, 0.5f)),
        P ("Pitch Down",         C_PITCH, S (A_PITCH, 0, 0.5f, 0, 0.3f, 0.6f, 0.5f)),
        P ("Octave Jump",        C_PITCH, S (A_PITCH, 0, 1, 0, 0.3f, 0.6f, 1)),
        P ("Pitch Spiral",       C_PITCH, S (A_PITCH, 0, 1, 0.5f, 0.85f, 0.4f, 0.5f)),
        P ("Pitch Wobble",       C_PITCH, M (S (A_PITCH, 0.4f, 0.6f, 0, 0.3f, 0.4f), 5, 0.06f, SIN)),
        P ("Detune Wide",        C_PITCH, S (A_PITCH, 0.45f, 0.55f, 0, 0.2f, 0.3f, 0, 0, 0, true), S (A_WIDTH, 0.8f, 0.8f, 0.2f, 0.2f)),
        P ("Harmonizer",         C_PITCH, S (A_PITCH, ST (-12), ST (12), 0, 0.4f, 0.6f, 0.5f, 0, 0, true)),
        P ("Sub Octave",         C_PITCH, S (A_OCTAVE, 0, 0.3f, 0, 1)),
        P ("Octave Up Fuzz",     C_PITCH, S (A_OCTAVE, 0.7f, 1, 0, 1), S (A_SHAPER, 0.3f, 0.3f, 0.66f, 0.66f, 0.3f, 0.3f)),
        P ("Pitch Echo Climb",   C_PITCH, S (A_PITCH, 0, 1, 0, 0.3f, 0.5f, 0.5f, 0, 0, true), S (A_DELAY, D (7), D (7), 0.55f, 0.55f, 0.6f, 1)),

        // ---------- RING MOD ----------
        P ("Ring Mod",           C_RING, S (A_RING, 0, 1, 0.3f, 1)),
        P ("Ring Mod Tuned",     C_RING, S (A_RING, 0.2f, 0.9f, 0.4f, 1, 0, 1)),
        P ("Square Ring",        C_RING, S (A_RING, 0, 1, 0.3f, 1, 0, 0, 1)),
        P ("Ring Sweep",         C_RING, M (S (A_RING, 0, 1, 0.4f, 1), 10, 0.3f, SAW)),
        P ("Ring Random",        C_RING, M (S (A_RING, 0.2f, 0.8f, 0.4f, 1, 0, 1), 2, 0.3f, SH)),
        P ("Frequency Shifter",  C_RING, S (A_FSHIFT, 0, 1, 0, 0.6f)),
        P ("Hitech Grinder",     C_RING, S (A_RING, 0.3f, 0.8f, 0.7f, 0.7f, 0, 1, 1), S (A_SHAPER, 0.5f, 0.5f, 0.4f, 1, 0.2f, 0.3f)),
        P ("Ring Echo",          C_RING, S (A_RING, 0, 1, 0.3f, 1), S (A_DELAY, D (7), D (7), 0.5f, 0.5f, 0.6f, 1)),
        P ("Ring Bell",          C_RING, S (A_RING, 0.4f, 0.9f, 0.6f, 0.6f, 0, 1), S (A_COMB, 0.6f, 0.6f, 0, 0.9f, 0.2f, 1)),
        P ("Ring Cloud",         C_RING, S (A_RING, 0, 1, 0.5f, 1), S (A_CLOUD, 0.5f, 0.5f, 0.5f, 0.5f, 0.6f, 1, 0.5f, 0, true)),

        // ---------- RESONATOR ----------
        P ("Resonator",          C_RESO, S (A_COMB, 0, 1, 0.3f, 0.95f, 0.3f, 1)),
        P ("Resonator Metal",    C_RESO, S (A_COMB, 0.2f, 1, 0.8f, 0.985f, 0.1f, 0, 0.6f)),
        P ("Tuned Strings",      C_RESO, S (A_COMB, 0.2f, 0.8f, 0.9f, 0.985f, 0.3f, 1, 0.4f), S (A_WIDTH, 0.9f, 0.9f, 0.2f, 0.2f)),
        P ("Resonator Arp",      C_RESO, M (S (A_COMB, 0.3f, 0.7f, 0.6f, 0.95f, 0.2f, 1, 0.5f), 2, 0.25f, SH)),
        P ("Resonator Wobble",   C_RESO, M (S (A_COMB, 0, 1, 0.5f, 0.95f, 0.3f, 0, 0.5f), 8, 0.08f, TRI)),
        P ("Robot Voice",        C_RESO, S (A_COMB, 0.1f, 0.4f, 0.6f, 0.9f, 0.2f, 1), S (A_RING, 0.35f, 0.35f, 0.4f, 0.4f, 0, 1)),
        P ("Resonator Drive",    C_RESO, S (A_COMB, 0, 1, 0.5f, 0.95f, 0.3f, 1), S (A_SHAPER, 0.5f, 0.5f, 0.4f, 0.4f, 0.2f, 0.3f)),
        P ("Metallic Echo",      C_RESO, S (A_COMB, 0.3f, 0.9f, 0.6f, 0.95f, 0.2f, 1), S (A_DELAY, D (7), D (7), 0.55f, 0.55f, 0.7f, 1)),
        P ("Reso Cloud",         C_RESO, S (A_COMB, 0, 1, 0.5f, 0.95f, 0.3f, 1), S (A_CLOUD, 0.5f, 0.5f, 0.6f, 0.6f, 0.6f, 1, 0.5f, 0, true)),

        // ---------- GLITCH ----------
        P ("Glitch Machine",     C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 0.3f)),
        P ("Tape Glitch",        C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 1)),
        P ("Glitch Mute",        C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 0.2f, 1)),
        P ("Stutter Glitch",     C_GLITCH, S (A_GLITCH, 0.5f, 1, 0.3f, 1, 0)),
        P ("Glitch Crush",       C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 0.4f), M (S (A_CRUSH, 0.5f, 0.5f, 0.6f, 0.6f), 2, 0.4f, SH)),
        P ("Glitch Space",       C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 0.5f), S (A_REVERB, 0.85f, 0.85f, 0.4f, 0.4f, 0.5f, 1, 0.3f)),
        P ("Glitch Shifter",     C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 0.3f), M (S (A_FSHIFT, 0.55f, 0.55f, 0.2f, 0.2f), 2, 0.15f, SH)),
        P ("Glitch Filter",      C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 0.4f), M (S (A_LPF, 0.6f, 0.6f, 0.75f, 0.75f, 0.4f), 2, 0.35f, SH)),
        P ("Bit Glitch",         C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 0.3f, 1), S (A_CRUSH, 0.6f, 0.6f, 0.7f, 0.7f), S (A_SLICER, R (2), R (2), 0.5f, 0.5f, 1, 0.6f)),
        P ("Broken Tape",        C_GLITCH, S (A_GLITCH, 0, 1, 0, 1, 1), S (A_DELAY, D (5), D (5), 0.4f, 0.4f, 0.4f, 0, 1, 0.3f)),

        // ---------- FREAK (wilde 3er-Ketten) ----------
        P ("Alien Radio",        C_FREAK, M (S (A_FSHIFT, 0.3f, 0.7f, 0.2f, 0.6f), 10, 0.1f, SIN), M (S (A_BPF, 0.6f, 0.6f, 0.6f, 0.6f, 0.4f), 5, 0.3f, SH), S (A_CRUSH, 0.45f, 0.45f, 0.45f, 0.45f)),
        P ("Psy Laser",          C_FREAK, M (S (A_COMB, 0.3f, 0.8f, 0.85f, 0.97f, 0.2f, 1), 2, 0.2f, SH), S (A_FLANGE, 0, 1, 0.7f, 0.7f, 0.3f), S (A_DELAY, D (4), D (4), 0.5f, 0.5f, 0.7f, 1)),
        P ("Hitech Shredder",    C_FREAK, M (S (A_RING, 0.3f, 0.9f, 0.5f, 1), 5, 0.3f, SAW), S (A_SHAPER, 0, 1, 0.35f, 0.35f, 0.2f, 0.3f), S (A_SLICER, R (2), R (2), 0.7f, 0.7f, 1, 0.55f)),
        P ("Wormhole",           C_FREAK, S (A_PITCH, 0, 1, 0.6f, 0.85f, 0.5f, 0.5f, 0, 0, true), M (S (A_FSHIFT, 0.55f, 0.55f, 0.3f, 0.3f), 11, 0.08f, TRI), S (A_REVERB, 0.95f, 0.95f, 0.3f, 1, 0.4f, 1, 0.3f)),
        P ("Robot Choir",        C_FREAK, M (S (A_TALK, 0.5f, 0.5f, 0.6f, 0.9f, 0.5f), 7, 0.5f, SH), S (A_COMB, 0, 1, 0.7f, 0.7f, 0.3f, 1), S (A_CLOUD, 0.5f, 0.5f, 0.5f, 0.5f, 0.6f, 1, 0.5f, 0, true)),
        P ("Broken Machine",     C_FREAK, S (A_GLITCH, 0, 1, 0.3f, 1, 0.6f, 1), M (S (A_RING, 0.4f, 0.4f, 0.6f, 0.6f, 0, 1), 2, 0.3f, SH), M (S (A_LPF, 0.6f, 0.6f, 0.7f, 0.7f, 0.4f), 10, 0.3f, TRI)),
        P ("Black Hole",         C_FREAK, S (A_PITCH, ST (-12), ST (-12), 0.4f, 0.4f, 0.8f, 1, 0, 0, true), S (A_REVERB, 0.8f, 1, 0.3f, 1, 0.8f, 1, 0.2f, 1), S (A_LPF, 0.3f, 1, 0.3f, 0.3f)),
        P ("Neon Wobble",        C_FREAK, S (A_LFOF, 0, 1, 0, 1, 0.25f, 0.25f, 0, 0.7f), S (A_OCTAVE, 0.2f, 0.2f, 0.4f, 0.4f), S (A_SHAPER, 0.5f, 0.5f, 0.3f, 0.3f, 0.3f, 0.2f)),
        P ("Zap Cannon",         C_FREAK, M (S (A_LPF, 0.3f, 1, 0.85f, 1, 0.7f), 2, 0.5f, SAW), S (A_FSHIFT, 0.5f, 0.5f, 0.3f, 0.3f), S (A_DELAY, D (5), D (5), 0.5f, 0.5f, 0.8f, 1)),
        P ("Bug Swarm",          C_FREAK, S (A_CLOUD, 0, 0.5f, 0.5f, 1, 1, 1, 0.3f), M (S (A_RING, 0.6f, 0.6f, 0.5f, 0.5f), 2, 0.3f, SH), S (A_WIDTH, 1, 1, 0.3f, 0.3f)),
        P ("Acid Rain",          C_FREAK, S (A_WAH, 0, 1, 0.6f, 1, 0.8f), S (A_SHAPER, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.2f), S (A_DELAY, D (3), D (3), 0.6f, 0.6f, 0.6f, 1)),
        P ("Time Melt",          C_FREAK, S (A_REVERSE, 0.4f, 1, 0.3f, 0.3f), M (S (A_PITCH, 0.4f, 0.4f, 0, 0.5f, 0.6f), 11, 0.05f, SIN), S (A_REVERB, 0.9f, 0.9f, 0.5f, 0.5f, 0.6f, 1, 0.3f)),
    };
    return p;
}

inline int numPresets() { return (int) presets().size(); }

inline bool axisMoves (float a, float b) { return std::abs (a - b) > 1.0e-6f; }

// Welcher Effekt bestimmt die X- bzw. Y-Anzeige (Pad-Achse, also mit Tausch)
inline const SlotDef* axisSlot (const PresetDef& p, bool padX, bool& algoX)
{
    for (const SlotDef& s : p.s)
    {
        if (s.algo < 0) continue;
        const bool ax = s.swap ? ! padX : padX;           // welche Effekt-Achse hängt an dieser Pad-Achse
        if (ax ? axisMoves (s.x0, s.x1) : axisMoves (s.y0, s.y1)) { algoX = ax; return &s; }
    }
    return nullptr;
}

inline juce::String axisLabel (const PresetDef& p, bool padX)
{
    bool ax = padX;
    if (auto* s = axisSlot (p, padX, ax))
    {
        juce::String name = ax ? xLabel (s->algo) : yLabel (s->algo);
        if (! ax && s->algo == A_REPEAT && s->p[3] > 0.5f) name = "GATE";
        return juce::String (padX ? "X / " : "Y / ") + name;
    }
    return padX ? "X / -" : "Y / -";
}

inline juce::String axisValue (const PresetDef& p, bool padX, float padValue)
{
    bool ax = padX;
    if (auto* s = axisSlot (p, padX, ax))
        return axisValueText (s->algo, ax, ax ? lerp (s->x0, s->x1, padValue) : lerp (s->y0, s->y1, padValue), s->p);
    return "-";
}

inline int firstOfCategory (int c)
{
    for (int i = 0; i < numPresets(); ++i) if (presets()[(size_t) i].cat == c) return i;
    return 0;
}

} // namespace swarm
