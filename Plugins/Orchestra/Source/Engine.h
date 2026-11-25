#pragma once

// Klangengine von Brainbugz Orchestra v2:
// Sample-Stimmen, Legato, Rhythm-Engine (Gate/Strike), Pad-Morph, Drum-Kits mit Synth-Layern, FX-Kette

#include <juce_dsp/juce_dsp.h>
#include "SampleBank.h"
#include <cmath>

namespace orch
{

enum Mode  { M_STRINGS = 0, M_CELLOS, M_ATMOS, M_DRUMS };
enum Artic { A_SUS = 0, A_MARC, A_SPIC, A_PIZZ, A_TREM, A_SWELL, A_NUM };
enum Kit   { K_EPIC = 0, K_PERC, K_METAL, K_TIMPANI, K_TRAILER, K_NUM };
enum Rhy   { R_OFF = 0, R_GATE, R_STRIKE };
enum Synth { SY_NONE = 0, SY_BOOM, SY_BOOMLONG, SY_KICK, SY_DROP, SY_RISE2, SY_RISE4, SY_WHOOSH, SY_NOISE, SY_DOWN };

static constexpr int keyswitchLow = 24; // C0 .. F0 = Artikulationen

inline const char* articNames[A_NUM] { "SUSTAIN", "MARCATO", "SPICCATO", "PIZZICATO", "TREMOLO", "SWELL" };
inline const char* kitNames[K_NUM]   { "EPIC", "PERCUSSION", "METAL & FX", "TIMPANI", "TRAILER" };

// ---------------------------------------------------------------------------------------------
// Pad-Quellen: Zonen aus Sample-Gruppen
struct Zone { int group, lo, hi; float gain, pan; };
struct PadSource { const char* name; Zone z[5]; int n; juce::uint32 colour; };

inline const PadSource padSources[] =
{
    { "STRINGS",   { { G_VC_SUS, 0, 49, 1.0f, 0.0f }, { G_VLA_SUS, 50, 61, 0.8f, 0.3f }, { G_VC_SUS, 50, 61, 0.5f, -0.3f },
                     { G_VLN_SUS, 62, 127, 0.8f, -0.3f }, { G_VLA_SUS, 62, 80, 0.6f, 0.3f } }, 5, 0xffb6ff1f },
    { "CELLOS",    { { G_CB_SUS, 0, 35, 1.0f, 0.0f }, { G_VC_SUS, 36, 127, 1.0f, 0.0f }, { G_CB_SUS, 36, 47, 0.45f, 0.2f } }, 3, 0xff7dff6a },
    { "HORNS",     { { G_HRN_SUS, 0, 127, 1.0f, 0.0f } }, 1, 0xffffb02e },
    { "LOW BRASS", { { G_TUBA_SUS, 0, 45, 1.0f, 0.0f }, { G_TBN_SUS, 46, 127, 1.0f, 0.0f }, { G_TBN_SUS, 36, 45, 0.5f, 0.3f } }, 3, 0xffff7a1a },
    { "TRUMPETS",  { { G_HRN_SUS, 0, 52, 1.0f, 0.0f }, { G_TPT_SUS, 53, 127, 0.9f, 0.0f } }, 2, 0xffffe14d },
    { "FLUTES",    { { G_CLA_SUS, 0, 64, 1.0f, 0.0f }, { G_FLT_SUS, 65, 127, 1.0f, 0.0f }, { G_CLA_SUS, 65, 80, 0.45f, 0.3f } }, 3, 0xff4de1ff },
    { "REEDS",     { { G_BSN_SUS, 0, 57, 1.0f, 0.0f }, { G_OBO_SUS, 58, 127, 0.9f, 0.0f } }, 2, 0xff47a6ff },
    { "ORGAN",     { { G_ORG_SUS, 0, 127, 0.8f, 0.0f } }, 1, 0xffc77dff },
    { "DEEP BASS", { { G_CB_SUS, 0, 59, 1.0f, 0.0f }, { G_VC_SUS, 60, 127, 0.9f, 0.0f } }, 2, 0xffff2f8e },
    { "PURE SYNTH",{ }, 0, 0xff8b3dff },
};
static constexpr int numPadSources = (int) (sizeof (padSources) / sizeof (padSources[0]));

// ---------------------------------------------------------------------------------------------
// Trailer-Kit: Hybrid-Hits aus Sample-Layern + Synth
struct TLayer { int group; float gain, semis; bool reverse; };
struct TPiece { int note; const char* name; TLayer l[3]; int nl; int synth; float synthGain; };

inline const TPiece trailerPieces[] =
{
    { 36, "Boom",          { { G_K0_36, 1.0f, 0, false } }, 1, SY_BOOM, 1.0f },
    { 37, "Taiko Impact",  { { G_K0_42, 1.0f, 0, false }, { G_K0_37, 0.8f, 0, false } }, 2, SY_BOOM, 0.8f },
    { 38, "Big Hit",       { { G_K0_55, 1.0f, 0, false }, { G_K0_64, 0.7f, 0, false } }, 2, SY_BOOM, 1.0f },
    { 39, "Metal Impact",  { { G_K2_36, 0.9f, 0, false }, { G_K2_37, 0.8f, 0, false }, { G_K2_43, 0.6f, 0, false } }, 3, SY_KICK, 0.7f },
    { 40, "Gong Impact",   { { G_K2_39, 1.0f, 0, false }, { G_K0_36, 0.9f, 0, false } }, 2, SY_BOOMLONG, 1.0f },
    { 41, "Timpani Boom",  { { G_K0_41, 1.0f, 0, false }, { G_K0_55, 0.7f, 0, false } }, 2, SY_BOOM, 0.9f },
    { 42, "Snare Crack",   { { G_K0_38, 1.0f, 0, false }, { G_K0_67, 0.8f, 0, false }, { G_K0_71, 0.6f, 0, false } }, 3, SY_NOISE, 0.4f },
    { 43, "Sub Drop",      { }, 0, SY_DROP, 1.0f },
    { 44, "Riser 2s",      { }, 0, SY_RISE2, 1.0f },
    { 45, "Riser 4s",      { }, 0, SY_RISE4, 1.0f },
    { 46, "Whoosh",        { }, 0, SY_WHOOSH, 1.0f },
    { 47, "Reverse Crash", { { G_K0_49, 1.0f, 0, true } }, 1, SY_NONE, 0.0f },
    { 48, "Reverse Gong",  { { G_K2_39, 1.0f, 0, true } }, 1, SY_NONE, 0.0f },
    { 49, "Reverse Swell", { { G_K0_51, 1.0f, 0, true }, { G_K2_49, 0.6f, 0, true } }, 2, SY_NONE, 0.0f },
    { 50, "Mega Impact",   { { G_K0_36, 1.0f, 0, false }, { G_K0_49, 0.8f, 0, false }, { G_K0_53, 0.8f, 0, false } }, 3, SY_BOOMLONG, 1.0f },
    { 51, "Taiko Ensemble",{ { G_K0_37, 1.0f, 0, false }, { G_K0_39, 0.9f, 0, false }, { G_K0_42, 0.9f, 0, false } }, 3, SY_BOOM, 0.6f },
    { 52, "Thunder",       { { G_K0_60, 1.0f, 0, false }, { G_K0_65, 0.8f, 0, false } }, 2, SY_DROP, 0.7f },
    { 53, "Click Hit",     { { G_K0_57, 1.0f, 0, false }, { G_K2_43, 0.6f, 0, false } }, 2, SY_KICK, 0.6f },
    { 54, "Low Taikos",    { { G_K0_42, 1.0f, -5, false }, { G_K0_37, 0.9f, -5, false } }, 2, SY_BOOM, 1.0f },
    { 55, "Anvil Ring",    { { G_K0_56, 1.0f, 0, false }, { G_K2_59, 0.7f, 0, false } }, 2, SY_NONE, 0.0f },
    { 56, "Sub Boom",      { }, 0, SY_BOOM, 1.0f },
    { 57, "Sub Kick",      { }, 0, SY_KICK, 1.0f },
    { 58, "Noise Hit",     { }, 0, SY_NOISE, 1.0f },
    { 59, "Downlifter",    { }, 0, SY_DOWN, 1.0f },
};
static constexpr int numTrailer = (int) (sizeof (trailerPieces) / sizeof (trailerPieces[0]));

// ---------------------------------------------------------------------------------------------
// Rhythm-Patterns (16 Steps, 0 = Pause, 1 leise, 2 normal, 3 Akzent)
struct Pattern { const char* name; int s[16]; };
inline const Pattern patterns[] =
{
    { "Straight",       { 3,2,2,2, 3,2,2,2, 3,2,2,2, 3,2,2,2 } },
    { "Gallop",         { 3,0,2,2, 3,0,2,2, 3,0,2,2, 3,0,2,2 } },
    { "Reverse Gallop", { 3,2,2,0, 3,2,2,0, 3,2,2,0, 3,2,2,0 } },
    { "Ostinato 3-3-2", { 3,1,1,3, 1,1,3,1, 3,1,1,3, 1,1,3,1 } },
    { "Drive",          { 3,1,2,1, 3,1,2,1, 3,1,2,1, 3,2,3,2 } },
    { "Heartbeat",      { 3,0,2,0, 0,0,0,0, 3,0,2,0, 0,0,0,0 } },
    { "Syncopated",     { 3,0,0,2, 0,0,2,0, 3,0,0,2, 0,2,0,0 } },
    { "Trance Gate",    { 3,0,2,2, 0,2,2,0, 3,0,2,2, 0,2,2,2 } },
    { "Dotted",         { 3,0,0,2, 0,0,2,0, 3,0,0,2, 0,0,2,0 } },
    { "Pulse",          { 3,0,0,0, 2,0,0,0, 2,0,0,0, 2,0,0,0 } },
    { "Build Up",       { 1,1,1,1, 1,2,1,2, 2,2,2,3, 2,3,3,3 } },
    { "Stutter",        { 3,3,3,0, 3,3,0,3, 3,0,3,3, 3,3,0,0 } },
};
static constexpr int numPatterns = (int) (sizeof (patterns) / sizeof (patterns[0]));
inline const char* rateNames[] { "1/4", "1/8", "1/16", "1/8 T", "1/16 T" };
inline const double rateBeats[] { 1.0, 0.5, 0.25, 1.0 / 3.0, 1.0 / 6.0 };

// ---------------------------------------------------------------------------------------------
struct Settings
{
    int mode = M_STRINGS, artic = A_SUS;
    float attack = 0.05f, release = 0.4f, tone = 0.0f, tune = 0.0f;
    float reverb = 0.25f, space = 0.6f, width = 1.0f, volumeDb = 0.0f, drive = 0.0f;
    float ensemble = 0.3f, humanize = 0.2f, basses = 0.0f, glide = 0.08f; bool legato = false;
    int padA = 0, padB = 2; float morph = 0.0f, evolve = 0.0f, air = 0.35f, shimmer = 0.25f, motion = 0.4f;
    int kit = K_EPIC; float punch = 0.3f, decay = 1.0f, sub = 0.0f, stack = 0.0f;
    int rhyMode = R_OFF, rhyPattern = 0, rhyRate = 2; float swing = 0.0f, rhyDepth = 1.0f;
};

struct Transport { double bpm = 120.0, ppq = 0.0; bool playing = false; };

// ---------------------------------------------------------------------------------------------
struct SVF   // TPT State Variable Filter
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;
    void set (float cutoff, float sr, float q = 0.7071f)
    {
        g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, sr * 0.45f, cutoff) / sr);
        k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void tick (float v0, float& lp, float& bp)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; lp = v2; bp = v1;
    }
    float lp (float v0) { float l, b; tick (v0, l, b); return l; }
    float bp (float v0) { float l, b; tick (v0, l, b); return b; }
    void reset() { ic1 = ic2 = 0; }
};

// ---------------------------------------------------------------------------------------------
struct Voice
{
    enum Kind { SAMPLE, AIR, DRUMSYNTH };
    bool active = false;
    Kind kind = SAMPLE;
    int note = 0, group = -1, side = 2;       // side: 0 = Pad A, 1 = Pad B, 2 = neutral
    const GroupData* data = nullptr;
    const Sample* smp = nullptr;
    bool oneShot = false, reverse = false, released = false, sustained = false;
    double pos = 0, inc = 1;
    float gain = 1, panL = 1, panR = 1;
    float env = 0, attackInc = 1, releaseMul = 0.999f, envPow = 2.0f;
    float accent = 0, accentMul = 1;           // Marcato-Akzent
    float decayMul = 1, drumEnv = 1;
    float glide = 0, glideStep = 0;            // Halbtoene, laufen gegen 0
    int delay = 0, autoOff = -1;
    juce::int64 age = 0;
    juce::uint32 order = 0;
    // Synths
    int synth = SY_NONE; float frq = 0, ph[3] {}, driftPh = 0, t = 0; SVF filt; juce::uint32 rng = 12345;

    static float hermite (const int16_t* d, int i, float f) noexcept
    {
        const float xm1 = d[i > 0 ? i - 1 : 0], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
        const float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((((a * f) - b) * f + c) * f + x0) * (1.0f / 32767.0f);
    }

    void release (float relSeconds, float sr)
    {
        if (released) return;
        released = true;
        releaseMul = std::exp (-6.9f / (std::max (0.005f, relSeconds) * sr));
    }

    float noise() { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 9) / 4194304.0f - 1.0f; }

    float drumSynth (float sr)
    {
        const float dt = 1.0f / sr; t += dt;
        const float twoPi = juce::MathConstants<float>::twoPi;
        float y = 0;
        switch (synth)
        {
            case SY_BOOM: case SY_BOOMLONG: case SY_KICK:
            {
                const bool kick = synth == SY_KICK;
                const float f0 = kick ? 160.0f : 115.0f, f1 = kick ? 46.0f : 38.0f, tpc = kick ? 0.035f : 0.09f;
                const float ta = synth == SY_BOOMLONG ? 1.6f : (kick ? 0.28f : 0.75f);
                frq = f1 + (f0 - f1) * std::exp (-t / tpc);
                ph[0] += frq * dt; if (ph[0] >= 1.0f) ph[0] -= 1.0f;
                y = std::tanh (1.6f * std::sin (ph[0] * twoPi)) * std::exp (-t / ta);
                if (t < 0.004f) y *= t / 0.004f;
                if (t > ta * 7.0f) active = false;
                break;
            }
            case SY_DROP:
            {
                frq = 26.0f + 70.0f * std::exp (-t / 0.9f);
                ph[0] += frq * dt; if (ph[0] >= 1.0f) ph[0] -= 1.0f;
                y = std::tanh (1.4f * std::sin (ph[0] * twoPi)) * std::exp (-t / 1.4f) * std::min (1.0f, t / 0.01f);
                if (t > 7.0f) active = false;
                break;
            }
            case SY_RISE2: case SY_RISE4:
            {
                const float L = synth == SY_RISE2 ? 2.0f : 4.0f, x = std::min (1.0f, t / L);
                filt.set (250.0f * std::pow (40.0f, x), sr, 2.5f);
                frq = 90.0f * std::pow (8.0f, x);
                ph[0] += frq * dt; if (ph[0] >= 1.0f) ph[0] -= 1.0f;
                const float saw = (2.0f * ph[0] - 1.0f) * 0.25f;
                y = (filt.bp (noise() + saw) * 1.6f) * x * x;
                if (t > L) { const float f = 1.0f - (t - L) / 0.06f; y *= std::max (0.0f, f); if (f <= 0) active = false; }
                break;
            }
            case SY_WHOOSH:
            {
                const float L = 1.7f, x = std::min (1.0f, t / L);
                filt.set (350.0f + 4200.0f * std::sin (x * juce::MathConstants<float>::pi), sr, 1.8f);
                y = filt.bp (noise()) * 1.8f * std::pow (std::sin (x * juce::MathConstants<float>::pi), 1.5f);
                if (t > L) active = false;
                break;
            }
            case SY_NOISE:
            {
                filt.set (800.0f + 11000.0f * std::exp (-t / 0.12f), sr, 0.8f);
                y = filt.lp (noise()) * std::exp (-t / 0.35f);
                if (t > 2.5f) active = false;
                break;
            }
            case SY_DOWN:
            {
                const float L = 3.0f, x = std::min (1.0f, t / L);
                filt.set (7000.0f * std::pow (250.0f / 7000.0f, x), sr, 2.2f);
                y = filt.bp (noise()) * 1.6f * (1.0f - x) * std::min (1.0f, t / 0.02f);
                if (t > L) active = false;
                break;
            }
            default: active = false;
        }
        return y;
    }

    void render (float* L, float* R, int n, float sr, float bend, const float* sideG)
    {
        const float sg = side < 2 ? sideG[side] : 1.0f;
        for (int i = 0; i < n; ++i)
        {
            if (delay > 0) { --delay; continue; }
            if (autoOff >= 0 && --autoOff < 0) release (0.06f, sr);

            if (! released) env = std::min (1.0f, env + attackInc);
            else { env *= releaseMul; if (env < 0.0001f) { active = false; return; } }
            if (oneShot && age > (juce::int64) (0.04f * sr)) drumEnv *= decayMul;
            if (accent > 0.0001f) accent *= accentMul;
            ++age;

            float l, r;
            if (kind == AIR)
            {
                const float drift = 1.0f + 0.0015f * std::sin (driftPh);
                driftPh += 0.6f / sr * juce::MathConstants<float>::twoPi;
                float s = 0;
                static constexpr float det[3] { 1.0f, 1.0058f, 0.9942f };
                for (int k = 0; k < 3; ++k)
                {
                    const float dt = frq * det[k] * drift * bend / sr;
                    float p = ph[k] + dt; if (p >= 1.0f) p -= 1.0f; ph[k] = p;
                    float saw = 2.0f * p - 1.0f;
                    if (p < dt) { const float tt = p / dt; saw -= tt + tt - tt * tt - 1.0f; }
                    else if (p > 1.0f - dt) { const float tt = (p - 1.0f) / dt; saw -= tt * tt + tt + tt + 1.0f; }
                    s += saw;
                }
                const float y = filt.lp (s * 0.3f + noise() * 0.06f);
                l = y * (1.0f + 0.2f * std::sin (ph[1] * 6.28f)); r = y * (1.0f + 0.2f * std::sin (ph[2] * 6.28f));
            }
            else if (kind == DRUMSYNTH)
            {
                l = r = drumSynth (sr);
                if (! active) return;
            }
            else
            {
                const bool looping = smp->loops() && ! oneShot && ! reverse;
                if (looping) while (pos >= smp->loopEnd) pos -= (smp->loopEnd - smp->loopStart);
                if (reverse && pos <= 1.0) { active = false; return; }
                const int ip = (int) pos; const float fr = (float) (pos - ip);
                if (! looping && ip >= smp->length - 1) { active = false; return; }
                l = hermite (smp->l.data(), ip, fr);
                r = hermite (smp->r.data(), ip, fr);
                float ratio = bend;
                if (std::abs (glide) > 1.0e-6f)
                {
                    ratio *= std::exp2 (glide / 12.0f);
                    glide = std::abs (glide) <= std::abs (glideStep) ? 0.0f : glide - glideStep;
                }
                pos += reverse ? -inc * ratio : inc * ratio;
            }
            const float a = std::pow (env, envPow) * gain * drumEnv * sg * (1.0f + accent);
            L[i] += l * a * panL;
            R[i] += r * a * panR;
        }
    }
};

// ---------------------------------------------------------------------------------------------
// 8-Kanal-FDN-Hall mit Daempfung und Predelay
class FDNReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : lines) d.assign ((size_t) (sr * 0.25f) + 8, 0.0f);
        pre.assign ((size_t) (sr * 0.1f) + 8, 0.0f);
        for (auto& f : damp) f = 0;
        hp = 0; size = -1;
        setSize (0.6f);
    }

    void setSize (float s)
    {
        if (std::abs (s - size) < 0.0005f) return;
        size = s;
        static constexpr float base[8] { 1123, 1291, 1447, 1613, 1777, 1949, 2129, 2311 };
        const float scale = (0.55f + 1.6f * s) * sr / 44100.0f;
        const float rt60 = 1.4f + 7.0f * s * s;
        for (int i = 0; i < 8; ++i)
        {
            len[i] = juce::jlimit (16, (int) lines[i].size() - 2, (int) (base[i] * scale));
            fb[i] = std::pow (10.0f, -3.0f * (float) len[i] / (rt60 * sr));
        }
        preLen = juce::jlimit (1, (int) pre.size() - 2, (int) (sr * (0.012f + 0.03f * s)));
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const float dampC = 0.35f, hpC = std::exp (-2.0f * juce::MathConstants<float>::pi * 180.0f / sr);
        for (int i = 0; i < n; ++i)
        {
            float in = (inL[i] + inR[i]) * 0.5f;
            pre[(size_t) prePos] = in;
            int rp = prePos - preLen; if (rp < 0) rp += (int) pre.size();
            float x = pre[(size_t) rp];
            if (++prePos >= (int) pre.size()) prePos = 0;
            hp = hpC * hp + (1 - hpC) * x; x -= hp;

            float o[8];
            for (int k = 0; k < 8; ++k)
            {
                int p = pos[k] - len[k]; if (p < 0) p += (int) lines[k].size();
                const float v = lines[k][(size_t) p];
                damp[k] += dampC * (v - damp[k]);
                o[k] = damp[k] * fb[k];
            }
            float h[8];
            for (int k = 0; k < 8; ++k) h[k] = o[k];
            for (int s = 1; s < 8; s <<= 1)
                for (int k = 0; k < 8; k += s * 2)
                    for (int j = k; j < k + s; ++j) { const float a = h[j], b = h[j + s]; h[j] = a + b; h[j + s] = a - b; }
            for (int k = 0; k < 8; ++k)
            {
                lines[k][(size_t) pos[k]] = h[k] * 0.35355339f + x * (k & 1 ? -0.5f : 0.5f);
                if (++pos[k] >= (int) lines[k].size()) pos[k] = 0;
            }
            outL[i] = (o[0] + o[2] + o[4] + o[6]) * 0.5f;
            outR[i] = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
        }
    }

private:
    float sr = 44100, size = -1, hp = 0;
    std::vector<float> lines[8], pre;
    int len[8] {}, pos[8] {}, prePos = 0, preLen = 1;
    float fb[8] {}, damp[8] {};
};

// ---------------------------------------------------------------------------------------------
class Engine
{
public:
    static constexpr int maxVoices = 256;

    explicit Engine (SampleBank& b) : bank (b) {}

    void prepare (double sampleRate, int maxBlock)
    {
        sr = (float) sampleRate;
        dry.setSize (2, std::max (maxBlock, 512)); wet.setSize (2, std::max (maxBlock, 512));
        reverb.prepare (sampleRate);
        for (auto& v : voices) v.active = false;
        for (auto& f : toneF) f.reset();
        chorus.assign ((size_t) (sr * 0.05f) + 4, 0.0f); chorusR.assign (chorus.size(), 0.0f);
        gateSm = 1.0f;
    }

    // instr 0 VLN, 1 VLA, 2 VC, 3 CB; Spalten: SUS, SPIC, PIZZ, TREM
    static const int* instrGroups (int instr)
    {
        static const int t[4][4] { { G_VLN_SUS, G_VLN_SPIC, G_VLN_PIZZ, G_VLN_TREM },
                                   { G_VLA_SUS, G_VLA_SPIC, G_VLA_PIZZ, G_VLA_TREM },
                                   { G_VC_SUS,  G_VC_SPIC,  G_VC_PIZZ,  G_VC_TREM },
                                   { G_CB_SUS,  G_CB_SPIC,  G_CB_PIZZ,  G_CB_TREM } };
        return t[juce::jlimit (0, 3, instr)];
    }

    static int baseOf (int artic)   // Sample-Artikulation hinter einer Spielweise
    {
        switch (artic) { case A_SPIC: return 1; case A_PIZZ: return 2; case A_TREM: return 3; default: return 0; }
    }

    // Gruppen, die fuer die aktuellen Einstellungen geladen sein muessen
    static std::set<int> groupsFor (const Settings& s)
    {
        std::set<int> g;
        auto addInstr = [&] (int instr) { for (int a = 0; a < 4; ++a) g.insert (instrGroups (instr)[a]); };
        switch (s.mode)
        {
            case M_STRINGS: addInstr (0); addInstr (1); addInstr (2); if (s.basses > 0.01f) addInstr (3); break;
            case M_CELLOS:  addInstr (2); if (s.basses > 0.01f) addInstr (3); break;
            case M_ATMOS:
                for (int src : { s.padA, s.padB })
                    if (juce::isPositiveAndBelow (src, numPadSources))
                        for (int i = 0; i < padSources[src].n; ++i) g.insert (padSources[src].z[i].group);
                break;
            default:
                if (s.kit == K_TIMPANI) { g.insert (G_TIMP_HIT); g.insert (G_TIMP_ROLL); }
                else if (s.kit == K_TRAILER)
                {
                    for (auto& p : trailerPieces) for (int i = 0; i < p.nl; ++i) g.insert (p.l[i].group);
                }
                else for (auto& k : kitPieces) if (k.kit == s.kit) g.insert (k.group);
        }
        return g;
    }

    void setSettings (const Settings& s, bool articFromParam)
    {
        const bool modeChange = s.mode != cur.mode || (s.mode == M_DRUMS && s.kit != cur.kit)
                             || (s.mode == M_ATMOS && (s.padA != cur.padA || s.padB != cur.padB));
        if (modeChange) for (auto& v : voices) if (v.active) v.release (0.015f, sr);
        if (s.mode != cur.mode) heldCount = 0;
        cur = s;
        if (articFromParam) curArtic = s.artic;
    }

    int getArtic() const { return curArtic; }

    void allNotesOff()
    {
        for (auto& v : voices) if (v.active) v.release (cur.release, sr);
        heldCount = 0;
    }

    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) noteOff (m.getNoteNumber());
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(); const float v = (float) m.getControllerValue() / 127.0f;
            if (cc == 1) modWheel = v;
            else if (cc == 11) expression = v;
            else if (cc == 64)
            {
                pedal = v >= 0.5f;
                if (! pedal) for (auto& vc : voices) if (vc.active && vc.sustained) { vc.sustained = false; vc.release (relFor(), sr); }
            }
            else if (cc == 120 || cc == 123) allNotesOff();
        }
        else if (m.isPitchWheel())
            bendSemis = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
    }

    void setTransport (const Transport& t)
    {
        tp.bpm = juce::jlimit (20.0, 400.0, t.bpm);
        tp.playing = t.playing;
        const double expected = tp.ppq;
        tp.ppq = t.playing ? t.ppq : freePpq;
        // Start oder Sprung im Song (Loop): Step-Zaehler neu aufsetzen
        if (t.playing && (! wasPlaying || std::abs (t.ppq - expected) > 0.05))
            lastStep = (long long) std::floor (t.ppq / rateBeats[juce::jlimit (0, 4, cur.rhyRate)]) - 1;
        wasPlaying = t.playing;
    }

    void render (juce::AudioBuffer<float>& out, int start, int n)
    {
        for (int done = 0; done < n;)
        {
            const int c = std::min (64, n - done);
            renderChunk (out, start + done, c);
            done += c;
        }
    }

    // fuer die GUI
    std::atomic<int> lastHitNote { -1 };
    std::atomic<juce::uint32> lastHitCount { 0 };
    std::atomic<int> currentStep { -1 };
    std::atomic<float> morphPos { 0.0f };
    std::atomic<float> level { 0.0f };
    std::atomic<int> pendingArtic { -1 };  // Keyswitch -> Parameter (Message-Thread)

private:
    // ------------------------------------------------------------------ Rhythm
    void advanceRhythm (int n)
    {
        const double beatsPerSample = tp.bpm / 60.0 / sr;
        const double stepBeats = rateBeats[juce::jlimit (0, 4, cur.rhyRate)];
        const bool active = cur.rhyMode != R_OFF && cur.mode != M_DRUMS;
        const auto& pat = patterns[juce::jlimit (0, numPatterns - 1, cur.rhyPattern)];
        const double ppqEnd = tp.ppq + beatsPerSample * n;

        if (active)
        {
            const long long s0 = (long long) std::floor (tp.ppq / stepBeats) - 1, s1 = (long long) std::floor (ppqEnd / stepBeats);
            for (long long s = s0; s <= s1; ++s)
            {
                double stepStart = (double) s * stepBeats;
                if ((s & 1) != 0) stepStart += stepBeats * 0.5 * cur.swing;   // Swing: jeder zweite Step spaeter
                if (stepStart >= tp.ppq && stepStart < ppqEnd && s > lastStep)
                {
                    lastStep = s;
                    const int idx = (int) (((s % 16) + 16) % 16);
                    currentStep = idx;
                    stepLevel = pat.s[idx];
                    stepSamples = std::max (1, (int) (stepBeats * 60.0 / tp.bpm * sr));
                    gateTimer = stepLevel > 0 ? (int) (stepSamples * 0.62f) : 0;
                    if (cur.rhyMode == R_STRIKE && stepLevel > 0 && cur.mode != M_ATMOS) strikeHeld (stepLevel);
                }
            }
        }
        else { currentStep = -1; lastStep = (long long) std::floor (ppqEnd / stepBeats); }
        tp.ppq = ppqEnd;
        freePpq = ppqEnd;
    }

    void strikeHeld (int lvl)
    {
        const float accentVel = lvl == 3 ? 1.0f : (lvl == 2 ? 0.78f : 0.55f);
        const int ar = curArtic == A_PIZZ ? A_PIZZ : (curArtic == A_MARC ? A_MARC : A_SPIC);
        for (int i = 0; i < heldCount; ++i)
        {
            const float v = juce::jlimit (0.05f, 1.0f, held[i].vel * (1.0f - cur.rhyDepth * (1.0f - accentVel)));
            playStringNote (held[i].note, v, ar, (int) (stepSamples * 0.85f));
        }
    }

    // ------------------------------------------------------------------ Render
    void renderChunk (juce::AudioBuffer<float>& out, int start, int n)
    {
        advanceRhythm (n);

        auto* dl = dry.getWritePointer (0); auto* dr = dry.getWritePointer (1);
        juce::FloatVectorOperations::clear (dl, n); juce::FloatVectorOperations::clear (dr, n);

        // Pad-Morph A <-> B (Evolve = langsame Bewegung)
        evoPh += (0.02f + 0.12f * cur.evolve) * (float) n / sr; if (evoPh >= 1.0f) evoPh -= 1.0f;
        const float m = juce::jlimit (0.0f, 1.0f, cur.morph + cur.evolve * 0.5f * std::sin (evoPh * juce::MathConstants<float>::twoPi));
        morphSm += (m - morphSm) * 0.2f;
        morphPos = morphSm;
        const float sideG[2] { std::cos (morphSm * juce::MathConstants<float>::halfPi), std::sin (morphSm * juce::MathConstants<float>::halfPi) };

        const float bend = std::exp2 (bendSemis / 12.0f);
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (v.kind == Voice::SAMPLE && bank.get (v.group) != v.data) { v.active = false; continue; }
            v.render (dl, dr, n, sr, bend, sideG);
        }

        const bool drums = cur.mode == M_DRUMS;
        const float exprTarget = drums ? 1.0f : (0.2f + 0.8f * modWheel) * (0.15f + 0.85f * expression);

        const bool gate = ! drums && (cur.rhyMode == R_GATE || (cur.rhyMode == R_STRIKE && cur.mode == M_ATMOS));
        const float gateCoef = std::exp (-1.0f / (0.004f * sr));
        const float baseCut = cur.tone < 0.0f ? 9000.0f * std::pow (700.0f / 9000.0f, -cur.tone)
                                              : 9000.0f * std::pow (20000.0f / 9000.0f, cur.tone);
        const float lfoRate = 0.05f + 0.25f * cur.motion;
        for (int i = 0; i < n; ++i)
        {
            exprSm += (exprTarget - exprSm) * 0.0008f;
            float g = 1.0f;
            if (gate)
            {
                const float lvlG = stepLevel == 3 ? 1.0f : (stepLevel == 2 ? 0.8f : 0.6f);
                const float open = gateTimer > 0 ? lvlG : 0.0f;
                if (gateTimer > 0) --gateTimer;
                const float target = 1.0f - cur.rhyDepth * (1.0f - open);
                gateSm = target + (gateSm - target) * gateCoef;
                g = gateSm;
            }
            else gateSm = 1.0f;
            float cut = baseCut;
            if (cur.mode == M_ATMOS)
            {
                lfo += lfoRate / sr; if (lfo >= 1.0f) lfo -= 1.0f;
                cut *= std::exp2 (cur.motion * 1.3f * std::sin (lfo * juce::MathConstants<float>::twoPi));
            }
            if ((i & 15) == 0)
            {
                toneF[0].set (std::min (cut * (0.55f + 0.45f * exprSm), 20000.0f), sr);
                toneF[1].g = toneF[0].g; toneF[1].k = toneF[0].k; toneF[1].a1 = toneF[0].a1; toneF[1].a2 = toneF[0].a2; toneF[1].a3 = toneF[0].a3;
            }
            float l = dl[i] * exprSm * g, r = dr[i] * exprSm * g;
            if (cut < 19000.0f || cur.mode == M_ATMOS) { l = toneF[0].lp (l); r = toneF[1].lp (r); }
            dl[i] = l; dr[i] = r;
        }

        if (cur.mode == M_ATMOS) applyChorus (dl, dr, n, 0.35f + 0.5f * cur.motion);
        if (drums && cur.punch > 0.001f) applyPunch (dl, dr, n);

        // Drive (Band-Saettigung) + Stereobreite
        const float drv = 1.0f + 5.0f * cur.drive, drvNorm = 1.0f / std::tanh (drv);
        for (int i = 0; i < n; ++i)
        {
            float l = dl[i], r = dr[i];
            if (cur.drive > 0.001f)
            {
                l += (std::tanh (l * drv) * drvNorm * 0.8f - l) * cur.drive;
                r += (std::tanh (r * drv) * drvNorm * 0.8f - r) * cur.drive;
            }
            const float mm = (l + r) * 0.5f, s = (l - r) * 0.5f * cur.width;
            dl[i] = mm + s; dr[i] = mm - s;
        }

        reverb.setSize (cur.space);
        auto* wl = wet.getWritePointer (0); auto* wr = wet.getWritePointer (1);
        reverb.process (dl, dr, wl, wr, n);
        const float wetG = cur.reverb * 0.9f, dryG = 1.0f - 0.45f * cur.reverb;
        const float vol = juce::Decibels::decibelsToGain (cur.volumeDb);
        auto* ol = out.getWritePointer (0, start);
        auto* orr = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;
        float pk = 0;
        for (int i = 0; i < n; ++i)
        {
            float l = (dl[i] * dryG + wl[i] * wetG) * vol, r = (dr[i] * dryG + wr[i] * wetG) * vol;
            l = std::abs (l) > 0.9f ? std::tanh (l) : l; r = std::abs (r) > 0.9f ? std::tanh (r) : r;
            ol[i] += l; if (orr) orr[i] += r;
            pk = std::max (pk, std::max (std::abs (l), std::abs (r)));
        }
        level = std::max (pk, level.load() * 0.97f);
    }

    float relFor() const
    {
        if (cur.mode == M_ATMOS) return cur.release + 1.2f;
        if (cur.mode == M_DRUMS) return 0.3f;
        return cur.release;
    }

    Voice* freeVoice()
    {
        Voice* best = nullptr;
        for (auto& v : voices) if (! v.active) return &v;
        for (auto& v : voices) if (v.released && (best == nullptr || v.order < best->order)) best = &v;
        if (best == nullptr) for (auto& v : voices) if (best == nullptr || v.order < best->order) best = &v;
        return best;
    }

    float rnd() { rngState = rngState * 1664525u + 1013904223u; return (float) (rngState >> 8) / 16777216.0f; }

    static void setPan (Voice& v, float pan)
    {
        const float p = juce::jlimit (-1.0f, 1.0f, pan);
        v.panL = std::sqrt (0.5f * (1.0f - p)) * 1.4142f; v.panR = std::sqrt (0.5f * (1.0f + p)) * 1.4142f;
    }

    struct StartOpts
    {
        float gain = 1, pan = 0, cents = 0, attack = 0.002f, envPow = 2.0f, accent = 0, glide = 0, glideTime = 0.1f;
        int delay = 0, side = 2, autoOff = -1; bool oneShot = false, reverse = false; double startSec = 0;
    };

    Voice* startSample (int group, int note, int playNote, float vel, const StartOpts& o)
    {
        auto* gd = bank.get (group);
        if (gd == nullptr || gd->samples.empty()) return nullptr;
        const int rr = rrCounter[(size_t) group]++;
        auto* s = gd->pick (playNote, vel, rr);
        if (s == nullptr) return nullptr;
        auto* v = freeVoice();
        *v = Voice();
        v->active = true; v->kind = Voice::SAMPLE; v->note = note; v->group = group; v->data = gd; v->smp = s;
        v->oneShot = o.oneShot; v->reverse = o.reverse; v->order = ++orderCounter; v->side = o.side;
        const float semis = (float) (playNote - s->root) + o.cents / 100.0f;
        v->inc = std::exp2 (semis / 12.0) * 44100.0 / sr;
        v->gain = o.gain;
        setPan (*v, o.pan);
        v->envPow = o.envPow;
        v->attackInc = 1.0f / std::max (1.0f, o.attack * sr);
        if (o.oneShot && ! o.reverse) { v->env = 1.0f; v->attackInc = 1.0f; }
        if (o.reverse) { v->pos = s->length - 3; v->env = 0.0f; v->attackInc = 1.0f / (0.02f * sr); v->envPow = 1.0f; }
        else v->pos = std::min ((double) s->length * 0.5, o.startSec * 44100.0);
        v->delay = o.delay;
        v->autoOff = o.autoOff;
        if (o.accent > 0) { v->accent = o.accent; v->accentMul = std::exp (-1.0f / (0.11f * sr)); }
        if (std::abs (o.glide) > 1.0e-6f) { v->glide = o.glide; v->glideStep = o.glide / std::max (1.0f, o.glideTime * sr); }
        if (o.oneShot && cur.decay < 0.999f)
        {
            const float tau = 0.08f + 3.0f * cur.decay * cur.decay;
            v->decayMul = std::exp (-1.0f / (tau * sr));
        }
        return v;
    }

    void startAir (int note, float vel, int side, float gain)
    {
        auto* v = freeVoice();
        *v = Voice();
        v->active = true; v->kind = Voice::AIR; v->note = note; v->order = ++orderCounter; v->side = side;
        v->frq = 440.0f * std::exp2 (((float) note + cur.tune - 69.0f) / 12.0f);
        v->filt.set (std::min (v->frq * 3.0f + 600.0f, 7000.0f), sr, 0.9f);
        for (auto& p : v->ph) p = rnd();
        v->driftPh = rnd() * 6.28f;
        v->rng = (juce::uint32) (rnd() * 1e9f) + 1;
        v->gain = gain * (0.5f + 0.5f * vel);
        v->envPow = 2.0f;
        v->attackInc = 1.0f / std::max (1.0f, (cur.attack + 0.9f) * sr);
    }

    void startDrumSynth (int note, int type, float gain, int delay = 0)
    {
        if (type == SY_NONE || gain <= 0.001f) return;
        auto* v = freeVoice();
        *v = Voice();
        v->active = true; v->kind = Voice::DRUMSYNTH; v->note = note; v->synth = type; v->order = ++orderCounter;
        v->gain = gain; v->env = 1.0f; v->attackInc = 1.0f; v->envPow = 1.0f; v->oneShot = true; v->delay = delay;
        v->rng = (juce::uint32) (rnd() * 1e9f) + 7;
        if (cur.decay < 0.999f && type != SY_RISE2 && type != SY_RISE4 && type != SY_WHOOSH)
            v->decayMul = std::exp (-1.0f / ((0.08f + 3.0f * cur.decay * cur.decay) * sr));
    }

    // ------------------------------------------------------------------ Strings / Cellos
    float tuneCents() const { return (cur.tune - std::round (cur.tune)) * 100.0f; }

    void playLayer (int group, int note, int playNote, float vel, float gain, float pan, int artic, int autoOff,
                    float glide, bool legatoStart)
    {
        const bool shortArt = artic == A_SPIC || artic == A_PIZZ;
        StartOpts o;
        o.autoOff = autoOff;
        o.attack = shortArt || artic == A_MARC ? 0.002f : cur.attack;
        if (artic == A_SWELL) { o.attack = cur.attack + 1.6f; o.envPow = 3.5f; }
        if (artic == A_MARC) o.accent = 0.9f;
        if (legatoStart) { o.attack = 0.07f; o.startSec = 0.18; o.glide = glide; o.glideTime = cur.glide; }

        const float hz = cur.humanize;
        o.delay = legatoStart ? 0 : (int) (rnd() * hz * 0.025f * sr);
        o.cents = (rnd() * 2.0f - 1.0f) * hz * 6.0f + tuneCents();
        o.gain = gain * (1.0f - hz * 0.25f * rnd());
        o.pan = pan;
        startSample (group, note, playNote, vel, o);

        if (cur.ensemble > 0.02f && ! shortArt)
        {
            StartOpts e = o;
            e.cents = (rnd() > 0.5f ? 1.0f : -1.0f) * (4.0f + 8.0f * cur.ensemble) + tuneCents();
            e.gain = o.gain * cur.ensemble * 0.7f;
            e.pan = -pan * 0.8f + (rnd() - 0.5f) * cur.ensemble;
            e.delay = o.delay + (int) ((0.012f + 0.02f * cur.ensemble) * sr);
            e.attack = o.attack * 1.3f;
            startSample (group, note, playNote, vel, e);
        }
    }

    void playStringNote (int note, float vel, int artic, int autoOff, float glide = 0, bool legatoStart = false)
    {
        const float vg = 0.35f + 0.65f * std::pow (vel, 1.3f);
        const int pn = note + (int) std::round (cur.tune);
        const int b = baseOf (artic);

        auto layer = [&] (int instr, int playN, float g, float pan)
        {
            playLayer (instrGroups (instr)[b], note, playN, vel, g, pan, artic, autoOff, glide, legatoStart);
            if (artic == A_MARC && ! legatoStart)   // Marcato: Spiccato-Biss obendrauf
            {
                StartOpts o; o.gain = g * 0.55f; o.pan = pan; o.cents = tuneCents(); o.autoOff = autoOff;
                startSample (instrGroups (instr)[1], note, playN, vel, o);
            }
        };

        if (cur.mode == M_CELLOS)
            layer (2, pn, vg, 0.0f);
        else
        {
            if (pn < 48) layer (2, pn, vg, 0.15f);
            else if (pn < 55) layer (1, pn, vg, 0.05f);
            else if (pn < 68) { layer (0, pn, vg * 0.78f, -0.25f); layer (1, pn, vg * 0.62f, 0.25f); }
            else layer (0, pn, vg, -0.1f);
        }
        // Kontrabass eine Oktave tiefer
        if (cur.basses > 0.01f && pn - 12 >= 26 && pn - 12 <= 64)
            layer (3, pn - 12, vg * cur.basses * 0.9f, 0.1f);
    }

    // ------------------------------------------------------------------ Pads
    void playPad (int note, float vel)
    {
        const float pv = (0.35f + 0.65f * std::pow (vel, 1.3f)) * 1.5f;
        const int pn = note + (int) std::round (cur.tune);
        for (int side = 0; side < 2; ++side)
        {
            const int src = side == 0 ? cur.padA : cur.padB;
            if (! juce::isPositiveAndBelow (src, numPadSources)) continue;
            if (side == 1 && src == cur.padA) continue;   // gleiche Quelle nur einmal
            const int sd = (cur.padA == cur.padB) ? 2 : side;
            const auto& ps = padSources[src];
            if (ps.n == 0) { startAir (note, vel, sd, 0.45f); continue; }
            for (int i = 0; i < ps.n; ++i)
            {
                const auto& z = ps.z[i];
                if (pn < z.lo || pn > z.hi) continue;
                StartOpts o; o.side = sd; o.attack = cur.attack + 0.35f; o.cents = tuneCents();
                o.gain = pv * z.gain; o.pan = z.pan;
                startSample (z.group, note, pn, vel, o);
                StartOpts e = o; e.gain = o.gain * 0.42f; e.pan = -z.pan * 0.8f + (rnd() - 0.5f) * 0.8f;
                e.cents = (rnd() > 0.5f ? 9.0f : -9.0f) + tuneCents(); e.delay = (int) (0.025f * sr); e.attack = o.attack * 1.3f;
                startSample (z.group, note, pn, vel, e);
                if (cur.shimmer > 0.01f)
                {
                    StartOpts s = o; s.gain = o.gain * cur.shimmer * 0.5f; s.pan = (rnd() - 0.5f) * 1.6f;
                    startSample (z.group, note, std::min (pn + 12, 108), vel, s);
                }
            }
        }
        if (cur.air > 0.01f) startAir (note, vel, 2, cur.air * 0.16f);
    }

    // ------------------------------------------------------------------ Drums
    void playDrum (int note, float vel)
    {
        const float vg = (0.35f + 0.65f * std::pow (vel, 1.3f)) * 1.1f;
        const float tuneC = cur.tune * 100.0f;
        auto hit = [&] (int group, int playNote, float gain, float semis, bool reverse, float pan, int delay)
        {
            auto* gd = bank.get (group);
            if (gd == nullptr || gd->samples.empty()) return;
            StartOpts o; o.oneShot = true; o.reverse = reverse; o.gain = gain; o.pan = pan; o.delay = delay;
            o.cents = tuneC + semis * 100.0f;
            const int root = playNote < 0 ? gd->samples.front().root : playNote;
            startSample (group, note, root, vel, o);
        };
        auto stacked = [&] (int group, int playNote, float gain, float semis, bool reverse)
        {
            hit (group, playNote, gain, semis, reverse, 0.0f, 0);
            const int extra = (int) std::round (cur.stack * 3.0f);   // Stack: Ensemble aus mehreren Spielern
            for (int k = 0; k < extra; ++k)
                hit (group, playNote, gain * 0.7f, semis + (rnd() - 0.5f) * 0.3f, reverse, (rnd() - 0.5f) * 1.2f,
                     (int) ((0.008f + rnd() * 0.03f) * sr));
        };

        int cnt = 0; Voice* oldest = nullptr;   // max. 8 Stimmen pro Taste
        for (auto& v : voices) if (v.active && v.note == note && ! v.released) { ++cnt; if (! oldest || v.order < oldest->order) oldest = &v; }
        if (cnt >= 8 && oldest) oldest->release (0.02f, sr);

        bool any = false;
        if (cur.kit == K_TIMPANI)
        {
            if (note >= 36 && note <= 59) { stacked (G_TIMP_HIT, note, vg, 0, false); startDrumSynth (note, SY_BOOM, cur.sub * vg * 0.8f); any = true; }
            else if (note >= 60 && note <= 83) { hit (G_TIMP_ROLL, note - 24, vg, 0, false, 0.0f, 0); any = true; }
        }
        else if (cur.kit == K_TRAILER)
        {
            for (auto& p : trailerPieces)
                if (p.note == note)
                {
                    for (int i = 0; i < p.nl; ++i) stacked (p.l[i].group, -1, vg * p.l[i].gain, p.l[i].semis, p.l[i].reverse);
                    startDrumSynth (note, p.synth, p.synthGain * vg * (0.6f + cur.sub) * 0.9f);
                    any = true;
                }
        }
        else
        {
            for (auto& k : kitPieces)
                if (k.kit == cur.kit && k.note == note)
                {
                    stacked (k.group, -1, vg, 0, false);
                    if (k.low) startDrumSynth (note, SY_BOOM, cur.sub * vg * 0.8f);
                    any = true;
                }
        }
        if (any) { lastHitNote = note; ++lastHitCount; }
    }

    // ------------------------------------------------------------------ Noten
    void noteOn (int note, float vel)
    {
        const bool stringsMode = cur.mode == M_STRINGS || cur.mode == M_CELLOS;
        if (stringsMode && note >= keyswitchLow && note < keyswitchLow + A_NUM)
        {
            curArtic = note - keyswitchLow;
            pendingArtic = curArtic;
            return;
        }
        if (cur.mode == M_DRUMS) { playDrum (note, vel); return; }

        for (auto& v : voices) if (v.active && v.note == note && ! v.released && ! v.oneShot && v.autoOff < 0) v.release (0.08f, sr);
        const int prevTop = heldCount > 0 ? held[heldCount - 1].note : -1;
        addHeld (note, vel);

        if (cur.rhyMode == R_STRIKE && stringsMode) return;   // Rhythm-Engine schlaegt im Tempo an
        if (cur.mode == M_ATMOS) { playPad (note, vel); return; }

        const bool shortArt = curArtic == A_SPIC || curArtic == A_PIZZ;
        if (cur.legato && ! shortArt && prevTop >= 0)
        {
            // Legato: alte Note weich raus, neue gleitet von der alten Tonhoehe heran
            for (auto& v : voices) if (v.active && v.note == prevTop && ! v.released) v.release (0.12f, sr);
            playStringNote (note, vel, curArtic, -1, (float) (prevTop - note), true);
            return;
        }
        playStringNote (note, vel, curArtic, -1);
    }

    void noteOff (int note)
    {
        if (cur.mode == M_DRUMS) return;
        const bool stringsMode = cur.mode == M_STRINGS || cur.mode == M_CELLOS;
        if (stringsMode && note >= keyswitchLow && note < keyswitchLow + A_NUM) return;
        const bool wasTop = heldCount > 0 && held[heldCount - 1].note == note;
        removeHeld (note);

        if (stringsMode && cur.legato && wasTop && heldCount > 0 && cur.rhyMode != R_STRIKE
            && curArtic != A_SPIC && curArtic != A_PIZZ && ! pedal)
        {
            // Legato zurueck zur noch gehaltenen Note
            const auto back = held[heldCount - 1];
            for (auto& v : voices) if (v.active && v.note == note && ! v.released) v.release (0.12f, sr);
            playStringNote (back.note, back.vel, curArtic, -1, (float) (note - back.note), true);
            return;
        }
        for (auto& v : voices)
            if (v.active && v.note == note && ! v.released && v.autoOff < 0)
            {
                if (pedal) v.sustained = true;
                else v.release (relFor(), sr);
            }
    }

    struct Held { int note; float vel; };
    void addHeld (int note, float vel)
    {
        removeHeld (note);
        if (heldCount < 32) held[heldCount++] = { note, vel };
    }
    void removeHeld (int note)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[i].note == note) { for (int j = i; j < heldCount - 1; ++j) held[j] = held[j + 1]; --heldCount; return; }
    }

    // ------------------------------------------------------------------ FX
    void applyChorus (float* l, float* r, int n, float depth)
    {
        const int sz = (int) chorus.size();
        for (int i = 0; i < n; ++i)
        {
            chorus[(size_t) chPos] = l[i]; chorusR[(size_t) chPos] = r[i];
            chLfo += 0.21f / sr; if (chLfo >= 1.0f) chLfo -= 1.0f;
            chLfo2 += 0.273f / sr; if (chLfo2 >= 1.0f) chLfo2 -= 1.0f;
            const float m1 = std::sin (chLfo * juce::MathConstants<float>::twoPi);
            const float m2 = std::cos (chLfo2 * juce::MathConstants<float>::twoPi);
            auto tap = [&] (std::vector<float>& buf, float ms)
            {
                const float d = ms * 0.001f * sr; float rp = (float) chPos - d; if (rp < 0) rp += (float) sz;
                const int i0 = (int) rp; const float f = rp - (float) i0;
                return buf[(size_t) i0] * (1 - f) + buf[(size_t) ((i0 + 1) % sz)] * f;
            };
            const float cl = tap (chorusR, 14.0f + 6.0f * m1), cr = tap (chorus, 17.0f + 6.0f * m2);
            l[i] = l[i] * (1.0f - depth * 0.35f) + cl * depth * 0.5f;
            r[i] = r[i] * (1.0f - depth * 0.35f) + cr * depth * 0.5f;
            if (++chPos >= sz) chPos = 0;
        }
    }

    void applyPunch (float* l, float* r, int n)
    {
        const float fa = std::exp (-1.0f / (0.001f * sr)), sa = std::exp (-1.0f / (0.05f * sr));
        const float drive = 1.0f + 3.0f * cur.punch, norm = 1.0f / std::tanh (drive);
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (l[i]), std::abs (r[i]));
            envF = x > envF ? x : envF * fa + x * (1 - fa);
            envS = sa * envS + (1 - sa) * x;
            const float tr = juce::jlimit (1.0f, 3.0f, 1.0f + cur.punch * 2.5f * std::max (0.0f, envF - envS) / (envS + 0.01f));
            float a = l[i] * tr, b = r[i] * tr;
            a = std::tanh (a * drive) * norm; b = std::tanh (b * drive) * norm;
            l[i] = l[i] + (a - l[i]) * cur.punch; r[i] = r[i] + (b - r[i]) * cur.punch;
        }
    }

    SampleBank& bank;
    Settings cur;
    int curArtic = A_SUS;
    Transport tp;
    double freePpq = 0;
    bool wasPlaying = false;
    long long lastStep = -1000000;
    int stepLevel = 0, stepSamples = 1, gateTimer = 0;
    float gateSm = 1.0f;
    float sr = 44100;
    std::array<Voice, maxVoices> voices;
    std::array<int, (size_t) G_NUM> rrCounter {};
    Held held[32] {}; int heldCount = 0;
    juce::uint32 orderCounter = 0, rngState = 22222;
    float modWheel = 1.0f, expression = 1.0f, exprSm = 1.0f, bendSemis = 0;
    bool pedal = false;
    float lfo = 0, chLfo = 0, chLfo2 = 0.25f, envF = 0, envS = 0, evoPh = 0, morphSm = 0;
    int chPos = 0;
    std::vector<float> chorus, chorusR;
    SVF toneF[2];
    FDNReverb reverb;
    juce::AudioBuffer<float> dry, wet;
};

} // namespace orch
