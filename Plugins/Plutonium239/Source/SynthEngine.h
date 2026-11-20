#pragma once

// Plutonium239 Klang-Engine
// Pro Stimme: 9 Oszillatoren mit eigener Huellkurve. Modulatoren (M1.1-M2.3) modulieren per FM
// einen frei waehlbaren anderen Oszillator, Carrier (C1-C3) gehen in Filter 1/2/3.
// Gerechnet wird in Teilbloecken (max. 32 Samples): LFOs und Filterkoeffizienten pro Teilblock,
// Oszillatoren pro Sample.

#include "Params.h"
#include "Wavetables.h"
#include <array>
#include <atomic>
#include <cmath>

namespace pluto
{
// ---------------- Patch: alle Werte als einfache Zahlen ----------------
struct OscP
{
    int wave = 0, wt = 0, keyMode = 0, sync = -1, dest = -1, fmType = 0, fdest = -1;
    float pitch = 0.0f;  // Oct/Semi/Cent/Coarse zusammen in Halbtoenen
    int pdMode = 0;
    float wtPos = 0.0f, pdAmt = 0.0f;
    float fineHz = 0.0f, pwm = 0.0f, a = 2, d = 400, s = 1, r = 150, lvl = 0.8f, fm = 0.3f, fb = 0.0f;
};
struct FiltP { bool on = true; int type = 1; float cut = 24, res = 0, mix = 1, drive = 0, pan = 0, xpan = 0, lvl = 0.8f; };
struct LfoP  { int shape = 0; float rate = 1; bool sync = false; int div = 4; bool retrig = false; int tA = 0, tB = 0; float aA = 0, aB = 0; };
struct MasterP { float pitch = 0, vol = 0.7f, glide = 0, vel = 0.5f; int voices = 4, bend = 2; };
struct Patch
{
    std::array<OscP, numOsc> o;
    std::array<FiltP, numFilt> f;
    std::array<LfoP, numLfo> l;
    MasterP m;
};

// Modulationswerte der LFOs fuer den aktuellen Teilblock
struct ModVals
{
    float pitch[numOsc] {}, fm[numOsc] {}, pwm[numOsc] {}, wtpos[numOsc] {}, pd[numOsc] {}, lvl[numOsc] {};
    float cut[numFilt] {}, res[numFilt] {}, mix[numFilt] {}, pan[numFilt] {};
    float mpitch = 0, mvol = 0;
    void clear() { *this = ModVals(); }
};

// ---------------- Huellkurve ----------------
struct Env
{
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float level = 0.0f, attInc = 0.0f, decCoef = 0.0f, relCoef = 0.0f, sus = 1.0f;

    void setParams (float aMs, float dMs, float s, float rMs, double sr)
    {
        attInc = 1.0f / (float) std::max (1.0, aMs * 0.001 * sr);
        decCoef = (float) std::exp (-4.6 / std::max (1.0, dMs * 0.001 * sr));
        relCoef = (float) std::exp (-4.6 / std::max (1.0, rMs * 0.001 * sr));
        sus = s;
    }
    void noteOn() { stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }
    bool idle() const { return stage == Idle; }

    inline float next()
    {
        switch (stage)
        {
            case Attack:
                level += attInc;
                if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                break;
            case Decay:
                level = sus + (level - sus) * decCoef;
                if (std::abs (level - sus) < 1.0e-4f) { level = sus; stage = Sustain; }
                break;
            case Sustain: level = sus; break;
            case Release:
                level *= relCoef;
                if (level < 1.0e-4f) { level = 0.0f; stage = Idle; }
                break;
            case Idle: break;
        }
        return level;
    }
};

// ---------------- Filter ----------------
// TPT-State-Variable-Filter (Zavalishin/Cytomic)
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 2, a1 = 0, a2 = 0, a3 = 0;
    void set (float fc, float kk, double sr)
    {
        g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.0f, (float) (sr * 0.49), fc) / (float) sr);
        k = kk;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    // gibt lp, bp, hp zurueck
    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

struct VoiceFilter
{
    static constexpr int combSize = 8192;
    Svf s1, s2, fm[3];
    std::array<float, combSize> comb {};
    int combW = 0;
    float combD = 100, combFb = 0, combLp = 0, combNorm = 1, dcX = 0, dcY = 0;
    int type = 1;
    float fmGain[3] { 1, 0.6f, 0.35f };

    void reset()
    {
        s1.reset(); s2.reset();
        for (auto& f : fm) f.reset();
        comb.fill (0.0f);
        combLp = dcX = dcY = 0;
    }

    void set (int t, float fc, float res, double sr)
    {
        type = t;
        res = juce::jlimit (0.0f, 1.0f, res);
        if (t == CombPlus || t == CombMinus)
        {
            combD = juce::jlimit (2.0f, (float) combSize - 4.0f, (float) (sr / juce::jmax (20.0f, fc)));
            const float fb = res * 0.97f;
            combFb = t == CombPlus ? fb : -fb;
            combNorm = 1.0f - fb * 0.75f;
        }
        else if (t == Formant)
        {
            static const float F[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
            const float pos = juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (20.0f, fc) / 20.0f) / std::log (1000.0f)) * 4.0f;
            const int a = juce::jmin ((int) pos, 3);
            const float fr = pos - (float) a;
            const float k = 1.0f / (4.0f + res * 18.0f);
            for (int i = 0; i < 3; ++i)
                fm[i].set (F[a][i] * (1.0f - fr) + F[a + 1][i] * fr, k, sr);
        }
        else
        {
            const float k = 2.0f * (1.0f - res * 0.985f);
            s1.set (fc, k, sr);
            const bool four = (t == HP4 || t == LP4 || t == BP4);
            if (four) s2.set (fc, t == BP4 ? k : 1.414f, sr);
        }
    }

    inline float tick (float x)
    {
        float lp, bp, hp;
        switch (type)
        {
            case HP2: s1.tick (x, lp, bp, hp); return hp;
            case LP2: s1.tick (x, lp, bp, hp); return lp;
            case BP2: s1.tick (x, lp, bp, hp); return bp * s1.k;
            case Notch: s1.tick (x, lp, bp, hp); return x - s1.k * bp;
            case HP4: { s1.tick (x, lp, bp, hp); float l2, b2, h2; s2.tick (hp, l2, b2, h2); return h2; }
            case LP4: { s1.tick (x, lp, bp, hp); float l2, b2, h2; s2.tick (lp, l2, b2, h2); return l2; }
            case BP4: { s1.tick (x, lp, bp, hp); float l2, b2, h2; s2.tick (bp * s1.k, l2, b2, h2); return b2 * s2.k; }
            case CombPlus:
            case CombMinus:
            {
                float rp = (float) combW - combD;
                if (rp < 0) rp += (float) combSize;
                const int i0 = (int) rp;
                const float fr = rp - (float) i0;
                const float d = comb[(size_t) i0] + (comb[(size_t) ((i0 + 1) & (combSize - 1))] - comb[(size_t) i0]) * fr;
                combLp += (d - combLp) * 0.7f; // leichte Daempfung im Feedback
                const float y = x + combFb * combLp;
                comb[(size_t) combW] = juce::jlimit (-4.0f, 4.0f, y);
                combW = (combW + 1) & (combSize - 1);
                // DC-Blocker (Kammfilter verstaerkt sonst Gleichanteile)
                const float o = y * combNorm;
                const float dcOut = o - dcX + 0.9974f * dcY;
                dcX = o; dcY = dcOut;
                return dcOut;
            }
            case Formant:
            {
                float s = 0;
                for (int i = 0; i < 3; ++i) { fm[i].tick (x, lp, bp, hp); s += bp * fm[i].k * fmGain[i]; }
                return s * 3.0f;
            }
            default: return x;
        }
    }
};

// ---------------- Oszillator-Grundformen (PolyBLEP) ----------------
inline float polyBlep (double t, double dt)
{
    if (dt <= 0.0) return 0.0f;
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

// ---------------- Stimme ----------------
struct Voice
{
    int note = -1;
    float velocity = 1.0f;
    bool gate = false, sustained = false;
    uint64_t age = 0;
    float curNote = 60.0f, targetNote = 60.0f, spread = 0.0f;

    double phase[numOsc] {};
    float out[numOsc] {}, prevOut[numOsc] {}, prevOut2[numOsc] {};
    bool wrapped[numOsc] {}, prevWrapped[numOsc] {};
    Env env[numOsc];
    VoiceFilter filt[numFilt];

    bool active() const { return note >= 0; }

    void start (int n, float vel, float fromNote, bool reset, float spr)
    {
        const bool wasIdle = ! active();
        note = n; velocity = vel; gate = true; sustained = false;
        targetNote = (float) n;
        curNote = fromNote;
        spread = spr;
        if (wasIdle || reset)
        {
            for (int o = 0; o < numOsc; ++o)
            {
                phase[o] = 0; out[o] = prevOut[o] = prevOut2[o] = 0;
                env[o].level = 0;
            }
            for (auto& f : filt) f.reset();
        }
        for (auto& e : env) e.noteOn();
    }

    void release() { gate = false; for (auto& e : env) e.noteOff(); }
    void kill() { note = -1; for (auto& e : env) { e.stage = Env::Idle; e.level = 0; } }
};

// ---------------- LFO ----------------
struct Lfo
{
    double phase = 0;
    float value = 0, rndA = 0, rndB = 0, held = 0;
    juce::Random rng;

    float shapeAt (int shape, double p)
    {
        const float x = (float) p;
        switch (shape)
        {
            case 0: return std::sin (juce::MathConstants<float>::twoPi * x);
            case 1: return x < 0.25f ? 4 * x : (x < 0.75f ? 2 - 4 * x : 4 * x - 4);
            case 2: return 2 * x - 1;
            case 3: return 1 - 2 * x;
            case 4: return x < 0.5f ? 1.0f : -1.0f;
            case 5: return held;
            default:
            {
                const float t = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * x);
                return rndA + (rndB - rndA) * t;
            }
        }
    }

    void advance (double inc)
    {
        phase += inc;
        if (phase >= 1.0) { phase -= std::floor (phase); newCycle(); }
    }
    void newCycle()
    {
        held = rng.nextFloat() * 2 - 1;
        rndA = rndB; rndB = rng.nextFloat() * 2 - 1;
    }
};

// ---------------- Synth ----------------
class SynthEngine
{
public:
    static constexpr int maxVoices = 16;
    static constexpr int subBlock = 32;

    std::atomic<float> lfoDisplay[numLfo] {};   // aktueller LFO-Wert fuer die Anzeige
    std::atomic<float> lfoPhaseDisplay[numLfo] {};

    explicit SynthEngine (const WavetableBank& b) : bank (b) { for (auto& u : userTables) u.store (nullptr); }

    // eigene Wavetables pro Oszillator (vom Editor/Processor gesetzt, Audio-Thread liest nur)
    std::atomic<const WtTable*> userTables[numOsc];

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v.kill();
        volSmooth = 0.7f;
    }

    void setPatch (const Patch& p) { patch = p; }
    void setTransport (bool playing, double ppq, double bpmIn) { isPlaying = playing; ppqPos = ppq; bpm = bpmIn > 0 ? bpmIn : 120.0; }

    // ---- MIDI ----
    void noteOn (int n, float vel)
    {
        const int maxV = juce::jlimit (1, maxVoices, patch.m.voices);
        for (int i = maxV; i < maxVoices; ++i) if (voices[i].active()) voices[i].release();
        for (int i = 0; i < numLfo; ++i)
            if (patch.l[(size_t) i].retrig) { lfos[i].phase = 0; lfos[i].newCycle(); }

        const float from = (patch.m.glide > 0.5f && lastNote >= 0) ? lastNoteGlide : (float) n;

        Voice* v = nullptr;
        // gleiche Note wiederverwenden
        for (int i = 0; i < maxV; ++i) if (voices[i].note == n) { v = &voices[i]; break; }
        if (v == nullptr) for (int i = 0; i < maxV; ++i) if (! voices[i].active()) { v = &voices[i]; break; }
        if (v == nullptr)
        {
            // aelteste Stimme klauen, bevorzugt schon losgelassene
            uint64_t bestRel = UINT64_MAX, bestAll = UINT64_MAX;
            Voice* rel = nullptr; Voice* any = nullptr;
            for (int i = 0; i < maxV; ++i)
            {
                if (! voices[i].gate && voices[i].age < bestRel) { bestRel = voices[i].age; rel = &voices[i]; }
                if (voices[i].age < bestAll) { bestAll = voices[i].age; any = &voices[i]; }
            }
            v = rel != nullptr ? rel : any;
        }
        if (v == nullptr) v = &voices[0];
        spreadToggle = -spreadToggle;
        const float spr = maxV > 1 ? spreadToggle * (0.4f + 0.6f * rng.nextFloat()) : 0.0f;
        v->start (n, vel, from, false, spr);
        v->age = ++ageCounter;
        lastNote = n;
        lastNoteGlide = (float) n;
    }

    void noteOff (int n)
    {
        for (auto& v : voices)
            if (v.note == n && v.gate)
            {
                if (sustainPedal) v.sustained = true;
                else v.release();
            }
    }

    void setSustain (bool on)
    {
        sustainPedal = on;
        if (! on)
            for (auto& v : voices)
                if (v.sustained) { v.sustained = false; v.release(); }
    }
    void setPitchBend (float norm) { bend = norm; }
    void allNotesOff() { for (auto& v : voices) v.release(); }

    // ---- Audio ----
    void render (float* L, float* R, int numSamples)
    {
        int pos = 0;
        while (pos < numSamples)
        {
            const int n = juce::jmin (subBlock, numSamples - pos);
            renderSub (L + pos, R + pos, n);
            pos += n;
        }
    }

private:
    const WavetableBank& bank;
    Patch patch;
    double sr = 44100.0;
    std::array<Voice, maxVoices> voices;
    std::array<Lfo, numLfo> lfos;
    ModVals mod;
    juce::Random rng;
    uint64_t ageCounter = 0;
    int lastNote = -1;
    float lastNoteGlide = 60.0f, bend = 0.0f, spreadToggle = 1.0f, volSmooth = 0.7f;
    bool sustainPedal = false, isPlaying = false;
    double ppqPos = 0, bpm = 120.0;

    // pro Teilblock berechnet
    bool used[numOsc] {};
    int srcList[numOsc][numOsc] {};
    int srcCount[numOsc] {};
    int order[numOsc] {};

    void updateLfos (int n)
    {
        mod.clear();
        for (int i = 0; i < numLfo; ++i)
        {
            const auto& lp = patch.l[(size_t) i];
            auto& lfo = lfos[(size_t) i];
            double hz = lp.rate;
            if (lp.sync)
            {
                const double beats = lfoDivBeats (lp.div);
                hz = bpm / 60.0 / beats;
                if (isPlaying && ! lp.retrig)
                {
                    const double ph = ppqPos / beats;
                    const double np = ph - std::floor (ph);
                    if (np < lfo.phase - 0.5) lfo.newCycle();
                    lfo.phase = np;
                }
            }
            const float v = lfo.shapeAt (lp.shape, lfo.phase);
            lfo.value = v;
            lfoDisplay[i].store (v, std::memory_order_relaxed);
            lfoPhaseDisplay[i].store ((float) lfo.phase, std::memory_order_relaxed);
            apply (lp.tA, lp.aA * v);
            apply (lp.tB, lp.aB * v);
            lfo.advance (hz * n / sr);
        }
    }

    void apply (int targetIdx, float amt)
    {
        if (targetIdx <= 0 || amt == 0.0f) return;
        const auto& t = targets()[(size_t) juce::jmin (targetIdx, (int) targets().size() - 1)];
        switch (t.kind)
        {
            case tPitch: mod.pitch[t.index] += amt * 24.0f; break;
            case tFm:    mod.fm[t.index] += amt; break;
            case tPwm:   mod.pwm[t.index] += amt; break;
            case tWtPos: mod.wtpos[t.index] += amt; break;
            case tPd:    mod.pd[t.index] += amt; break;
            case tLevel: mod.lvl[t.index] += amt; break;
            case tCut:   mod.cut[t.index] += amt * 6.0f; break; // Oktaven
            case tRes:   mod.res[t.index] += amt; break;
            case tMix:   mod.mix[t.index] += amt; break;
            case tPan:   mod.pan[t.index] += amt; break;
            case tMasterPitch: mod.mpitch += amt * 24.0f; break;
            case tMasterVol:   mod.mvol += amt; break;
            default: break;
        }
    }

    void updateRouting()
    {
        // welche Oszillatoren werden gebraucht? Carrier mit Filterziel, plus alles was sie (indirekt) moduliert oder synct
        for (int o = 0; o < numOsc; ++o) used[o] = isCarrier (o) && patch.o[(size_t) o].fdest >= 0;
        for (int iter = 0; iter < numOsc; ++iter)
            for (int o = 0; o < numOsc; ++o)
            {
                const auto& p = patch.o[(size_t) o];
                if (! isCarrier (o) && p.dest >= 0 && used[p.dest]) used[o] = true;
                if (used[o] && p.sync >= 0) used[p.sync] = true;
            }
        for (int o = 0; o < numOsc; ++o) srcCount[o] = 0;
        for (int s = 0; s < numMod; ++s)
        {
            const auto& p = patch.o[(size_t) s];
            if (used[s] && p.dest >= 0) srcList[p.dest][srcCount[p.dest]++] = s;
        }
    }

    void renderSub (float* L, float* R, int n)
    {
        updateLfos (n);
        updateRouting();

        const double glideCoef = patch.m.glide > 0.5f ? std::exp (-1.0 * n / (patch.m.glide * 0.001 * sr / 4.0)) : 0.0;
        const float masterPitch = patch.m.pitch + mod.mpitch + bend * (float) patch.m.bend;

        // pro Oszillator: FM-Tiefe, PWM, Pegel
        float depth[numOsc] {}, pwm[numOsc] {}, lvl[numOsc] {}, wtp[numOsc] {}, pdA[numOsc] {};
        const WtTable* ut[numOsc] {};
        for (int o = 0; o < numOsc; ++o)
        {
            const auto& p = patch.o[(size_t) o];
            pwm[o] = juce::jlimit (0.0f, 1.0f, p.pwm + mod.pwm[o]);
            wtp[o] = juce::jlimit (0.0f, 1.0f, p.wtPos + mod.wtpos[o]);
            pdA[o] = p.pdAmt + mod.pd[o];
            ut[o] = userTables[o].load (std::memory_order_acquire);
            lvl[o] = juce::jlimit (0.0f, 1.0f, p.lvl + mod.lvl[o]);
            if (! isCarrier (o))
            {
                const float d = juce::jlimit (0.0f, 1.0f, p.fm + mod.fm[o]);
                depth[o] = d * d * lvl[o];
            }
        }

        for (auto& v : voices)
        {
            if (! v.active()) continue;
            renderVoice (v, L, R, n, glideCoef, masterPitch, depth, pwm, lvl, wtp, pdA, ut);
        }

        // Master
        const float target = juce::jlimit (0.0f, 1.0f, patch.m.vol + mod.mvol);
        const float tgain = target * target * 1.6f;
        const float step = (tgain - volSmooth) / (float) n;
        for (int i = 0; i < n; ++i)
        {
            volSmooth += step;
            L[i] = softClip (L[i] * volSmooth);
            R[i] = softClip (R[i] * volSmooth);
        }
    }

    static inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a < 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
        return x < 0 ? -y : y;
    }

    void renderVoice (Voice& v, float* L, float* R, int n, double glideCoef, float masterPitch,
                      const float* depth, const float* pwm, const float* lvl,
                      const float* wtp, const float* pdA, const WtTable* const* ut)
    {
        v.curNote = (float) (v.targetNote + (v.curNote - v.targetNote) * glideCoef);
        if (std::abs (v.curNote - v.targetNote) < 0.001f) v.curNote = v.targetNote;

        double inc[numOsc] {};
        for (int o = 0; o < numOsc; ++o)
        {
            if (! used[o]) continue;
            const auto& p = patch.o[(size_t) o];
            float key = v.curNote;
            if (p.keyMode == 1) key = 60.0f;
            else if (p.keyMode == 2) key = 60.0f + (v.curNote - 60.0f) * 0.5f;
            const float midi = key + p.pitch + masterPitch + mod.pitch[o];
            const double hz = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0) + p.fineHz;
            inc[o] = juce::jlimit (-0.45, 0.45, hz / sr);
            v.env[o].setParams (p.a, p.d, p.s, p.r, sr);
        }

        // Filterkoeffizienten
        float fMix[numFilt], fLvl[numFilt], gL[numFilt], gR[numFilt], fDrive[numFilt];
        bool fOn[numFilt], fUsed[numFilt] {};
        for (int c = numMod; c < numOsc; ++c)
            if (used[c] && patch.o[(size_t) c].fdest >= 0) fUsed[patch.o[(size_t) c].fdest] = true;
        for (int f = 0; f < numFilt; ++f)
        {
            const auto& fp = patch.f[(size_t) f];
            fOn[f] = fp.on;
            const float fc = fp.cut * std::pow (2.0f, mod.cut[f]);
            v.filt[f].set (fp.type, juce::jlimit (20.0f, 20000.0f, fc), fp.res + mod.res[f], sr);
            fMix[f] = juce::jlimit (0.0f, 1.0f, fp.mix + mod.mix[f]);
            fLvl[f] = fp.lvl;
            fDrive[f] = 1.0f + fp.drive * 8.0f;
            const float pan = juce::jlimit (-1.0f, 1.0f, fp.pan + mod.pan[f] + fp.xpan * v.spread);
            const float ang = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            gL[f] = std::cos (ang) * fLvl[f];
            gR[f] = std::sin (ang) * fLvl[f];
        }

        const float velGain = 1.0f - patch.m.vel * (1.0f - v.velocity);
        const float voiceGain = 0.35f * velGain;
        bool anyCarrierRouted = false;
        for (int c = numMod; c < numOsc; ++c) if (used[c]) anyCarrierRouted = true;

        for (int i = 0; i < n; ++i)
        {
            float fin[numFilt] {};
            for (int o = 0; o < numOsc; ++o)
            {
                if (! used[o]) continue;
                const auto& p = patch.o[(size_t) o];

                // FM-Eingaenge: Lin/Log aendern die Frequenz, PM verschiebt die Phase, PD steuert die Phase Distortion
                double rel = 1.0, logOct = 0.0, pmOff = 0.0;
                float pdMod = 0.0f;
                for (int k = 0; k < srcCount[o]; ++k)
                {
                    const int s = srcList[o][k];
                    float sig = s < o ? v.out[s] : v.prevOut[s];
                    const int t = patch.o[(size_t) s].fmType;
                    if (t & 1) sig = (sig + 1.0f) * 0.5f;    // "+"-Varianten: nur positiv
                    switch (t >> 1)
                    {
                        case 0: rel += (double) (sig * depth[s] * 8.0f); break;
                        case 1: logOct += (double) (sig * depth[s] * 4.0f); break;
                        case 2: pmOff += (double) (sig * depth[s] * 2.0f); break;
                        default: pdMod += sig * depth[s] * 1.5f; break;
                    }
                }
                double dInc = inc[o] * rel;
                if (logOct != 0.0) dInc *= std::exp2 (logOct);
                dInc = juce::jlimit (-0.49, 0.49, dInc);

                double ph = v.phase[o] + dInc;
                bool wrap = false;
                if (ph >= 1.0) { ph -= 1.0; wrap = true; }
                else if (ph < 0.0) { ph += 1.0; wrap = true; }

                if (p.sync >= 0)
                {
                    const bool mw = p.sync < o ? v.wrapped[p.sync] : v.prevWrapped[p.sync];
                    if (mw) ph = 0.0;
                }
                v.phase[o] = ph;
                v.prevWrapped[o] = v.wrapped[o];
                v.wrapped[o] = wrap;

                // Feedback (Phasenmodulation, gemittelt gegen Flattern) + PM von anderen Oszillatoren
                double rp = ph + pmOff;
                if (! isCarrier (o) && p.fb > 0.0f)
                    rp += (double) (p.fb * p.fb * 0.3f * (v.prevOut[o] + v.prevOut2[o]));
                rp -= std::floor (rp);

                // Phase Distortion
                const double pdD = juce::jlimit (0.0, 1.0, (double) (pdA[o] + pdMod));
                const bool reso = pdIsReso (p.pdMode);
                if (p.pdMode != pdOff && ! reso) rp = pdWarp (p.pdMode, pdD, rp);

                const double dt = std::abs (dInc);
                float w;
                if (reso) w = pdReso (p.pdMode, pdD, rp);
                else switch (p.wave)
                {
                    case 0: w = std::sin (juce::MathConstants<float>::twoPi * (float) rp); break;
                    case 1:
                    {
                        const float sk = 0.5f - pwm[o] * 0.49f;
                        const float x = (float) rp;
                        const float tri = x < sk ? x / sk : 1.0f - (x - sk) / (1.0f - sk);
                        w = 2.0f * tri - 1.0f;
                        break;
                    }
                    case 2: w = (float) (2.0 * rp - 1.0) - polyBlep (rp, dt); break;
                    case 3:
                    {
                        const double pw = 0.5 - pwm[o] * 0.45;
                        w = rp < pw ? 1.0f : -1.0f;
                        w += polyBlep (rp, dt);
                        double t2 = rp - pw; if (t2 < 0) t2 += 1.0;
                        w -= polyBlep (t2, dt);
                        w -= (float) (2.0 * pw - 1.0);
                        break;
                    }
                    default:
                        w = (p.wt == userTableIndex && ut[o] != nullptr) ? ut[o]->read (wtp[o], rp, dt)
                                                                         : bank.read (p.wt, wtp[o], rp, dt);
                        break;
                }

                const float e = v.env[o].next();
                v.prevOut2[o] = v.prevOut[o];
                v.prevOut[o] = v.out[o];
                v.out[o] = w * e;

                if (isCarrier (o) && p.fdest >= 0)
                    fin[p.fdest] += v.out[o] * lvl[o];
            }

            float l = 0, r = 0;
            for (int f = 0; f < numFilt; ++f)
            {
                float x = fin[f];
                if (! fUsed[f]) continue;
                if (fOn[f])
                {
                    float xd = x;
                    if (fDrive[f] > 1.001f) xd = std::tanh (x * fDrive[f]) / std::sqrt (fDrive[f]);
                    const float y = v.filt[f].tick (xd);
                    x = x + (y - x) * fMix[f];
                }
                l += x * gL[f];
                r += x * gR[f];
            }
            L[i] += l * voiceGain;
            R[i] += r * voiceGain;
        }

        // Stimme fertig?
        bool done = true;
        if (anyCarrierRouted)
        {
            for (int c = numMod; c < numOsc; ++c)
                if (used[c] && ! v.env[c].idle()) { done = false; break; }
        }
        else if (v.gate) done = false;
        if (done && ! v.gate) v.kill();
        else if (! anyCarrierRouted && ! v.gate) v.kill();
    }
};
} // namespace pluto
