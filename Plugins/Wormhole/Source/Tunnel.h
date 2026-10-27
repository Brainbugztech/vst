#pragma once

// Das Wurmloch: ein Trichter aus feinen Ringen (Flamm-Paraboloid, schräg von oben).
// Ruhig im Normalfall. Alle paar Sekunden läuft ein heller Scan-Ring in den Schlund,
// gelegentlich mit einem kurzen Glitch. Tempo verändert die Tiefe, Pitch verdreht die Linien.
// Beim Rendern läuft der Scan schneller und der Schlund leuchtet mit dem Fortschritt.

#include "WormholeLook.h"

namespace whl
{
class Tunnel : public juce::Component, private juce::Timer
{
public:
    Tunnel()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
        nextScan = juce::Time::getMillisecondCounterHiRes() * 0.001 + 2.5;
        startTimerHz (30);
    }

    // Design-Koordinaten (960 x 600 Fläche)
    juce::Point<float> centre { 480.0f, 150.0f };
    std::array<juce::Point<float>, 2> calmSpots { juce::Point<float> { 165.0f, 250.0f }, { 795.0f, 250.0f } };

    void setTargets (double timeRatio, double pitchSemis)
    {
        tgtDepth = (float) juce::jlimit (0.55, 1.6, std::pow (timeRatio, 0.55));
        tgtTwist = (float) juce::jlimit (-1.0, 1.0, pitchSemis / 24.0);
    }
    void setRendering (bool r, float progress) { rendering = r; renderProgress = progress; }

    void paint (juce::Graphics& g) override
    {
        if (glitchFrames > 0)
        {
            // Glitch: Tunnel in ein Bild zeichnen und in versetzten Streifen ausgeben
            const float sc = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
            juce::Image img (juce::Image::ARGB, juce::roundToInt (getWidth() * sc), juce::roundToInt (getHeight() * sc), true);
            {
                juce::Graphics ig (img);
                ig.addTransform (juce::AffineTransform::scale (sc));
                drawTunnel (ig, false);
            }
            juce::Random rnd (glitchSeed);
            const float top = centre.y - 40.0f, bottom = centre.y + 220.0f, H = (float) getHeight();
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
            drawCalm (g);
            return;
        }
        drawTunnel (g, true);
    }

private:
    float ringY (float r) const
    {
        const float K = 2300.0f * depth, rMax = 310.0f;
        return centre.y + K * (1.0f / r - 1.0f / rMax) * 1.05f;
    }

    void drawTunnel (juce::Graphics& g, bool withCalm)
    {
        const float cx = centre.x, tilt = 0.28f, rMax = 310.0f, rMin = 14.0f;
        const int NR = 31;
        float rings[NR];
        for (int i = 0; i < NR; ++i) rings[i] = rMin * std::pow (rMax / rMin, (float) i / (NR - 1));
        const float ty = ringY (rMin);

        // Glow am Schlund
        const float glow = 0.85f + (rendering ? 0.15f + 0.6f * renderProgress : 0.0f);
        {
            juce::ColourGradient tg (juce::Colour::fromFloatRGBA (0.78f, 0.96f, 1.0f, 0.85f * juce::jmin (1.0f, glow)), cx, ty,
                                     juce::Colour::fromFloatRGBA (0.55f, 0.24f, 1.0f, 0.0f), cx + 90.0f, ty, true);
            tg.addColour (0.18, col::cyan.withAlpha (0.45f * glow));
            tg.addColour (0.50, col::vio.withAlpha (0.16f * glow));
            g.setGradientFill (tg);
            g.fillEllipse (cx - 90.0f, ty - 60.0f, 180.0f, 120.0f);
        }

        // Meridiane
        const int M = 48;
        const float twistRad = twist * 1.4f;
        for (int k = 0; k < M; ++k)
        {
            const float a0 = (float) k / M * juce::MathConstants<float>::twoPi;
            juce::Path p;
            bool front = std::sin (a0) > 0.0f;
            for (int i = 0; i < NR; ++i)
            {
                const float r = rings[i];
                const float a = a0 + twistRad * (1.0f - (float) i / (NR - 1)); // zum Schlund hin verdreht
                const juce::Point<float> pt (cx + std::cos (a) * r, ringY (r) + std::sin (a) * r * tilt);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            juce::ColourGradient gr (col::cyan.withAlpha (front ? 0.20f : 0.10f), cx, ty,
                                     col::vio.withAlpha (0.0f), cx, centre.y + rMax * tilt, false);
            g.setGradientFill (gr);
            g.strokePath (p, juce::PathStrokeType (0.6f));
        }

        // Ringe
        for (int i = 0; i < NR; ++i)
        {
            const float t = (float) i / (NR - 1), r = rings[i];
            const float al = 0.62f * std::pow (1.0f - t, 0.9f) + 0.04f;
            juce::ColourGradient gr (col::vio.withAlpha (al), cx - r, 0, col::cyan.withAlpha (al), cx + r, 0, false);
            gr.addColour (0.5, juce::Colour (0xffb046ff).withAlpha (al * 0.75f));
            g.setGradientFill (gr);
            g.drawEllipse (cx - r, ringY (r) - r * tilt, r * 2.0f, r * 2.0f * tilt, 0.5f + 1.0f * (1.0f - t));
        }

        // Scan-Ring
        if (scanPos >= 0.0f)
        {
            const float lr = std::log (rMin) + (std::log (rMax) - std::log (rMin)) * scanPos;
            const float r = std::exp (lr), y = ringY (r);
            const float fade = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, 1.0f - scanPos));
            const float al = 0.85f * std::sqrt (fade);
            for (int pass = 0; pass < 2; ++pass)
            {
                juce::ColourGradient gr (juce::Colour (0xffb478ff).withAlpha (al * (pass ? 1.0f : 0.25f)), cx - r, 0,
                                         juce::Colour (0xff82f0ff).withAlpha (al * (pass ? 1.0f : 0.25f)), cx + r, 0, false);
                g.setGradientFill (gr);
                g.drawEllipse (cx - r, y - r * tilt, r * 2.0f, r * 2.0f * tilt, pass ? 1.1f : 5.0f);
            }
        }

        // Kern
        {
            juce::ColourGradient cg (juce::Colours::white.withAlpha (0.95f), cx, ty, col::cyan.withAlpha (0.0f), cx + 12.0f, ty, true);
            g.setGradientFill (cg);
            g.fillEllipse (cx - 16.0f, ty - 7.0f, 32.0f, 14.0f);
        }

        if (withCalm) drawCalm (g);
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
        const double dt = juce::jlimit (0.0, 0.1, now - lastTick);
        lastTick = now;
        bool dirty = false;

        // weiche Annäherung an die Zielwerte (Tempo/Pitch)
        const float k = 1.0f - std::exp (-(float) dt * 6.0f);
        if (std::abs (tgtDepth - depth) > 0.0005f) { depth += (tgtDepth - depth) * k; dirty = true; }
        if (std::abs (tgtTwist - twist) > 0.0005f) { twist += (tgtTwist - twist) * k; dirty = true; }

        // Scan
        if (scanPos < 0.0f && now >= nextScan)
        {
            scanPos = 1.0f;
            glitchAt = random.nextFloat() < 0.3f ? 0.25f + random.nextFloat() * 0.5f : -1.0f;
        }
        if (scanPos >= 0.0f)
        {
            const float speed = rendering ? 1.0f / 0.8f : 1.0f / 1.6f;
            const float prev = scanPos;
            scanPos -= (float) dt * speed;
            if (glitchAt > 0.0f && prev > glitchAt && scanPos <= glitchAt) { glitchFrames = 3; glitchSeed = random.nextInt64(); }
            if (scanPos < 0.0f)
            {
                scanPos = -1.0f;
                nextScan = now + (rendering ? 0.15 : 4.5 + random.nextDouble() * 2.5);
            }
            dirty = true;
        }
        if (glitchFrames > 0) { --glitchFrames; dirty = true; }
        if (rendering != wasRendering || (rendering && std::abs (renderProgress - lastProgress) > 0.01f))
        {
            wasRendering = rendering; lastProgress = renderProgress; dirty = true;
            if (rendering && scanPos < 0.0f) nextScan = now;
        }
        if (dirty) repaint();
    }

    float depth = 1.0f, tgtDepth = 1.0f, twist = 0.0f, tgtTwist = 0.0f;
    float scanPos = -1.0f, glitchAt = -1.0f;
    int glitchFrames = 0; juce::int64 glitchSeed = 0;
    double nextScan = 0.0, lastTick = 0.0;
    bool rendering = false, wasRendering = false;
    float renderProgress = 0.0f, lastProgress = 0.0f;
    juce::Random random;
};
} // namespace whl
