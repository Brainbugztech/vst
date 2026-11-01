#pragma once

// Step-Delay-Look nach Benetts Entwurf: lila Cyberpunk-Rahmen, Neon-Grün und Magenta,
// Regler mit farbigen Ringen und Skalen-Strichen.

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzLook.h"

namespace sd
{
namespace col
{
    inline const juce::Colour bg       { 0xff0d0614 };
    inline const juce::Colour bgPurple { 0xff2a0f45 };
    inline const juce::Colour panel    { 0xff150a20 };
    inline const juce::Colour panel2   { 0xff21122f };
    inline const juce::Colour edge     { 0xff4a2a66 };
    inline const juce::Colour green    { 0xffb4ff1e };
    inline const juce::Colour greenDk  { 0xff4fa800 };
    inline const juce::Colour magenta  { 0xffff24e0 };
    inline const juce::Colour magentaDk{ 0xff7a0f6c };
    inline const juce::Colour cyan     { 0xff2ee6ff };
    inline const juce::Colour yellow   { 0xffd9ff1e };
    inline const juce::Colour ink      { 0xffeee6f6 };
    inline const juce::Colour inkDim   { 0xffa595b8 };
}

// Rechteck mit abgeschrägten Ecken (oben links und unten rechts)
inline juce::Path chamfer (juce::Rectangle<float> r, float c, bool allCorners = false)
{
    juce::Path p;
    const float c2 = allCorners ? c : 0.0f;
    p.startNewSubPath (r.getX() + c, r.getY());
    p.lineTo (r.getRight() - c2, r.getY());
    p.lineTo (r.getRight(), r.getY() + c2);
    p.lineTo (r.getRight(), r.getBottom() - c);
    p.lineTo (r.getRight() - c, r.getBottom());
    p.lineTo (r.getX() + c2, r.getBottom());
    p.lineTo (r.getX(), r.getBottom() - c2);
    p.lineTo (r.getX(), r.getY() + c);
    p.closeSubPath();
    return p;
}

// Panel mit Neon-Kante: glow = Leuchtstärke der Kante
inline void neonPanel (juce::Graphics& g, juce::Rectangle<float> r, float u, juce::Colour edgeCol, float glow, float corner = 14.0f)
{
    auto p = chamfer (r, corner * u);
    juce::ColourGradient fill (col::panel.withAlpha (0.92f), r.getX(), r.getY(), juce::Colour (0xff0b0512).withAlpha (0.95f), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillPath (p);
    if (glow > 0.0f)
    {
        g.setColour (edgeCol.withAlpha (0.18f * glow));
        g.strokePath (p, juce::PathStrokeType (7.0f * u));
        g.setColour (edgeCol.withAlpha (0.35f * glow));
        g.strokePath (p, juce::PathStrokeType (3.5f * u));
    }
    g.setColour (edgeCol);
    g.strokePath (p, juce::PathStrokeType (1.4f * u));
}

// "///"-Streifen als Deko
inline void slashes (juce::Graphics& g, float x, float y, float h, int count, juce::Colour c, float u)
{
    for (int i = 0; i < count; ++i)
    {
        juce::Path p;
        const float x0 = x + (float) i * 7.0f * u;
        p.startNewSubPath (x0 + h * 0.45f, y);
        p.lineTo (x0 + h * 0.45f + 3.5f * u, y);
        p.lineTo (x0 + 3.5f * u, y + h);
        p.lineTo (x0, y + h);
        p.closeSubPath();
        g.setColour (c.withAlpha (1.0f - 0.12f * (float) i));
        g.fillPath (p);
    }
}

class LookAndFeel : public bb::LookAndFeel
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float, float, juce::Slider& s) override
    {
        const float size = (float) std::min (w, h);
        const float u = size / 100.0f;
        const juce::Point<float> c ((float) x + w * 0.5f, (float) y + h * 0.5f);
        const auto ring = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("ring", (juce::int64) col::magenta.getARGB()));
        const float a0 = startAngle, a1 = endAngle;
        const float a = a0 + pos * (a1 - a0);

        // Skalen-Striche außen
        for (int i = 0; i <= 40; ++i)
        {
            const float t = (float) i / 40.0f;
            const float ang = a0 + t * (a1 - a0);
            const float r0 = 44.0f * u, r1 = (i % 5 == 0 ? 49.0f : 47.0f) * u;
            const auto d = juce::Point<float> (std::sin (ang), -std::cos (ang));
            g.setColour ((t <= pos ? ring.withAlpha (0.55f) : col::edge.withAlpha (0.8f)));
            g.drawLine ({ c + d * r0, c + d * r1 }, 1.1f * u);
        }

        // dunkle Fassung
        g.setColour (juce::Colour (0xff07030b));
        g.fillEllipse (c.x - 41 * u, c.y - 41 * u, 82 * u, 82 * u);

        // Ring: gedimmter Rest + leuchtender Wert
        const float rr = 35.0f * u, th = 6.0f * u;
        juce::Path track;
        track.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, a1, true);
        g.setColour (ring.withAlpha (0.18f));
        g.strokePath (track, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        if (pos > 0.002f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, a, true);
            g.setColour (ring.withAlpha (0.22f));
            g.strokePath (val, juce::PathStrokeType (th * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (ring);
            g.strokePath (val, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        // Knopf mit Metall-Verlauf
        juce::ColourGradient body (juce::Colour (0xff3a3442), c.x - 10 * u, c.y - 26 * u, juce::Colour (0xff09070c), c.x + 12 * u, c.y + 26 * u, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - 26 * u, c.y - 26 * u, 52 * u, 52 * u);
        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - 26 * u, c.y - 26 * u, 52 * u, 52 * u, 1.5f * u);
        juce::ColourGradient cap (juce::Colour (0xff221d28), c.x, c.y - 18 * u, juce::Colour (0xff0e0b12), c.x, c.y + 18 * u, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - 18 * u, c.y - 18 * u, 36 * u, 36 * u);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - 18 * u, c.y - 18 * u, 36 * u, 36 * u, 1.0f * u);

        // Zeiger
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.6f * u, -25.0f * u, 3.2f * u, 9.0f * u, 1.4f * u);
        g.setColour (juce::Colours::white);
        g.fillPath (pointer, juce::AffineTransform::rotation (a).translated (c));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const float u = (float) b.getHeight() / 32.0f;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto p = chamfer (r, 5.0f * u, true);
        const bool on = b.getToggleState();
        const bool accent = (bool) b.getProperties().getWithDefault ("accent", false);
        const auto hot = col::magenta;

        juce::ColourGradient grad (on ? juce::Colour (0xff3a0f3d) : juce::Colour (0xff2a2232),
                                   0, r.getY(), on ? juce::Colour (0xff1c0820) : juce::Colour (0xff120d17), 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (p);

        if (on || accent)
        {
            g.setColour (hot.withAlpha (on ? 0.35f : 0.22f));
            g.strokePath (p, juce::PathStrokeType (4.0f * u));
            g.setColour (hot);
            g.strokePath (p, juce::PathStrokeType (1.6f * u));
        }
        else
        {
            g.setColour (over ? col::edge.brighter (0.5f) : juce::Colour (0xff05030a));
            g.strokePath (p, juce::PathStrokeType (1.2f * u));
            g.setColour (juce::Colours::white.withAlpha (down ? 0.03f : 0.09f));
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 5 * u, r.getRight() - 5 * u);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        const bool accent = (bool) b.getProperties().getWithDefault ("accent", false);
        g.setFont (bb::fonts::display ((float) b.getHeight() * 0.44f, true));
        g.setColour (on || accent ? col::magenta.brighter (0.35f) : col::ink.withAlpha (0.88f));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }
};
} // namespace sd
