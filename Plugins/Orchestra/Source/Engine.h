#pragma once

// Klangengine von Brainbugz Orchestra: Sample-Stimmen, Air-Synth fuer Pads, FX-Kette

#include <juce_dsp/juce_dsp.h>
#include "SampleBank.h"
#include <cmath>

namespace orch
{

enum Mode { M_STRINGS = 0, M_CELLOS, M_ATMOS, M_DRUMS };
enum Artic { A_SUS = 0, A_SPIC, A_PIZZ, A_TREM };

struct Settings
{
    int mode = M_STRINGS, artic = A_SUS;
    float attack = 0.05f, release = 0.4f;   // Sekunden
    float tone = 0.0f;                      // -1 dunkel .. +1 hell
    float tune = 0.0f;                      // Halbtoene
    float reverb = 0.25f, space = 0.6f, width = 1.0f, volumeDb = 0.0f;
    float ensemble = 0.3f, humanize = 0.2f;
    float air = 0.35f, shimmer = 0.25f, motion = 0.4f;
    float punch = 0.3f, decay = 1.0f;
};

// ---------------------------------------------------------------------------------------------
struct SVF   // TPT State Variable Filter (Lowpass)
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;
    void set (float cutoff, float sr, float q = 0.7071f)
    {
        g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, sr * 0.45f, cutoff) / sr);
        k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    float lp (float v0)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; return v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

// ---------------------------------------------------------------------------------------------
struct Voice
{
    bool active = false;
    int note = 0, group = -1;          // Note fuer Note-Off, Gruppe fuer Sicherheitscheck
    const GroupData* data = nullptr;
    const Sample* smp = nullptr;
    bool isSynth = false;              // Air-Layer der Pads
    bool oneShot = false;
    bool released = false, sustained = false;
    double pos = 0, inc = 1;
    float baseRatio = 1;               // ohne Pitchbend
    float gain = 1, panL = 1, panR = 1;
    float env = 0, attackInc = 1, releaseMul = 0.999f;
    float decayMul = 1, drumEnv = 1;
    int delay = 0, age = 0;
    juce::uint32 order = 0;
    // Air-Synth
    float ph[3] {}, frq = 0; SVF filt; juce::uint32 rng = 12345; float driftPh = 0;

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
        releaseMul = std::exp (-6.9f / (std::max (0.01f, relSeconds) * sr)); // -60 dB in relSeconds
    }

    void kill (float sr) { release (0.012f, sr); }

    // addiert in L/R; gibt false zurueck, wenn die Stimme fertig ist
    void render (float* L, float* R, int n, float sr, float bend, float airCut)
    {
        for (int i = 0; i < n; ++i)
        {
            if (delay > 0) { --delay; continue; }

            if (! released) env = std::min (1.0f, env + attackInc);
            else { env *= releaseMul; if (env < 0.0001f) { active = false; return; } }
            if (oneShot && age > (int) (0.04f * sr)) drumEnv *= decayMul;
            ++age;

            float l, r;
            if (isSynth)
            {
                // drei leicht verstimmte Saegezaehne + Hauch Rauschen, weich gefiltert
                const float drift = 1.0f + 0.0015f * std::sin (driftPh);
                driftPh += 0.6f / sr * juce::MathConstants<float>::twoPi;
                float s = 0;
                static constexpr float det[3] { 1.0f, 1.0058f, 0.9942f };
                for (int k = 0; k < 3; ++k)
                {
                    const float dt = frq * det[k] * drift * bend / sr;
                    float p = ph[k] + dt; if (p >= 1.0f) p -= 1.0f; ph[k] = p;
                    float saw = 2.0f * p - 1.0f;
                    // PolyBLEP gegen Aliasing
                    if (p < dt) { const float t = p / dt; saw -= t + t - t * t - 1.0f; }
                    else if (p > 1.0f - dt) { const float t = (p - 1.0f) / dt; saw -= t * t + t + t + 1.0f; }
                    s += saw;
                }
                rng = rng * 1664525u + 1013904223u;
                const float noise = ((float) (rng >> 9) / 4194304.0f - 1.0f) * 0.25f;
                const float y = filt.lp (s * 0.3f + noise);
                l = y * (1.0f + 0.2f * std::sin (ph[1] * 6.28f)); r = y * (1.0f + 0.2f * std::sin (ph[2] * 6.28f));
                juce::ignoreUnused (airCut);
            }
            else
            {
                const bool looping = smp->loops() && ! oneShot;
                if (looping) while (pos >= smp->loopEnd) pos -= (smp->loopEnd - smp->loopStart);
                const int ip = (int) pos; const float fr = (float) (pos - ip);
                if (! looping && ip >= smp->length - 1) { active = false; return; }
                l = hermite (smp->l.data(), ip, fr);
                r = hermite (smp->r.data(), ip, fr);
                pos += inc * bend;
            }
            const float a = env * env * gain * drumEnv;
            L[i] += l * a * panL;
            R[i] += r * a * panR;
        }
    }
};

// ---------------------------------------------------------------------------------------------
// 8-Kanal-FDN-Hall mit Daempfung und Predelay: grosser, weicher Filmhall
class FDNReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : lines) d.assign ((size_t) (sr * 0.25f) + 8, 0.0f);
        pre.assign ((size_t) (sr * 0.1f) + 8, 0.0f);
        for (auto& f : damp) f = 0;
        hpL = hpR = 0;
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
            // Predelay + Hochpass (Hall soll nicht matschen)
            float in = (inL[i] + inR[i]) * 0.5f;
            pre[(size_t) prePos] = in;
            int rp = prePos - preLen; if (rp < 0) rp += (int) pre.size();
            float x = pre[(size_t) rp];
            if (++prePos >= (int) pre.size()) prePos = 0;
            hpL = hpC * hpL + (1 - hpC) * x; x -= hpL;

            float o[8];
            for (int k = 0; k < 8; ++k)
            {
                int p = pos[k] - len[k]; if (p < 0) p += (int) lines[k].size();
                float v = lines[k][(size_t) p];
                damp[k] += dampC * (v - damp[k]);       // Hoehen im Nachhall abdaempfen
                o[k] = damp[k] * fb[k];
            }
            // Hadamard-Mix (8x8, normiert)
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
    float sr = 44100, size = -1, hpL = 0, hpR = 0;
    std::vector<float> lines[8], pre;
    int len[8] {}, pos[8] {}, prePos = 0, preLen = 1;
    float fb[8] {}, damp[8] {};
};

// ---------------------------------------------------------------------------------------------
class Engine
{
public:
    static constexpr int maxVoices = 128;

    explicit Engine (SampleBank& b) : bank (b) {}

    void prepare (double sampleRate, int maxBlock)
    {
        sr = (float) sampleRate;
        dry.setSize (2, maxBlock); wet.setSize (2, maxBlock);
        reverb.prepare (sampleRate);
        for (auto& v : voices) v.active = false;
        for (auto& f : toneF) f.reset();
        chorus.assign ((size_t) (sr * 0.05f) + 4, 0.0f); chorusR.assign (chorus.size(), 0.0f);
    }

    // Gruppen, die fuer Modus+Artikulation gebraucht werden
    static std::set<int> groupsFor (int mode, int artic)
    {
        static const int vln[4] { G_VLN_SUS, G_VLN_SPIC, G_VLN_PIZZ, G_VLN_TREM };
        static const int vla[4] { G_VLA_SUS, G_VLA_SPIC, G_VLA_PIZZ, G_VLA_TREM };
        static const int vc[4]  { G_VC_SUS,  G_VC_SPIC,  G_VC_PIZZ,  G_VC_TREM };
        artic = juce::jlimit (0, 3, artic);
        switch (mode)
        {
            case M_STRINGS: return { vln[artic], vla[artic], vc[artic] };
            case M_CELLOS:  return { vc[artic] };
            case M_ATMOS:   return { G_VLN_SUS, G_VLA_SUS, G_VC_SUS };
            default:
            {
                std::set<int> s;
                for (auto& k : kitPieces) s.insert (G_DRUM36 + (k.note - 36));
                return s;
            }
        }
    }

    void setSettings (const Settings& s)
    {
        if (s.mode != cur.mode || (s.artic != cur.artic && s.mode != M_ATMOS && s.mode != M_DRUMS))
            for (auto& v : voices) if (v.active) v.kill (sr);
        cur = s;
    }

    void allNotesOff() { for (auto& v : voices) if (v.active) v.release (cur.release, sr); }

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

    void render (juce::AudioBuffer<float>& out, int start, int n)
    {
        auto* dl = dry.getWritePointer (0); auto* dr = dry.getWritePointer (1);
        juce::FloatVectorOperations::clear (dl, n); juce::FloatVectorOperations::clear (dr, n);

        const float bend = std::pow (2.0f, bendSemis / 12.0f);
        for (auto& v : voices)
        {
            if (! v.active) continue;
            // Sicherheitscheck: Gruppe wurde inzwischen freigegeben -> Stimme sofort aus
            if (! v.isSynth && bank.get (v.group) != v.data) { v.active = false; continue; }
            v.render (dl, dr, n, sr, bend, 0.0f);
        }

        // Ausdruck (Modwheel/CC11) weich nachfuehren
        const float exprTarget = (0.2f + 0.8f * modWheel) * (0.15f + 0.85f * expression);
        const bool drums = cur.mode == M_DRUMS;

        // Ton: Tiefpass, im Atmos-Modus langsam moduliert (Motion)
        const float lfoRate = 0.05f + 0.25f * cur.motion;
        // Tone: -1 = 700 Hz (dunkel), 0 = 9 kHz, +1 = 20 kHz (offen)
        float baseCut = cur.tone < 0.0f ? 9000.0f * std::pow (700.0f / 9000.0f, -cur.tone)
                                        : 9000.0f * std::pow (20000.0f / 9000.0f, cur.tone);
        for (int i = 0; i < n; ++i)
        {
            exprSm += (drums ? 1.0f - exprSm : exprTarget - exprSm) * 0.0008f;
            float cut = baseCut;
            if (cur.mode == M_ATMOS)
            {
                lfo += lfoRate / sr; if (lfo >= 1.0f) lfo -= 1.0f;
                cut *= std::pow (2.0f, cur.motion * 1.3f * std::sin (lfo * juce::MathConstants<float>::twoPi));
            }
            if ((i & 15) == 0) { toneF[0].set (std::min (cut * (0.55f + 0.45f * exprSm), 20000.0f), sr); toneF[1] = copyCoef (toneF[1], toneF[0]); }
            float l = dl[i] * exprSm, r = dr[i] * exprSm;
            if (cut < 19000.0f || cur.mode == M_ATMOS) { l = toneF[0].lp (l); r = toneF[1].lp (r); }
            dl[i] = l; dr[i] = r;
        }

        // Pads: breiter Chorus fuer Bewegung
        if (cur.mode == M_ATMOS) applyChorus (dl, dr, n, 0.35f + 0.5f * cur.motion);

        // Drums: Punch = Transienten + Saettigung
        if (drums && cur.punch > 0.001f) applyPunch (dl, dr, n);

        // Stereobreite (Mid/Side)
        for (int i = 0; i < n; ++i)
        {
            const float m = (dl[i] + dr[i]) * 0.5f, s = (dl[i] - dr[i]) * 0.5f * cur.width;
            dl[i] = m + s; dr[i] = m - s;
        }

        // Hall
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
            l = softClip (l); r = softClip (r);
            ol[i] += l; if (orr) orr[i] += r;
            pk = std::max (pk, std::max (std::abs (l), std::abs (r)));
        }
        level = std::max (pk, level * 0.9f);
    }

    std::atomic<int> lastHitNote { -1 };
    std::atomic<juce::uint32> lastHitTime { 0 };
    float level = 0;   // fuer die GUI-Animation (grob, nicht atomar noetig)

private:
    static SVF copyCoef (SVF dst, const SVF& src) { dst.g = src.g; dst.k = src.k; dst.a1 = src.a1; dst.a2 = src.a2; dst.a3 = src.a3; return dst; }
    static float softClip (float x) { return std::abs (x) > 0.9f ? std::tanh (x) : x; }

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
        // stehlen: aelteste losgelassene, sonst aelteste
        for (auto& v : voices) if (v.released && (best == nullptr || v.order < best->order)) best = &v;
        if (best == nullptr) for (auto& v : voices) if (best == nullptr || v.order < best->order) best = &v;
        return best;
    }

    float rnd() { rngState = rngState * 1664525u + 1013904223u; return (float) (rngState >> 8) / 16777216.0f; }

    void startSample (int group, int note, int playNote, float vel, float gain, float pan, int delaySamples,
                      float detuneCents, bool oneShot, float attackSec)
    {
        auto* gd = bank.get (group);
        if (gd == nullptr || gd->samples.empty()) return;
        const int rr = rrCounter[(size_t) group]++;
        auto* s = gd->pick (playNote, vel, rr);
        if (s == nullptr) return;
        auto* v = freeVoice();
        *v = Voice();
        v->active = true; v->note = note; v->group = group; v->data = gd; v->smp = s;
        v->oneShot = oneShot; v->order = ++orderCounter;
        const float semis = (float) (playNote - s->root) + detuneCents / 100.0f;
        v->inc = std::pow (2.0, semis / 12.0) * 44100.0 / sr;
        v->gain = gain;
        const float p = juce::jlimit (-1.0f, 1.0f, pan);
        v->panL = std::sqrt (0.5f * (1.0f - p)) * 1.4142f; v->panR = std::sqrt (0.5f * (1.0f + p)) * 1.4142f;
        v->attackInc = 1.0f / std::max (1.0f, attackSec * sr);
        if (oneShot) { v->env = 1.0f; v->attackInc = 1.0f; }
        v->delay = delaySamples;
        if (oneShot && cur.decay < 0.999f)
        {
            const float tau = 0.08f + 3.0f * cur.decay * cur.decay;
            v->decayMul = std::exp (-1.0f / (tau * sr));
        }
    }

    void startAir (int note, float vel)
    {
        auto* v = freeVoice();
        *v = Voice();
        v->active = true; v->note = note; v->isSynth = true; v->order = ++orderCounter;
        v->frq = 440.0f * std::pow (2.0f, ((float) note + cur.tune - 69.0f) / 12.0f);
        v->filt.set (std::min (v->frq * 3.0f + 600.0f, 7000.0f), sr, 0.9f);
        for (auto& p : v->ph) p = rnd();
        v->driftPh = rnd() * 6.28f;
        v->rng = (juce::uint32) (rnd() * 1e9f) + 1;
        v->gain = cur.air * 0.16f * (0.5f + 0.5f * vel);
        v->attackInc = 1.0f / std::max (1.0f, (cur.attack + 0.9f) * sr);
    }

    void noteOn (int note, float vel)
    {
        const float vg = 0.35f + 0.65f * std::pow (vel, 1.3f);
        const int t = (int) std::round (cur.tune);
        const float tuneFrac = (cur.tune - (float) t) * 100.0f;
        const int pn = note + t;

        if (cur.mode == M_DRUMS)
        {
            // Tune verstimmt die Drums, gespielt wird die Taste selbst
            if (note < 36 || note > 63) return;
            const int g = G_DRUM36 + (note - 36);
            // max. 4 Stimmen pro Drum-Taste
            int cnt = 0; Voice* oldest = nullptr;
            for (auto& v : voices) if (v.active && v.note == note) { ++cnt; if (! oldest || v.order < oldest->order) oldest = &v; }
            if (cnt >= 4 && oldest) oldest->kill (sr);
            auto* gd = bank.get (g);
            if (gd == nullptr || gd->samples.empty()) return;
            const int root = gd->samples.front().root;
            startSample (g, note, root, vel, vg * 1.1f, 0.0f, 0, cur.tune * 100.0f, true, 0.0f);
            lastHitNote = note; lastHitTime = juce::Time::getMillisecondCounter();
            return;
        }

        // gehaltene gleiche Note neu anschlagen
        for (auto& v : voices) if (v.active && v.note == note && ! v.released) v.release (0.08f, sr);

        const bool shortArt = cur.mode != M_ATMOS && (cur.artic == A_SPIC || cur.artic == A_PIZZ);
        const float att = cur.mode == M_ATMOS ? cur.attack + 0.35f : (shortArt ? 0.002f : cur.attack);
        const int a = cur.mode == M_ATMOS ? A_SUS : cur.artic;
        static const int vln[4] { G_VLN_SUS, G_VLN_SPIC, G_VLN_PIZZ, G_VLN_TREM };
        static const int vla[4] { G_VLA_SUS, G_VLA_SPIC, G_VLA_PIZZ, G_VLA_TREM };
        static const int vc[4]  { G_VC_SUS,  G_VC_SPIC,  G_VC_PIZZ,  G_VC_TREM };

        auto play = [&] (int group, int playNote, float gain, float pan)
        {
            // Humanize: kleine Zufallsabweichungen in Timing, Stimmung und Lautstaerke
            const float hz = cur.mode == M_ATMOS ? 0.0f : cur.humanize;
            const int dly = (int) (rnd() * hz * 0.025f * sr);
            const float det = (rnd() * 2.0f - 1.0f) * hz * 6.0f + tuneFrac;
            const float g = gain * (1.0f - hz * 0.25f * rnd());
            startSample (group, note, playNote, vel, g, pan, dly, det, false, att);
            // Ensemble: zweite, leicht verstimmte und verzoegerte Stimme fuer mehr Breite
            const float ens = cur.mode == M_ATMOS ? 0.6f : cur.ensemble;
            if (ens > 0.02f && ! shortArt)
            {
                const float d2 = (rnd() > 0.5f ? 1.0f : -1.0f) * (4.0f + 8.0f * ens) + tuneFrac;
                startSample (group, note, playNote, vel, g * ens * 0.7f, -pan * 0.8f + (rnd() - 0.5f) * ens,
                             dly + (int) ((0.012f + 0.02f * ens) * sr), d2, false, att * 1.3f);
            }
        };

        if (cur.mode == M_CELLOS)
            play (vc[a], pn, vg, 0.0f);
        else if (cur.mode == M_STRINGS)
        {
            if (pn < 48) play (vc[a], pn, vg, 0.15f);
            else if (pn < 55) play (vla[a], pn, vg, 0.05f);
            else if (pn < 68) { play (vln[a], pn, vg * 0.78f, -0.25f); play (vla[a], pn, vg * 0.62f, 0.25f); }
            else play (vln[a], pn, vg, -0.1f);
        }
        else // Atmos
        {
            const float pv = vg * 1.5f; // Pads sind durch Attack/Filter leiser, daher angehoben
            if (pn < 50) play (G_VC_SUS, pn, pv, 0.0f);
            else if (pn < 62) { play (G_VLA_SUS, pn, pv * 0.8f, 0.3f); play (G_VC_SUS, pn, pv * 0.5f, -0.3f); }
            else { play (G_VLN_SUS, pn, pv * 0.8f, -0.3f); play (G_VLA_SUS, pn, pv * 0.6f, 0.3f); }
            if (cur.shimmer > 0.01f) // Oktave darueber, leise und breit
                play (G_VLN_SUS, std::min (pn + 12, 98), pv * cur.shimmer * 0.55f, (rnd() - 0.5f) * 1.6f);
            if (cur.air > 0.01f) startAir (note, vel);
        }
    }

    void noteOff (int note)
    {
        if (cur.mode == M_DRUMS) return; // One-Shots klingen aus
        for (auto& v : voices)
            if (v.active && v.note == note && ! v.released)
            {
                if (pedal) v.sustained = true;
                else v.release (relFor(), sr);
            }
    }

    void applyChorus (float* l, float* r, int n, float depth)
    {
        const int sz = (int) chorus.size();
        for (int i = 0; i < n; ++i)
        {
            chorus[(size_t) chPos] = l[i]; chorusR[(size_t) chPos] = r[i];
            chLfo += 0.21f / sr; if (chLfo >= 1.0f) chLfo -= 1.0f;
            chLfo2 += 0.273f / sr; if (chLfo2 >= 1.0f) chLfo2 -= 1.0f;   // eigene Phase, sonst Sprung beim Umlauf
            const float m1 = std::sin (chLfo * juce::MathConstants<float>::twoPi);
            const float m2 = std::cos (chLfo2 * juce::MathConstants<float>::twoPi);
            auto tap = [&] (std::vector<float>& buf, float ms)
            {
                float d = ms * 0.001f * sr; float rp = (float) chPos - d; if (rp < 0) rp += (float) sz;
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
    float sr = 44100;
    std::array<Voice, maxVoices> voices;
    std::array<int, (size_t) G_NUM> rrCounter {};
    juce::uint32 orderCounter = 0, rngState = 22222;
    float modWheel = 1.0f, expression = 1.0f, exprSm = 1.0f, bendSemis = 0;
    bool pedal = false;
    float lfo = 0, chLfo = 0, chLfo2 = 0.25f, envF = 0, envS = 0;
    int chPos = 0;
    std::vector<float> chorus, chorusR;
    SVF toneF[2];
    FDNReverb reverb;
    juce::AudioBuffer<float> dry, wet;
};

} // namespace orch
