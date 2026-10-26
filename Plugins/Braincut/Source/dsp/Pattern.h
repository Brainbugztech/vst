#pragma once

// Schnitt-Muster: wie eine Phrase in Einheiten ("Units") zerlegt wird und woher jede Einheit
// ihr Audio nimmt. Eigene Umsetzung nach der Idee von Break-Cuttern wie Livecut/BBCut:
//   CUT  - bleibt nah am Original, ersetzt pro Beat gelegentlich durch Wiederholung, Rolls,
//          Rückwärts oder einen anderen Beat
//   WARP - springt freier: Blöcke unterschiedlicher Länge lesen von wechselnden Stellen
// Alle Zufallswerte kommen aus einer Folge, die nur vom Seed abhängt. Das Muster ist also
// reproduzierbar und unabhängig davon, welche anderen Sektionen an oder aus sind.

#include <algorithm>
#include <array>
#include <cstdint>
#include "Rng.h"

namespace bc
{

struct Unit
{
    int16_t start = 0;   // erster Schritt der Einheit in der Phrase
    int16_t len = 1;     // Länge in Schritten
    int16_t src = 0;     // Schritt, ab dem gelesen wird
    bool    rev = false; // rückwärts lesen
    float   vr = 0.0f;   // Zufallswert -1..1 für die Streckung (Richtung und Stärke)
    float   vsel = 0.0f; // Zufallswert 0..1: ob diese Einheit gestreckt wird
};

struct Pattern
{
    static constexpr int kMax = 512;

    int steps = 0;
    int count = 0;
    std::array<Unit, kMax>     units {};
    std::array<int16_t, kMax>  unitOfStep {};
    std::array<int16_t, kMax>  srcOfStep {};   // für die Anzeige: von welchem Schritt kommt dieser Schritt
    std::array<uint8_t, kMax>  moved {};       // 1 = nicht am Originalplatz

    void begin (int n)
    {
        steps = std::min (n, kMax);
        count = 0;
    }

    // Ein Block aus "len" Schritten ab "start". Innerhalb wird die Einheit von "u" Schritten wiederholt.
    void addBlock (int start, int len, int src, bool rev, int u, Rng& rv)
    {
        len = std::min (len, steps - start);
        if (len <= 0) return;
        u = std::max (1, std::min (u, len));
        const float vr = rv.uni() * 2.0f - 1.0f;
        const float vsel = rv.uni();
        src = std::max (0, std::min (src, steps - 1));
        for (int off = 0; off < len; off += u)
        {
            if (count >= kMax) return;
            Unit& x = units[(size_t) count++];
            x.start = (int16_t) (start + off);
            x.len = (int16_t) std::min (u, len - off);
            x.src = (int16_t) src;
            x.rev = rev;
            x.vr = vr;
            x.vsel = vsel;
        }
    }

    // Tabellen für schnellen Zugriff pro Schritt füllen
    void finish()
    {
        for (int k = 0; k < count; ++k)
        {
            const Unit& u = units[(size_t) k];
            for (int s = u.start; s < u.start + u.len && s < steps; ++s)
            {
                const int rel = s - u.start;
                int srcStep = u.rev ? (u.src + u.len - 1 - rel) : (u.src + rel);
                srcStep = ((srcStep % steps) + steps) % steps;
                unitOfStep[(size_t) s] = (int16_t) k;
                srcOfStep[(size_t) s] = (int16_t) srcStep;
                moved[(size_t) s] = (uint8_t) (srcStep != s ? 1 : 0);
            }
        }
    }
};

// ---------------------------------------------------------------------------------------------
inline void generateCut (Pattern& P, int N, int sb, float rp, uint64_t key)
{
    Rng rg (key ^ 0xC07A11ull), rv (key ^ 0x57E7C4ull);
    P.begin (N);
    const int beatCount = (N + sb - 1) / sb;

    for (int pos = 0; pos < N; pos += sb)
    {
        const int L = std::min (sb, N - pos);
        const bool last = pos + sb >= N;

        // Alle Zufallswerte werden immer gezogen, damit das Muster nicht von Schwellwerten abhängt
        const float r  = rg.uni();
        const float r2 = rg.uni();
        const int variant = rg.range (0, 5);
        const int other = rg.range (0, std::max (0, beatCount - 1)) * sb;
        const float pRep = std::min (0.95f, rp * (last ? 1.5f : 1.0f));

        if (r >= pRep)
        {
            P.addBlock (pos, L, pos, false, L, rv);
            continue;
        }

        switch (variant)
        {
            case 0:  P.addBlock (pos, L, pos, false, std::max (1, L / 2), rv); break;       // Halb-Beat wiederholen
            case 1:  P.addBlock (pos, L, pos, false, std::max (1, L / 4), rv); break;       // Roll
            case 2:  P.addBlock (pos, L, std::min (N - 1, pos + (int) (r2 * (float) L)), false, 1, rv); break; // ein Schritt stottert
            case 3:  P.addBlock (pos, L, pos, true, L, rv); break;                           // Beat rückwärts
            case 4:  P.addBlock (pos, L, std::min (N - 1, other), false, L, rv); break;      // anderer Beat
            default: P.addBlock (pos, L, pos, false, std::max (1, (L * 3) / 4), rv); break;  // 3 über 4
        }
    }
    P.finish();
}

inline void generateWarp (Pattern& P, int N, int sb, float rp, uint64_t key)
{
    static constexpr int lens[8] { 1, 2, 2, 3, 3, 4, 5, 6 };
    Rng rg (key ^ 0x3A4B5Cull), rv (key ^ 0x9D2E11ull);
    P.begin (N);

    int pos = 0;
    while (pos < N)
    {
        const int L = std::min (lens[rg.range (0, 7)] * std::max (1, sb / 4), N - pos);
        const float rAnchor = rg.uni(), rJump = rg.uni(), rRev = rg.uni(), rRep = rg.uni();
        const int rndSrc = rg.range (0, N - 1);

        const bool onBeat = (pos % std::max (1, sb)) == 0;
        const bool anchored = onBeat && rAnchor < (0.55f - 0.45f * rp);
        const float pJump = 0.3f + 0.7f * rp;

        int src = (!anchored && rJump < pJump) ? rndSrc : pos;
        if (pos == 0 && rAnchor < 0.8f) src = 0; // der Downbeat bleibt meistens erhalten
        const bool rev = !anchored && rRev < rp * 0.25f;
        const int u = (L >= 2 && rRep < rp * 0.5f) ? std::max (1, L / 2) : L;

        P.addBlock (pos, L, src, rev, u, rv);
        pos += L;
    }
    P.finish();
}

} // namespace bc
