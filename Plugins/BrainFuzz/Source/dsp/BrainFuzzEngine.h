#pragma once

// Brainbugz Brainfuzz - Engine (ohne JUCE, damit sie in den Test-Tools laeuft)
//
// Signalweg pro Band:
//   Weiche -> [FX Pre] -> Saettigung (4x Oversampling) -> Tight -> [FX Post]
//          -> Width / Spread (+ FX auf der Breite) -> Level / Solo / Mute / Bypass
// Danach: Summe -> Mix mit Dry (phasengleich) -> Mono Bass -> Output -> Limiter
//
// Tight: Die Verzerrung erzeugt Anteile ausserhalb des Bandes. Tight filtert nur diesen
// Verzerrungs-Anteil (Ausgang minus linear verstaerktes sauberes Band) an den Bandgrenzen.
// Der saubere Anteil bleibt unangetastet, dadurch aendert sich nicht, wie die Baender
// sich treffen.

#include "Crossover.h"
#include "Oversampler.h"
#include "Saturation.h"
#include "Effects.h"

namespace bfz
{

enum FxPos { posPre = 0, posPost = 1 };
enum FxTarget { targetBand = 0, targetWidth = 1, targetBoth = 2 };

struct FxSlot
{
    bool on = false;
    int pos = posPost;
    int target = targetBand;
};

struct BandParams
{
    float driveDb = 0.0f;     // 0 .. 36
    float levelDb = 0.0f;     // -24 .. +12
    float width = 1.0f;       // 0 .. 2 (Seitensignal)
    float spread = 0.0f;      // 0 .. 1 (zusaetzliche, mono-sichere Breite)
    bool solo = false, mute = false, bypass = false;
    FxSlot crushSlot, chorusSlot, verbSlot;
    CrushParams crush;
    ChorusParams chorus;
    ReverbParams verb;
};

struct EngineParams
{
    std::array<BandParams, 4> band {};
    std::array<float, 3> xover { 120.0f, 900.0f, 5000.0f };
    int xoverType = (int) XoverType::linkwitzRiley;
    int slope = 1;            // 0 = 12, 1 = 24, 2 = 48 dB
    int satType = (int) SatType::fuzz;
    float bite = 0.5f;
    float satBlend = 1.0f;    // Parallel: Saettigung
    float spreadBlend = 1.0f; // Parallel: Spread (0 .. 2)
    float mix = 1.0f;         // Parallel: gesamt
    float inDb = 0.0f, outDb = 0.0f;
    bool limit = true;
    bool tightHp = false, tightLp = false;
    int tightSlope = 0;       // 0 = 24 dB, 1 = 48 dB
    float masterCrush = 1.0f, masterChorus = 1.0f, masterVerb = 1.0f; // 0 .. 2
    bool monoBass = false;
    float monoFreq = 120.0f;
    bool bypass = false;
};

class BrainFuzzEngine
{
public:
    static constexpr int kBands = 4;
    static constexpr int kLatency = Oversampler4x::kLatency;
    static constexpr float kXMin[3] { 30.0f, 150.0f, 900.0f };
    static constexpr float kXMax[3] { 1000.0f, 6000.0f, 18000.0f };

    static std::array<float, 3> sanitiseXover (std::array<float, 3> x)
    {
        for (int i = 0; i < 3; ++i) x[(size_t) i] = std::clamp (x[(size_t) i], kXMin[i], kXMax[i]);
        x[1] = std::max (x[1], x[0] * 1.26f);
        x[2] = std::max (x[2], x[1] * 1.26f);
        return x;
    }

    static int slopeOrder (int slope) { return slope <= 0 ? 2 : slope == 1 ? 4 : 8; }

    // Ausgleich: wie laut die Saettigung bei einem Drive wird (Teil-Kompensation, Drive bleibt hoerbar)
    static float driveMakeup (float driveDb) { return std::pow (10.0f, -0.62f * driveDb / 20.0f); }

    // Pegel-Ausgleich pro Typ (laute Typen etwas leiser), waechst mit dem Drive bis 12 dB
    static float typeTrim (int satType, float driveDb)
    {
        static constexpr float trimDb[7] { -4.5f, 0.0f, -3.5f, 0.0f, -2.5f, -1.5f, -1.5f };
        const float amt = std::clamp (driveDb / 12.0f, 0.0f, 1.0f);
        return std::pow (10.0f, trimDb[std::clamp (satType, 0, 6)] * amt / 20.0f);
    }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = (float) sampleRate;
        maxBlockSize = std::max (1, maxBlock);
        xo.prepare (sampleRate);
        for (int b = 0; b < kBands; ++b)
        {
            auto& s = bands[(size_t) b];
            for (auto& o : s.os) o.reset();
            s.chorusBand.prepare (fs); s.chorusWidth.prepare (fs);
            s.verbBand.prepare (fs); s.verbWidth.prepare (fs);
            static constexpr float dMs[4] { 11.0f, 8.3f, 6.1f, 4.3f };
            s.decor.prepare (fs, dMs[b], 2.9f + 0.7f * b, 4.1f + 0.5f * b);
            s.rawDelay[0].prepare (kLatency + 2); s.rawDelay[1].prepare (kLatency + 2);
            s.refDelay[0].prepare (kLatency + 2); s.refDelay[1].prepare (kLatency + 2);
            for (int c = 0; c < 2; ++c)
            {
                s.bandOut[(size_t) c].assign ((size_t) maxBlockSize, 0.0f);
            }
        }
        inputDelay[0].prepare (kLatency + 2); inputDelay[1].prepare (kLatency + 2);
        for (auto& v : vizIn) v.assign ((size_t) maxBlockSize, 0.0f);
        reset();
    }

    void reset()
    {
        xo.reset();
        for (auto& s : bands)
        {
            for (auto& o : s.os) o.reset();
            s.crushBand.reset(); s.crushWidth.reset();
            s.chorusBand.reset(); s.chorusWidth.reset();
            s.verbBand.reset(); s.verbWidth.reset();
            s.decor.reset();
            for (auto& d : s.rawDelay) d.reset();
            for (auto& d : s.refDelay) d.reset();
            for (auto& t : s.tightHp) t.reset();
            for (auto& t : s.tightLp) t.reset();
            s.dc = { 0.0f, 0.0f };
            s.initialised = false;
            for (auto& f : s.fx) f = {};
        }
        for (auto& d : inputDelay) d.reset();
        monoHp.reset();
        limGain = 1.0f;
        initialised = false;
        fade = 1.0f;
        pendingStructure = false;
    }

    int getLatencySamples() const { return kLatency; }

    // buffer: zwei Kanaele, n <= maxBlock
    void process (float* const* ch, int n, const EngineParams& pIn)
    {
        if (n <= 0) return;
        n = std::min (n, maxBlockSize);
        EngineParams p = pIn;
        p.xover = sanitiseXover (p.xover);
        const auto type = (XoverType) std::clamp (p.xoverType, 0, 7);
        const int order = slopeOrder (p.slope);

        // Typ/Steilheit wechseln: kurz ausblenden, umschalten, einblenden (kein Knacksen)
        if (! initialised)
        {
            curType = type; curOrder = order; curTightOrder = p.tightSlope;
            xo.set (curType, curOrder, toD (p.xover));
            updateTight (p);
        }
        else if (type != curType || order != curOrder || p.tightSlope != curTightOrder)
            pendingStructure = true;

        // Weichen-Frequenzen glaetten (pro Block)
        for (int k = 0; k < 3; ++k)
        {
            if (! initialised) xSm[(size_t) k] = p.xover[(size_t) k];
            else xSm[(size_t) k] *= std::pow (p.xover[(size_t) k] / xSm[(size_t) k], std::min (1.0f, n / (0.03f * fs)));
        }
        xo.set (curType, curOrder, toD (xSm));
        updateTight (p);

        shaper.set ((SatType) std::clamp (p.satType, 0, 6), p.bite);
        const float shaperGain = shaper.slope0();

        // Solo
        bool anySolo = false;
        for (auto& b : p.band) anySolo = anySolo || b.solo;

        // Ziele fuer die Glaettung
        struct BandTargets { float drive, comp, wet, level, width, spread, on, byp; };
        std::array<BandTargets, kBands> tg {};
        for (int b = 0; b < kBands; ++b)
        {
            const auto& bp = p.band[(size_t) b];
            const float d = std::clamp (bp.driveDb, 0.0f, 48.0f);
            tg[(size_t) b].drive = std::pow (10.0f, d / 20.0f);
            tg[(size_t) b].comp = driveMakeup (d) * typeTrim (p.satType, d);
            tg[(size_t) b].wet = std::clamp (d / 1.5f, 0.0f, 1.0f) * std::clamp (p.satBlend, 0.0f, 1.0f);
            tg[(size_t) b].level = std::pow (10.0f, std::clamp (bp.levelDb, -60.0f, 24.0f) / 20.0f);
            tg[(size_t) b].width = std::clamp (bp.width, 0.0f, 2.0f);
            tg[(size_t) b].spread = std::clamp (bp.spread, 0.0f, 1.0f) * std::clamp (p.spreadBlend, 0.0f, 2.0f);
            const bool audible = ! bp.mute && (! anySolo || bp.solo);
            tg[(size_t) b].on = audible ? 1.0f : 0.0f;
            tg[(size_t) b].byp = bp.bypass ? 1.0f : 0.0f;
            // FX-Routing: Wechsel nur bei ausgeblendetem Effekt
            auto& st = bands[(size_t) b];
            const FxSlot* slots[3] { &bp.crushSlot, &bp.chorusSlot, &bp.verbSlot };
            for (int f = 0; f < 3; ++f)
            {
                auto& fx = st.fx[(size_t) f];
                const FxSlot& want = *slots[f];
                if (fx.amt <= 0.0f) { fx.pos = want.pos; fx.target = want.target; }
                fx.wantOn = want.on && want.pos == fx.pos && want.target == fx.target;
                fx.nextPos = want.pos; fx.nextTarget = want.target;
            }
            st.verbBand.update (bp.verb);
            st.verbWidth.update (bp.verb);
        }
        const float tgIn = std::pow (10.0f, p.inDb / 20.0f);
        const float tgOut = std::pow (10.0f, p.outDb / 20.0f);
        const float tgMix = std::clamp (p.mix, 0.0f, 1.0f);
        const float tgByp = p.bypass ? 1.0f : 0.0f;
        const float masters[3] { p.masterCrush, p.masterChorus, p.masterVerb };

        if (! initialised)
        {
            inSm = tgIn; outSm = tgOut; mixSm = tgMix; bypSm = tgByp;
            for (int b = 0; b < kBands; ++b)
            {
                auto& s = bands[(size_t) b];
                s.drive = tg[(size_t) b].drive; s.comp = tg[(size_t) b].comp; s.wet = tg[(size_t) b].wet;
                s.level = tg[(size_t) b].level; s.width = tg[(size_t) b].width; s.spread = tg[(size_t) b].spread;
                s.on = tg[(size_t) b].on; s.byp = tg[(size_t) b].byp;
                s.initialised = true;
            }
            initialised = true;
        }

        if (p.monoBass && std::abs (p.monoFreq - monoFreqSet) > 0.5f)
        {
            monoFreqSet = p.monoFreq;
            monoHp.f = bilinear (toHighpass (butterworth (4)), std::clamp (p.monoFreq, 20.0f, 1000.0f), fs);
        }

        const float kSm = 1.0f - std::exp (-1.0f / (0.015f * fs));
        const float kFx = 1.0f / (0.012f * fs);
        const float kDc = 1.0f - std::exp (-6.2831853f * 8.0f / fs);
        const float kFade = 1.0f / (0.008f * fs);
        const float limRel = std::exp (-1.0f / (0.08f * fs));
        const float ceiling = 0.9886f; // -0.1 dBFS

        for (int i = 0; i < n; ++i)
        {
            inSm += (tgIn - inSm) * kSm;
            outSm += (tgOut - outSm) * kSm;
            mixSm += (tgMix - mixSm) * kSm;
            bypSm += (tgByp - bypSm) * kSm;

            // Struktur-Wechsel mit Blende
            if (pendingStructure)
            {
                fade -= kFade;
                if (fade <= 0.0f)
                {
                    fade = 0.0f;
                    curType = type; curOrder = order; curTightOrder = p.tightSlope;
                    xo.set (curType, curOrder, toD (xSm));
                    tightFreqSet = { -1, -1, -1 };
                    updateTight (p);
                    for (auto& s : bands) { for (auto& t : s.tightHp) t.reset(); for (auto& t : s.tightLp) t.reset(); }
                    pendingStructure = false;
                }
            }
            else if (fade < 1.0f) fade = std::min (1.0f, fade + kFade);

            const float inL = ch[0][i], inR = ch[1][i];
            inputDelay[0].push (inL); inputDelay[1].push (inR);
            const float xl = inL * inSm, xr = inR * inSm;
            vizIn[0][(size_t) i] = xl; vizIn[1][(size_t) i] = xr;

            double bl[4], br[4];
            xo.process (0, xl, bl);
            xo.process (1, xr, br);

            float wetL = 0.0f, wetR = 0.0f, dryL = 0.0f, dryR = 0.0f;

            for (int b = 0; b < kBands; ++b)
            {
                auto& s = bands[(size_t) b];
                const auto& bp = p.band[(size_t) b];
                const auto& t = tg[(size_t) b];
                s.drive += (t.drive - s.drive) * kSm;
                s.comp += (t.comp - s.comp) * kSm;
                s.wet += (t.wet - s.wet) * kSm;
                s.level += (t.level - s.level) * kSm;
                s.width += (t.width - s.width) * kSm;
                s.spread += (t.spread - s.spread) * kSm;
                s.on += (t.on - s.on) * kSm;
                s.byp += (t.byp - s.byp) * kSm;

                // FX-Blenden
                for (int f = 0; f < 3; ++f)
                {
                    auto& fx = s.fx[(size_t) f];
                    if (fx.wantOn)
                    {
                        if (fx.amt <= 0.0f) resetFx (s, f); // alten Nachhall verwerfen
                        fx.amt = std::min (1.0f, fx.amt + kFx);
                    }
                    else
                    {
                        fx.amt = std::max (0.0f, fx.amt - kFx);
                        if (fx.amt <= 0.0f && (fx.pos != fx.nextPos || fx.target != fx.nextTarget))
                        {
                            fx.pos = fx.nextPos; fx.target = fx.nextTarget;
                            resetFx (s, f);
                        }
                    }
                }

                float L = (float) bl[b], R = (float) br[b];
                s.rawDelay[0].push (L); s.rawDelay[1].push (R);
                const float rawL = s.rawDelay[0].read (kLatency + 1);
                const float rawR = s.rawDelay[1].read (kLatency + 1);

                // FX vor der Saettigung (auf dem Band)
                runFx (s, bp, L, R, posPre, false, masters);

                s.refDelay[0].push (L); s.refDelay[1].push (R);
                const float refL = s.refDelay[0].read (kLatency + 1);
                const float refR = s.refDelay[1].read (kLatency + 1);

                // Saettigung (4x)
                const float g = s.drive, cmp = s.comp;
                const Shaper& sh = shaper;
                const float yL = s.os[0].process (L, [&] (float v) { return sh (v * g) * cmp; });
                const float yR = s.os[1].process (R, [&] (float v) { return sh (v * g) * cmp; });
                const float w = s.wet;
                const float G = (1.0f - w) + w * shaperGain * g * cmp; // lineare Verstaerkung des Saettigungs-Wegs
                float oL = refL + (yL - refL) * w;
                float oR = refR + (yR - refR) * w;

                // Verzerrungs-Anteil: DC weg, Tight an den Bandgrenzen
                float rL = oL - G * refL, rR = oR - G * refR;
                s.dc[0] += (rL - s.dc[0]) * kDc; rL -= s.dc[0];
                s.dc[1] += (rR - s.dc[1]) * kDc; rR -= s.dc[1];
                if (p.tightHp && b > 0) { rL = (float) s.tightHp[0].process (0, rL); rR = (float) s.tightHp[0].process (1, rR); }
                if (p.tightLp && b < 3) { rL = (float) s.tightLp[0].process (0, rL); rR = (float) s.tightLp[0].process (1, rR); }
                L = G * refL + rL;
                R = G * refR + rR;

                // FX nach der Saettigung (auf dem Band)
                runFx (s, bp, L, R, posPost, false, masters);

                // Breite: Seite skalieren, Spread aus der Mitte (mono-sicher)
                const float M = 0.5f * (L + R);
                const float S = 0.5f * (L - R) * s.width;
                float sp = s.decor.process (M) * s.spread;
                float spR = sp;
                runFx (s, bp, sp, spR, -1, true, masters);
                sp = 0.5f * (sp + spR);
                L = M + S + sp;
                R = M - S - sp;

                // Level, dann Bypass (sauberes Band), dann Solo/Mute
                L *= s.level; R *= s.level;
                L += (rawL - L) * s.byp;
                R += (rawR - R) * s.byp;
                L *= s.on; R *= s.on;

                s.bandOut[0][(size_t) i] = L;
                s.bandOut[1][(size_t) i] = R;
                wetL += L; wetR += R;
                dryL += rawL; dryR += rawR;
            }

            // Mix mit phasengleichem Dry (Summe der sauberen Baender)
            float l = dryL + (wetL - dryL) * mixSm;
            float r = dryR + (wetR - dryR) * mixSm;

            if (p.monoBass)
            {
                const float m = 0.5f * (l + r);
                const float sd = (float) monoHp.process (0, 0.5f * (l - r));
                l = m + sd; r = m - sd;
            }

            l *= outSm * fade; r *= outSm * fade;

            if (p.limit)
            {
                const float pk = std::max (std::abs (l), std::abs (r));
                const float want = pk > ceiling ? ceiling / pk : 1.0f;
                limGain = want < limGain ? want : want + (limGain - want) * limRel;
                l *= limGain; r *= limGain;
            }
            else limGain = 1.0f;

            // eigener Bypass: Eingang zeitgleich (gleiche Latenz)
            const float dl = inputDelay[0].read (kLatency + 1), dr = inputDelay[1].read (kLatency + 1);
            l += (dl - l) * bypSm;
            r += (dr - r) * bypSm;

            // Sicherheit
            if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0.0f; reset(); }
            ch[0][i] = std::clamp (l, -4.0f, 4.0f);
            ch[1][i] = std::clamp (r, -4.0f, 4.0f);
        }
        lastLimGain = limGain;
    }

    // --- Fuer die Anzeige ---
    const std::vector<float>& bandOutput (int b, int c) const { return bands[(size_t) b].bandOut[(size_t) c]; }
    const std::vector<float>& inputSignal (int c) const { return vizIn[(size_t) c]; }
    float getLimiterGain() const { return lastLimGain; }
    const Crossover4& getCrossover() const { return xo; }
    const Shaper& getShaper() const { return shaper; }

private:
    struct FxState
    {
        int pos = posPost, target = targetBand, nextPos = posPost, nextTarget = targetBand;
        float amt = 0.0f;
        bool wantOn = false;
    };

    struct BandState
    {
        std::array<Oversampler4x, 2> os;
        Crusher crushBand, crushWidth;
        Chorus chorusBand, chorusWidth;
        Reverb verbBand, verbWidth;
        Decorrelator decor;
        DelayLine rawDelay[2], refDelay[2];
        std::array<SosRunner<2>, 1> tightHp, tightLp;
        std::array<float, 2> dc {};
        std::array<FxState, 3> fx {};
        float drive = 1, comp = 1, wet = 0, level = 1, width = 1, spread = 0, on = 1, byp = 0;
        bool initialised = false;
        std::array<std::vector<float>, 2> bandOut;
    };

    static std::array<double, 3> toD (const std::array<float, 3>& a) { return { a[0], a[1], a[2] }; }

    static void resetFx (BandState& s, int f)
    {
        if (f == 0) { s.crushBand.reset(); s.crushWidth.reset(); }
        if (f == 1) { s.chorusBand.reset(); s.chorusWidth.reset(); }
        if (f == 2) { s.verbBand.reset(); s.verbWidth.reset(); }
    }

    // pos = posPre/posPost fuer das Band; widthPath = true fuer die Breite (pos egal)
    static inline void runFx (BandState& s, const BandParams& bp, float& l, float& r, int pos, bool widthPath, const float* masters)
    {
        for (int f = 0; f < 3; ++f)
        {
            const auto& fx = s.fx[(size_t) f];
            if (fx.amt <= 0.0f) continue;
            if (widthPath) { if (fx.target == targetBand) continue; }
            else { if (fx.target == targetWidth || fx.pos != pos) continue; }
            const float m = std::clamp (masters[f], 0.0f, 2.0f) * fx.amt;
            if (f == 0)
            {
                const float mix = std::clamp (bp.crush.mix * m, 0.0f, 1.0f);
                (widthPath ? s.crushWidth : s.crushBand).process (l, r, bp.crush, mix);
            }
            else if (f == 1)
            {
                const float mix = std::clamp (bp.chorus.mix * m, 0.0f, 1.0f);
                (widthPath ? s.chorusWidth : s.chorusBand).process (l, r, bp.chorus, mix);
            }
            else
            {
                const float mix = std::clamp (bp.verb.mix * m, 0.0f, 1.0f);
                (widthPath ? s.verbWidth : s.verbBand).process (l, r, bp.verb, mix);
            }
        }
    }

    void updateTight (const EngineParams& p)
    {
        const int o = curTightOrder == 0 ? 4 : 8;
        for (int k = 0; k < 3; ++k)
        {
            if (std::abs (xSm[(size_t) k] - tightFreqSet[(size_t) k]) < 0.01f && o == tightOrderSet) continue;
            tightFreqSet[(size_t) k] = xSm[(size_t) k];
            // Band k+1 bekommt den Hochpass an Weiche k, Band k den Tiefpass an Weiche k
            const auto hpF = bilinear (toHighpass (butterworth (o)), xSm[(size_t) k], fs);
            const auto lpF = bilinear (butterworth (o), xSm[(size_t) k], fs);
            bands[(size_t) k + 1].tightHp[0].f = hpF;
            bands[(size_t) k].tightLp[0].f = lpF;
        }
        tightOrderSet = o;
        (void) p;
    }

    float fs = 48000.0f;
    int maxBlockSize = 512;
    Crossover4 xo;
    std::array<BandState, kBands> bands;
    Shaper shaper;
    DelayLine inputDelay[2];
    std::array<std::vector<float>, 2> vizIn;
    SosRunner<2> monoHp;
    float monoFreqSet = -1.0f;
    std::array<float, 3> xSm { 120, 900, 5000 }, tightFreqSet { -1, -1, -1 };
    int tightOrderSet = -1;
    XoverType curType = XoverType::linkwitzRiley;
    int curOrder = 4, curTightOrder = 0;
    bool pendingStructure = false, initialised = false;
    float fade = 1.0f;
    float inSm = 1, outSm = 1, mixSm = 1, bypSm = 0;
    float limGain = 1.0f, lastLimGain = 1.0f;
};

} // namespace bfz
