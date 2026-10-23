#pragma once

// Brainfuzz – 4-Band-Weiche mit 8 Filtertypen und 12/24/48 dB
//
// Aufbau (Baum mit Ausgleich):
//   Band 1 = LP1 * C2 * C3      Band 2 = HP1 * LP2 * C3
//   Band 3 = HP1 * HP2 * LP3    Band 4 = HP1 * HP2 * HP3
// C_k = LP_k + HP_k ist die "Summe" der Weiche k. Damit ergibt die Summe aller Baender
// genau C1 * C2 * C3: bei Linkwitz-Riley ein reiner Allpass (flach), bei den anderen
// Typen der eigene Charakter des Typs an den Uebergaengen. Die Polaritaet des Hochpasses
// wird pro Typ/Ordnung so gewaehlt, dass die Summe moeglichst flach ist.

#include "FilterDesign.h"

namespace bfz
{

struct XoverDesign
{
    Zpk lp, hp;
    double hpSign = 1.0;

    static XoverDesign make (XoverType t, int order)
    {
        XoverDesign best;
        double bestWorst = 1e9;
        // Uebergang bei -3 dB oder -6 dB, Hochpass normal oder invertiert: die flachste Summe gewinnt
        for (int lvl = 0; lvl < 2; ++lvl)
        {
            if (t == XoverType::linkwitzRiley && lvl == 1) break; // LR kreuzt schon bei -6 dB
            XoverDesign d;
            d.lp = prototype (t, order);
            if (lvl == 1) normaliseCutoff (d.lp, 0.5);
            d.hp = toHighpass (d.lp);
            for (int s = 0; s < 2; ++s)
            {
                const double sign = s == 0 ? 1.0 : -1.0;
                double worst = 0.0;
                for (int i = 0; i <= 120; ++i)
                {
                    const double w = std::pow (10.0, -2.0 + 4.0 * i / 120.0);
                    const cd jw (0.0, w);
                    const double m = std::abs (d.lp.eval (jw) + sign * d.hp.eval (jw));
                    worst = std::max (worst, std::abs (20.0 * std::log10 (std::max (m, 1e-9))));
                }
                if (worst < bestWorst) { bestWorst = worst; best = d; best.hpSign = sign; }
            }
        }
        return best;
    }

    // Betrag der Summe in dB an w/wc (fuer die Anzeige)
    double sumDb (double ratio) const
    {
        const cd jw (0.0, ratio);
        return 20.0 * std::log10 (std::max (1e-9, std::abs (lp.eval (jw) + hpSign * hp.eval (jw))));
    }
};

class Crossover4
{
public:
    static constexpr int kBands = 4;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        designed = false;
        reset();
    }

    void reset()
    {
        for (auto* r : allRunners()) r->reset();
    }

    // Typ/Ordnung/Frequenzen setzen; neue Koeffizienten nur bei Aenderung
    void set (XoverType t, int order, const std::array<double, 3>& freqs)
    {
        if (! designed || t != type || order != ord)
        {
            type = t; ord = order;
            design = XoverDesign::make (t, order);
            designed = true;
            lastF = { -1.0, -1.0, -1.0 };
            reset();
        }
        for (int k = 0; k < 3; ++k)
        {
            if (std::abs (freqs[(size_t) k] - lastF[(size_t) k]) < 1e-6) continue;
            lastF[(size_t) k] = freqs[(size_t) k];
            const auto lpF = bilinear (design.lp, freqs[(size_t) k], fs);
            const auto hpF = bilinear (design.hp, freqs[(size_t) k], fs);
            lp[(size_t) k].f = lpF;
            hp[(size_t) k].f = hpF;
            hp[(size_t) k].gainSign = design.hpSign;
            if (k == 1) { c2aL.f = lpF; c2aH.f = hpF; c2aH.gainSign = design.hpSign; }
            if (k == 2)
            {
                c3aL.f = lpF; c3aH.f = hpF; c3aH.gainSign = design.hpSign;
                c3bL.f = lpF; c3bH.f = hpF; c3bH.gainSign = design.hpSign;
            }
        }
    }

    inline void process (int ch, double x, double* bands)
    {
        const double l1 = lp[0].process (ch, x);
        const double h1 = hp[0].process (ch, x);
        // Band 1: durch C2 und C3
        const double b0a = c2aL.process (ch, l1) + c2aH.process (ch, l1);
        bands[0] = c3aL.process (ch, b0a) + c3aH.process (ch, b0a);
        const double l2 = lp[1].process (ch, h1);
        const double h2 = hp[1].process (ch, h1);
        bands[1] = c3bL.process (ch, l2) + c3bH.process (ch, l2);
        bands[2] = lp[2].process (ch, h2);
        bands[3] = hp[2].process (ch, h2);
    }

    const XoverDesign& getDesign() const { return design; }

    // Komplexer Frequenzgang eines Bandes (fuer Anzeige / Tests)
    cd bandResponse (int b, double f) const
    {
        auto L = [&] (int k) { return sosResponse (lp[(size_t) k].f, f, fs); };
        auto H = [&] (int k) { return sosResponse (hp[(size_t) k].f, f, fs) * design.hpSign; };
        auto C = [&] (int k) { return L (k) + H (k); };
        switch (b)
        {
            case 0:  return L (0) * C (1) * C (2);
            case 1:  return H (0) * L (1) * C (2);
            case 2:  return H (0) * H (1) * L (2);
            default: return H (0) * H (1) * H (2);
        }
    }

private:
    std::vector<SosRunner<2>*> allRunners()
    {
        return { &lp[0], &lp[1], &lp[2], &hp[0], &hp[1], &hp[2], &c2aL, &c2aH, &c3aL, &c3aH, &c3bL, &c3bH };
    }

    double fs = 48000.0;
    XoverType type = XoverType::linkwitzRiley;
    int ord = 4;
    bool designed = false;
    XoverDesign design;
    std::array<double, 3> lastF { -1.0, -1.0, -1.0 };
    std::array<SosRunner<2>, 3> lp {}, hp {};
    SosRunner<2> c2aL, c2aH, c3aL, c3aH, c3bL, c3bH;
};

} // namespace bfz
