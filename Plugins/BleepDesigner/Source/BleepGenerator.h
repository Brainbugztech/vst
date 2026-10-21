#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

// Etappe 2: Der Bleep-Generator.
// Hoerbar ist nur die Square. Eine stumme Triangle (Standard -3 Oktaven) verschiebt
// die Lesephase der Square (Phase Distortion), PD-Amount bestimmt wie stark.
// BleepCore wird jetzt live von den Voices benutzt und spaeter offline zum Rendern der Slots.

namespace bleep
{

struct GenParams
{
    int   sqOct      = -1;      // Oktave der Square relativ zur Note
    float sqGain     = 1.0f;    // linear
    float pw         = 0.5f;    // 0.05 .. 0.95
    int   modOct     = -3;      // Oktave der Triangle relativ zur Square
    float pd         = 0.3f;    // 0 .. 1
    bool  pitchLink  = true;    // Triangle folgt Pitch-Env und LFO
    float penvAmt    = 24.0f;   // Halbtoene
    float penvDecay  = 60.0f;   // ms
    float lfoRate    = 4.0f;    // Hz
    float lfoPitch   = 0.0f;    // Halbtoene
    float lfoPd      = 0.0f;    // -1 .. 1
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
        phSq = phMod = phLfo = 0.0;
        penv = 1.0;
        prevOffset = 0.0;
        update (p);
        adsr.reset();
        adsr.noteOn();
    }

    void release()          { adsr.noteOff(); }
    void kill()             { adsr.reset(); }
    bool isActive() const   { return adsr.isActive(); }

    // Einmal pro Block aufrufen, damit Regler-Aenderungen auch bei laufenden Noten greifen
    void update (const GenParams& p)
    {
        adsr.setParameters ({ p.attack * 0.001f, p.decay * 0.001f, p.sustain, p.release * 0.001f });
        penvCoef = std::exp (-1.0 / (std::max (1.0, (double) p.penvDecay) * 0.001 * sr));
    }

    float next (const GenParams& p)
    {
        // LFO (Triangle), startet mit jeder Note neu
        const float lfo = triangle (phLfo);
        phLfo += p.lfoRate / sr;
        phLfo -= std::floor (phLfo);

        // Tonhoehe: Pitch-Env plus LFO
        const double semis = p.penvAmt * penv + p.lfoPitch * lfo;
        penv *= penvCoef;

        const double f0   = baseHz * std::exp2 ((double) p.sqOct);
        const double fSq  = f0 * std::exp2 (semis / 12.0);
        const double fMod = (p.pitchLink ? fSq : f0) * std::exp2 ((double) p.modOct);
        const double dt   = juce::jlimit (1.0e-6, 0.45, fSq / sr);

        // Stumme Triangle verschiebt die Lesephase der Square
        const float mod = triangle (phMod);
        phMod += fMod / sr;
        phMod -= std::floor (phMod);

        const double pd = juce::jlimit (0.0, 1.0, (double) p.pd + (double) p.lfoPd * lfo);
        const double offset = 0.5 * pd * mod;
        double ph = phSq + offset;
        ph -= std::floor (ph);

        // PolyBLEP mit der tatsaechlichen, modulierten Phasenschrittweite
        const double dtEff = juce::jlimit (1.0e-6, 0.45, std::abs (dt + offset - prevOffset));
        prevOffset = offset;

        const double pw = juce::jlimit (0.05, 0.95, (double) p.pw);
        double v = ph < pw ? 1.0 : -1.0;
        v += polyBlep (ph, dtEff);
        double ph2 = ph - pw;
        if (ph2 < 0.0) ph2 += 1.0;
        v -= polyBlep (ph2, dtEff);

        phSq += dt;
        phSq -= std::floor (phSq);

        return (float) v * p.sqGain * vel * adsr.getNextSample();
    }

private:
    double sr = 48000.0, baseHz = 440.0;
    double phSq = 0.0, phMod = 0.0, phLfo = 0.0;
    double penv = 1.0, penvCoef = 0.999, prevOffset = 0.0;
    float vel = 1.0f;
    juce::ADSR adsr;
};

} // namespace bleep
