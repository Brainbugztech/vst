#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "NoteGrouper.h"
#include "Scales.h"

namespace prizm
{
// Tonart per Sidechain: erkennt laufend die Noten einer anderen Spur (z. B. Bass oder Keys)
// und liefert ihre Tonklassen als Maske. Nur Grundtöne zählen (NoteGrouper), damit Obertöne
// keine falschen Noten erzeugen. Wie MIDI KEY: bei Stille bleibt der letzte Akkord stehen.
class ChordTracker
{
public:
    // hopDivider: 8 = alle ca. 21 ms (Sidechain-Tonart), 4 = alle ca. 43 ms (Anzeige)
    explicit ChordTracker (int hopDividerToUse = 8) : hopDivider (hopDividerToUse) {}

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        order = sampleRate > 50000.0 ? 14 : 13;
        size = 1 << order;
        hop = size / hopDivider;
        fft = std::make_unique<juce::dsp::FFT> (order);
        window.resize ((size_t) size);
        for (int i = 0; i < size; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
        input.assign ((size_t) size, 0.0f);
        frame.assign ((size_t) size * 2, 0.0f);
        decay = std::exp (-(float) hop / ((float) fs * 0.35f)); // Noten halten ca. 0,35 s nach
        reset();
    }

    void reset()
    {
        std::fill (input.begin(), input.end(), 0.0f);
        chroma.fill (0.0f);
        writePos = hopCounter = 0;
        mask = 0;
    }

    void process (const float* const* channels, int numChannels, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            float x = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                x += channels[ch][n];
            input[(size_t) writePos] = x / (float) juce::jmax (1, numChannels);
            writePos = (writePos + 1) % size;
            if (++hopCounter >= hop)
            {
                hopCounter = 0;
                analyseFrame();
            }
        }
    }

    PitchMask getMask() const noexcept { return mask; }
    // Stärke pro Tonklasse (nur Grundtöne, mit Haltezeit), unnormiert
    const std::array<float, 12>& getChroma() const noexcept { return chroma; }

private:
    void analyseFrame()
    {
        float energy = 0.0f;
        for (int i = 0; i < size; ++i)
        {
            const float v = input[(size_t) ((writePos + i) % size)];
            energy += v * v;
            frame[(size_t) i] = v * window[(size_t) i];
        }
        std::fill (frame.begin() + size, frame.end(), 0.0f);

        for (auto& c : chroma)
            c *= decay;

        // Stille (unter ca. -55 dBFS): letzten Akkord stehen lassen
        if (energy / (float) size < 3.0e-6f)
            return;

        fft->performFrequencyOnlyForwardTransform (frame.data(), true);
        const int bins = size / 2;
        float maxMag = 0.0f;
        for (int k = 1; k < bins; ++k)
            maxMag = juce::jmax (maxMag, frame[(size_t) k]);
        const float floorMag = maxMag * 0.01f;
        const double binHz = fs / size;

        grouper.peaks.clear();
        for (int k = 2; k < bins - 1; ++k)
        {
            const float m = frame[(size_t) k];
            if (m < floorMag || m <= frame[(size_t) k - 1] || m < frame[(size_t) k + 1])
                continue;
            const double left = frame[(size_t) k - 1], right = frame[(size_t) k + 1];
            const double a = (right > left ? right : left) / m;
            const double delta = (right > left ? 1.0 : -1.0) * (2.0 * a - 1.0) / (a + 1.0); // Hann (Grandke)
            const double hz = ((double) k + juce::jlimit (-0.5, 0.5, delta)) * binHz;
            if (hz < 30.0 || hz > 8000.0)
                continue;
            NoteGrouper::Peak pk;
            pk.hz = (float) hz;
            pk.mag = m;
            grouper.peaks.push_back (pk);
        }
        grouper.group();

        for (const auto& note : grouper.notes)
        {
            if (note.f0 < 30.0f || note.f0 > 5000.0f)
                continue;
            const int pc = (((int) std::lround (hzToMidi (note.f0))) % 12 + 12) % 12;
            chroma[(size_t) pc] = juce::jmax (chroma[(size_t) pc], note.score);
        }

        // Maske: Noten mit mindestens 25 % der stärksten, höchstens 6
        float top = 0.0f;
        for (auto c : chroma)
            top = juce::jmax (top, c);
        if (top <= 0.0f)
            return;
        PitchMask m = 0;
        int count = 0;
        for (int pass = 0; pass < 12 && count < 6; ++pass)
        {
            int best = -1;
            float bestVal = 0.0f;
            for (int pc = 0; pc < 12; ++pc)
                if (! ((m >> pc) & 1u) && chroma[(size_t) pc] > bestVal)
                {
                    bestVal = chroma[(size_t) pc];
                    best = pc;
                }
            if (best < 0 || bestVal < top * 0.25f)
                break;
            m |= (PitchMask) (1u << best);
            ++count;
        }
        if (m != 0)
            mask = m;
    }

    double fs = 48000.0;
    int hopDivider = 8;
    int order = 13, size = 8192, hop = 1024;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, input, frame;
    std::array<float, 12> chroma {};
    float decay = 0.9f;
    int writePos = 0, hopCounter = 0;
    PitchMask mask = 0;
    NoteGrouper grouper;
};
} // namespace prizm
