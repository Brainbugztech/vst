#pragma once

// Das Wurmloch: ein Trichter aus feinen Ringen (Flamm-Paraboloid, schräg von oben).
//
// Ruhig, solange nichts passiert: alle paar Sekunden ein feiner Scan, selten ein kurzer Glitch.
// Reaktionen:
//  - Regler drehen: Impuls, die Ringe strömen und wellen sich kurz
//  - Tempo: Tiefe und Strömung (Extrembereich: tief und langsam, "Zeitdehnung")
//  - Pitch: Linien verdrehen sich, Farbe wandert (hoch = Cyan, tief = Violett)
//  - Formant: Trichter wird flacher oder steiler
//  - Rendern: Sog mit Lichtpunkten, wird mit dem Fortschritt schneller, am Ende ein Lichtimpuls
//  - Vorhören: Strömung im Tempo des Ergebnisses, Glow und Wellen folgen der Musik

#include "WormholeLook.h"
#include <array>

namespace whl
{
class Tunnel : public juce::Component, private juce::Timer
{
public:
    Tunnel()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        nextScan = now + 2.5; lastTick = now;
        for (auto& pt : particles) { pt.a = random.nextFloat() * juce::MathConstants<float>::twoPi; pt.t = random.nextFloat(); pt.v = 0.6f + random.nextFloat() * 0.8f; }
        startTimerHz (30);
    }

    // Design-Koordinaten (960 x 600 Fläche)
    juce::Point<float> centre { 480.0f, 150.0f };
    std::array<juce::Point<float>, 2> calmSpots {{ juce::Point<float> (165.0f, 250.0f), juce::Point<float> (795.0f, 250.0f) }};

    void setTargets (double timeRatio, double pitchSemis, double formantSemis)
    {
        tgtDepth = (float) juce::jlimit (0.55, 1.42, std::pow (timeRatio, 0.22));  // Schlund bleibt über der Wellenform
        tgtTwist = (float) juce::jlimit (-1.0, 1.0, pitchSemis / 24.0);
        tgtTilt = (float) juce::jlimit (0.16, 0.42, 0.28 * std::pow (2.0, formantSemis / 18.0));
        ratio = (float) timeRatio;
    }
    void setRendering (bool r, float progress) { rendering = r; renderProgress = progress; }
    void setPreview (bool playing, float level, float timeRatio)
    {
        previewing = playing;
        audioLevel = playing ? juce::jlimit (0.0f, 1.0f, level * 4.0f) : 0.0f;
        previewRatio = timeRatio;
    }
    void kick (float amount) { energy = juce::jmin (1.4f, energy + amount); if (! isTimerRunning()) startTimerHz (30); }
    void flash() { flashPos = 0.0f; energy = juce::jmin (1.4f, energy + 0.8f); }

    void paint (juce::Graphics& g) override
    {
        if (glitchFrames > 0)
        {
            // Glitch: Tunnel in ein Bild, in versetzten Streifen ausgeben (die ruhigen Zonen bleiben stehen)
            const float sc = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
            juce::Image img (juce::Image::ARGB, juce::roundToInt (getWidth() * sc), juce::roundToInt (getHeight() * sc), true);
            {
                juce::Graphics ig (img);
                ig.addTransform (juce::AffineTransform::scale (sc));
                drawTunnel (ig);
            }
            juce::Random rnd (glitchSeed);
            const float top = centre.y - 40.0f, bottom = centre.y + 240.0f, H = (float) getHeight();
            float y = 0.0f;
            while (y < H)
            {
                const bool zone = y >= top && y < bottom;
                const float hgt = zone ? 4.0f + rnd.nextFloat() * 22.0f : (y < top ? top - y : H - y);
                const float dx = zone && rnd.nextFloat() < 0.6f ? (rnd.nextFloat() - 0.5f) * 10.0f : 0.0f;
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (juce::Rectangle<float> (0.0f, y, (float) getWidth(), hgt).getSmallestIntegerContainer());
                g.drawImageTransformed (img, juce::AffineTransform::scale (1.0f / sc).translated (dx, 0.0f));
                if (dx != 0.0f)
                {
                    g.setOpacity (0.35f);
                    g.drawImageTransformed (img, juce::AffineTransform::scale (1.0f / sc).translated (-dx * 0.5f, 0.0f));
                }
                y += hgt;
            }
        }
        else drawTunnel (g);
        drawCalm (g);
    }

private:
    static constexpr float rMax = 310.0f, rMin = 14.0f;

    float ringY (float r) const
    {
        const float K = 2300.0f * depth;
        return centre.y + K * (1.0f / r - 1.0f / rMax) * 1.05f;
    }

    // Farben je nach Pitch: hoch -> mehr Cyan, tief -> mehr Violett
    juce::Colour leftCol() const  { return col::vio.interpolatedWith (col::cyan, juce::jmax (0.0f, twist) * 0.75f); }
    juce::Colour rightCol() const { return col::cyan.interpolatedWith (col::vio, juce::jmax (0.0f, -twist) * 0.75f); }

    juce::Path ringPath (float r, float ripple, float phase) const
    {
        juce::Path p;
        const int S = 72;
        const float cy = ringY (r);
        for (int k = 0; k <= S; ++k)
        {
            const float a = (float) k / S * juce::MathConstants<float>::twoPi;
            const float rr = r * (1.0f + ripple * std::sin (a * 5.0f + phase + r * 0.02f));
            const juce::Point<float> pt (centre.x + std::cos (a) * rr, cy + std::sin (a) * rr * tilt);
            if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    }

    void drawTunnel (juce::Graphics& g)
    {
        const float cx = centre.x;
        const float ty = ringY (rMin);
        const float e = juce::jmin (1.0f, energy);
        const float act = juce::jmin (1.0f, e + audioLevel * 0.8f + (rendering ? 0.35f + 0.5f * renderProgress : 0.0f));
        const juce::Colour lc = leftCol(), rc = rightCol();

        // Glow am Schlund
        {
            const float glow = 0.8f + 0.7f * act + (flashPos >= 0.0f ? 0.8f * (1.0f - flashPos) : 0.0f);
            const float rad = 90.0f * (0.85f + 0.4f * act);
            juce::ColourGradient tg (juce::Colour::fromFloatRGBA (0.78f, 0.96f, 1.0f, juce::jmin (1.0f, 0.75f * glow)), cx, ty,
                                     lc.withAlpha (0.0f), cx + rad, ty, true);
            tg.addColour (0.18, rc.withAlpha (juce::jmin (1.0f, 0.42f * glow)));
            tg.addColour (0.50, lc.withAlpha (juce::jmin (1.0f, 0.15f * glow)));
            g.setGradientFill (tg);
            g.fillEllipse (cx - rad, ty - rad * 0.66f, rad * 2.0f, rad * 1.33f);
        }

        // Meridiane
        const int NR = 30, M = 48;
        const float twistRad = twist * 1.4f + spin;
        for (int k = 0; k < M; ++k)
        {
            const float a0 = (float) k / M * juce::MathConstants<float>::twoPi;
            juce::Path p;
            for (int i = 0; i <= NR; ++i)
            {
                const float t = (float) i / NR;
                const float r = rMin * std::pow (rMax / rMin, t);
                const float a = a0 + twistRad * (1.0f - t);
                const juce::Point<float> pt (cx + std::cos (a) * r, ringY (r) + std::sin (a) * r * tilt);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            const bool front = std::sin (a0) > 0.0f;
            const float al = (front ? 0.20f : 0.10f) * (1.0f + 0.9f * act);
            g.setGradientFill (juce::ColourGradient (rc.withAlpha (juce::jmin (1.0f, al)), cx, ty, lc.withAlpha (0.0f), cx, centre.y + rMax * tilt, false));
            g.strokePath (p, juce::PathStrokeType (0.6f));
        }

        // Ringe (strömen nach innen, wenn etwas passiert)
        const float ripple = 0.018f * e + 0.03f * audioLevel;
        for (int i = 0; i < NR + 1; ++i)
        {
            const float t = ((float) i + flow) / NR;      // flow 0..1 verschiebt alle Ringe
            if (t > 1.0f) continue;
            const float r = rMin * std::pow (rMax / rMin, t);
            const float edge = juce::jmin (1.0f, t * 8.0f) * juce::jmin (1.0f, (1.0f - t) * 6.0f + 0.15f);
            const float al = (0.62f * std::pow (1.0f - t, 0.9f) + 0.04f) * edge * (1.0f + 0.6f * act);
            juce::ColourGradient gr (lc.withAlpha (juce::jmin (1.0f, al)), cx - r, 0, rc.withAlpha (juce::jmin (1.0f, al)), cx + r, 0, false);
            gr.addColour (0.5, juce::Colour (0xffb046ff).interpolatedWith (rc, juce::jmax (0.0f, twist)).withAlpha (juce::jmin (1.0f, al * 0.75f)));
            g.setGradientFill (gr);
            const float w = 0.5f + 1.0f * (1.0f - t);
            if (ripple > 0.001f) g.strokePath (ringPath (r, ripple, wavePhase), juce::PathStrokeType (w));
            else g.drawEllipse (cx - r, ringY (r) - r * tilt, r * 2.0f, r * 2.0f * tilt, w);
        }

        // Lichtpunkte, die in den Schlund spiralen (Rendern und Vorhören)
        if (particleAlpha > 0.01f)
        {
            for (auto& pt : particles)
            {
                const float t = pt.t;
                const float r = rMin * std::pow (rMax / rMin, t);
                const float a = pt.a + twistRad * (1.0f - t) + (1.0f - t) * 2.2f;
                const juce::Point<float> pos (cx + std::cos (a) * r, ringY (r) + std::sin (a) * r * tilt);
                const float al = particleAlpha * juce::jmin (1.0f, t * 5.0f) * std::pow (1.0f - t, 1.6f);   // außen fast unsichtbar, zum Schlund hin hell
                const float sz = 1.2f + 1.6f * t;
                g.setColour (rc.withAlpha (0.25f * al)); g.fillEllipse (pos.x - sz * 2.0f, pos.y - sz * 2.0f, sz * 4.0f, sz * 4.0f);
                g.setColour (juce::Colours::white.withAlpha (0.85f * al)); g.fillEllipse (pos.x - sz * 0.5f, pos.y - sz * 0.5f, sz, sz);
            }
        }

        // Scan-Ring (selten, ruhig)
        if (scanPos >= 0.0f) drawBrightRing (g, scanPos, 0.85f * std::sqrt (std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, 1.0f - scanPos))));
        // Lichtimpuls nach fertigem Render: läuft vom Schlund nach außen
        if (flashPos >= 0.0f) drawBrightRing (g, flashPos, 1.0f - flashPos * 0.8f);

        // Kern
        {
            const float k = 1.0f + 0.6f * act;
            juce::ColourGradient cg (juce::Colours::white.withAlpha (0.95f), cx, ty, rc.withAlpha (0.0f), cx + 12.0f * k, ty, true);
            g.setGradientFill (cg);
            g.fillEllipse (cx - 16.0f * k, ty - 7.0f * k, 32.0f * k, 14.0f * k);
        }
    }

    void drawBrightRing (juce::Graphics& g, float pos, float alpha)
    {
        const float cx = centre.x;
        const float r = std::exp (std::log (rMin) + (std::log (rMax) - std::log (rMin)) * pos), y = ringY (r);
        for (int pass = 0; pass < 2; ++pass)
        {
            const float al = juce::jlimit (0.0f, 1.0f, alpha * (pass ? 1.0f : 0.25f));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb478ff).withAlpha (al), cx - r, 0, juce::Colour (0xff82f0ff).withAlpha (al), cx + r, 0, false));
            g.drawEllipse (cx - r, y - r * tilt, r * 2.0f, r * 2.0f * tilt, pass ? 1.1f : 5.0f);
        }
    }

    // ruhige Zonen hinter den Reglern
    void drawCalm (juce::Graphics& g)
    {
        for (auto sp : calmSpots)
        {
            juce::ColourGradient s (juce::Colour (0xf20d0e12), sp.x, sp.y, juce::Colour (0x000d0e12), sp.x + 200.0f, sp.y, true);
            s.addColour (0.2, juce::Colour (0xf20d0e12));
            s.addColour (0.6, juce::Colour (0xa60d0e12));
            g.setGradientFill (s);
            g.fillRect (sp.x - 210.0f, sp.y - 210.0f, 420.0f, 440.0f);
        }
    }

    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        static const bool fixedStep = juce::SystemStats::getEnvironmentVariable ("WORMHOLE_FIXED_DT", {}).isNotEmpty(); // nur für Test-Filme
        const float dt = fixedStep ? 0.04f : (float) juce::jlimit (0.0, 0.1, now - lastTick);
        lastTick = now;
        bool dirty = false;

        // weiche Annäherung an die Zielwerte
        const float k = 1.0f - std::exp (-dt * 6.0f);
        auto ease = [&] (float& v, float tgt) { if (std::abs (tgt - v) > 0.0005f) { v += (tgt - v) * k; dirty = true; } };
        ease (depth, tgtDepth); ease (twist, tgtTwist); ease (tilt, tgtTilt);

        // Strömung: Impuls, Vorhören (Tempo des Ergebnisses), Rendern (beschleunigt)
        float speed = energy * 0.9f;
        if (previewing) speed += 0.30f / std::pow (juce::jmax (0.05f, previewRatio), 0.35f) + audioLevel * 0.4f;
        if (rendering) speed += 0.5f + 2.6f * renderProgress * renderProgress;
        if (speed > 0.002f)
        {
            flow -= speed * dt; while (flow < 0.0f) flow += 1.0f;
            wavePhase += dt * (2.0f + 4.0f * energy);
            spin += (energy * 0.25f + (rendering ? 0.15f : 0.0f)) * dt * (twist >= 0.0f ? 1.0f : -1.0f);
            dirty = true;
        }
        else if (std::abs (spin) > 0.0005f) { spin *= std::exp (-dt * 1.5f); dirty = true; }  // Verdrehung kehrt zurück
        if (energy > 0.0f) { energy = juce::jmax (0.0f, energy * std::exp (-dt * 2.2f) - 0.001f); dirty = true; }

        // Lichtpunkte
        const float pTarget = rendering ? 1.0f : previewing ? 0.35f + 0.5f * audioLevel : 0.0f;
        if (std::abs (pTarget - particleAlpha) > 0.003f) { particleAlpha += (pTarget - particleAlpha) * (1.0f - std::exp (-dt * 3.0f)); dirty = true; }
        if (particleAlpha > 0.01f)
            for (auto& pt : particles)
            {
                pt.t -= dt * pt.v * (0.12f + 0.25f * speed);
                if (pt.t < 0.0f) { pt.t = 1.0f; pt.a = random.nextFloat() * juce::MathConstants<float>::twoPi; }
            }

        // Lichtimpuls
        if (flashPos >= 0.0f) { flashPos += dt / 0.9f; if (flashPos > 1.0f) flashPos = -1.0f; dirty = true; }

        // Scan (nur in Ruhe, damit nichts doppelt läuft)
        const bool calm = ! rendering && ! previewing && energy < 0.05f;
        if (scanPos < 0.0f && now >= nextScan && calm)
        {
            scanPos = 1.0f;
            glitchAt = random.nextFloat() < 0.3f ? 0.25f + random.nextFloat() * 0.5f : -1.0f;
        }
        if (scanPos >= 0.0f)
        {
            const float prev = scanPos;
            scanPos -= dt / 1.6f;
            if (glitchAt > 0.0f && prev > glitchAt && scanPos <= glitchAt) { glitchFrames = 3; glitchSeed = random.nextInt64(); }
            if (scanPos < 0.0f) { scanPos = -1.0f; nextScan = now + 4.5 + random.nextDouble() * 2.5; }
            dirty = true;
        }
        if (! calm && scanPos < 0.0f) nextScan = juce::jmax (nextScan, now + 3.0);
        if (glitchFrames > 0) { --glitchFrames; dirty = true; }
        if (rendering != wasRendering || previewing != wasPreviewing) { wasRendering = rendering; wasPreviewing = previewing; dirty = true; }
        if (dirty) repaint();
    }

    struct Particle { float a = 0, t = 0, v = 1; };
    std::array<Particle, 42> particles;

    float depth = 1.0f, tgtDepth = 1.0f, twist = 0.0f, tgtTwist = 0.0f, tilt = 0.28f, tgtTilt = 0.28f, ratio = 1.0f;
    float energy = 0.0f, flow = 0.0f, wavePhase = 0.0f, spin = 0.0f, particleAlpha = 0.0f, flashPos = -1.0f;
    float scanPos = -1.0f, glitchAt = -1.0f;
    int glitchFrames = 0; juce::int64 glitchSeed = 0;
    double nextScan = 0.0, lastTick = 0.0;
    bool rendering = false, wasRendering = false, previewing = false, wasPreviewing = false;
    float renderProgress = 0.0f, audioLevel = 0.0f, previewRatio = 1.0f;
    juce::Random random;
};
} // namespace whl
