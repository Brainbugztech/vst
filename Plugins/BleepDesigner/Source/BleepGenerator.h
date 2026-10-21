#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>
#include <vector>

// Der Bleep-Generator (Version 2).
// Oszillator mit Form-Morph (Sinus -> Triangle -> Square -> Saw), dessen Lesephase von einem
// stummen Modulator verbogen wird (Phase Distortion / Phasen-FM). Dazu pro Bleep: Pitch-Env,
// Ratchet (schnelle Wiederholungen mit Tonschritt), Rauschen, resonanter Filter-Sweep, LFO,
// Amp-Env und leichte Saettigung. BleepCore laeuft live in den Voices und offline fuer die Slots.

namespace bleep
{

struct GenParams
{
    // Oszillator
    int   sqOct      = -1;      // Oktave relativ zur Note
    float sqGain     = 1.0f;    // linear
    float shape      = 2.0f;    // 0 Sinus, 1 Triangle, 2 Square, 3 Saw (stufenlos)
    float pw         = 0.5f;    // Pulsbreite der Square, 0.05 .. 0.95

    // Modulator (stumm)
    int   modOct     = -3;      // Oktave relativ zum Oszillator, -4 .. +3
    float modFine    = 0.0f;    // Halbtoene dazu, 0 .. 12 (krumme Werte = metallisch)
    float pd         = 0.3f;    // 0 .. 1, Phasenverschiebung bis zu einem ganzen Zyklus
    bool  pitchLink  = true;    // Modulator folgt Pitch-Env, Ratchet und LFO

    // Tonhoehe
    float penvAmt    = 24.0f;   // Halbtoene, + = faellt von oben
    float penvDecay  = 60.0f;   // ms
    int   ratchets   = 1;       // 1 .. 8 Wiederholungen
    float ratchetMs  = 70.0f;   // Abstand der Wiederholungen
    float ratchetStep = 0.0f;   // Halbtoene pro Wiederholung, -12 .. +12

    // LFO (Triangle)
    float lfoRate    = 4.0f;    // Hz
    float lfoPitch   = 0.0f;    // Halbtoene
    float lfoPd      = 0.0f;    // -1 .. 1

    // Rauschen und Filter (im Bleep eingebacken)
    float noise      = 0.0f;    // 0 .. 1, kurzer Rausch-Anteil am Anfang
    float fltCut     = 18000.0f;// Hz
    float fltRes     = 0.2f;    // 0 .. 1
    float fltEnv     = 0.0f;    // -1 .. 1, +-6 Oktaven
    float fltDecay   = 120.0f;  // ms
    float drive      = 0.2f;    // 0 .. 1

    // Chiptune
    int   chipArp    = 0;       // 0 aus, 1 Dur, 2 Moll, 3 Oktave, 4 Quinte+Oktave, 5 vermindert, 6 Oktav-Sprung runter
    float chipArpMs  = 30.0f;   // Tempo des Arpeggios innerhalb des Tons
    bool  pitchSteps = false;   // Tonhoehe springt in Halbtonstufen statt zu gleiten
    float pwmDepth   = 0.0f;    // 0 .. 0.45, LFO auf die Pulsbreite
    float crushBits  = 0.0f;    // 0 = aus, sonst 2 .. 12 Bit
    int   crushDown  = 1;       // Samplerate geteilt durch 1 .. 16

    // Ringmodulator und Zerhacker
    float ringRatio  = 0.0f;    // 0 = aus, sonst Frequenz relativ zum Ton (krumme Werte = klirrend)
    float ringMix    = 0.0f;    // 0 .. 1
    float gateRate   = 0.0f;    // Hz, 0 = aus
    float gateDepth  = 0.0f;    // 0 .. 1

    // Nur beim Einfrieren in den Slot
    float echoMs     = 0.0f;    // 0 = aus, sonst einzelner Delay-Throw
    float echoFb     = 0.0f;    // 0 .. 0.85
    float stretch    = 0.0f;    // 0 .. 1, Ton verzieht sich und wird zum Ende langsamer und tiefer

    // Lautstaerke
    float attack     = 1.0f;    // ms
    float decay      = 180.0f;  // ms
    float sustain    = 0.0f;    // 0 .. 1
    float release    = 40.0f;   // ms
};

// Klassisches PolyBLEP gegen Aliasing an den Flanken
inline double polyBlep (double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

inline float triangle (double ph)
{
    return (float) (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph);
}

class BleepCore
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        adsr.setSampleRate (sampleRate);
    }

    void start (double noteHz, float velocity, const GenParams& p)
    {
        baseHz = noteHz;
        vel = velocity;
        phOsc = phMod = phLfo = 0.0;
        prevOffset = 0.0;
        ic1 = ic2 = 0.0f;
        repeat = 0;
        sinceRepeat = 0;
        released = false;
        phRing = phGate = 0.0;
        gateSm = 1.0f;
        sinceStart = 0;
        crushCount = 0;
        crushHold = 0.0f;
        noiseSeed = 0x2545F491u ^ (uint32_t) (noteHz * 1000.0);
        update (p);
        retrigger();
        adsr.reset();
        adsr.noteOn();
    }

    void release()          { released = true; adsr.noteOff(); }
    void kill()             { adsr.reset(); }
    bool isActive() const   { return adsr.isActive(); }

    // Einmal pro Block aufrufen, damit Regler-Aenderungen auch bei laufenden Noten greifen
    void update (const GenParams& p)
    {
        adsr.setParameters ({ p.attack * 0.001f, p.decay * 0.001f, p.sustain, p.release * 0.001f });
        penvCoef  = std::exp (-1.0 / (std::max (1.0, (double) p.penvDecay) * 0.001 * sr));
        fenvCoef  = (float) std::exp (-1.0 / (std::max (1.0, (double) p.fltDecay) * 0.001 * sr));
        noiseCoef = (float) std::exp (-1.0 / (0.025 * sr));
        repeatLen = juce::jmax (16, (int) (juce::jlimit (15.0f, 500.0f, p.ratchetMs) * 0.001 * sr));
    }

    float next (const GenParams& p)
    {
        // Ratchet: Bleep innerhalb der Note mehrfach neu anstossen, jeweils einen Tonschritt weiter
        if (! released && repeat < juce::jlimit (1, 8, p.ratchets) - 1 && ++sinceRepeat >= repeatLen)
        {
            ++repeat;
            sinceRepeat = 0;
            retrigger();
            adsr.noteOn();
        }

        // LFO, startet mit jeder Note neu
        const float lfo = triangle (phLfo);
        phLfo += p.lfoRate / sr;
        phLfo -= std::floor (phLfo);

        // Tonhoehe
        const double stepSemis = (double) p.ratchetStep * repeat;
        double semis = p.penvAmt * penv + p.lfoPitch * lfo + stepSemis;
        penv *= penvCoef;

        // Chip-Arpeggio: springt im Takt von chipArpMs durch ein Akkordmuster
        if (p.chipArp > 0)
        {
            static constexpr int patterns[6][4] = { { 0, 4, 7, 4 }, { 0, 3, 7, 3 }, { 0, 12, 0, 12 },
                                                    { 0, 7, 12, 7 }, { 0, 3, 6, 9 }, { 12, 0, -12, 0 } };
            const int len = juce::jmax (8, (int) (juce::jlimit (8.0f, 120.0f, p.chipArpMs) * 0.001 * sr));
            semis += patterns[juce::jlimit (1, 6, p.chipArp) - 1][(sinceStart / len) % 4];
        }
        if (p.pitchSteps)
            semis = std::round (semis);
        ++sinceStart;

        const double f0   = baseHz * std::exp2 ((double) p.sqOct);
        const double fOsc = f0 * std::exp2 (semis / 12.0);
        const double fMod = (p.pitchLink ? fOsc : f0 * std::exp2 (stepSemis / 12.0))
                          * std::exp2 ((double) p.modOct + (double) p.modFine / 12.0);
        const double dt   = juce::jlimit (1.0e-6, 0.45, fOsc / sr);

        // Stummer Modulator (Sinus) verbiegt die Lesephase
        const float mod = (float) std::sin (phMod * juce::MathConstants<double>::twoPi);
        phMod += fMod / sr;
        phMod -= std::floor (phMod);

        const double pd = juce::jlimit (0.0, 1.0, (double) p.pd + (double) p.lfoPd * lfo);
        const double offset = pd * pd * 1.0 * mod;   // quadratisch, damit kleine Werte fein bleiben
        double ph = phOsc + offset;
        ph -= std::floor (ph);
        const double dtEff = juce::jlimit (1.0e-6, 0.45, std::abs (dt + offset - prevOffset));
        prevOffset = offset;

        const double pwNow = juce::jlimit (0.05, 0.95, (double) p.pw + (double) p.pwmDepth * lfo);
        float osc = waveAt (ph, dtEff, juce::jlimit (0.0f, 3.0f, p.shape), pwNow);

        // Ringmodulator: Ton mal Sinus mit krummem Verhaeltnis
        if (p.ringRatio > 0.0f && p.ringMix > 0.0f)
        {
            const float rm = (float) std::sin (phRing * juce::MathConstants<double>::twoPi);
            phRing += fOsc * (double) p.ringRatio / sr;
            phRing -= std::floor (phRing);
            osc += (osc * rm * 1.3f - osc) * juce::jlimit (0.0f, 1.0f, p.ringMix);
        }
        phOsc += dt;
        phOsc -= std::floor (phOsc);

        // Rauschen: kurzer Anteil am Anfang jedes Anschlags
        noiseSeed = noiseSeed * 1664525u + 1013904223u;
        const float white = (float) (noiseSeed >> 8) * (2.0f / 16777216.0f) - 1.0f;
        float x = osc + white * p.noise * noiseEnv * 1.2f;
        noiseEnv *= noiseCoef;

        // Resonanter Filter-Sweep (TPT-SVF, Tiefpass), folgt der Tonhoehe zur Haelfte
        if (p.fltCut < 17900.0f || p.fltEnv != 0.0f)
        {
            const float oct = p.fltEnv * 6.0f * fenv + 0.5f * (float) (semis / 12.0);
            const float fc = juce::jlimit (30.0f, (float) (sr * 0.45), p.fltCut * std::exp2 (oct));
            const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
            const float k = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, p.fltRes);
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            x = v2;
        }
        fenv *= fenvCoef;

        // Leichte Saettigung
        const float dr = 1.0f + 4.0f * juce::jlimit (0.0f, 1.0f, p.drive);
        x = std::tanh (x * dr) / std::tanh (dr);

        // Bitcrusher im Ton (Chiptune-Koernung)
        if (p.crushBits >= 1.0f)
        {
            if (crushCount <= 0)
            {
                const float levels = std::exp2 (juce::jlimit (1.0f, 16.0f, p.crushBits) - 1.0f);
                crushHold = std::round (x * levels) / levels;
                crushCount = juce::jlimit (1, 16, p.crushDown);
            }
            --crushCount;
            x = crushHold;
        }

        // Zerhacker: Lautstaerke flackert im Takt von gateRate
        if (p.gateRate > 0.0f && p.gateDepth > 0.0f)
        {
            const float target = phGate < 0.5 ? 1.0f : 1.0f - juce::jlimit (0.0f, 1.0f, p.gateDepth);
            phGate += p.gateRate / sr;
            phGate -= std::floor (phGate);
            gateSm += (target - gateSm) * 0.08f;
            x *= gateSm;
        }

        return x * p.sqGain * vel * adsr.getNextSample();
    }

private:
    void retrigger()
    {
        penv = 1.0;
        fenv = 1.0f;
        noiseEnv = 1.0f;
    }

    static float waveAt (double ph, double dt, float shape, double pw)
    {
        const int seg = juce::jmin (2, (int) shape);
        const float t = shape - (float) seg;
        const float a = basic (seg, ph, dt, pw);
        if (t < 0.001f)
            return a;
        return a + (basic (seg + 1, ph, dt, pw) - a) * t;
    }

    static float basic (int kind, double ph, double dt, double pw)
    {
        switch (kind)
        {
            case 0: return (float) std::sin (ph * juce::MathConstants<double>::twoPi);
            case 1: return triangle (ph);
            case 2:
            {
                double v = ph < pw ? 1.0 : -1.0;
                v += polyBlep (ph, dt);
                double p2 = ph - pw;
                if (p2 < 0.0) p2 += 1.0;
                v -= polyBlep (p2, dt);
                return (float) v * 0.8f;
            }
            default:
                return (float) ((2.0 * ph - 1.0) - polyBlep (ph, dt)) * 0.8f;
        }
    }

    double sr = 48000.0, baseHz = 440.0;
    double phOsc = 0.0, phMod = 0.0, phLfo = 0.0;
    double penv = 1.0, penvCoef = 0.999, prevOffset = 0.0;
    float fenv = 1.0f, fenvCoef = 0.999f, noiseEnv = 1.0f, noiseCoef = 0.999f;
    float ic1 = 0.0f, ic2 = 0.0f;
    float vel = 1.0f;
    int repeat = 0, sinceRepeat = 0, repeatLen = 3000;
    double phRing = 0.0, phGate = 0.0;
    float gateSm = 1.0f, crushHold = 0.0f;
    int sinceStart = 0, crushCount = 0;
    bool released = false;
    uint32_t noiseSeed = 1;
    juce::ADSR adsr;
};

// Rendert einen Bleep offline in einen Mono-Puffer (fuer die Slots).
// maxLengthMs ist die Obergrenze, die tatsaechliche Laenge ergibt sich aus dem Ton:
// Stille am Ende wird abgeschnitten. Danach optional Stretch und Delay-Throw.
inline juce::AudioBuffer<float> renderBleep (const GenParams& p, double sr, int rootNote, double maxLengthMs)
{
    const int maxN = juce::jmax (64, (int) std::round (maxLengthMs * 0.001 * sr));
    const double toneMs = (double) p.attack + (double) p.decay * (p.sustain > 0.01f ? 1.5 : 1.0)
                        + (double) juce::jmax (0, p.ratchets - 1) * p.ratchetMs + (p.sustain > 0.01f ? 200.0 : 0.0);
    const int toneN = juce::jlimit (64, maxN, (int) ((toneMs + (double) p.release) * 0.001 * sr * 1.3));

    std::vector<float> d ((size_t) toneN, 0.0f);
    BleepCore core;
    core.prepare (sr);
    core.start (juce::MidiMessage::getMidiNoteInHertz (rootNote), 1.0f, p);
    const int releaseAt = juce::jmax (0, toneN - (int) std::round (p.release * 0.001 * sr));
    for (int i = 0; i < toneN; ++i)
    {
        if (i == releaseAt)
            core.release();
        d[(size_t) i] = core.next (p);
    }

    // Stretch: der Ton wird zum Ende hin langsamer abgespielt, also laenger und tiefer
    if (p.stretch > 0.01f)
    {
        const double endSpeed = 1.0 - 0.8 * juce::jlimit (0.0, 1.0, (double) p.stretch);
        std::vector<float> st;
        st.reserve ((size_t) maxN);
        double pos = 0.0;
        while (pos < (double) (toneN - 2) && (int) st.size() < maxN)
        {
            const int i0 = (int) pos;
            const float fr = (float) (pos - i0);
            st.push_back (d[(size_t) i0] + (d[(size_t) i0 + 1] - d[(size_t) i0]) * fr);
            const double t = pos / (double) toneN;
            pos += 1.0 + (endSpeed - 1.0) * t;
        }
        d.swap (st);
    }

    // Delay-Throw: einzelnes, abfallendes Echo, auf 3 kHz gedaempft
    if (p.echoMs > 1.0f && p.echoFb > 0.01f)
    {
        const int del = juce::jmax (1, (int) (p.echoMs * 0.001 * sr));
        const float fb = juce::jlimit (0.0f, 0.85f, p.echoFb);
        const size_t outN = (size_t) juce::jmin (maxN * 2, (int) d.size() + del * 6);
        d.resize (outN, 0.0f);
        const float lpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3000.0f / (float) sr);
        float lp = 0.0f;
        for (size_t i = (size_t) del; i < outN; ++i)
        {
            lp += lpA * (d[i - (size_t) del] - lp);
            d[i] += fb * lp;
        }
    }

    // Stille am Ende abschneiden, dann 2 ms ausblenden
    int n = (int) d.size();
    while (n > 64 && std::abs (d[(size_t) n - 1]) < 1.0e-4f)
        --n;
    n = juce::jmin ((int) d.size(), n + (int) (0.002 * sr));
    juce::AudioBuffer<float> out (1, n);
    std::copy (d.begin(), d.begin() + n, out.getWritePointer (0));
    const int fade = juce::jmin (n, (int) (0.002 * sr));
    auto* o = out.getWritePointer (0);
    for (int i = 0; i < fade; ++i)
        o[n - 1 - i] *= (float) i / (float) fade;
    return out;
}

} // namespace bleep
