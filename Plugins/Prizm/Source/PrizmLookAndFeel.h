#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzLook.h" // gemeinsame Schriften (Chakra Petch, Share Tech Mono)

namespace prizm
{
// Hologram blue for the interface chrome, the Brain Bugz colours for signal and accents.
namespace Col
{
    inline const juce::Colour bg         { 0xff05080d };
    inline const juce::Colour glass      { 0xff0a1828 }; // panel fill (used with alpha)
    inline const juce::Colour glassDeep  { 0xff060e18 };
    inline const juce::Colour holo       { 0xff3fb8ff };
    inline const juce::Colour holoBright { 0xffa6e6ff };
    inline const juce::Colour holoDim    { 0xff1d5a86 };
    inline const juce::Colour ink        { 0xffe2f5ff };
    inline const juce::Colour muted      { 0xff7ea6c6 };
    inline const juce::Colour dim        { 0xff4a6984 };
    inline const juce::Colour lit        { 0xffbfe8ff }; // beleuchtetes Glas: Note in der Tonart, Wert bleibt
    inline const juce::Colour litInk     { 0xff061420 }; // Schrift auf beleuchtetem Glas
    inline const juce::Colour pink       { 0xffe65ccf }; // Magenta (MAP, Hinweise)
    inline const juce::Colour amber      { 0xffffaa28 };
    inline const juce::Colour cyan       { 0xff2fe6ff };
    inline const juce::Colour violet     { 0xff9b5cff };

    inline const juce::Colour crystal    { 0xffeaf7ff }; // eisweißes Licht
    inline const juce::Colour fringeWarm { 0xffff6a5c }; // Farbsaum (Prisma) warm
    inline const juce::Colour fringeCool { 0xff5c8dff }; // Farbsaum (Prisma) kalt

    // Spektralfarbe 0..1 (rot -> violett), leuchtend aber nicht grell
    inline juce::Colour prismHue (float t, float alpha = 1.0f)
    {
        const float hue = juce::jlimit (0.0f, 1.0f, t) * 0.78f; // rot bis violett
        return juce::Colour::fromHSV (hue, 0.62f, 1.0f, alpha);
    }
    // jede der 12 Noten hat ihre Spektralfarbe (C = rot ... B = violett)
    inline juce::Colour noteHue (int pc, float alpha = 1.0f) { return prismHue ((float) pc / 11.0f, alpha); }

    inline juce::ColourGradient spectrum (float x1, float y, float x2, float alpha = 1.0f)
    {
        juce::ColourGradient g (prismHue (0.0f, alpha), x1, y, prismHue (1.0f, alpha), x2, y, false);
        for (int i = 1; i < 6; ++i)
            g.addColour ((double) i / 6.0, prismHue ((float) i / 6.0f, alpha));
        return g;
    }
}

namespace holo
{
    // Little L-shaped brackets on the corners of a hologram frame.
    inline void cornerBrackets (juce::Graphics& g, juce::Rectangle<float> r, float len, juce::Colour c, float thickness = 1.6f)
    {
        g.setColour (c);
        const float t = thickness;
        g.fillRect (r.getX(), r.getY(), len, t);                     g.fillRect (r.getX(), r.getY(), t, len);
        g.fillRect (r.getRight() - len, r.getY(), len, t);           g.fillRect (r.getRight() - t, r.getY(), t, len);
        g.fillRect (r.getX(), r.getBottom() - t, len, t);            g.fillRect (r.getX(), r.getBottom() - len, t, len);
        g.fillRect (r.getRight() - len, r.getBottom() - t, len, t);  g.fillRect (r.getRight() - t, r.getBottom() - len, t, len);
    }

    // Translucent hologram glass box with a thin frame (for combo boxes, value readouts, buttons).
    inline void box (juce::Graphics& g, juce::Rectangle<float> r, float alpha = 1.0f, bool hot = false)
    {
        juce::ColourGradient fill (Col::glass.withAlpha (0.78f * alpha), 0.0f, r.getY(), Col::glassDeep.withAlpha (0.86f * alpha), 0.0f, r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Col::holo.withAlpha (hot ? 0.18f : 0.08f));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f), 3.0f);
        g.setColour (Col::holo.withAlpha ((hot ? 0.95f : 0.55f) * alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        cornerBrackets (g, r.reduced (0.5f), 5.0f, Col::holoBright.withAlpha ((hot ? 1.0f : 0.75f) * alpha), 1.4f);
    }
}

namespace holo
{
    // Glas-Kachel (Tasten oben, Ausgänge der Pitch Matrix).
    // tint: Farbe des Lichts im Glas, lit 0..1: wie hell es leuchtet, fill 0..1: Füllhöhe (Gewichtung),
    // glow 0..1 + glowColour: diffuses farbiges Aufleuchten (z. B. wenn die Note gerade reinkommt).
    inline void glassTile (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour tint, float lit, float glow, juce::Colour glowColour,
                           float fill = 1.0f, float corner = 4.0f)
    {
        using namespace juce;
        // Rauchglas
        ColourGradient base (Col::glass.withAlpha (0.55f), 0.0f, r.getY(), Col::glassDeep.withAlpha (0.80f), 0.0f, r.getBottom(), false);
        g.setGradientFill (base);
        g.fillRoundedRectangle (r, corner);

        Graphics::ScopedSaveState s (g);
        Path shape;
        shape.addRoundedRectangle (r, corner);
        g.reduceClipRegion (shape);

        // Licht im Glas, von unten bis zur Füllhöhe
        if (lit > 0.001f)
        {
            const auto light = r.withTop (r.getBottom() - r.getHeight() * jlimit (0.05f, 1.0f, fill));
            ColourGradient l (tint.withAlpha (0.05f + 0.22f * lit), 0.0f, light.getY(), tint.withAlpha (0.18f + 0.50f * lit), 0.0f, light.getBottom(), false);
            g.setGradientFill (l);
            g.fillRect (light);
            if (fill < 0.995f) // Lichtkante der Füllhöhe
            {
                g.setColour (tint.withAlpha (0.8f));
                g.fillRect (light.getX(), light.getY(), light.getWidth(), 1.2f);
            }
            g.setColour (tint.withAlpha (0.22f * lit)); // innerer Lichtschein am Rand (Glasdicke)
            g.drawRoundedRectangle (r.reduced (2.5f), corner - 1.5f, 2.0f);
        }

        // diffuses farbiges Aufleuchten von unten
        if (glow > 0.01f)
        {
            const float cx = r.getCentreX(), cy = r.getBottom() + r.getHeight() * 0.1f;
            ColourGradient d (glowColour.withAlpha (0.65f * glow), cx, cy, glowColour.withAlpha (0.0f), cx + r.getWidth() * 0.95f, cy, true);
            d.addColour (0.45, glowColour.withAlpha (0.25f * glow));
            g.setGradientFill (d);
            g.fillRect (r);
        }

        // Spiegelung oben
        ColourGradient spec (juce::Colours::white.withAlpha (0.16f + 0.10f * lit), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, r.getY() + r.getHeight() * 0.45f, false);
        g.setGradientFill (spec);
        g.fillRect (r.withHeight (r.getHeight() * 0.45f));
        g.setColour (juce::Colours::white.withAlpha (0.35f + 0.3f * lit));
        g.fillRoundedRectangle (r.getX() + corner, r.getY() + 1.2f, r.getWidth() - 2.0f * corner, 1.0f, 0.5f);

        // Kanten: Glasdicke innen dunkel, außen Licht, links/rechts feiner Farbsaum
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (1.5f), corner - 1.0f, 1.0f);
        g.setColour (tint.interpolatedWith (juce::Colours::white, 0.4f).withAlpha (0.30f + 0.55f * lit));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
}

class PrizmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PrizmLookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, Col::bg);
        setColour (Slider::textBoxTextColourId, Col::ink);
        setColour (Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, Col::holo.withAlpha (0.35f));
        setColour (TextEditor::textColourId, Col::ink);
        setColour (TextEditor::backgroundColourId, Col::glass);
        setColour (TextEditor::outlineColourId, Col::holo);
        setColour (TextEditor::focusedOutlineColourId, Col::holoBright);
        setColour (CaretComponent::caretColourId, Col::holoBright);
        setColour (Label::textColourId, Col::ink);
        setColour (ComboBox::textColourId, Col::ink);
        setColour (ComboBox::arrowColourId, Col::holoBright);
        setColour (PopupMenu::backgroundColourId, Col::glassDeep);
        setColour (PopupMenu::textColourId, Col::ink);
        setColour (PopupMenu::highlightedBackgroundColourId, Col::holo.withAlpha (0.35f));
        setColour (PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (TooltipWindow::backgroundColourId, Col::glassDeep);
        setColour (TooltipWindow::textColourId, Col::ink);
        setColour (TooltipWindow::outlineColourId, Col::holo);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using namespace juce;
        if ((bool) slider.getProperties().getWithDefault ("small", false))
        {
            drawSmallKnob (g, Rectangle<int> (x, y, width, height).toFloat(), pos, startAngle, endAngle, slider);
            return;
        }
        const auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float valueAngle = startAngle + pos * (endAngle - startAngle);
        const bool hasOverdrive = (bool) slider.getProperties().getWithDefault ("overdrive", false);
        const bool hot = slider.isMouseOverOrDragging();

        // --- holo highlight: thin value ring around the knob ---------------------
        const float ringR = radius - 4.0f;
        Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (track, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Col::holoDim.withAlpha (0.55f));
        g.strokePath (track, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));

        // Über 100 % (Color): kein Farbwechsel, sondern mehr Leuchten und ein weiß-heißer Kern
        const float over = hasOverdrive ? jlimit (0.0f, 1.0f, (pos - 0.5f) * 2.0f) : 0.0f;
        // Leuchten außen um den Regler, wird mit der Stellung stärker
        {
            const float R = ringR + 14.0f;
            const float amt = 0.10f + 0.70f * pos;
            const auto col = Col::holo.interpolatedWith (Col::violet, 0.35f * pos);
            ColourGradient halo (col.withAlpha (0.0f), c.x, c.y, col.withAlpha (0.0f), c.x + R, c.y, true);
            halo.addColour ((ringR - 3.0f) / R, col.withAlpha (0.0f));
            halo.addColour ((ringR + 1.0f) / R, col.withAlpha (0.22f * amt));
            g.setGradientFill (halo);
            g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
        }
        // Über 100 %: ein feiner Holo-Nebel außenrum (kühle Neonfarben, langsam wabernd).
        // Läuft radial genau bis zum Rand der Komponente aus, damit nichts eckig abgeschnitten wird.
        if (over > 0.01f)
        {
            const float tm = (float) (Time::getMillisecondCounterHiRes() * 0.001);
            const float lim = jmin ((float) slider.getWidth(), (float) slider.getHeight()) * 0.5f - 1.0f;
            const Colour mist[] { Colour (0xff00e5ff), Colour (0xff8a5cff), Colour (0xff3affc8), Colour (0xffff4fd8) };
            for (int i = 0; i < 4; ++i)
            {
                const float a = tm * (0.28f + 0.07f * (float) i) + (float) i * 1.7f;
                const auto o = Point<float> (std::cos (a), std::sin (a)) * 3.5f; // Schwerpunkt wandert -> Farbe wandert um den Regler
                const auto ctr = c + o;
                const float r1 = jmax (ringR + 1.0f, lim - 4.0f);
                ColourGradient fog (mist[i].withAlpha (0.0f), ctr.x, ctr.y, mist[i].withAlpha (0.0f), ctr.x + r1, ctr.y, true);
                fog.addColour (jlimit (0.0f, 0.99f, (ringR - 3.0f) / r1), mist[i].withAlpha (0.0f));
                fog.addColour (jlimit (0.0f, 0.99f, (ringR + 2.0f) / r1), mist[i].withAlpha (0.20f * over));
                g.setGradientFill (fog);
                g.fillEllipse (Rectangle<float> (r1 * 2.0f, r1 * 2.0f).withCentre (ctr));
            }
        }
        drawCrystalArc (g, c, ringR, startAngle, endAngle, valueAngle, hot, over, 2.0f, pos);

        drawModRing (g, c, ringR, startAngle, endAngle, slider);

        // end ticks
        for (float a : { startAngle, endAngle })
        {
            g.setColour (Col::muted.withAlpha (0.7f));
            g.drawLine ({ c.getPointOnCircumference (ringR - 5.0f, a), c.getPointOnCircumference (ringR + 4.0f, a) }, 1.2f);
        }

        // --- dark metal knob -------------------------------------------------------
        const float skirtR = ringR - 9.0f;
        const float capR = skirtR * 0.78f;
        const auto skirt = Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c);
        const auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);

        // contact shadow on the wet plate
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillEllipse (skirt.translated (0.0f, skirtR * 0.06f * (float) i).expanded ((float) i * 1.5f));
        }

        // knurled skirt
        ColourGradient skirtFill (Colour (0xff2b3238), c.x, skirt.getY(), Colour (0xff0a0d10), c.x, skirt.getBottom(), false);
        g.setGradientFill (skirtFill);
        g.fillEllipse (skirt);
        const int ridges = 64;
        for (int i = 0; i < ridges; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / (float) ridges;
            const float light = 0.5f + 0.5f * std::cos (a + 0.7f); // lit from the upper left
            g.setColour ((i % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (i % 2 == 0 ? 0.04f + 0.10f * light : 0.25f));
            g.drawLine ({ c.getPointOnCircumference (capR + 1.0f, a), c.getPointOnCircumference (skirtR - 0.8f, a) }, 1.1f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (skirt, 1.0f);

        // Fassung zwischen Rändel und Glas
        ColourGradient bevel (Colour (0xff7a8590), c.x - capR, c.y - capR, Colour (0xff050608), c.x + capR * 0.6f, c.y + capR, false);
        g.setGradientFill (bevel);
        g.fillEllipse (cap.expanded (1.8f));

        // Glaslinse: dunkles Rauchglas. Je weiter aufgedreht, desto mehr leuchtet das Glas innen
        // diffus und farbig auf (kühl -> violett, ganz oben kommt Wärme dazu), Color über 100 % noch heller.
        {
            ColourGradient lens (Colour (0xff0e2133), c.x - capR * 0.2f, c.y - capR * 0.3f, Colour (0xff03070c), c.x + capR, c.y + capR, true);
            g.setGradientFill (lens);
            g.fillEllipse (cap);

            Graphics::ScopedSaveState clip (g);
            Path lensPath;
            lensPath.addEllipse (cap);
            g.reduceClipRegion (lensPath);

            const float amt = jlimit (0.0f, 1.0f, 0.20f + 0.80f * pos);
            const float neon = jlimit (0.0f, 1.0f, (pos - 0.55f) / 0.45f); // voll aufgedreht: greller, neonartiger
            const float tm = (float) (Time::getMillisecondCounterHiRes() * 0.001) + (float) (reinterpret_cast<std::uintptr_t> (&slider) % 97) * 0.37f;
            // Das Licht im Glas dreht sich nicht mit dem Poti: die Lichter wabern unabhängig und die Farben mischen
            // sich dynamisch durcheinander. Die Stellung ändert nur die Lichtintensität.
            const Colour pal[] { Colour (0xff10e8ff), Colour (0xffa060ff), Colour (0xff30ffc0), Colour (0xffff50d0), Colour (0xffffc040) };
            const float k = 1.0f + 0.6f * neon;
            for (int i = 0; i < 5; ++i)
            {
                const float fi = (float) i;
                const float ax = tm * (0.21f + 0.06f * fi) + fi * 1.9f;
                const float ay = tm * (0.17f + 0.05f * fi) + fi * 2.7f;
                const auto pt = Point<float> (c.x + capR * 0.50f * std::sin (ax) * std::cos (ay * 0.7f),
                                              c.y + capR * 0.50f * std::sin (ay));
                const float mixAmt = 0.5f + 0.5f * std::sin (tm * 0.45f + fi * 1.3f);
                const auto col = pal[i].interpolatedWith (pal[(i + 2) % 5], mixAmt);
                const float alpha = (0.55f - 0.04f * fi) * amt * k;
                const float size = 1.0f - 0.03f * fi;
                ColourGradient glow (col.withAlpha (alpha), pt.x, pt.y, col.withAlpha (0.0f), pt.x + capR * size, pt.y, true);
                g.setGradientFill (glow);
                g.fillEllipse (Rectangle<float> (capR * size * 2.0f, capR * size * 2.0f).withCentre (pt));
            }
            if (neon > 0.0f) // heller Neonkern, wabert leicht in der Mitte
            {
                const auto pt = Point<float> (c.x + capR * 0.10f * std::sin (tm * 0.6f), c.y + capR * 0.10f * std::cos (tm * 0.5f));
                ColourGradient core (Colour (0xffd0f8ff).withAlpha (0.35f * neon), pt.x, pt.y, Colour (0xffd0f8ff).withAlpha (0.0f), pt.x + capR * 0.7f, pt.y, true);
                g.setGradientFill (core);
                g.fillEllipse (Rectangle<float> (capR * 1.4f, capR * 1.4f).withCentre (pt));
            }

            // Spiegelung oben links und feine Lichtkante
            ColourGradient gloss (juce::Colours::white.withAlpha (0.20f), c.x - capR * 0.35f, c.y - capR * 0.6f,
                                  juce::Colours::white.withAlpha (0.0f), c.x - capR * 0.35f, c.y + capR * 0.05f, true);
            g.setGradientFill (gloss);
            g.fillEllipse (cap.reduced (capR * 0.14f).translated (-capR * 0.12f, -capR * 0.24f).withHeight (capR * 0.85f));
        }
        {
            Path rim;
            rim.addCentredArc (c.x, c.y, capR - 0.8f, capR - 0.8f, 0.0f, -2.4f, -0.5f, true);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.strokePath (rim, PathStrokeType (1.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawEllipse (cap, 0.9f);
        }

        // --- holo highlight: glowing indicator in a machined groove --------------
        const auto tip = c.getPointOnCircumference (capR * 0.86f, valueAngle);
        const auto inner = c.getPointOnCircumference (capR * 0.30f, valueAngle);
        const auto pc = Col::holoBright.interpolatedWith (Col::crystal, over);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawLine ({ inner, tip }, 4.0f);
        g.setColour (pc.withAlpha (hot ? 0.40f : 0.25f));
        g.drawLine ({ inner, tip }, 6.0f);
        g.setColour (pc);
        g.drawLine ({ inner, tip }, 1.8f);
        const auto edgeDot = c.getPointOnCircumference (skirtR - 4.5f, valueAngle);
        g.setColour (pc.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (edgeDot));
        g.setColour (pc);
        g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (edgeDot));
    }

    // Wertebogen aus Licht: wird zur Spitze hin heller. over (0..1) verstärkt Leuchten und Kern (Color über 100 %).
    // disperse (0..1, = Reglerwert): je weiter aufgedreht, desto stärker leuchtet der Bogen.
    static void drawCrystalArc (juce::Graphics& g, juce::Point<float> c, float r, float start, float end, float value, bool hot, float over,
                                float width = 2.0f, float disperse = 0.0f)
    {
        using namespace juce;
        if (value <= start + 0.0005f)
            return;
        const float span = end - start;
        // Halo
        {
            Path p;
            p.addCentredArc (c.x, c.y, r, r, 0.0f, start, value, true);
            const float glow = 0.5f + 0.7f * disperse; // mit dem Wert heller
            g.setColour (Col::holo.withAlpha (((hot ? 0.14f : 0.09f) + 0.10f * over) * glow));
            g.strokePath (p, PathStrokeType (width * 4.5f + 3.0f * disperse + 6.0f * over, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Col::holo.withAlpha (((hot ? 0.28f : 0.20f) + 0.15f * over) * glow));
            g.strokePath (p, PathStrokeType (width * 2.2f + 2.0f * over, PathStrokeType::curved, PathStrokeType::rounded));
        }
        const int n = jmax (2, (int) ((value - start) / 0.04f));
        // Kern: Verlauf von gedämpftem Blau zu eisweiß
        for (int i = 0; i < n; ++i)
        {
            const float a0 = start + (value - start) * (float) i / (float) n;
            const float a1 = start + (value - start) * (float) (i + 1) / (float) n + 0.004f;
            const float t = ((a0 + a1) * 0.5f - start) / span;
            Path seg;
            seg.addCentredArc (c.x, c.y, r, r, 0.0f, a0, jmin (a1, value), true);
            g.setColour (Col::holo.interpolatedWith (Col::crystal, jlimit (0.0f, 1.0f, 0.25f + t * 0.9f)));
            g.strokePath (seg, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::butt));
        }
        // Lichtpunkt an der Spitze, über 100 % mit Blüte
        const auto tip = c.getPointOnCircumference (r, value);
        const float bloom = 6.0f + 10.0f * over + (hot ? 2.0f : 0.0f);
        ColourGradient glow (Col::crystal.withAlpha (0.55f + 0.35f * over), tip.x, tip.y, Col::holo.withAlpha (0.0f), tip.x + bloom, tip.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (Rectangle<float> (bloom * 2.0f, bloom * 2.0f).withCentre (tip));
        g.setColour (juce::Colours::white);
        g.fillEllipse (Rectangle<float> (2.6f + 1.4f * over, 2.6f + 1.4f * over).withCentre (tip));
    }

    // Beim Ziehen einer Modulationsquelle leuchtet der Regler als Ziel auf.
    static void drawModRing (juce::Graphics& g, juce::Point<float> c, float ringR, float startAngle, float endAngle, juce::Slider& slider)
    {
        using namespace juce;
        const auto& props = slider.getProperties();
        const int drop = (int) props.getWithDefault ("dropHover", 0);
        const bool ready = (bool) props.getWithDefault ("dropReady", false);
        if (drop > 0 || ready)
        {
            const auto col = drop > 0 ? Colour ((uint32) (int64) props.getWithDefault ("dropCol", (int64) 0xff3fb8ff)) : Col::holo;
            Path p;
            p.addCentredArc (c.x, c.y, ringR + 1.0f, ringR + 1.0f, 0.0f, 0.0f, MathConstants<float>::twoPi, true);
            g.setColour (col.withAlpha (drop > 0 ? 0.30f : 0.10f));
            g.strokePath (p, PathStrokeType (drop > 0 ? 12.0f : 7.0f));
            g.setColour (col.withAlpha (drop > 0 ? 0.9f : 0.35f));
            g.strokePath (p, PathStrokeType (1.2f));
        }
        // Modulationsbögen und Tiefen-Griffe zeichnet ModKnob selbst (PluginEditor)
    }

    // Kleiner Regler (Modulations-Bereich): Ring + dunkle Kappe, Wert rechts daneben
    void drawSmallKnob (juce::Graphics& g, juce::Rectangle<float> area, float pos, float startAngle, float endAngle, juce::Slider& slider)
    {
        using namespace juce;
        const float d = jmin (area.getWidth(), area.getHeight()) - 4.0f;
        const auto b = Rectangle<float> (d, d).withCentre (area.getCentre());
        const auto c = b.getCentre();
        const float ringR = d * 0.5f - 2.0f;
        const float valueAngle = startAngle + pos * (endAngle - startAngle);
        const bool hot = slider.isMouseOverOrDragging();
        Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
        drawCrystalArc (g, c, ringR, startAngle, endAngle, valueAngle, hot, 0.0f, 1.6f, pos);
        const float capR = ringR - 5.0f;
        const auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
        ColourGradient fill (Colour (0xff4a545e), c.x - capR, c.y - capR, Colour (0xff07090b), c.x + capR * 0.6f, c.y + capR, false);
        g.setGradientFill (fill);
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (cap, 0.8f);
        g.setColour (Col::holoBright);
        g.drawLine ({ c.getPointOnCircumference (capR * 0.25f, valueAngle), c.getPointOnCircumference (capR * 0.9f, valueAngle) }, 1.6f);
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name) override
    {
        g.setFont (bb::fonts::mono (12.0f));
        g.setColour (Col::holoBright);
        g.drawText ("// " + name, area.reduced (10, 0).withTrimmedTop (4), juce::Justification::bottomLeft);
        g.setColour (Col::holoDim.withAlpha (0.6f));
        g.fillRect (area.getX() + 10, area.getBottom() - 1, area.getWidth() - 20, 1);
    }

    // Value readouts under the knobs: framed hologram boxes.
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent()))
        {
            const auto r = label.getLocalBounds().toFloat().reduced (1.0f);
            holo::box (g, r, 1.0f, slider->isMouseOverOrDragging());
            if (! label.isBeingEdited())
            {
                g.setColour (Col::ink);
                g.setFont (bb::fonts::mono ((bool) slider->getProperties().getWithDefault ("small", false) ? 13.0f : 16.0f));
                g.drawText (label.getText(), r, juce::Justification::centred);
            }
            return;
        }
        LookAndFeel_V4::drawLabel (g, label);
    }

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& slider) override
    {
        auto layout = LookAndFeel_V4::getSliderLayout (slider);
        if (slider.getTextBoxPosition() == juce::Slider::TextBoxBelow)
        {
            const int boxW = 92, boxH = 24;
            auto area = slider.getLocalBounds();
            layout.textBoxBounds = { area.getCentreX() - boxW / 2, area.getBottom() - boxH, boxW, boxH };
            layout.sliderBounds = area.withTrimmedBottom (boxH + 6);
        }
        else if (slider.getTextBoxPosition() == juce::Slider::TextBoxRight)
        {
            auto area = slider.getLocalBounds();
            layout.sliderBounds = area.removeFromLeft (area.getHeight());
            area.removeFromLeft (6);
            layout.textBoxBounds = area.removeFromLeft (slider.getTextBoxWidth()).withSizeKeepingCentre (slider.getTextBoxWidth(), slider.getTextBoxHeight());
        }
        return layout;
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        const auto r = Rectangle<int> (0, 0, width, height).toFloat();
        holo::box (g, r, 1.0f, box.isMouseOver (true) || box.isPopupActive());
        if (width < 80)
            return; // kleine Auswahl (Noten-Map): kein Pfeil

        Path arrow;
        const float ax = (float) width - 18.0f, ay = (float) height * 0.5f;
        arrow.startNewSubPath (ax - 4.5f, ay - 2.0f);
        arrow.lineTo (ax, ay + 2.5f);
        arrow.lineTo (ax + 4.5f, ay - 2.0f);
        g.setColour (Col::holoBright);
        g.strokePath (arrow, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        if (box.getWidth() < 80)
        {
            label.setBounds (1, 1, box.getWidth() - 2, box.getHeight() - 2);
            label.setFont (bb::fonts::display (14.0f, true));
            label.setJustificationType (juce::Justification::centred);
            return;
        }
        label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return bb::fonts::display (17.0f, true); }
    juce::Font getPopupMenuFont() override { return bb::fonts::display (16.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (Col::glassDeep);
        g.setColour (Col::holo.withAlpha (0.7f));
        g.drawRect (0, 0, width, height);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace juce;
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        if (b.getToggleState())
        {
            // an: beleuchtetes Glas mit Spektrum-Linie unten (statt Farbfläche)
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (Col::holo.withAlpha (0.07f / (float) i));
                g.fillRoundedRectangle (r.expanded ((float) i * 2.0f), 3.0f + (float) i * 2.0f);
            }
            ColourGradient fill (Col::holo.withAlpha (0.42f), 0.0f, r.getY(), Col::glassDeep.withAlpha (0.92f), 0.0f, r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Col::crystal.withAlpha (over || down ? 1.0f : 0.85f));
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.2f);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
            const float lineY = r.getBottom() - 3.0f;
            g.setGradientFill (Col::spectrum (r.getX() + 6.0f, lineY, r.getRight() - 6.0f, 0.95f));
            g.fillRoundedRectangle (r.getX() + 6.0f, lineY - 0.75f, r.getWidth() - 12.0f, 1.5f, 0.75f);
            holo::cornerBrackets (g, r.reduced (0.5f), 5.0f, juce::Colours::white, 1.4f);
        }
        else
        {
            holo::box (g, r, 1.0f, over || down);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.getToggleState() ? juce::Colours::white : (over ? Col::ink : Col::muted));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }

    // Tooltips: erste Zeile = Titel (grün), Rest = Erklärung. Breite max. 320 px.
    static juce::TextLayout tooltipLayout (const juce::String& text)
    {
        juce::AttributedString a;
        a.setWordWrap (juce::AttributedString::byWord);
        a.setLineSpacing (2.0f);
        const auto title = text.upToFirstOccurrenceOf ("\n", false, false);
        const auto body = text.fromFirstOccurrenceOf ("\n", false, false);
        a.append (title, bb::fonts::display (14.5f, true).withExtraKerningFactor (0.04f), Col::holoBright);
        if (body.isNotEmpty())
            a.append ("\n" + body, bb::fonts::display (14.0f), Col::ink);
        juce::TextLayout layout;
        layout.createLayout (a, 320.0f);
        return layout;
    }

    static float layoutWidth (const juce::TextLayout& layout)
    {
        float w = 0.0f;
        for (int i = 0; i < layout.getNumLines(); ++i)
            w = juce::jmax (w, layout.getLine (i).getLineBoundsX().getEnd());
        return w;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
    {
        const auto layout = tooltipLayout (tipText);
        const int w = (int) std::ceil (layoutWidth (layout)) + 26;
        const int h = (int) std::ceil (layout.getHeight()) + 20;
        const int x = screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 14) : screenPos.x + 18;
        const int y = screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 10) : screenPos.y + 20;
        return juce::Rectangle<int> (x, y, w, h).constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        const auto r = juce::Rectangle<int> (0, 0, width, height).toFloat();
        g.fillAll (Col::glassDeep);
        holo::box (g, r, 1.0f, true);
        auto layout = tooltipLayout (text);
        layout.draw (g, r.reduced (13.0f, 10.0f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return bb::fonts::display (juce::jmin (14.0f, (float) buttonHeight * 0.55f), true).withExtraKerningFactor (0.08f);
    }
};
} // namespace prizm
