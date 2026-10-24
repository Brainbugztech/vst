// SnareHead - Brain Bugz
// Header-only snare synth engine (no JUCE dependency, used by plugin + CLI renderer)
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace snarehead {

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

// ---------------------------------------------------------------- parameters
struct Params {
    // Ring (tonal cluster)
    float tune = 800.f;       // Hz, 700..1600
    float spread = 0.5f;      // 0..1  cluster detune / shimmer
    float metal = 0.f;        // 0..1  harmonic cluster -> inharmonic clang
    float ringLevel = 1.f;    // 0..1
    float punch = 0.4f;       // 0..1  click + pitch drop
    // Noise
    float noiseLevel = 0.5f;  // 0..1
    float noiseTone = 0.6f;   // 0..1  (1.5k .. 9k)
    // Shape (box envelope)
    float hold = 150.f;       // ms 10..800
    float release = 40.f;     // ms 2..600
    // Resonance EQ
    float eqFreq = 800.f;     // Hz 700..1600
    float eqGain = 6.f;       // dB 0..18
    float eqQ = 4.f;          // 0.5..12
    bool eqLink = false;      // EQ follows ring tune
    // XTREME NOISY macro (comb, ring mod, drive, crush)
    float noisy = 0.f;        // 0..1
    // Reverb
    float verbSize = 0.6f;    // 0..1
    float verbMix = 0.f;      // 0..1
    float verbGate = 2000.f;  // ms 30..2000 (2000 = gate off)
    int verbSync = 0;         // 0 = use ms, 1..9 = note division (see kSyncDivs)
    float verbCut = 0.1f;     // 0..1  gate release (hard cut .. short fade)
    // Output
    float width = 0.5f;       // 0 mono, 0.5 natural, 1 extra wide
    float volume = 0.f;       // dB -24..+6
    bool keyTrack = false;    // MIDI note transposes tune (C3/60 = tune)
    // Sample layer (own snare, runs through the whole chain)
    float smpLevel = 0.f;     // dB -30..+12 (-30 = off)
    float smpPitch = 0.f;     // semitones -24..+24
    float smpStart = 0.f;     // 0..0.5 of the sample length
    bool smpReverse = false;
    bool smpShape = false;    // Hold/Release also cut the sample
};

// Loaded user sample (always 2 channels, owned by the plugin, read-only for the engine)
struct SampleData {
    std::vector<float> ch[2];
    double sampleRate = 44100.0;
    int length() const { return (int)ch[0].size(); }
};

// note divisions in quarter notes: 1/64 1/32 1/16T 1/16 1/8T 1/8 1/4 1/2 1/1
static const float kSyncDivs[10] = {0.f, 1.f / 16, 1.f / 8, 1.f / 6, 1.f / 4, 1.f / 3, 1.f / 2, 1.f, 2.f, 4.f};
static const char* const kSyncNames[10] = {"ms", "1/64", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/2", "1/1"};

// ---------------------------------------------------------------- helpers
struct Rng {
    uint32_t s = 0x9E3779B9u;
    inline float next() { // -1..1
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)((int32_t)s) * (1.0f / 2147483648.0f);
    }
    inline float uni() { return 0.5f * (next() + 1.f); }
};

inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// TPT state variable filter
struct Svf {
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set(float fc, float q, float sr) {
        fc = std::clamp(fc, 20.f, sr * 0.45f);
        g = std::tan(kPi * fc / sr); k = 1.f / q;
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    // returns lp, bp, hp
    inline void tick(float x, float& lp, float& bp, float& hp) {
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

// RBJ biquad
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void peaking(float fc, float q, float gainDb, float sr) {
        float A = std::pow(10.f, gainDb / 40.f);
        float w = kTwoPi * std::clamp(fc, 20.f, sr * 0.45f) / sr;
        float al = std::sin(w) / (2 * q), c = std::cos(w);
        float a0 = 1 + al / A;
        b0 = (1 + al * A) / a0; b1 = -2 * c / a0; b2 = (1 - al * A) / a0;
        a1 = -2 * c / a0; a2 = (1 - al / A) / a0;
    }
    void highpass(float fc, float q, float sr) {
        float w = kTwoPi * fc / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
        float a0 = 1 + al;
        b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0;
        a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void reset() { z1 = z2 = 0; }
    inline float tick(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct DelayLine {
    std::vector<float> buf; int w = 0;
    void init(int n) { buf.assign((size_t)std::max(n, 2), 0.f); w = 0; }
    void reset() { std::fill(buf.begin(), buf.end(), 0.f); w = 0; }
    inline void push(float x) { buf[(size_t)w] = x; if (++w >= (int)buf.size()) w = 0; }
    inline float read(int d) const { // d samples ago (1..size)
        int i = w - d; while (i < 0) i += (int)buf.size(); return buf[(size_t)i];
    }
    inline float readFrac(float d) const {
        int i = (int)d; float f = d - (float)i;
        return lerp(read(std::max(i, 1)), read(std::max(i + 1, 1)), f);
    }
};

// ---------------------------------------------------------------- reverb (8-line FDN)
class FdnReverb {
public:
    void prepare(float sr) {
        fs = sr;
        const float scale = sr / 44100.f;
        for (int i = 0; i < 8; ++i) lines[i].init((int)(kBase[i] * 1.7f * scale) + 8);
        for (int i = 0; i < 4; ++i) ap[i].init((int)(kAp[i] * scale) + 2);
        pre.init((int)(0.05f * sr) + 2);
        for (auto& l : lp) l = 0;
        hpL.highpass(220.f, 0.7f, sr); hpR.highpass(220.f, 0.7f, sr);
        setSize(0.6f);
    }
    void reset() {
        for (auto& l : lines) l.reset();
        for (auto& a : ap) a.reset();
        pre.reset(); for (auto& l : lp) l = 0; hpL.reset(); hpR.reset();
    }
    void setSize(float size) {
        const float scale = fs / 44100.f;
        float lenScale = 0.45f + size * 1.25f;
        float t60 = 0.35f + size * size * 5.5f;
        for (int i = 0; i < 8; ++i) {
            len[i] = std::max(8.f, kBase[i] * lenScale * scale);
            gain[i] = std::pow(10.f, -3.f * len[i] / (t60 * fs));
        }
        float damp = 9000.f - size * 4500.f;
        dampCoef = std::exp(-kTwoPi * damp / fs);
        preDelay = (0.004f + size * 0.025f) * fs;
    }
    inline void tick(float inL, float inR, float& outL, float& outR) {
        float x = 0.5f * (inL + inR);
        pre.push(x);
        float d = pre.readFrac(preDelay);
        // input diffusion
        for (int i = 0; i < 4; ++i) {
            float z = ap[i].read((int)ap[i].buf.size() - 1);
            float v = d + 0.6f * z;
            ap[i].push(v);
            d = z - 0.6f * v;
        }
        float s[8];
        float sum = 0;
        for (int i = 0; i < 8; ++i) {
            float y = lines[i].readFrac(len[i]);
            lp[i] = y * (1 - dampCoef) + lp[i] * dampCoef;
            s[i] = lp[i] * gain[i];
            sum += s[i];
        }
        sum *= 0.25f; // Householder: s - 2/N * sum
        float injL = d + 0.3f * inL, injR = d + 0.3f * inR;
        for (int i = 0; i < 8; ++i) lines[i].push(s[i] - sum + ((i & 1) ? injR : injL) * 0.5f);
        float oL = s[0] - s[2] + s[4] - s[6] + 0.5f * (s[1] + s[5]);
        float oR = s[1] - s[3] + s[5] - s[7] + 0.5f * (s[2] + s[6]);
        outL = hpL.tick(oL * 0.45f);
        outR = hpR.tick(oR * 0.45f);
    }
private:
    static constexpr float kBase[8] = {1031, 1327, 1523, 1741, 1997, 2273, 2539, 2861};
    static constexpr float kAp[4] = {142, 107, 379, 277};
    float fs = 44100;
    DelayLine lines[8], ap[4], pre;
    float len[8]{}, gain[8]{}, lp[8]{};
    float dampCoef = 0.5f, preDelay = 100;
    Biquad hpL, hpR;
};

// ---------------------------------------------------------------- voice
class Voice {
public:
    static constexpr int kPartials = 6;

    void prepare(float sr) { fs = sr; for (auto& f : nf) f.reset(); for (auto& f : nl) f.reset(); for (auto& f : nb) f.reset(); }
    bool isActive() const { return active; }

    void start(float f0_, float vel, const Params& p, uint32_t seed, const SampleData* sd = nullptr, float keyRatio = 1.f) {
        rng.s = seed | 1u;
        smp = nullptr;
        if (sd != nullptr && sd->length() > 4 && p.smpLevel > -29.9f) {
            const int len = sd->length();
            const double rate = sd->sampleRate / fs * std::pow(2.0, p.smpPitch / 12.0) * keyRatio;
            const double st = std::clamp(p.smpStart, 0.f, 0.95f) * (len - 1);
            sRev = p.smpReverse;
            sPos = sRev ? (len - 2) - st : st;
            sInc = sRev ? -rate : rate;
            // level 0 dB = sample plays at its own level at the output (engine scales by kHeadroom)
            sGain = dbToGain(p.smpLevel) * 4.f;
            sShape = p.smpShape;
            sFadeIn = st > 0.5 ? 1.f / (0.002f * fs) : 1.f; // short fade when starting mid-sample
            sFadeAmt = st > 0.5 ? 0.f : 1.f;
            smp = sd;
        }
        f0 = f0_;
        amp = std::pow(vel, 0.8f);
        bright = 0.7f + 0.3f * vel;
        t = 0; active = true; choke = false; chokeGain = 1;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kPartials; ++k) { ph[c][k] = rng.uni(); last[c][k] = 0; }
        for (auto& f : nf) f.reset();
        for (auto& f : nl) f.reset();
        for (auto& f : nb) f.reset();
        update(p);
    }
    void chokeOut() { if (active) { choke = true; } }
    void dropSample() { smp = nullptr; }

    // ratios (relative to tune) and normalised amplitudes of the ring partials - also used by the UI
    static void ringPartials(const Params& p, float* ratioOut, float* ampOut) {
        static const float dC[kPartials] = {-0.17f, -0.10f, -0.035f, 0.03f, 0.09f, 0.16f};
        static const float aC[kPartials] = {0.55f, 0.8f, 1.0f, 0.9f, 0.7f, 0.5f};
        static const float rM[kPartials] = {1.0f, 1.506f, 1.968f, 2.411f, 2.932f, 3.671f};
        static const float aM[kPartials] = {1.0f, 0.85f, 0.7f, 0.6f, 0.45f, 0.35f};
        float ampSum = 0;
        for (int k = 0; k < kPartials; ++k) {
            float cl = 1.f + p.spread * dC[k];
            float mt = rM[k] * (1.f + p.spread * dC[k] * 0.3f);
            ratioOut[k] = lerp(cl, mt, p.metal);
            ampOut[k] = lerp(aC[k], aM[k], p.metal);
            ampSum += ampOut[k];
        }
        for (int k = 0; k < kPartials; ++k) ampOut[k] /= ampSum;
    }

    void update(const Params& p) {
        ringPartials(p, ratio, pa);
        stereoDetune = 1.f + p.width * 0.008f;
        fmIdx = p.metal * 1.4f;
        punch = p.punch;
        ringLevel = p.ringLevel;
        noiseLevel = p.noiseLevel;
        widthN = std::clamp(p.width, 0.f, 1.f);
        holdS = std::max(1.f, p.hold * 0.001f * fs);
        relS = std::max(1.f, p.release * 0.001f * fs);
        holdTau = p.hold * 0.001f * fs * 3.0f;
        sShape = p.smpShape;
        if (smp != nullptr) sGain = p.smpLevel > -29.9f ? dbToGain(p.smpLevel) * 4.f : 0.f;
        float nfc = 1500.f * std::pow(6.f, p.noiseTone) * bright;
        for (auto& f : nf) f.set(nfc * 0.3f, 0.7f, fs);              // highpass
        for (auto& f : nl) f.set(std::min(nfc * 1.7f, 16000.f), 0.7f, fs); // lowpass
        for (auto& f : nb) f.set(nfc, 1.6f, fs);                     // presence peak
    }

    // adds into L/R
    inline void tick(float& L, float& R) {
        if (!active) return;
        // box envelope
        float env = 0.f;
        bool envDone = false;
        float att = std::min(1.f, t / (0.0004f * fs));
        if (t < holdS) env = std::exp(-t / holdTau);
        else {
            float x = (t - holdS) / relS;
            if (x >= 1.f) envDone = true;
            else {
                float e0 = std::exp(-holdS / holdTau);
                float u = 1.f - x;
                env = e0 * u * u * u;
            }
        }
        env *= att;
        if (choke) {
            chokeGain -= 1.f / (0.004f * fs);
            if (chokeGain <= 0) { active = false; return; }
            env *= chokeGain;
        }

        // sample layer
        if (smp != nullptr) {
            if (sShape && envDone) smp = nullptr;
            else {
                const int len = smp->length();
                const int i0 = (int)std::floor(sPos);
                if (i0 < 0 || i0 >= len - 1) smp = nullptr;
                else {
                    const float fr = (float)(sPos - i0);
                    const int im = std::max(i0 - 1, 0), i2 = i0 + 1, i3 = std::min(i0 + 2, len - 1);
                    float g = sGain * amp * (sShape ? env : att * (choke ? chokeGain : 1.f)) * sFadeAmt;
                    // short fade at the end of the sample so it never clicks
                    const double left = sRev ? sPos : (len - 1) - sPos;
                    const float endFade = (float)std::min(1.0, left / std::max(1.0, std::fabs(sInc) * 0.003 * fs));
                    g *= endFade;
                    for (int c = 0; c < 2; ++c) {
                        const float* d = smp->ch[c].data();
                        const float y0 = d[im], y1 = d[i0], y2 = d[i2], y3 = d[i3];
                        // 4-point Hermite
                        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
                        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                        const float v = ((c3 * fr + c2) * fr + c1) * fr + y1;
                        (c == 0 ? L : R) += v * g;
                    }
                    sPos += sInc;
                    sFadeAmt = std::min(1.f, sFadeAmt + sFadeIn);
                }
            }
        }
        if (envDone) {
            if (smp == nullptr) active = false;
            t += 1.f;
            return;
        }

        float pEnv = 1.f + punch * 0.6f * std::exp(-t / (0.006f * fs));
        float outC[2];
        for (int c = 0; c < 2; ++c) {
            float sum = 0;
            float det = (c == 0) ? 1.f / stereoDetune : stereoDetune;
            for (int k = 0; k < kPartials; ++k) {
                float dk = (k & 1) ? det : 1.f / det;
                float inc = f0 * ratio[k] * pEnv * dk / fs;
                ph[c][k] += inc; ph[c][k] -= std::floor(ph[c][k]);
                float fm = fmIdx * last[c][(k + 1) % kPartials];
                float v = std::sin(kTwoPi * ph[c][k] + fm);
                last[c][k] = v;
                sum += v * pa[k];
            }
            outC[c] = sum * ringLevel;
        }
        // noise
        float common = rng.next(), a = rng.next(), b = rng.next();
        float w = widthN;
        float n[2] = {(1 - w) * common + w * a, (1 - w) * common + w * b};
        float nGain = 1.f / std::sqrt((1 - w) * (1 - w) + w * w);
        float burst = 1.f + punch * 2.0f * std::exp(-t / (0.004f * fs));
        float click = punch * 1.2f * std::exp(-t / (0.0007f * fs));
        for (int c = 0; c < 2; ++c) {
            float lp, bp, hp, lp2, bp2, hp2;
            nf[c].tick(n[c] * nGain, lp, bp, hp);
            nl[c].tick(hp, lp2, bp2, hp2);
            float shaped = lp2;
            nb[c].tick(shaped, lp, bp, hp);
            float ns = (shaped * 0.8f + bp * 0.9f) * noiseLevel * burst;
            outC[c] += ns * 0.9f + click * n[c];
        }
        L += outC[0] * env * amp;
        R += outC[1] * env * amp;
        t += 1.f;
    }

private:
    float fs = 44100, t = 0, f0 = 800, amp = 1, bright = 1;
    bool active = false, choke = false;
    float chokeGain = 1;
    float ph[2][kPartials]{}, last[2][kPartials]{};
    float ratio[kPartials]{}, pa[kPartials]{};
    float stereoDetune = 1, fmIdx = 0, punch = 0, ringLevel = 1, noiseLevel = 0.5f, widthN = 0.5f;
    float holdS = 1000, relS = 100, holdTau = 1000;
    const SampleData* smp = nullptr;
    double sPos = 0, sInc = 1;
    float sGain = 1, sFadeIn = 1, sFadeAmt = 1;
    bool sRev = false, sShape = false;
    Svf nf[2], nl[2], nb[2];
    Rng rng;
};

// ---------------------------------------------------------------- engine
class SnareEngine {
public:
    void prepare(double sampleRate) {
        fs = (float)sampleRate;
        for (auto& v : voices) v.prepare(fs);
        for (int c = 0; c < 2; ++c) {
            combA[c].init((int)(fs / 200.f) + 4);
            combB[c].init((int)(fs / 200.f) + 4);
            dcHp[c].highpass(140.f, 0.7f, fs);
            eq[c].reset();
        }
        verb.prepare(fs);
        reset();
        setParams(params, true);
    }
    void reset() {
        for (int c = 0; c < 2; ++c) { combA[c].reset(); combB[c].reset(); dcHp[c].reset(); eq[c].reset(); crushHold[c] = 0; }
        verb.reset(); gateEnv = 1; gateT = 1e9f; crushCount = 0; rmPh = 0;
    }
    void setTempo(double bpm_) { if (bpm_ > 20 && bpm_ < 999) bpm = (float)bpm_; }

    void setParams(const Params& p, bool force = false) {
        bool sizeChanged = force || std::fabs(p.verbSize - params.verbSize) > 1e-6f;
        params = p;
        if (sizeChanged) verb.setSize(p.verbSize);
        float ef = p.eqLink ? curF0 : p.eqFreq;
        for (int c = 0; c < 2; ++c) eq[c].peaking(ef, std::clamp(p.eqQ, 0.3f, 16.f), p.eqGain, fs);
        for (auto& v : voices) if (v.isActive()) v.update(p);
        outGain = dbToGain(p.volume);
    }
    const Params& getParams() const { return params; }
    float getBpm() const { return bpm; }
    // audio thread: swap the sample (voices playing the old one stop it immediately)
    void setSample(const SampleData* s) {
        if (s == sample) return;
        sample = s;
        for (auto& v : voices) v.dropSample();
    }
    // current reverb gate length in ms (resolves tempo sync), >= 1999 = gate off
    static float gateLengthMs(const Params& p, float bpm) {
        if (p.verbSync > 0 && p.verbSync < 10) return kSyncDivs[p.verbSync] * 60000.f / bpm;
        return p.verbGate;
    }

    void noteOn(int note, float velocity) {
        float f0 = params.tune;
        if (params.keyTrack) f0 *= std::pow(2.f, (float)(note - 60) / 12.f);
        f0 = std::clamp(f0, 60.f, 8000.f);
        curF0 = f0;
        if (params.eqLink) for (int c = 0; c < 2; ++c) eq[c].peaking(f0, std::clamp(params.eqQ, 0.3f, 16.f), params.eqGain, fs);
        for (auto& v : voices) v.chokeOut();
        Voice* target = nullptr;
        for (auto& v : voices) if (!v.isActive()) { target = &v; break; }
        if (!target) target = &voices[(size_t)(nextVoice++ % voices.size())];
        const float keyRatio = params.keyTrack ? std::pow(2.f, (float)(note - 60) / 12.f) : 1.f;
        target->start(f0, std::clamp(velocity, 0.f, 1.f), params, seed, sample, keyRatio);
        seed = seed * 1664525u + 1013904223u;
        gateT = 0; // retrigger reverb gate
    }

    // stereo process, overwrites buffers
    void process(float* outL, float* outR, int n) {
        const Params& p = params;
        const float nz = std::clamp(p.noisy, 0.f, 1.f);
        const float combFb = 0.88f * nz;
        const float dA = fs / (curF0 * 0.37f), dB = fs / (curF0 * 0.61f);
        const float rmMix = 0.55f * nz * nz;
        const float rmInc = curF0 * 1.37f / fs;
        const float drive = 1.f + 14.f * std::pow(nz, 1.5f);
        const float driveNorm = 1.f / std::pow(drive, 0.75f);
        const float crushAmt = std::clamp((nz - 0.35f) / 0.65f, 0.f, 1.f);
        const float levels = std::pow(2.f, 15.f - 10.f * crushAmt);
        const int hold = 1 + (int)(5.f * crushAmt);
        // gate length
        float gateMs = p.verbGate;
        bool gateOn = p.verbGate < 1999.f;
        if (p.verbSync > 0 && p.verbSync < 10) { gateMs = kSyncDivs[p.verbSync] * 60000.f / bpm; gateOn = true; }
        const float gateS = gateMs * 0.001f * fs;
        const float cutS = (0.0008f + p.verbCut * p.verbCut * 0.2f) * fs;
        const float mix = std::clamp(p.verbMix, 0.f, 1.f);
        const float dryG = std::min(1.f, 2.f * (1.f - mix)), wetG = std::min(1.f, 2.f * mix) * 1.6f;
        const float side = std::clamp(p.width, 0.f, 1.f) * 2.f;

        for (int i = 0; i < n; ++i) {
            float L = 0, R = 0;
            for (auto& v : voices) v.tick(L, R);
            float x[2] = {L * kHeadroom, R * kHeadroom};
            // resonance EQ
            for (int c = 0; c < 2; ++c) x[c] = eq[c].tick(x[c]);
            // XTREME NOISY
            if (nz > 0.001f) {
                float rm = std::sin(kTwoPi * rmPh); rmPh += rmInc; rmPh -= std::floor(rmPh);
                bool takeCrush = (crushCount++ % (unsigned) hold) == 0;
                for (int c = 0; c < 2; ++c) {
                    float s = x[c];
                    float ca = combA[c].readFrac(dA), cb = combB[c].readFrac(dB);
                    combA[c].push(s + combFb * ca);
                    combB[c].push(s + combFb * 0.9f * cb);
                    s = s + nz * 0.6f * (ca + cb);
                    s = lerp(s, s * rm * 1.6f, rmMix);
                    s = std::tanh(s * drive) * driveNorm;
                    if (crushAmt > 0) {
                        if (takeCrush) crushHold[c] = std::round(s * levels) / levels;
                        s = lerp(s, crushHold[c], crushAmt);
                    }
                    x[c] = dcHp[c].tick(s);
                }
            }
            // reverb + gate
            float wl, wr;
            verb.tick(x[0], x[1], wl, wr);
            if (gateOn) {
                if (gateT < gateS) gateEnv = 1.f;
                else gateEnv = std::max(0.f, 1.f - (gateT - gateS) / cutS);
                gateT += 1.f;
            } else gateEnv = 1.f;
            float g = gateEnv * gateEnv * (3 - 2 * gateEnv);
            float oL = x[0] * dryG + wl * wetG * g;
            float oR = x[1] * dryG + wr * wetG * g;
            // width (M/S)
            float m = 0.5f * (oL + oR), s = 0.5f * (oL - oR) * side;
            oL = (m + s) * outGain; oR = (m - s) * outGain;
            outL[i] = softClip(oL); outR[i] = softClip(oR);
        }
    }

private:
    static inline float softClip(float x) {
        float a = std::fabs(x);
        if (a < 0.85f) return x;
        float y = 0.85f + 0.15f * std::tanh((a - 0.85f) / 0.15f);
        return x < 0 ? -y : y;
    }
    static constexpr float kHeadroom = 0.25f;
    float fs = 44100, bpm = 145, curF0 = 800, outGain = 1;
    Params params;
    std::array<Voice, 3> voices;
    const SampleData* sample = nullptr;
    unsigned nextVoice = 0;
    uint32_t seed = 12345u;
    Biquad eq[2], dcHp[2];
    DelayLine combA[2], combB[2];
    float crushHold[2]{};
    unsigned crushCount = 0;
    float rmPh = 0;
    FdnReverb verb;
    float gateEnv = 1, gateT = 1e9f;
};

} // namespace snarehead
