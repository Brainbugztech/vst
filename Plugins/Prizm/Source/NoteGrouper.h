#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "EngineParams.h"

namespace prizm
{
// Polyphone Notenerkennung für einen FFT-Frame: ordnet Teiltöne (Peaks) Noten zu,
// damit eine Note mit allen Obertönen gemeinsam verschoben wird. Beispiel: wird E auf Eb
// umgeleitet, wandert die gespielte Note E, aber nicht der Oberton E eines gespielten C.
// Verfahren: iterativ den Grundton mit der besten gewichteten Obertonsumme suchen
// (Gewicht 1/sqrt(h), Toleranz 35 Cent), dessen Teiltöne vergeben, wiederholen.
class NoteGrouper
{
public:
    static constexpr int kMaxNotes = 8;
    static constexpr int kMaxHarmonics = 12;

    struct Peak
    {
        float hz = 0.0f;
        float mag = 0.0f;
        int group = -1;      // Index der Note oder -1 (gehört zu keiner erkannten Note)
        float ratio = 1.0f;  // Ergebnis: Frequenzverhältnis für das Verschieben
        float gain = 1.0f;   // Ergebnis: 0 = stummgeschaltet
    };

    struct Note
    {
        float f0 = 0.0f;
        float score = 0.0f;
    };

    std::vector<Peak> peaks;  // vom Aufrufer gefüllt (hz = echte Frequenz)
    std::vector<Note> notes;  // erkannte Noten

    // refScale = 440 / Stimmton
    void run (const EngineParams& p, float refScale)
    {
        group();
        plan (p, refScale);
    }

    void group()
    {
        notes.clear();
        for (auto& pk : peaks)
            pk.group = -1;
        if (peaks.empty())
            return;

        // nach Frequenz sortierte Indizes für die Obertonsuche
        byFreq.resize (peaks.size());
        for (size_t i = 0; i < peaks.size(); ++i)
            byFreq[i] = (int) i;
        std::sort (byFreq.begin(), byFreq.end(), [this] (int a, int b) { return peaks[(size_t) a].hz < peaks[(size_t) b].hz; });

        float maxMag = 0.0f;
        for (auto& pk : peaks)
            maxMag = std::max (maxMag, pk.mag);
        const float floorMag = maxMag * 0.003f; // -50 dB

        float firstScore = 0.0f;
        for (int n = 0; n < kMaxNotes; ++n)
        {
            // Kandidaten: die stärksten noch freien Peaks, als Grundton oder 2./3. Oberton
            strong.clear();
            for (size_t i = 0; i < peaks.size(); ++i)
                if (peaks[i].group < 0 && peaks[i].mag > floorMag && peaks[i].hz >= 40.0f && peaks[i].hz <= 5000.0f)
                    strong.push_back ((int) i);
            if (strong.empty())
                break;
            std::sort (strong.begin(), strong.end(), [this] (int a, int b) { return peaks[(size_t) a].mag > peaks[(size_t) b].mag; });
            if (strong.size() > 12)
                strong.resize (12);

            float bestScore = 0.0f, bestF0 = 0.0f;
            int bestMatches = 0;
            for (int idx : strong)
                for (int div = 1; div <= 3; ++div)
                {
                    const float f0 = peaks[(size_t) idx].hz / (float) div;
                    if (f0 < 40.0f)
                        continue;
                    int matches = 0;
                    bool hasLow = false;
                    const float sc = score (f0, matches, hasLow);
                    if (sc > bestScore && (hasLow || matches >= 3))
                    {
                        bestScore = sc;
                        bestF0 = f0;
                        bestMatches = matches;
                    }
                }

            if (bestF0 <= 0.0f || (n > 0 && bestScore < firstScore * 0.03f))
                break;
            if (n == 0)
                firstScore = bestScore;
            juce::ignoreUnused (bestMatches);

            // Grundton aus den gefundenen Obertönen verfeinern, dann Teiltöne vergeben
            float num = 0.0f, den = 0.0f;
            for (int h = 1; h <= kMaxHarmonics; ++h)
            {
                const int i = find (bestF0 * (float) h);
                if (i < 0)
                    continue;
                num += peaks[(size_t) i].hz / (float) h * peaks[(size_t) i].mag;
                den += peaks[(size_t) i].mag;
            }
            const float f0 = den > 0.0f ? num / den : bestF0;
            const int g = (int) notes.size();
            notes.push_back ({ f0, bestScore });
            for (int h = 1; h <= kMaxHarmonics; ++h)
            {
                const int i = find (f0 * (float) h);
                if (i >= 0)
                    peaks[(size_t) i].group = g;
            }
        }
    }

    // Für jede Note und jeden freien Teilton: Ziel bestimmen (Map, sonst Einrasten mit SHIFT)
    void plan (const EngineParams& p, float refScale)
    {
        const float shift = juce::jlimit (0.0f, 1.0f, p.shift);
        noteRatio.assign (notes.size(), 1.0f);
        noteGain.assign (notes.size(), 1.0f);
        for (size_t g = 0; g < notes.size(); ++g)
            target (notes[g].f0 * refScale, p, shift, noteRatio[g], noteGain[g]);

        for (auto& pk : peaks)
        {
            if (pk.group >= 0)
            {
                pk.ratio = noteRatio[(size_t) pk.group];
                pk.gain = noteGain[(size_t) pk.group];
            }
            else if (pk.hz > 25.0f && pk.hz < 18000.0f)
            {
                target (pk.hz * refScale, p, shift, pk.ratio, pk.gain);
            }
            else
            {
                pk.ratio = 1.0f;
                pk.gain = 1.0f;
            }
        }
    }

private:
    // Ziel einer Tonhöhe (hz auf 440-Reihe umgerechnet)
    static void target (float hz440, const EngineParams& p, float shift, float& ratio, float& gain)
    {
        ratio = 1.0f;
        gain = 1.0f;
        if (hz440 < 20.0f)
            return;
        const float midi = hzToMidi (hz440);
        const int n = (int) std::lround (midi);
        const int pc = ((n % 12) + 12) % 12;
        const int m = p.map[(size_t) pc];

        if (m == kMapMute)
        {
            gain = 0.0f;
            return;
        }
        if (m >= 0 && m < 12)
        {
            // Ziel-Tonklasse in der nächstgelegenen Oktave, sauber gestimmt
            int delta = ((m - pc) % 12 + 12) % 12; // 0..11 nach oben
            if (delta > 6)
                delta -= 12;
            ratio = std::pow (2.0f, ((float) (n + delta) - midi) / 12.0f);
            return;
        }
        if (shift > 0.001f)
        {
            const float t = midi + shift * ((float) nearestAllowedNote (p.mask, midi) - midi);
            ratio = std::pow (2.0f, (t - midi) / 12.0f);
        }
    }

    // nächster freier Peak bei hz (innerhalb 35 Cent), sonst -1
    int find (float hz) const
    {
        auto it = std::lower_bound (byFreq.begin(), byFreq.end(), hz,
                                    [this] (int a, float v) { return peaks[(size_t) a].hz < v; });
        int best = -1;
        float bestCents = 35.0f;
        for (auto c : { it, it == byFreq.begin() ? it : it - 1 })
        {
            if (c == byFreq.end())
                continue;
            const auto& pk = peaks[(size_t) *c];
            if (pk.group >= 0)
                continue;
            const float cents = std::abs (1200.0f * std::log2 (pk.hz / hz));
            if (cents < bestCents)
            {
                bestCents = cents;
                best = *c;
            }
        }
        return best;
    }

    float score (float f0, int& matches, bool& hasLow) const
    {
        float s = 0.0f;
        matches = 0;
        hasLow = false;
        for (int h = 1; h <= kMaxHarmonics; ++h)
        {
            const float hz = f0 * (float) h;
            if (hz > 8000.0f)
                break;
            const int i = find (hz);
            if (i < 0)
                continue;
            s += peaks[(size_t) i].mag / std::sqrt ((float) h);
            ++matches;
            if (h <= 2)
                hasLow = true;
        }
        return s;
    }

    std::vector<int> byFreq, strong;
    std::vector<float> noteRatio, noteGain;
};
} // namespace prizm
