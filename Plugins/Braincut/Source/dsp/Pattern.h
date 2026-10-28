#pragma once

// Schnitt-Muster: wie eine Phrase in Einheiten ("Units") zerlegt wird und woher jede Einheit
// ihr Audio nimmt. Eigene Umsetzung nach der Idee von Break-Cuttern wie Livecut/BBCut:
//   CUT  - bleibt nah am Original, ersetzt pro Beat gelegentlich durch Wiederholung, Rolls,
//          Rückwärts (Anteil = rev) oder einen anderen Beat (Anteil = jump)
//   WARP - springt freier: Blöcke unterschiedlicher Länge lesen von wechselnden Stellen
// Alle Zufallswerte kommen aus einer Folge, die nur vom Seed abhängt. Das Muster ist also
// reproduzierbar und unabhängig davon, welche anderen Sektionen an oder aus sind.
//
// Begriffe: Der "Loop" ist die aufgenommene Quelle (M Schritte). Die "Phrase" ist der Abschnitt der
// Ausgabe, für den dieses Muster gilt (N Schritte, Länge zwischen Min und Max Takten).
// "src" zählt immer in Schritten des Loops.

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
    int16_t src = 0;     // Schritt im Loop, ab dem gelesen wird
    bool    rev = false; // rückwärts lesen
    float   vr = 0.0f;   // Zufallswert -1..1 für die Streckung (Richtung und Stärke)
    float   vsel = 0.0f; // Zufallswert 0..1: ob diese Einheit gestreckt wird
};

struct GenParams
{
    int N = 16;          // Schritte der Phrase
    int M = 16;          // Schritte des Loops
    int base = 0;        // Loop-Schritt, an dem die Phrase beginnt
    int sb = 4;          // Schritte pro Beat
    float rp = 0.4f;     // Umbau-Wahrscheinlichkeit pro Beat
    int repMin = 0, repMax = 3;
    float rev = 0.3f;    // 0..1: Anteil rückwärts
    float jump = 0.5f;   // 0..1: wie oft von einer anderen Stelle gelesen wird
};

struct Pattern
{
    static constexpr int kMax = 512;

    int steps = 0, count = 0, loopSteps = 1, base = 0;
    std::array<Unit, kMax>     units {};
    std::array<int16_t, kMax>  unitOfStep {};
    std::array<int16_t, kMax>  srcOfStep {};   // für die Anzeige: von welchem Loop-Schritt kommt dieser Schritt
    std::array<uint8_t, kMax>  moved {};       // 1 = nicht am Originalplatz
    std::array<uint8_t, kMax>  revOfStep {};   // 1 = rückwärts
    std::array<uint8_t, kMax>  unitStart {};   // 1 = hier beginnt eine neue Einheit

    void begin (int n, int m, int baseStep)
    {
        steps = std::min (std::max (1, n), kMax);
        loopSteps = std::max (1, m);
        base = ((baseStep % loopSteps) + loopSteps) % loopSteps;
        count = 0;
    }

    // Loop-Schritt, der zum Phrasen-Schritt "pos" gehört, wenn nichts umgebaut wird
    int natural (int pos) const { return (base + pos) % loopSteps; }

    // Ein Block aus "len" Schritten ab "start". Innerhalb wird die Einheit von "u" Schritten wiederholt.
    void addBlock (int start, int len, int src, bool rev, int u, Rng& rv)
    {
        len = std::min (len, steps - start);
        if (len <= 0) return;
        u = std::max (1, std::min (u, len));
        const float vr = rv.uni() * 2.0f - 1.0f;
        const float vsel = rv.uni();
        src = ((src % loopSteps) + loopSteps) % loopSteps;
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
                srcStep = ((srcStep % loopSteps) + loopSteps) % loopSteps;
                unitOfStep[(size_t) s] = (int16_t) k;
                srcOfStep[(size_t) s] = (int16_t) srcStep;
                moved[(size_t) s] = (uint8_t) (srcStep != natural (s) ? 1 : 0);
                revOfStep[(size_t) s] = (uint8_t) (u.rev ? 1 : 0);
                unitStart[(size_t) s] = (uint8_t) (s == u.start ? 1 : 0);
            }
        }
    }
};

// ---------------------------------------------------------------------------------------------
inline void generateCut (Pattern& P, const GenParams& g, uint64_t key)
{
    Rng rg (key ^ 0xC07A11ull), rv (key ^ 0x57E7C4ull);
    P.begin (g.N, g.M, g.base);
    const int sb = std::max (1, g.sb);
    const int loopBeats = std::max (1, (g.M + sb - 1) / sb);
    const int rLo = std::min (g.repMin, g.repMax), rHi = std::max (g.repMin, g.repMax);

    for (int pos = 0; pos < P.steps; pos += sb)
    {
        const int L = std::min (sb, P.steps - pos);
        const bool last = pos + sb >= P.steps;

        // Alle Zufallswerte werden immer gezogen, damit das Muster nicht von Schwellwerten abhängt
        const float r  = rg.uni();
        const float r2 = rg.uni();
        const int variant = rg.range (0, 2);
        const float rJump = rg.uni(), rRev = rg.uni();
        const int other = rg.range (0, loopBeats - 1) * sb;
        const int reps = rg.range (rLo, rHi);
        const float pRep = std::min (0.95f, g.rp * (last ? 1.5f : 1.0f));
        const int nat = P.natural (pos);

        if (r >= pRep)
        {
            P.addBlock (pos, L, nat, false, L, rv);
            continue;
        }

        // Anzahl Wiederholungen bestimmt die Länge der wiederholten Einheit
        const int u = reps <= 0 ? L : std::max (1, L / (reps + 1));

        const bool rev = rRev < g.rev;
        if (rJump < g.jump * 0.4f)
            P.addBlock (pos, L, other, rev, L, rv);                                  // anderer Beat (evtl. rückwärts)
        else if (variant == 2)
            P.addBlock (pos, L, nat + (int) (r2 * (float) L), rev, rev ? L : u, rv); // Ausschnitt aus der Beat-Mitte
        else
            P.addBlock (pos, L, nat, rev, u, rv);                                    // Anfang des Beats wiederholen / rückwärts stottern
    }
    P.finish();
}

inline void generateWarp (Pattern& P, const GenParams& g, uint64_t key)
{
    static constexpr int lens[8] { 1, 2, 2, 3, 3, 4, 5, 6 };
    Rng rg (key ^ 0x3A4B5Cull), rv (key ^ 0x9D2E11ull);
    P.begin (g.N, g.M, g.base);
    const int sb = std::max (1, g.sb);
    const int rLo = std::min (g.repMin, g.repMax), rHi = std::max (g.repMin, g.repMax);

    int pos = 0;
    while (pos < P.steps)
    {
        const int L = std::min (lens[rg.range (0, 7)] * std::max (1, sb / 4), P.steps - pos);
        const float rAnchor = rg.uni(), rJump = rg.uni(), rRev = rg.uni(), rRep = rg.uni();
        const int rndSrc = rg.range (0, P.loopSteps - 1);
        const int reps = rg.range (rLo, rHi);

        const bool onBeat = (pos % sb) == 0;
        const bool anchored = onBeat && rAnchor < (0.55f - 0.45f * g.rp);
        const float pJump = std::min (1.0f, (0.3f + 0.7f * g.rp) * (0.4f + 1.2f * g.jump));
        const int nat = P.natural (pos);

        int src = (! anchored && rJump < pJump) ? rndSrc : nat;
        if (pos == 0 && rAnchor < 0.8f) src = nat; // der Downbeat bleibt meistens erhalten
        const bool rev = ! anchored && rRev < g.rev * 0.5f;
        const int u = (L >= 2 && reps > 0 && rRep < 0.2f + g.rp * 0.6f) ? std::max (1, L / (reps + 1)) : L;

        P.addBlock (pos, L, src, rev, u, rv);
        pos += L;
    }
    P.finish();
}

} // namespace bc
