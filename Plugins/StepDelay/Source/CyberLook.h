#pragma once

// Step-Delay-Look nach Benetts Vorlage (Assets/stepdelay_bg.jpg).
// Alle Koordinaten im Editor sind Pixel dieser Vorlage (1774 x 887).

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzLook.h"
#include "StepDelayAssets.h"

namespace sd
{
namespace col
{
    inline const juce::Colour field    { 0xff120a1b };
    inline const juce::Colour slotTop  { 0xff1f1030 };
    inline const juce::Colour slotBot  { 0xff12081a };
    inline const juce::Colour sep      { 0xff3a2352 };
    inline const juce::Colour green    { 0xffb8ff1e };
    inline const juce::Colour greenHi  { 0xffdcff4a };
    inline const juce::Colour greenDk  { 0xff4cbf00 };
    inline const juce::Colour magenta  { 0xffff1ee6 };
    inline const juce::Colour magentaDk{ 0xffb800aa };
    inline const juce::Colour cyan     { 0xff2ee6ff };
    inline const juce::Colour lime     { 0xffcfff1a };
    inline const juce::Colour ink      { 0xffe8def4 };
    inline const juce::Colour inkDim   { 0xffb3a3c8 };
    inline const juce::Colour lilac    { 0xffcbbbe6 };
}

// Schrift wie in der Vorlage (Rajdhani Bold, kommt im Build dazu), sonst Chakra Petch
inline juce::Font font (float height)
{
    static juce::Typeface::Ptr tf = []
    {
        int size = 0;
        if (auto* data = SDAssets::getNamedResource ("RajdhaniBold_ttf", size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        return juce::Typeface::Ptr();
    }();
    if (tf != nullptr)
        return juce::Font (juce::FontOptions (tf).withHeight (height));
    return bb::fonts::display (height * 0.86f, true);
}

// Rechteck mit vier abgeschrägten Ecken
inline juce::Path chamfer (juce::Rectangle<float> r, float c)
{
    juce::Path p;
    p.startNewSubPath (r.getX() + c, r.getY());
    p.lineTo (r.getRight() - c, r.getY());
    p.lineTo (r.getRight(), r.getY() + c);
    p.lineTo (r.getRight(), r.getBottom() - c);
    p.lineTo (r.getRight() - c, r.getBottom());
    p.lineTo (r.getX() + c, r.getBottom());
    p.lineTo (r.getX(), r.getBottom() - c);
    p.lineTo (r.getX(), r.getY() + c);
    p.closeSubPath();
    return p;
}

class LookAndFeel : public bb::LookAndFeel
{
public:
    // Regler wie in der Vorlage: farbiger Ring, dunkle Fassung, Metallknopf mit weißem Zeiger
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float, float, juce::Slider& s) override
    {
        const float size = (float) std::min (w, h);
        const float u = size / 176.0f;  // Komponente ist 176 Vorlage-Pixel groß
        const juce::Point<float> c ((float) x + w * 0.5f, (float) y + h * 0.5f);
        const auto ring = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("ring", (juce::int64) col::magenta.getARGB()));
        const float a0 = startAngle, a1 = endAngle;
        const float a = a0 + pos * (a1 - a0);
        auto circle = [&] (float r) { return juce::Rectangle<float> (c.x - r, c.y - r, r * 2, r * 2); };

        // Grund (deckt die Stelle in der Vorlage ab)
        g.setColour (juce::Colour (0xff0d0612));
        g.fillEllipse (circle (85 * u));

        // Ring
        const float rr = 74.0f * u, th = 10.0f * u;
        juce::Path track;
        track.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, a1, true);
        g.setColour (ring.withAlpha (0.16f));
        g.strokePath (track, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        if (pos > 0.002f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, a, true);
            g.setColour (ring.withAlpha (0.16f));
            g.strokePath (val, juce::PathStrokeType (th * 2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (ring.withAlpha (0.35f));
            g.strokePath (val, juce::PathStrokeType (th * 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (ring);
            g.strokePath (val, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (ring.brighter (0.6f).withAlpha (0.7f));
            g.strokePath (val, juce::PathStrokeType (th * 0.25f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
        // Markierung oben (12 Uhr)
        g.setColour (juce::Colour (0xff0d0612));
        g.fillRect (juce::Rectangle<float> (c.x - 1.5f * u, c.y - rr - th, 3.0f * u, th * 2));

        // dunkle Fassung
        juce::ColourGradient rim (juce::Colour (0xff2c2433), c.x, c.y - 60 * u, juce::Colour (0xff050307), c.x, c.y + 60 * u, false);
        g.setGradientFill (rim);
        g.fillEllipse (circle (60 * u));
        g.setColour (juce::Colour (0xff050307));
        g.fillEllipse (circle (54 * u));
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (circle (54 * u), 1.0f * u);

        // Metallknopf
        juce::ColourGradient body (juce::Colour (0xff5a5463), c.x - 18 * u, c.y - 40 * u, juce::Colour (0xff0f0d13), c.x + 20 * u, c.y + 42 * u, false);
        body.addColour (0.45, juce::Colour (0xff2b2731));
        g.setGradientFill (body);
        g.fillEllipse (circle (44 * u));
        juce::ColourGradient cap (juce::Colour (0xff2a2630), c.x, c.y - 30 * u, juce::Colour (0xff121016), c.x, c.y + 30 * u, false);
        g.setGradientFill (cap);
        g.fillEllipse (circle (31 * u));
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawEllipse (circle (31 * u), 1.2f * u);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (circle (44 * u), 1.5f * u);

        // Zeiger (Tropfen am Rand des Knopfs)
        juce::Path pointer;
        pointer.startNewSubPath (0.0f, -42.0f * u);
        pointer.quadraticTo (3.5f * u, -36.0f * u, 0.0f, -31.0f * u);
        pointer.quadraticTo (-3.5f * u, -36.0f * u, 0.0f, -42.0f * u);
        pointer.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.strokePath (pointer, juce::PathStrokeType (3.0f * u), juce::AffineTransform::rotation (a).translated (c));
        g.setColour (juce::Colours::white);
        g.fillPath (pointer, juce::AffineTransform::rotation (a).translated (c));
    }

    // Taster wie in der Vorlage: abgeschrägt, lila-grau, aktiv = Magenta mit Leuchtkante
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const float u = (float) b.getHeight() / 48.0f;
        auto outer = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState() || (bool) b.getProperties().getWithDefault ("accent", false);

        // äußere dunkle Fassung
        g.setColour (juce::Colour (0xff07040b));
        g.fillPath (chamfer (outer, 11 * u));
        g.setColour (juce::Colour (0xff2c2238));
        g.strokePath (chamfer (outer.reduced (0.5f), 11 * u), juce::PathStrokeType (1.0f * u));

        auto r = outer.reduced (4.0f * u);
        auto p = chamfer (r, 8 * u);
        if (on)
        {
            juce::ColourGradient grad (juce::Colour (0xff6a1470), 0, r.getY(), juce::Colour (0xff320838), 0, r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillPath (p);
            g.setColour (col::magenta.withAlpha (0.45f));
            g.strokePath (p, juce::PathStrokeType (5.0f * u));
            g.setColour (col::magenta.brighter (0.2f));
            g.strokePath (p, juce::PathStrokeType (2.2f * u));
        }
        else
        {
            juce::ColourGradient grad (juce::Colour (over ? 0xff474057 : 0xff3a3447), 0, r.getY(),
                                       juce::Colour (0xff1b1623), 0, r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillPath (p);
            g.setColour (juce::Colour (0xff5a5068).withAlpha (down ? 0.3f : 0.7f));
            g.strokePath (p, juce::PathStrokeType (1.0f * u));
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.drawHorizontalLine ((int) (r.getY() + 1.5f * u), r.getX() + 8 * u, r.getRight() - 8 * u);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState() || (bool) b.getProperties().getWithDefault ("accent", false);
        const bool arrow = (bool) b.getProperties().getWithDefault ("arrow", false);
        const float u = (float) b.getHeight() / 48.0f;
        if (arrow)
        {
            // gefülltes Dreieck wie in der Vorlage
            const auto c = b.getLocalBounds().toFloat().getCentre();
            const float s = 11 * u;
            const bool left = b.getButtonText() == "<";
            juce::Path t;
            if (left) t.addTriangle (c.x - s, c.y, c.x + s * 0.8f, c.y - s, c.x + s * 0.8f, c.y + s);
            else      t.addTriangle (c.x + s, c.y, c.x - s * 0.8f, c.y - s, c.x - s * 0.8f, c.y + s);
            g.setColour (col::lilac);
            g.fillPath (t);
            return;
        }
        g.setFont (font (25.0f * u));
        g.setColour (on ? juce::Colour (0xffffd0fa) : col::lilac);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1, 0.8f);
    }
};
} // namespace sd
