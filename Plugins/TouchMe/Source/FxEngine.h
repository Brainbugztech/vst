#pragma once

// BrainBugz Touch Me!: Effekt-Algorithmen im XY-Pad-Stil
// Jeder Algorithmus bekommt X/Y (0..1) und vier feste Preset-Parameter p[0..3] (0..1).

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace swarm
{
constexpr float kPi = 3.14159265358979f;

struct Ctx
{
    double sr = 44100.0;
    double bpm = 120.0;
    double ppq = 0.0;      // Songposition in Vierteln (am Anfang des Kontrollblocks)
    bool ppqValid = false; // Host spielt und liefert Position
    bool touched = false;
    bool onset = false;    // Pad gerade erst berührt
};

inline float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float expMap (float t, float lo, float hi) { return lo * std::pow (hi / lo, clamp01 (t)); }
inline float softClip (float v) { return std::tanh (v); }

// Notenwerte in Vierteln: 1/32 .. 1/1
constexpr int kNumDivs = 12;
constexpr float kDivBeats[kNumDivs] = { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f,
                                        2.0f / 3.0f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f };
inline const char* divName (int i)
{
    static const char* n[kNumDivs] = { "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8",
                                       "1/4T", "1/8D", "1/4", "1/4D", "1/2", "1/1" };
    return n[juce::jlimit (0, kNumDivs - 1, i)];
}
inline int divIndex (float t) { return juce::jlimit (0, kNumDivs - 1, (int) (clamp01 (t) * kNumDivs)); }
inline double beatsToSamples (float beats, const Ctx& c) { return beats * 60.0 / std::max (20.0, c.bpm) * c.sr; }

//==============================================================================
// Ringpuffer (Stereo) mit fraktionalem Lesen
struct Ring
{
    std::vector<float> d[2];
    int size = 0;
    long long w = 0; // absoluter Schreibzähler

    void setSize (int n) { size = std::max (8, n); for (auto& v : d) v.assign ((size_t) size, 0.0f); w = 0; }
    void clear() { for (auto& v : d) std::fill (v.begin(), v.end(), 0.0f); }
    void push (float l, float r)
    {
        const int i = (int) (w % size);
        d[0][(size_t) i] = l; d[1][(size_t) i] = r; ++w;
    }
    float at (int ch, long long absIdx) const
    {
        long long i = absIdx % size; if (i < 0) i += size;
        return d[ch][(size_t) i];
    }
    // absolute fraktionale Position, Hermite-Interpolation
    float readAbs (int ch, double pos) const
    {
        const long long i = (long long) std::floor (pos);
        const float f = (float) (pos - (double) i);
        const float xm1 = at (ch, i - 1), x0 = at (ch, i), x1 = at (ch, i + 1), x2 = at (ch, i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    // Verzögerung in Samples hinter dem zuletzt geschriebenen Sample (>= 1)
    float readDelay (int ch, double delay) const { return readAbs (ch, (double) w - std::max (1.0, delay)); }
};

// Zustandsvariablen-Filter (TPT)
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 1;
    void set (float fc, float q, double sr)
    {
        fc = juce::jlimit (10.0f, (float) sr * 0.45f, fc);
        const float g = std::tan (kPi * fc / (float) sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void tick (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

struct OnePole
{
    float z = 0, a = 0;
    void setLP (float fc, double sr) { a = std::exp (-2.0f * kPi * juce::jlimit (5.0f, (float) sr * 0.45f, fc) / (float) sr); }
    float lp (float x) { z = x + a * (z - x); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0; }
};

// LFO-Phase, optional an die Host-Position gekoppelt
struct SyncPhase
{
    double phase = 0, inc = 0;
    void control (float beats, const Ctx& c, bool sync = true)
    {
        const double len = beatsToSamples (beats, c);
        inc = 1.0 / std::max (1.0, len);
        if (sync && c.ppqValid) phase = std::fmod (c.ppq / beats, 1.0);
    }
    void freeRate (float hz, double sr) { inc = hz / sr; }
    float next() { const float p = (float) phase; phase += inc; if (phase >= 1.0) phase -= 1.0; return p; }
};

inline float lfoShape (float p, int shape)
{
    switch (shape)
    {
        case 0:  return 0.5f - 0.5f * std::cos (2.0f * kPi * p);         // Sinus 0..1
        case 1:  return p < 0.5f ? p * 2.0f : 2.0f - p * 2.0f;           // Dreieck
        case 2:  return 1.0f - p;                                        // Saw runter
        case 3:  return p < 0.5f ? 1.0f : 0.0f;                          // Rechteck
        default: return p;
    }
}

//==============================================================================
enum AlgoId
{
    A_NONE = -1,
    A_LPF, A_HPF, A_BPF, A_NOTCH, A_DJ, A_TALK, A_LFOF, A_DELAY, A_REVERB, A_FLANGE, A_CHORUS,
    A_PHASER, A_PITCH, A_REPEAT, A_REVERSE, A_TAPE, A_CRUSH, A_DIST, A_RING, A_SLICER, A_PAN,
    A_COMB, A_PUMP, A_ISO, A_FSHIFT, A_GLITCH, A_CLOUD, A_WAH, A_SHAPER, A_OCTAVE, A_WIDTH, A_NUM
};

inline const char* xLabel (int a)
{
    static const char* n[A_NUM] = { "CUTOFF", "CUTOFF", "CUTOFF", "CUTOFF", "LP <> HP", "VOWEL", "LFO RATE", "TIME",
                                    "SIZE", "SWEEP", "RATE", "SWEEP", "PITCH", "LENGTH", "LENGTH", "SPEED", "SAMPLE RATE",
                                    "DRIVE", "FREQ", "RATE", "RATE", "PITCH", "RATE", "BAND",
                                    "SHIFT", "STEP", "GRAIN SIZE", "CUTOFF", "DRIVE", "SUB <> UP", "WIDTH" };
    return a >= 0 && a < A_NUM ? n[a] : "-";
}
inline const char* yLabel (int a)
{
    static const char* n[A_NUM] = { "RESONANCE", "RESONANCE", "RESONANCE", "WIDTH", "RESONANCE", "RESONANCE", "DEPTH", "FEEDBACK",
                                    "AMOUNT", "FEEDBACK", "DEPTH", "FEEDBACK", "FEEDBACK", "SPEED", "FEEDBACK", "GLIDE",
                                    "BITS", "TONE", "MIX", "DEPTH", "DEPTH", "FEEDBACK", "DEPTH", "KILL",
                                    "FEEDBACK", "CHAOS", "SPREAD", "RESONANCE", "SHAPE", "AMOUNT", "HAAS" };
    return a >= 0 && a < A_NUM ? n[a] : "-";
}
// Effekte, die als Send laufen (Fahne läuft nach dem Loslassen weiter)
inline bool isSendAlgo (int a) { return a == A_DELAY || a == A_REVERB; }

struct Algo
{
    virtual ~Algo() = default;
    virtual void prepare (double sr) = 0;
    virtual void reset() = 0;
    virtual void control (float x, float y, const float* p, const Ctx& c) = 0; // alle 32 Samples
    virtual void tick (float& l, float& r) = 0;
};

//==============================================================================
// LPF / HPF / BPF / Notch / DJ-Filter
// 4-Pol-Ladder (24 dB, TPT) mit Sättigung in der Rückkopplung: kreischt wie ein Hardware-Filter
struct Ladder
{
    float s[4] {}, G = 0, g = 0, k = 0;
    void set (float fc, float res, double sr)
    {
        fc = juce::jlimit (15.0f, (float) sr * 0.45f, fc);
        g = std::tan (kPi * fc / (float) sr); G = g / (1.0f + g); k = res;
    }
    float tick (float x)
    {
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]) / (1.0f + g);
        float u = std::tanh ((x - k * S) / (1.0f + k * G4));
        for (auto& st : s) { const float v = (u - st) * G; const float y = v + st; st = y + v; u = y; }
        return u;
    }
    void reset() { for (auto& st : s) st = 0; }
};

// LPF / HPF / BPF / Notch / DJ-Filter
struct FilterAlgo : Algo
{
    int type; double sr = 44100;
    Ladder lad[2]; Svf f[2][2];
    float logFc = std::log (1000.0f), res = 0, q = 0.7f, drive = 1.0f;
    int djMode = 0;

    explicit FilterAlgo (int t) : type (t) {}
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { for (auto& l : lad) l.reset(); for (auto& a : f) for (auto& b : a) b.reset(); }

    void control (float x, float y, const float* p, const Ctx&) override
    {
        float fc;
        if (type == A_DJ)
        {
            // links Lowpass, Mitte offen, rechts Highpass
            if (x < 0.48f) { djMode = 0; fc = expMap (x / 0.48f, 60.0f, 20000.0f); }
            else if (x > 0.52f) { djMode = 2; fc = expMap ((x - 0.52f) / 0.48f, 20.0f, 9000.0f); }
            else { djMode = 1; fc = 1000.0f; }
        }
        else if (type == A_HPF) fc = expMap (x, 20.0f, 12000.0f);
        else fc = expMap (x, 50.0f, 16000.0f);

        logFc += (std::log (fc) - logFc) * 0.35f;
        const float fcS = std::exp (logFc);
        res = std::pow (clamp01 (y), 1.3f);                  // 0..1
        drive = 1.0f + p[0] * 6.0f;
        for (auto& l : lad) l.set (fcS, res * 3.95f, sr);    // ab ~3.8 pfeift es fast von selbst
        q = type == A_NOTCH ? expMap (1.0f - y, 0.3f, 6.0f) : 0.6f + res * 24.0f;
        for (int c = 0; c < 2; ++c)
        {
            f[c][0].set (fcS, 0.707f, sr);                   // erste Stufe neutral
            f[c][1].set (fcS, q, sr);                        // zweite Stufe mit Resonanz
        }
    }

    float proc (int ch, float v)
    {
        const float in = v;
        float lp, bp, hp, lp2, bp2, hp2;
        const bool lowpass = type == A_LPF || (type == A_DJ && djMode == 0);
        const bool highpass = type == A_HPF || (type == A_DJ && djMode == 2);
        if (type == A_DJ && djMode == 1) return in;
        if (lowpass)
        {
            // Ladder: Eingang leicht angezerrt, Bassverlust bei hoher Resonanz ausgleichen
            const float y = lad[ch].tick (v * drive * 0.8f) * (1.0f + res * 1.6f);
            return softClip (y * 0.95f / std::pow (drive, 0.45f)) * 1.15f;
        }
        v = softClip (v * drive * 0.8f) / std::pow (drive, 0.55f) * 1.25f;
        f[ch][0].tick (v, lp, bp, hp);
        if (highpass)
        {
            f[ch][1].tick (hp, lp2, bp2, hp2);
            return softClip (hp2 * 0.95f) * 1.1f;
        }
        if (type == A_BPF)
        {
            f[ch][1].tick (v, lp2, bp2, hp2);
            const float k = f[ch][1].k;                       // Spitze auf ~0 dB normieren, dann kräftig anheben
            return softClip (bp2 * k * (3.2f + res * 1.5f)) * 1.05f;
        }
        f[ch][1].tick (v, lp2, bp2, hp2);                    // Notch
        return lp2 + hp2;
    }
    void tick (float& l, float& r) override { l = proc (0, l); r = proc (1, r); }
};

//==============================================================================
// Talk-Filter: Vokal-Morph über drei Formanten
struct TalkAlgo : Algo
{
    double sr = 44100; Svf f[2][3]; float g[3] = { 1, 0.7f, 0.4f };
    float cur[3] = { 700, 1200, 2600 };
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { for (auto& a : f) for (auto& b : a) b.reset(); }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        static const float F[5][3] = { { 800, 1150, 2900 },  // a
                                       { 400, 1600, 2700 },  // e
                                       { 300, 2300, 3000 },  // i
                                       { 450, 800, 2830 },   // o
                                       { 325, 700, 2530 } }; // u
        const float pos = clamp01 (x) * 3.999f;
        const int i = (int) pos; const float t = pos - (float) i;
        const float shift = expMap (p[0], 0.6f, 1.6f);
        const float q = expMap (y, 3.0f, 22.0f);
        for (int k = 0; k < 3; ++k)
        {
            const float target = lerp (F[i][k], F[i + 1][k], t) * shift;
            cur[k] += (target - cur[k]) * 0.3f;
            for (int ch = 0; ch < 2; ++ch) f[ch][k].set (cur[k], q * (k == 0 ? 0.7f : 1.0f), sr);
        }
    }
    void tick (float& l, float& r) override
    {
        float* io[2] = { &l, &r };
        for (int ch = 0; ch < 2; ++ch)
        {
            const float in = softClip (*io[ch] * 1.5f);
            float sum = 0, lp, bp, hp;
            for (int k = 0; k < 3; ++k) { f[ch][k].tick (in, lp, bp, hp); sum += bp * g[k]; }
            *io[ch] = softClip (sum * 0.7f);
        }
    }
};

//==============================================================================
// LFO-Filter (Wobble)
struct LfoFilterAlgo : Algo
{
    double sr = 44100; Svf f[2]; Ladder lad[2]; SyncPhase ph; float base = 500, depthOct = 3, q = 4, res = 0.5f; int shape = 0, ftype = 0;
    float sh = 0.5f, lastP = 0, mod = 0; juce::Random rnd;
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { for (auto& x : f) x.reset(); for (auto& l : lad) l.reset(); ph.phase = 0; }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        ph.control (kDivBeats[divIndex (1.0f - x)], c);
        depthOct = y * 7.5f;
        base = expMap (p[0], 50.0f, 3000.0f);
        shape = juce::jlimit (0, 4, (int) std::round (p[1] * 4.0f));
        ftype = juce::jlimit (0, 2, (int) std::round (p[2] * 2.0f));
        res = 0.35f + p[3] * 0.6f;
        q = 1.5f + res * 18.0f;
    }
    int cnt = 0;
    void tick (float& l, float& r) override
    {
        const float p = ph.next();
        float v;
        if (shape == 4) { if (p < lastP) sh = rnd.nextFloat(); v = sh; }
        else v = lfoShape (p, shape);
        lastP = p;
        mod += (v - mod) * (shape == 3 || shape == 4 ? 0.01f : 0.05f);
        if ((cnt++ & 7) == 0)
        {
            const float fc = base * std::pow (2.0f, mod * depthOct);
            for (auto& x : f) x.set (fc, q, sr);
            for (auto& d : lad) d.set (fc, res * 3.9f, sr);
        }
        float* io[2] = { &l, &r };
        for (int ch = 0; ch < 2; ++ch)
        {
            if (ftype == 0) { *io[ch] = softClip (lad[ch].tick (*io[ch] * 1.2f) * (1.0f + res * 1.6f)) * 1.1f; continue; }
            float lp, bp, hp; f[ch].tick (softClip (*io[ch] * 1.3f), lp, bp, hp);
            const float o = ftype == 1 ? bp * f[ch].k * 3.6f : hp * 1.2f;
            *io[ch] = softClip (o) * 1.1f;
        }
    }
};

//==============================================================================
// Sync-Delay (Stereo, Ping-Pong, Tape)
struct DelayAlgo : Algo
{
    double sr = 44100; Ring buf; float dTarget = 1000, dSm = 1000, fb = 0.4f; bool pingpong = false;
    OnePole lp[2], hp[2]; float wobble = 0, wPh = 0; float tone = 0.7f, hpAmt = 0;
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 4.2)); reset(); }
    void reset() override { buf.clear(); for (auto& o : lp) o.reset(); for (auto& o : hp) o.reset(); }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        dTarget = (float) juce::jlimit (16.0, (double) buf.size - 8.0, beatsToSamples (kDivBeats[divIndex (x)], c));
        fb = std::min (0.85f, y);
        for (auto& o : lp) o.setLP (expMap (p[0], 700.0f, 18000.0f), sr);
        for (auto& o : hp) o.setLP (expMap (p[3], 20.0f, 1200.0f), sr);
        pingpong = p[1] > 0.5f;
        wobble = p[2];
    }
    void tick (float& l, float& r) override
    {
        dSm += (dTarget - dSm) * (float) (1.0 / (0.07 * sr)); // gleitet wie Band beim Drehen
        wPh += (float) (0.7 / sr); if (wPh > 1) wPh -= 1;
        const float mod = wobble * (float) sr * 0.0025f * std::sin (2.0f * kPi * wPh);
        const double d = dSm + mod;
        float oL = buf.readDelay (0, d), oR = buf.readDelay (1, d);
        float fL = hp[0].hp (lp[0].lp (oL)) * fb, fR = hp[1].hp (lp[1].lp (oR)) * fb;
        if (fb > 0.85f) { fL = softClip (fL); fR = softClip (fR); }
        if (pingpong) buf.push ((l + r) * 0.5f + fR, fL);
        else buf.push (l + fL, r + fR);
        l = oL; r = oR;
    }
};

//==============================================================================
struct ReverbAlgo : Algo
{
    juce::Reverb rv; float level = 0.5f; OnePole pre[2]; juce::Reverb::Parameters prm; double sr = 44100;
    bool freezeMode = false, frozen = false;
    void prepare (double s) override { sr = s; rv.setSampleRate (s); reset(); }
    void reset() override { rv.reset(); for (auto& o : pre) o.reset(); }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        juce::Reverb::Parameters n;
        n.roomSize = lerp (0.3f, 0.985f, x);
        n.damping = p[0];
        n.width = lerp (0.5f, 1.0f, p[1]);
        n.wetLevel = 1.0f; n.dryLevel = 0.0f;
        // Freeze: Fahne einfrieren, solange das Pad gehalten wird
        freezeMode = p[3] > 0.5f;
        const bool fr = freezeMode && c.touched;
        n.freezeMode = fr ? 1.0f : 0.0f;
        if (fr != frozen || std::abs (n.roomSize - prm.roomSize) + std::abs (n.damping - prm.damping) + std::abs (n.width - prm.width) > 1.0e-5f)
        { rv.setParameters (n); prm = n; frozen = fr; }
        level = y;
        for (auto& o : pre) o.setLP (expMap (p[2], 20.0f, 800.0f), sr);
    }
    void tick (float& l, float& r) override
    {
        float a = pre[0].hp (l), b = pre[1].hp (r);
        rv.processStereo (&a, &b, 1);
        l = a * level; r = b * level;
    }
};

//==============================================================================
// Flanger / Chorus (gleiche Basis)
struct ModDelayAlgo : Algo
{
    // Flanger: X fährt die Verzögerung von Hand (Jet-Sweep wie beim Kaoss Pad), Y = Feedback.
    // p[2] > 0.5: statt Hand-Sweep ein LFO im Songtempo. Chorus: immer LFO.
    bool chorus; double sr = 44100; Ring buf; SyncPhase ph; float fb = 0, depth = 1, baseMs = 1; bool neg = false, manual = true;
    float spread = 0.25f, manTarget = 2.0f, manSm = 2.0f, wob = 0.0f, wobPh = 0.0f;
    explicit ModDelayAlgo (bool c) : chorus (c) {}
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 0.1)); reset(); }
    void reset() override { buf.clear(); }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        if (chorus)
        {
            manual = false;
            ph.freeRate (expMap (x, 0.08f, 6.0f), sr);
            depth = lerp (1.0f, 14.0f, y); baseMs = 10.0f; fb = 0.0f; spread = lerp (0.25f, 0.5f, p[0]);
            return;
        }
        manual = p[2] < 0.5f;
        fb = std::min (0.97f, y); neg = p[1] > 0.5f; spread = 0.25f;
        if (manual) { manTarget = expMap (1.0f - x, 0.22f, 11.0f); wob = 0.04f + p[0] * 0.25f; }
        else { ph.control (kDivBeats[divIndex (1.0f - x)] * 4.0f, c); depth = lerp (2.0f, 9.0f, p[0]); baseMs = 0.25f; }
    }
    void tick (float& l, float& r) override
    {
        double dL, dR;
        if (manual)
        {
            manSm += (manTarget - manSm) * (float) (1.0 / (0.025 * sr));   // weich gleiten = Jet-Swoosh
            wobPh += (float) (0.23 / sr); if (wobPh > 1) wobPh -= 1;
            const float w = wob * std::sin (2.0f * kPi * wobPh);
            dL = (manSm + w) * 0.001 * sr; dR = (manSm * 1.06f - w) * 0.001 * sr;
        }
        else
        {
            const float p = ph.next();
            const float mL = 0.5f - 0.5f * std::cos (2.0f * kPi * p);
            float p2 = p + spread; if (p2 >= 1) p2 -= 1;
            const float mR = 0.5f - 0.5f * std::cos (2.0f * kPi * p2);
            dL = (baseMs + depth * mL) * 0.001 * sr; dR = (baseMs + depth * mR) * 0.001 * sr;
        }
        const float wL = buf.readDelay (0, std::max (2.0, dL)), wR = buf.readDelay (1, std::max (2.0, dR));
        const float sg = neg ? -1.0f : 1.0f;
        buf.push (l + softClip (wL * fb * sg * 1.05f), r + softClip (wR * fb * sg * 1.05f));
        if (chorus) { l = (l + wL) * 0.75f; r = (r + wR) * 0.75f; }
        else { l = softClip ((l + wL * sg) * 0.75f) * 1.1f; r = softClip ((r + wR * sg) * 0.75f) * 1.1f; }
    }
};

//==============================================================================
struct PhaserAlgo : Algo
{
    // X = Phaser-Frequenz von Hand (Manual Phaser), Y = Resonanz. p[2] > 0.5: LFO statt Hand.
    double sr = 44100; SyncPhase ph; int stages = 8; float fb = 0.5f, z[2][12] {}, last[2] {}, depth = 1;
    bool manual = true; float manT = 0.5f, manSm = 0.5f;
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { for (auto& a : z) for (auto& b : a) b = 0; last[0] = last[1] = 0; }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        manual = p[2] < 0.5f;
        if (manual) manT = x; else ph.freeRate (expMap (x, 0.04f, 7.0f), sr);
        fb = y * 0.95f;
        stages = p[0] < 0.33f ? 4 : (p[0] < 0.66f ? 8 : 12);
        depth = lerp (0.6f, 1.0f, p[1]);
    }
    void tick (float& l, float& r) override
    {
        float base;
        if (manual) { manSm += (manT - manSm) * (float) (1.0 / (0.02 * sr)); base = manSm; }
        else { const float p = ph.next(); base = 0.5f - 0.5f * std::cos (2.0f * kPi * p); }
        float* io[2] = { &l, &r };
        for (int ch = 0; ch < 2; ++ch)
        {
            const float m = manual ? clamp01 (base + (ch ? 0.03f : 0.0f)) : base;
            const float fc = expMap (m * depth, 80.0f, 9000.0f);
            const float t = std::tan (kPi * std::min (fc, (float) sr * 0.45f) / (float) sr);
            const float a = (t - 1.0f) / (t + 1.0f);
            float v = *io[ch] + softClip (last[ch] * fb * 1.1f);
            for (int s = 0; s < stages; ++s) { const float y = a * v + z[ch][s]; z[ch][s] = v - a * y; v = y; }
            last[ch] = v;
            *io[ch] = softClip ((*io[ch] + v) * 0.7f) * 1.1f;
        }
    }
};

//==============================================================================
// Granularer Pitch-Shifter (zwei Köpfe mit Überblendung)
struct PitchAlgo : Algo
{
    double sr = 44100; Ring buf; double phase = 0; float ratio = 1, win = 2000, fb = 0, fbL = 0, fbR = 0;
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 0.5)); reset(); }
    void reset() override { buf.clear(); phase = 0; fbL = fbR = 0; }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        float st = lerp (-12.0f, 12.0f, x);
        if (p[1] > 0.75f) st = 12.0f * std::round (st / 12.0f);
        else if (p[1] > 0.25f) st = std::round (st);
        ratio = std::pow (2.0f, st / 12.0f);
        fb = y * 0.9f;
        win = (float) (expMap (p[0], 20.0f, 120.0f) * 0.001 * sr);
    }
    void tick (float& l, float& r) override
    {
        buf.push (l + fbL * fb, r + fbR * fb);
        phase += (1.0 - ratio) / win;
        phase -= std::floor (phase);
        double p2 = phase + 0.5; if (p2 >= 1) p2 -= 1;
        const float g1 = std::sin (kPi * (float) phase), g2 = std::sin (kPi * (float) p2);
        const double d1 = 3.0 + phase * win, d2 = 3.0 + p2 * win;
        const float oL = buf.readDelay (0, d1) * g1 * g1 + buf.readDelay (0, d2) * g2 * g2;
        const float oR = buf.readDelay (1, d1) * g1 * g1 + buf.readDelay (1, d2) * g2 * g2;
        fbL = softClip (oL); fbR = softClip (oR);
        l = oL; r = oR;
    }
};

//==============================================================================
// Beat-Repeat / Looper / Grain-Shifter
struct RepeatAlgo : Algo
{
    // Looper (wie KP3+): nimmt ab dem Moment des Antippens auf und wiederholt dieses Stück.
    // Grain (p[0] > 0.5): friert sofort das letzte winzige Stück ein und wiederholt es.
    // Übergänge an der Schleife werden überblendet, damit es tight und ohne Klicks stottert.
    double sr = 44100; Ring buf; bool active = false; long long loopStart = 0; double ph = 0;
    double len = 4000, lenSm = 4000; float speed = 1, gate = 1, gateSm = 1; bool reverse = false, grain = false;
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 6.0)); reset(); }
    void reset() override { buf.clear(); active = false; ph = 0; }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        grain = p[0] > 0.5f;
        if (grain) len = expMap (1.0f - x, 0.010f, 0.35f) * sr;
        else len = beatsToSamples (kDivBeats[divIndex (1.0f - x)], c);
        len = juce::jlimit (64.0, (double) buf.size * 0.45, len);
        reverse = p[1] > 0.5f;
        if (p[3] > 0.5f) { speed = 1.0f; gate = lerp (1.0f, 0.12f, y); }
        else
        {
            speed = std::pow (2.0f, lerp (-1.0f, 1.0f, y));
            if (std::abs (speed - 1.0f) < 0.03f) speed = 1.0f;
            gate = lerp (1.0f, 0.25f, p[2]);
        }
        if (c.onset || (c.touched && ! active))
        {
            active = true; ph = 0; lenSm = len;
            // Grain: Stück endet jetzt. Looper: Stück beginnt jetzt (wird während des ersten Durchlaufs aufgenommen)
            loopStart = grain ? buf.w - (long long) len : buf.w;
        }
        if (! c.touched) active = false;
    }
    void tick (float& l, float& r) override
    {
        buf.push (l, r);   // immer weiter aufnehmen (Puffer 6 s, Schleife max. 2,7 s)
        if (! active) return;
        lenSm += (len - lenSm) * 0.003;
        const double L = std::max (64.0, lenSm);
        if (ph >= L) ph = std::fmod (ph, L);
        const double xf = std::min (0.004 * sr, L * 0.2);         // Überblendlänge
        auto readAt = [&] (double offs, float& a, float& b)
        {
            const double pos = reverse ? (double) loopStart + L - 1.0 - offs : (double) loopStart + offs;
            a = buf.readAbs (0, pos); b = buf.readAbs (1, pos);
        };
        float a, b;
        readAt (ph, a, b);
        // erster Durchlauf beim Looper = live, danach am Schleifenende in den Anfang überblenden
        if (ph > L - xf)
        {
            float a2, b2; readAt (ph - L, a2, b2);
            const float t = (float) ((ph - (L - xf)) / xf);
            a = a * (1.0f - t) + a2 * t; b = b * (1.0f - t) + b2 * t;
        }
        // Gate: Stück verkürzen (Stutter), weich ausblenden
        gateSm += (gate - gateSm) * 0.002f;
        const double gEnd = L * gateSm;
        const float env = (float) juce::jlimit (0.0, 1.0, (gEnd - ph) / std::max (1.0, xf));
        l = a * (gateSm > 0.98f ? 1.0f : env); r = b * (gateSm > 0.98f ? 1.0f : env);
        ph += speed;
    }
};

//==============================================================================
// Reverse-Stücke (zwei überlappende Köpfe)
struct ReverseAlgo : Algo
{
    double sr = 44100; Ring buf; double L = 10000, c1 = 0, c2 = 0; float fb = 0, fbL = 0, fbR = 0; double Lnext = 10000;
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 4.5)); reset(); }
    void reset() override { buf.clear(); c1 = 0; c2 = L * 0.5; fbL = fbR = 0; }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        juce::ignoreUnused (p);
        Lnext = juce::jlimit (256.0, buf.size * 0.45, beatsToSamples (kDivBeats[juce::jlimit (2, kNumDivs - 1, divIndex (x))], c));
        fb = y * 0.75f;
    }
    void tick (float& l, float& r) override
    {
        buf.push (l + fbL * fb, r + fbR * fb);
        auto head = [&] (double& cc, float& oL, float& oR)
        {
            if (cc >= L) cc -= L;
            const float w = std::sin (kPi * (float) (cc / L));
            const double d = 2.0 * cc + 2.0;
            oL += buf.readDelay (0, d) * w * w; oR += buf.readDelay (1, d) * w * w;
            cc += 1.0;
        };
        float oL = 0, oR = 0;
        if (c1 >= L) { c1 = 0; L = Lnext; } // neue Länge nur am Stückwechsel
        head (c1, oL, oR);
        c2 = c1 + L * 0.5; if (c2 >= L) c2 -= L;
        double tmp = c2; head (tmp, oL, oR);
        fbL = softClip (oL); fbR = softClip (oR);
        l = oL; r = oR;
    }
};

//==============================================================================
// Tape-Stop / Vinyl-Brake
struct TapeAlgo : Algo
{
    double sr = 44100; Ring buf; double readPos = 0; float speed = 1, target = 1, glideK = 0.001f; bool active = false;
    OnePole lp[2]; float dark = 0;
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 4.5)); reset(); }
    void reset() override { buf.clear(); active = false; speed = 1; for (auto& o : lp) o.reset(); }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        target = x * x;
        glideK = (float) (1.0 / (expMap (y, 0.03f, 1.5f) * sr));
        dark = p[0];
        if (c.onset || (c.touched && !active)) { active = true; readPos = (double) buf.w - 2.0; speed = 1.0f; }
        if (!c.touched) active = false;
        const float fc = expMap (lerp (1.0f, speed, dark), 300.0f, 20000.0f);
        for (auto& o : lp) o.setLP (fc, sr);
    }
    void tick (float& l, float& r) override
    {
        buf.push (l, r);
        if (!active) return;
        speed += (target - speed) * glideK * 4.0f;
        readPos += speed;
        const double maxBack = buf.size - 8.0;
        if (readPos > (double) buf.w - 2.0) readPos = (double) buf.w - 2.0;
        if ((double) buf.w - readPos > maxBack) readPos = (double) buf.w - maxBack;
        const float g = std::min (1.0f, speed * 8.0f); // bei Stillstand leise
        l = lp[0].lp (buf.readAbs (0, readPos)) * g; r = lp[1].lp (buf.readAbs (1, readPos)) * g;
    }
};

//==============================================================================
struct CrushAlgo : Algo
{
    double sr = 44100; float step = 1, cnt = 0, holdL = 0, holdR = 0, levels = 32768, drive = 1;
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { cnt = 0; holdL = holdR = 0; }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        step = (float) sr / expMap (1.0f - x, 400.0f, (float) sr);
        levels = std::pow (2.0f, lerp (16.0f, 2.0f, y)) * 0.5f;
        drive = 1.0f + p[0] * 4.0f;
    }
    void tick (float& l, float& r) override
    {
        cnt += 1.0f;
        if (cnt >= step) { cnt -= step; holdL = l; holdR = r; }
        auto q = [this] (float v) { if (drive > 1.01f) v = softClip (v * drive) / std::pow (drive, 0.8f); return std::round (v * levels) / levels; };
        l = q (holdL); r = q (holdR);
    }
};

//==============================================================================
struct DistAlgo : Algo
{
    double sr = 44100; float drive = 1, comp = 1; int type = 0; OnePole tone[2], dc[2];
    void prepare (double s) override { sr = s; reset(); for (auto& o : dc) o.setLP (12.0f, sr); }
    void reset() override { for (auto& o : tone) o.reset(); for (auto& o : dc) o.reset(); }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        drive = expMap (x, 1.0f, 60.0f);
        comp = 1.0f / (1.0f + std::log (drive) * 0.8f);
        type = juce::jlimit (0, 3, (int) std::round (p[0] * 3.0f));
        for (auto& o : tone) o.setLP (expMap (y, 500.0f, 18000.0f), sr);
    }
    float shape (float v) const
    {
        v *= drive;
        switch (type)
        {
            case 0:  return std::tanh (v);
            case 1:  return std::sin (v * 0.5f);                                  // Wavefolder
            case 2:  return std::tanh (std::abs (v) * 1.2f + v * 0.4f) * 0.9f;   // Fuzz (gleichgerichtet)
            default: return juce::jlimit (-0.8f, 0.8f, v);                       // Hard Clip
        }
    }
    void tick (float& l, float& r) override
    {
        l = dc[0].hp (tone[0].lp (shape (l))) * comp * 1.3f;
        r = dc[1].hp (tone[1].lp (shape (r))) * comp * 1.3f;
    }
};

//==============================================================================
struct RingAlgo : Algo
{
    double sr = 44100; double ph = 0; float fTarget = 200, f = 200, mix = 1; bool square = false;
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { ph = 0; }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        float hz = expMap (x, 25.0f, 6000.0f);
        if (p[1] > 0.5f) { const float n = std::round (12.0f * std::log2 (hz / 440.0f)); hz = 440.0f * std::pow (2.0f, n / 12.0f); }
        fTarget = hz; mix = y; square = p[2] > 0.5f;
    }
    void tick (float& l, float& r) override
    {
        f += (fTarget - f) * 0.002f;
        ph += f / sr; if (ph >= 1) ph -= 1;
        float m = std::sin (2.0f * kPi * (float) ph);
        if (square) m = std::tanh (m * 6.0f);
        l = softClip (lerp (l, l * m * 1.5f, mix) * 1.2f); r = softClip (lerp (r, r * m * 1.5f, mix) * 1.2f);
    }
};

//==============================================================================
// Slicer / Trance-Gate
struct SlicerAlgo : Algo
{
    double sr = 44100; SyncPhase ph; float depth = 1, duty = 0.5f, g = 1; int pattern = 0; double stepPh = 0;
    int step = 0; float lastP = 0; juce::Random rnd; bool rndOn = true;
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { g = 1; step = 0; }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        const float beats = kDivBeats[divIndex (1.0f - x)];
        ph.control (beats, c);
        if (c.ppqValid) step = (int) std::floor (c.ppq / beats) & 15;
        depth = y;
        pattern = juce::jlimit (0, 3, (int) std::round (p[0] * 3.0f));
        duty = lerp (0.2f, 0.85f, p[1]);
    }
    bool stepOn (int s)
    {
        static const unsigned short pa[3] = { 0xFFFF, 0xB6DB /* Trance A */, 0xEB5A /* Trance B */ };
        if (pattern == 3) return rndOn;
        return (pa[pattern] >> (15 - s)) & 1;
    }
    void tick (float& l, float& r) override
    {
        const float p = ph.next();
        if (p < lastP) { step = (step + 1) & 15; rndOn = rnd.nextFloat() > 0.4f; }
        lastP = p;
        const float target = (stepOn (step) && p < duty) ? 1.0f : 1.0f - depth;
        g += (target - g) * (float) (1.0 / (0.0015 * sr));
        l *= g; r *= g;
    }
};

//==============================================================================
struct PanAlgo : Algo
{
    double sr = 44100; SyncPhase ph; float depth = 1; bool square = false; float sm = 0.5f;
    void prepare (double s) override { sr = s; }
    void reset() override {}
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        ph.control (kDivBeats[divIndex (1.0f - x)] * 2.0f, c);
        depth = y; square = p[0] > 0.5f;
    }
    void tick (float& l, float& r) override
    {
        float m = lfoShape (ph.next(), square ? 3 : 0);
        sm += (m - sm) * (float) (1.0 / (0.002 * sr));
        const float pan = lerp (0.5f, sm, depth);
        const float gl = std::cos (pan * kPi * 0.5f) * 1.414f, gr = std::sin (pan * kPi * 0.5f) * 1.414f;
        const float mono = (l + r) * 0.5f, side = (l - r) * 0.5f;
        l = (mono + side * (1 - depth)) * gl; r = (mono - side * (1 - depth)) * gr;
    }
};

//==============================================================================
// Comb-Resonator
struct CombAlgo : Algo
{
    // zwei gestimmte Kammfilter (Grundton + Oktave) für einen kräftigen, metallischen Klang
    double sr = 44100; Ring buf, buf2; float dT = 100, dS = 100, fb = 0.8f, detune = 0; OnePole damp[2], damp2[2], dc[2];
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 0.06)); buf2.setSize ((int) (s * 0.06)); reset(); for (auto& o : dc) o.setLP (25.0f, sr); }
    void reset() override { buf.clear(); buf2.clear(); for (auto& o : damp) o.reset(); for (auto& o : damp2) o.reset(); for (auto& o : dc) o.reset(); }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        float note = lerp (24.0f, 96.0f, x);
        if (p[1] > 0.5f) note = std::round (note);
        const float hz = 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f);
        dT = (float) (sr / hz);
        fb = std::min (0.995f, 0.6f + y * 0.4f);
        for (auto& o : damp) o.setLP (expMap (1.0f - p[0], 1800.0f, 19000.0f), sr);
        for (auto& o : damp2) o.setLP (expMap (1.0f - p[0], 2500.0f, 19000.0f), sr);
        detune = p[2] * 0.012f;
    }
    void tick (float& l, float& r) override
    {
        dS += (dT - dS) * 0.003f;
        const float inL = softClip (l * 1.4f), inR = softClip (r * 1.4f);
        const float oL = buf.readDelay (0, dS), oR = buf.readDelay (1, dS * (1.0f + detune));
        const float o2L = buf2.readDelay (0, dS * 0.5f), o2R = buf2.readDelay (1, dS * 0.5f * (1.0f + detune));
        buf.push (inL * 0.6f + softClip (damp[0].lp (oL) * fb), inR * 0.6f + softClip (damp[1].lp (oR) * fb));
        buf2.push (inL * 0.4f + softClip (damp2[0].lp (o2L) * fb * 0.97f), inR * 0.4f + softClip (damp2[1].lp (o2R) * fb * 0.97f));
        l = softClip (dc[0].hp (oL + o2L * 0.6f) * 1.4f); r = softClip (dc[1].hp (oR + o2R * 0.6f) * 1.4f);
    }
};

//==============================================================================
// Sidechain-Pump
struct PumpAlgo : Algo
{
    double sr = 44100; SyncPhase ph; float depth = 1, curve = 4;
    void prepare (double s) override { sr = s; }
    void reset() override {}
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        ph.control (kDivBeats[divIndex (1.0f - x)], c);
        depth = y; curve = lerp (2.0f, 9.0f, p[0]);
    }
    void tick (float& l, float& r) override
    {
        const float p = ph.next();
        const float att = std::min (1.0f, p * 120.0f); // kurzer Abfall statt Klick
        const float duck = std::exp (-p * curve) * att + (1.0f - att);
        const float g = 1.0f - depth * duck;
        l *= g; r *= g;
    }
};

//==============================================================================
// Isolator: 3 Bänder, X wählt das Band, Y wie stark es weggedreht wird
struct IsoAlgo : Algo
{
    double sr = 44100; Svf lo[2], hi[2]; float gL = 1, gM = 1, gH = 1, tL = 1, tM = 1, tH = 1;
    void prepare (double s) override { sr = s; for (int c = 0; c < 2; ++c) { lo[c].set (260.0f, 0.707f, sr); hi[c].set (3200.0f, 0.707f, sr); } reset(); }
    void reset() override { for (auto& f : lo) f.reset(); for (auto& f : hi) f.reset(); }
    static float bump (float x, float c) { return std::max (0.0f, 1.0f - std::abs (x - c) * 2.2f); }
    void control (float x, float y, const float*, const Ctx&) override
    {
        tL = 1.0f - y * bump (x, 0.0f); tM = 1.0f - y * bump (x, 0.5f); tH = 1.0f - y * bump (x, 1.0f);
    }
    void tick (float& l, float& r) override
    {
        const float k = 0.002f;
        gL += (tL - gL) * k; gM += (tM - gM) * k; gH += (tH - gH) * k;
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            float lp, bp, hp, lp2, bp2, hp2;
            lo[c].tick (*io[c], lp, bp, hp);
            hi[c].tick (*io[c], lp2, bp2, hp2);
            const float mid = *io[c] - lp - hp2;
            *io[c] = lp * gL + mid * gM + hp2 * gH;
        }
    }
};

//==============================================================================
// Frequency Shifter (Hilbert-Allpass-Paar), mit Rückkopplung = Barberpole-Spirale
struct FShiftAlgo : Algo
{
    struct AP { float a = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float tick (float x) { const float y = a * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
    struct Hilbert
    {
        AP p1[4], p2[4]; float d = 0;
        Hilbert()
        {
            const float c1[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            const float c2[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            for (int i = 0; i < 4; ++i) { p1[i].a = c1[i] * c1[i]; p2[i].a = c2[i] * c2[i]; }
        }
        void tick (float x, float& re, float& im)
        {
            float a = x, b = x;
            for (auto& p : p1) a = p.tick (a);
            for (auto& p : p2) b = p.tick (b);
            re = a; im = d; d = b;
        }
        void reset() { for (auto& p : p1) p = AP { p.a }; for (auto& p : p2) p = AP { p.a }; d = 0; }
    };
    double sr = 44100; Hilbert h[2]; double ph = 0; float hzT = 0, hz = 0, fb = 0, fbL = 0, fbR = 0; Ring buf; float fbDelay = 1;
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 0.6)); reset(); }
    void reset() override { h[0].reset(); h[1].reset(); buf.clear(); fbL = fbR = 0; }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        const float s = (x - 0.5f) * 2.0f;
        hzT = std::abs (s) < 0.02f ? 0.0f : (s < 0 ? -1.0f : 1.0f) * expMap (std::abs (s), 0.5f, 2500.0f);
        fb = y * 0.92f;
        fbDelay = (float) std::max (1.0, p[0] > 0.01f ? beatsToSamples (kDivBeats[divIndex (p[0])], c) : 1.0);
    }
    void tick (float& l, float& r) override
    {
        hz += (hzT - hz) * 0.0015f;
        ph += hz / sr; ph -= std::floor (ph);
        const float cs = std::cos (2.0f * kPi * (float) ph), sn = std::sin (2.0f * kPi * (float) ph);
        float re, im;
        const float inL = l + fbL * fb, inR = r + fbR * fb;
        h[0].tick (inL, re, im); const float oL = re * cs - im * sn;
        h[1].tick (inR, re, im); const float oR = re * cs + im * sn;   // rechts gegenläufig = breiter
        buf.push (softClip (oL), softClip (oR));
        fbL = buf.readDelay (0, fbDelay); fbR = buf.readDelay (1, fbDelay);
        l = softClip (oL * 1.1f); r = softClip (oR * 1.1f);
    }
};

//==============================================================================
// Glitch-Engine: würfelt pro Notenwert Repeat, Reverse, Pitch hoch/runter, Tape-Stop, Stutter, Mute
struct GlitchAlgo : Algo
{
    enum Act { PASS, REP2, REP4, REP8, REV, UP, DOWN, STOP, MUTE, NUMACT };
    double sr = 44100; Ring buf; juce::Random rnd;
    double stepLen = 10000, stepPos = 0; long long stepStart = 0; int act = PASS; float chaos = 0.5f; long long lastIdx = -1;
    float weights[NUMACT] { 0, 3, 3, 2, 3, 2, 2, 2, 1 };
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 5.0)); reset(); }
    void reset() override { buf.clear(); stepPos = 0; act = PASS; lastIdx = -1; }
    void control (float x, float y, const float* p, const Ctx& c) override
    {
        const float beats = kDivBeats[juce::jlimit (0, 10, divIndex (1.0f - x))];
        stepLen = juce::jlimit (256.0, buf.size * 0.4, beatsToSamples (beats, c));
        chaos = y;
        // p[0]: Charakter (0 = rhythmisch, 1 = pitch/tape-lastig), p[1]: Mute erlaubt
        const float pc = p[0];
        weights[REP2] = 3 - pc * 2; weights[REP4] = 3 - pc; weights[REP8] = 2;
        weights[REV] = 3; weights[UP] = 1 + pc * 3; weights[DOWN] = 1 + pc * 3; weights[STOP] = 1 + pc * 3;
        weights[MUTE] = p[1] > 0.5f ? 2.0f : 0.0f;
        synced = c.ppqValid;
        if (c.ppqValid)
        {
            const long long idx = (long long) std::floor (c.ppq / beats);
            if (idx != lastIdx) { lastIdx = idx; newStep(); stepPos = (c.ppq / beats - (double) idx) * stepLen; }
        }
    }
    void newStep()
    {
        stepStart = buf.w; stepPos = 0;
        if (rnd.nextFloat() > chaos) { act = PASS; return; }
        float tot = 0; for (int i = 1; i < NUMACT; ++i) tot += weights[i];
        float r = rnd.nextFloat() * tot; act = REP2;
        for (int i = 1; i < NUMACT; ++i) { r -= weights[i]; if (r <= 0) { act = i; break; } }
    }
    bool synced = false;
    void tick (float& l, float& r) override
    {
        buf.push (l, r);
        if (stepPos >= stepLen && ! synced) newStep();
        const double L = stepLen, t = stepPos;
        double pos = -1; float g = 1.0f;
        switch (act)
        {
            case REP2: case REP4: case REP8: { const double ch = L / (act == REP2 ? 2 : (act == REP4 ? 4 : 8)); pos = (double) stepStart - ch + std::fmod (t, ch); break; }
            case REV:  pos = (double) stepStart - 1.0 - t; break;
            case UP:   pos = (double) stepStart - L + std::fmod (t * 2.0, L); break;
            case DOWN: pos = (double) stepStart - L + t * 0.5; break;
            case STOP: pos = (double) stepStart - L + (t - t * t / (2.0 * L)); g = (float) (1.0 - t / L * 0.6); break;
            case MUTE: g = 0.0f; break;
            default: break;
        }
        const double F = std::min (0.003 * sr, L * 0.1);
        const float w = (float) juce::jlimit (0.0, 1.0, std::min (t / F, (L - t) / F));
        if (act != PASS)
        {
            float a = 0, b = 0;
            if (pos >= 0 || act != MUTE) { if (act != MUTE) { a = buf.readAbs (0, pos) * g; b = buf.readAbs (1, pos) * g; } }
            l = l * (1.0f - w) + a * w; r = r * (1.0f - w) + b * w;
        }
        stepPos += 1.0;
    }
};

//==============================================================================
// Granular-Cloud: viele kleine Körner aus der jüngsten Vergangenheit, verstimmt und im Stereo verteilt
struct CloudAlgo : Algo
{
    struct Grain { bool on = false; double pos = 0, rate = 1; int len = 0, t = 0; float pl = 1, pr = 1; };
    double sr = 44100; Ring buf; Grain gr[12]; juce::Random rnd; float size = 4000, spread = 0, density = 4; double nextIn = 0; bool octaves = false; float reach = 1;
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 2.5)); reset(); }
    void reset() override { buf.clear(); for (auto& g : gr) g.on = false; nextIn = 0; }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        size = (float) (expMap (x, 0.02f, 0.35f) * sr);
        spread = y * 12.0f; octaves = p[1] > 0.5f;
        density = 2.0f + p[0] * 6.0f;
        reach = 0.1f + p[2] * 1.8f;
    }
    void spawn()
    {
        for (auto& g : gr)
            if (! g.on)
            {
                g.on = true; g.t = 0; g.len = (int) (size * (0.7f + rnd.nextFloat() * 0.6f));
                float st = (rnd.nextFloat() * 2.0f - 1.0f) * spread;
                if (octaves) { const float o[5] = { -12, 0, 7, 12, 19 }; st = spread < 0.5f ? 0.0f : o[rnd.nextInt (5)]; }
                g.rate = std::pow (2.0, st / 12.0);
                const double back = (double) g.len * std::max (1.0, g.rate) + 64.0 + rnd.nextFloat() * reach * sr * 0.5;
                g.pos = (double) buf.w - std::min (back, buf.size * 0.9);
                const float pan = rnd.nextFloat();
                g.pl = std::cos (pan * kPi * 0.5f) * 1.4f; g.pr = std::sin (pan * kPi * 0.5f) * 1.4f;
                return;
            }
    }
    void tick (float& l, float& r) override
    {
        buf.push (l, r);
        nextIn -= 1.0;
        if (nextIn <= 0) { spawn(); nextIn = size / density * (0.6 + rnd.nextFloat() * 0.8); }
        float oL = 0, oR = 0;
        for (auto& g : gr)
        {
            if (! g.on) continue;
            const float w = std::sin (kPi * (float) g.t / (float) g.len);
            const float m = 0.5f * (buf.readAbs (0, g.pos) + buf.readAbs (1, g.pos)) * w * w;
            oL += m * g.pl; oR += m * g.pr;
            g.pos += g.rate;
            if (++g.t >= g.len) g.on = false;
        }
        const float norm = 1.0f / std::sqrt (std::max (1.0f, density * 0.5f));
        l = softClip (oL * norm); r = softClip (oR * norm);
    }
};

//==============================================================================
// Auto-Wah / Envelope-Filter: Lautstärke des Signals öffnet den Filter
struct WahAlgo : Algo
{
    double sr = 44100; Ladder lad[2]; Svf f[2]; float env = 0, att = 0, rel = 0, base = 200, sens = 1, res = 0.5f; int mode = 0, cnt = 0;
    void prepare (double s) override { sr = s; att = 1.0f - std::exp (-1.0f / (0.004f * (float) s)); rel = 1.0f - std::exp (-1.0f / (0.14f * (float) s)); reset(); }
    void reset() override { for (auto& d : lad) d.reset(); for (auto& x : f) x.reset(); env = 0; }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        base = expMap (x, 60.0f, 3000.0f); res = y; sens = 2.0f + p[0] * 8.0f; mode = p[1] > 0.5f ? 1 : 0;
    }
    void tick (float& l, float& r) override
    {
        const float a = std::abs (l + r) * 0.5f;
        env += (a - env) * (a > env ? att : rel);
        if ((cnt++ & 7) == 0)
        {
            const float fc = std::min (base * std::pow (2.0f, std::min (1.0f, env * 3.0f) * sens), 18000.0f);
            for (auto& d : lad) d.set (fc, res * 3.9f, sr);
            for (auto& x : f) x.set (fc, 1.0f + res * 20.0f, sr);
        }
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            if (mode == 0) *io[c] = softClip (lad[c].tick (*io[c] * 1.3f) * (1.0f + res * 1.6f)) * 1.1f;
            else { float lp, bp, hp; f[c].tick (*io[c], lp, bp, hp); *io[c] = softClip (bp * f[c].k * 5.0f); }
        }
    }
};

//==============================================================================
// Morph-Shaper: X = Drive, Y morpht stufenlos Tube -> Fold -> Fuzz -> Hard
struct ShaperAlgo : Algo
{
    double sr = 44100; float drive = 1, morph = 0; OnePole tone[2], dc[2], pre[2]; float comp = 1;
    void prepare (double s) override { sr = s; for (auto& o : dc) o.setLP (15.0f, sr); reset(); }
    void reset() override { for (auto& o : tone) o.reset(); for (auto& o : dc) o.reset(); for (auto& o : pre) o.reset(); }
    void control (float x, float y, const float* p, const Ctx&) override
    {
        drive = expMap (x, 1.0f, 50.0f); morph = y * 3.0f;
        comp = 1.0f / (1.0f + std::log (drive) * 0.55f);
        for (auto& o : tone) o.setLP (expMap (1.0f - p[0], 1500.0f, 19000.0f), sr);
        for (auto& o : pre) o.setLP (expMap (p[1], 40.0f, 900.0f), sr);
    }
    static float shape (float v, int k)
    {
        switch (k)
        {
            case 0:  return std::tanh (v + 0.25f) - std::tanh (0.25f);               // Tube (asymmetrisch)
            case 1:  return std::sin (v * 0.9f);                                     // Fold
            case 2:  return std::tanh (std::abs (v) * 1.5f) * (v > 0 ? 1.0f : 0.6f); // Fuzz
            default: return juce::jlimit (-0.7f, 0.7f, v) * 1.3f;                   // Hard
        }
    }
    void tick (float& l, float& r) override
    {
        const int k = juce::jlimit (0, 2, (int) morph); const float t = morph - (float) k;
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            const float v = pre[c].hp (*io[c]) * drive;                  // Bass vorher raus = sauberer Zerr
            const float o = shape (v, k) * (1.0f - t) + shape (v, k + 1) * t;
            *io[c] = dc[c].hp (tone[c].lp (o)) * comp * 1.4f;
        }
    }
};

//==============================================================================
// Octaver: Sub-Oktave (Flip-Flop) und Oktave hoch (Gleichrichter)
struct OctaveAlgo : Algo
{
    double sr = 44100; OnePole lpIn[2], lpSub[2], hpUp[2], env[2]; float last[2] {}; float flip[2] { 1, 1 }; float blend = 0.5f, amt = 1;
    void prepare (double s) override
    {
        sr = s;
        for (auto& o : lpIn) o.setLP (900.0f, sr);
        for (auto& o : lpSub) o.setLP (400.0f, sr);
        for (auto& o : hpUp) o.setLP (120.0f, sr);
        for (auto& o : env) o.setLP (30.0f, sr);
        reset();
    }
    void reset() override { for (auto* a : { lpIn, lpSub, hpUp, env }) for (int i = 0; i < 2; ++i) a[i].reset(); last[0] = last[1] = 0; }
    void control (float x, float y, const float*, const Ctx&) override { blend = x; amt = y; }
    void tick (float& l, float& r) override
    {
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            const float in = *io[c];
            const float low = lpIn[c].lp (in);
            if ((low > 0) != (last[c] > 0) && low > 0) flip[c] = -flip[c];  // bei jeder 2. Nulldurchgang umschalten
            last[c] = low;
            const float e = env[c].lp (std::abs (low)) * 2.2f;
            const float sub = lpSub[c].lp (flip[c] * e);
            const float up = hpUp[c].hp (std::abs (in)) * 1.6f;
            const float oct = sub * (1.0f - blend) * 2.0f + up * blend;
            *io[c] = softClip (in * (1.0f - amt * 0.6f) + oct * amt * 1.3f);
        }
    }
};

//==============================================================================
// Stereo-Width + Haas
struct WidthAlgo : Algo
{
    double sr = 44100; Ring buf; float width = 1, haas = 0, hs = 0;
    void prepare (double s) override { sr = s; buf.setSize ((int) (s * 0.05)); reset(); }
    void reset() override { buf.clear(); }
    void control (float x, float y, const float*, const Ctx&) override { width = x * 2.5f; haas = (float) (y * 0.025 * sr); }
    void tick (float& l, float& r) override
    {
        hs += (haas - hs) * 0.002f;
        buf.push (l, r);
        const float rr = hs > 1.0f ? buf.readDelay (1, hs) : r;
        const float m = (l + rr) * 0.5f, s = (l - rr) * 0.5f * width;
        l = m + s; r = m - s;
    }
};

//==============================================================================
inline std::unique_ptr<Algo> makeAlgo (int id)
{
    switch (id)
    {
        case A_LPF: case A_HPF: case A_BPF: case A_NOTCH: case A_DJ: return std::make_unique<FilterAlgo> (id);
        case A_TALK:    return std::make_unique<TalkAlgo>();
        case A_LFOF:    return std::make_unique<LfoFilterAlgo>();
        case A_DELAY:   return std::make_unique<DelayAlgo>();
        case A_REVERB:  return std::make_unique<ReverbAlgo>();
        case A_FLANGE:  return std::make_unique<ModDelayAlgo> (false);
        case A_CHORUS:  return std::make_unique<ModDelayAlgo> (true);
        case A_PHASER:  return std::make_unique<PhaserAlgo>();
        case A_PITCH:   return std::make_unique<PitchAlgo>();
        case A_REPEAT:  return std::make_unique<RepeatAlgo>();
        case A_REVERSE: return std::make_unique<ReverseAlgo>();
        case A_TAPE:    return std::make_unique<TapeAlgo>();
        case A_CRUSH:   return std::make_unique<CrushAlgo>();
        case A_DIST:    return std::make_unique<DistAlgo>();
        case A_RING:    return std::make_unique<RingAlgo>();
        case A_SLICER:  return std::make_unique<SlicerAlgo>();
        case A_PAN:     return std::make_unique<PanAlgo>();
        case A_COMB:    return std::make_unique<CombAlgo>();
        case A_PUMP:    return std::make_unique<PumpAlgo>();
        case A_ISO:     return std::make_unique<IsoAlgo>();
        case A_FSHIFT:  return std::make_unique<FShiftAlgo>();
        case A_GLITCH:  return std::make_unique<GlitchAlgo>();
        case A_CLOUD:   return std::make_unique<CloudAlgo>();
        case A_WAH:     return std::make_unique<WahAlgo>();
        case A_SHAPER:  return std::make_unique<ShaperAlgo>();
        case A_OCTAVE:  return std::make_unique<OctaveAlgo>();
        case A_WIDTH:   return std::make_unique<WidthAlgo>();
        default:        return nullptr;
    }
}

// Alle Algorithmen einmal vorhalten, damit beim Presetwechsel nichts im Audio-Thread allokiert wird
struct AlgoBank
{
    std::array<std::unique_ptr<Algo>, A_NUM> algos;
    AlgoBank() { for (int i = 0; i < A_NUM; ++i) algos[(size_t) i] = makeAlgo (i); }
    void prepare (double sr) { for (auto& a : algos) a->prepare (sr); }
    Algo* get (int id) { return id >= 0 && id < A_NUM ? algos[(size_t) id].get() : nullptr; }
};

//==============================================================================
// Anzeige-Text für die X/Y-LCDs (gleiche Formeln wie in der DSP)
inline juce::String hzText (float hz)
{
    if (hz >= 1000.0f) return juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (juce::roundToInt (hz)) + " Hz";
}
inline juce::String pctText (float v) { return juce::String (juce::roundToInt (clamp01 (v) * 100.0f)) + " %"; }
inline juce::String noteText (float note)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int k = juce::roundToInt (note);
    return juce::String (n[((k % 12) + 12) % 12]) + juce::String (k / 12 - 1);
}

inline juce::String axisValueText (int a, bool isX, float v, const float* p)
{
    v = clamp01 (v);
    if (isX)
    {
        switch (a)
        {
            case A_LPF: case A_BPF: case A_NOTCH: return hzText (expMap (v, 50.0f, 16000.0f));
            case A_HPF:    return hzText (expMap (v, 20.0f, 12000.0f));
            case A_DJ:     return v < 0.48f ? "LP " + hzText (expMap (v / 0.48f, 60.0f, 20000.0f))
                                 : (v > 0.52f ? "HP " + hzText (expMap ((v - 0.52f) / 0.48f, 20.0f, 9000.0f)) : juce::String ("OPEN"));
            case A_TALK:   { static const char* vw[5] = { "A", "E", "I", "O", "U" }; return juce::String ("VOWEL ") + vw[juce::jlimit (0, 4, juce::roundToInt (v * 4.0f))]; }
            case A_LFOF: case A_SLICER: case A_PUMP: return divName (divIndex (1.0f - v));
            case A_PAN:    return divName (divIndex (1.0f - v));
            case A_DELAY:  return divName (divIndex (v));
            case A_REVERSE:return divName (juce::jlimit (2, kNumDivs - 1, divIndex (v)));
            case A_REVERB: return pctText (v);
            case A_FLANGE: return p[2] > 0.5f ? juce::String (divName (divIndex (1.0f - v))) + " x4" : juce::String (expMap (1.0f - v, 0.22f, 11.0f), 2) + " ms";
            case A_CHORUS: return juce::String (expMap (v, 0.08f, 6.0f), 2) + " Hz";
            case A_PHASER: return p[2] > 0.5f ? juce::String (expMap (v, 0.04f, 7.0f), 2) + " Hz" : hzText (expMap (v * lerp (0.6f, 1.0f, p[1]), 80.0f, 9000.0f));
            case A_PITCH:
            {
                float st = lerp (-12.0f, 12.0f, v);
                if (p[1] > 0.75f) st = 12.0f * std::round (st / 12.0f);
                else if (p[1] > 0.25f) st = std::round (st);
                return (st >= 0 ? "+" : "") + juce::String (st, std::abs (st - std::round (st)) < 0.05f ? 0 : 1) + " st";
            }
            case A_REPEAT: return p[0] > 0.5f ? juce::String (juce::roundToInt (expMap (1.0f - v, 0.010f, 0.35f) * 1000.0f)) + " ms"
                                              : juce::String (divName (divIndex (1.0f - v)));
            case A_TAPE:   return pctText (v * v);
            case A_CRUSH:  return hzText (expMap (1.0f - v, 400.0f, 44100.0f));
            case A_DIST:   return "+" + juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (expMap (v, 1.0f, 60.0f)))) + " dB";
            case A_RING:
            {
                float hz = expMap (v, 25.0f, 6000.0f);
                if (p[1] > 0.5f) { const float n = std::round (12.0f * std::log2 (hz / 440.0f)); hz = 440.0f * std::pow (2.0f, n / 12.0f); }
                return hzText (hz);
            }
            case A_COMB:   { float note = lerp (24.0f, 96.0f, v); if (p[1] > 0.5f) note = std::round (note); return noteText (note) + " " + hzText (440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f)); }
            case A_FSHIFT: { const float sft = (v - 0.5f) * 2.0f; if (std::abs (sft) < 0.02f) return "0 Hz";
                             const float hz = expMap (std::abs (sft), 0.5f, 2500.0f); return (sft < 0 ? "-" : "+") + hzText (hz); }
            case A_GLITCH: return divName (juce::jlimit (0, 10, divIndex (1.0f - v)));
            case A_CLOUD:  return juce::String (juce::roundToInt (expMap (v, 0.02f, 0.35f) * 1000.0f)) + " ms";
            case A_WAH:    return hzText (expMap (v, 60.0f, 3000.0f));
            case A_SHAPER: return "+" + juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (expMap (v, 1.0f, 50.0f)))) + " dB";
            case A_OCTAVE: return v < 0.4f ? "SUB " + pctText (1.0f - v) : (v > 0.6f ? "UP " + pctText (v) : juce::String ("SUB+UP"));
            case A_WIDTH:  return pctText (v * 2.5f);
            case A_ISO:    return v < 0.25f ? "LOW" : (v > 0.75f ? "HIGH" : (v > 0.4f && v < 0.6f ? "MID" : (v < 0.5f ? "LOW-MID" : "MID-HIGH")));
            default:       return "-";
        }
    }
    switch (a)
    {
        case A_LPF: case A_HPF: case A_BPF: case A_DJ: return juce::String (0.6f + 17.4f * v, 1) + " Q";
        case A_NOTCH:  return pctText (v);
        case A_TALK:   return juce::String (expMap (v, 3.0f, 22.0f), 1) + " Q";
        case A_DELAY:  return pctText (std::min (0.85f, v));
        case A_PITCH:  return pctText (v * 0.9f);
        case A_REPEAT: return p[3] > 0.5f ? pctText (lerp (1.0f, 0.15f, v)) : "x" + juce::String (std::pow (2.0f, lerp (-1.0f, 1.0f, v)), 2);
        case A_REVERSE:return pctText (v * 0.75f);
        case A_TAPE:   return juce::String (expMap (v, 0.03f, 1.5f), 2) + " s";
        case A_CRUSH:  return juce::String (juce::roundToInt (lerp (16.0f, 2.0f, v))) + " bit";
        case A_DIST:   return hzText (expMap (v, 500.0f, 18000.0f));
        case A_COMB:   return pctText (std::min (0.995f, 0.6f + v * 0.4f));
        case A_FLANGE: return pctText (std::min (0.97f, v));
        case A_FSHIFT: return pctText (v * 0.92f);
        case A_CLOUD:  return "+/-" + juce::String (v * 12.0f, 1) + " st";
        case A_SHAPER: { static const char* n[4] = { "TUBE", "FOLD", "FUZZ", "HARD" }; return n[juce::jlimit (0, 3, juce::roundToInt (v * 3.0f))]; }
        case A_WIDTH:  return juce::String (v * 25.0f, 1) + " ms";
        case A_PHASER: return pctText (v * 0.95f);
        default:       return pctText (v);
    }
}

} // namespace swarm
