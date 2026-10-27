#pragma once

// Braincut: Look nach dem Hintergrundbild (Neon-Vortex im Cyberpunk-Rahmen).
// Nur Violett/Indigo-Töne, abgeschrägte Ecken und Streifen wie im Bildrahmen.
// Jede Sektion hat eine eigene Abstufung; Regler und Taster lesen sie aus ihren Properties.

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzLook.h"

namespace bcui
{
namespace col
{
    inline const juce::Colour slicer  { 0xff8b6bff };  // Hauptfarbe, wie die Rahmenlinien im Bild
    inline const juce::Colour stretch { 0xff6676ff };  // bläuliches Indigo
    inline const juce::Colour repeat  { 0xffb45cff };  // rötliches Lila
    inline const juce::Colour glitch  { 0xffcbb8ff };  // helles Lila
    inline const juce::Colour master  { 0xffa48cff };
    inline const juce::Colour frame   { 0xff4b2fe6 };  // dunkle Rahmenlinie
    inline const juce::Colour glass   { 0xff06031a };  // Glasfläche der Karten
    inline const juce::Colour bg      { 0xff04020b };
    inline const juce::Colour ink     { 0xffece7ff };
    inline const juce::Colour inkDim  { 0xff8c82b8 };
    inline const juce::Colour line    { 0xff2a2066 };
}

// Verlauf dunkles Indigo -> Violett -> Lila -> fast Weiß für die Slice-Anzeige (t = 0..1)
inline juce::Colour spectrum (float t)
{
    const juce::Colour stops[5] { juce::Colour (0xff3a2ae0), juce::Colour (0xff5d4cff), juce::Colour (0xff8f63ff),
                                  juce::Colour (0xffbf9bff), juce::Colour (0xffeee6ff) };
    t = juce::jlimit (0.0f, 1.0f, t) * 4.0f;
    const int i = juce::jmin (3, (int) t);
    return stops[i].interpolatedWith (stops[i + 1], t - (float) i);
}

// Rechteck mit abgeschrägten Ecken (oben links und unten rechts, wie der Bildrahmen)
inline juce::Path chamfer (juce::Rectangle<float> r, float c, bool topRight = false, bool bottomLeft = false)
{
    c = std::min (c, std::min (r.getWidth(), r.getHeight()) * 0.45f);
    const float c2 = c * 0.6f;
    juce::Path p;
    p.startNewSubPath (r.getX() + c, r.getY());
    if (topRight) { p.lineTo (r.getRight() - c2, r.getY()); p.lineTo (r.getRight(), r.getY() + c2); }
    else            p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight(), r.getBottom() - c);
    p.lineTo (r.getRight() - c, r.getBottom());
    if (bottomLeft) { p.lineTo (r.getX() + c2, r.getBottom()); p.lineTo (r.getX(), r.getBottom() - c2); }
    else              p.lineTo (r.getX(), r.getBottom());
    p.lineTo (r.getX(), r.getY() + c);
    p.closeSubPath();
    return p;
}

// Schräge Streifen ("/////") wie oben und unten im Bildrahmen
inline void hatch (juce::Graphics& g, float x, float y, int count, float h, juce::Colour c)
{
    for (int i = 0; i < count; ++i)
    {
        const float x0 = x + (float) i * 6.0f;
        juce::Path p;
        p.startNewSubPath (x0 + h * 0.6f, y);
        p.lineTo (x0 + h * 0.6f + 3.0f, y);
        p.lineTo (x0 + 3.0f, y + h);
        p.lineTo (x0, y + h);
        p.closeSubPath();
        g.setColour (c);
        g.fillPath (p);
    }
}

inline void setAccent (juce::Component& c, juce::Colour colour)
{
    c.getProperties().set ("acc", (juce::int64) colour.getARGB());
}

inline juce::Colour accentOf (const juce::Component& c, juce::Colour fallback = col::slicer)
{
    const auto v = c.getProperties()["acc"];
    return v.isVoid() ? fallback : juce::Colour ((juce::uint32) (juce::int64) v);
}

class Look : public bb::LookAndFeel
{
public:
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0b0722));
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a1b78));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0a0620));
        setColour (juce::ComboBox::textColourId, col::ink);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::arrowColourId, col::slicer);
    }

    // Regler im HUD-Stil: Skalenstriche, Leuchtbogen, dunkle Indigo-Kappe mit Zeigerpunkt
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override
    {
        const float size = (float) std::min (w, h);
        const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
        const float R = size * 0.5f - 3.0f;
        const float a = startAngle + pos * (endAngle - startAngle);
        const auto accent = accentOf (s);
        const auto ac = s.isEnabled() ? accent : col::inkDim;
        auto at = [&] (float rad, float ang) { return juce::Point<float> (c.x + rad * std::sin (ang), c.y - rad * std::cos (ang)); };

        // Skalenstriche
        const int ticks = size > 90.0f ? 25 : 17;
        for (int k = 0; k < ticks; ++k)
        {
            const float t = (float) k / (float) (ticks - 1);
            const float ang = startAngle + t * (endAngle - startAngle);
            const bool lit = t <= pos + 0.001f;
            g.setColour (lit ? ac.withAlpha (0.95f) : col::frame.withAlpha (0.45f));
            g.drawLine (juce::Line<float> (at (R - (k % 4 == 0 ? 5.0f : 3.0f), ang), at (R, ang)), 1.3f);
        }

        // Leuchtbogen
        const float ar = R - 9.0f;
        const float thick = std::max (2.0f, size * 0.04f);
        juce::Path track;
        track.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (track, juce::PathStrokeType (thick + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (col::frame.withAlpha (0.35f));
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        if (pos > 0.003f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, a, true);
            g.setColour (ac.withAlpha (0.25f));
            g.strokePath (arc, juce::PathStrokeType (thick * 3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (ac);
            g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        // Kappe
        const float cr = ar - thick * 1.8f;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.x - cr, c.y - cr + 1.5f, cr * 2.0f, cr * 2.0f);
        juce::ColourGradient cap (juce::Colour (0xff1c1446), c.x, c.y - cr, juce::Colour (0xff05030e), c.x, c.y + cr, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (ac.withAlpha (s.isEnabled() ? 0.40f : 0.12f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);
        if (size > 90.0f)
        {
            // innerer gestrichelter Ring wie die Vortex-Ringe im Bild
            juce::Path ring, dashed;
            ring.addEllipse (c.x - cr * 0.72f, c.y - cr * 0.72f, cr * 1.44f, cr * 1.44f);
            const float dl[2] { 2.0f, 5.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, ring, dl, 2);
            g.setColour (ac.withAlpha (0.18f));
            g.fillPath (dashed);
        }

        // Zeiger: Punkt am Rand der Kappe
        const auto d = at (cr - 4.0f, a);
        g.setColour (ac.withAlpha (0.45f));
        g.fillEllipse (d.x - 4.5f, d.y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (d.x - 2.2f, d.y - 2.2f, 4.4f, 4.4f);
    }

    // Wertebalken (Min/Max Phrase, Min/Max Rep): Füllung zeigt den Wert, Beschriftung links, Wert rechts
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
        const auto accent = accentOf (s);
        const auto shape = chamfer (r, 6.0f);
        g.setColour (juce::Colour (0xaa05030f));
        g.fillPath (shape);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (shape);
            juce::ColourGradient fill (accent.withAlpha (0.45f), r.getX(), 0, accent.withAlpha (0.14f), sliderPos, 0, false);
            g.setGradientFill (fill);
            g.fillRect (r.withRight (std::max (r.getX() + 2.0f, sliderPos)));
            g.setColour (accent);
            g.fillRect (std::max (r.getX(), sliderPos - 2.0f), r.getY(), 2.0f, r.getHeight());
        }
        g.setColour (s.isMouseOverOrDragging() ? accent.withAlpha (0.85f) : col::line.brighter (0.3f));
        g.strokePath (shape, juce::PathStrokeType (1.0f));
        g.setFont (bb::fonts::display (11.5f, true));
        g.setColour (col::inkDim);
        g.drawText (s.getProperties()["lbl"].toString(), r.reduced (9.0f, 0.0f), juce::Justification::centredLeft);
        g.setFont (bb::fonts::mono (12.5f));
        g.setColour (col::ink);
        g.drawText (s.getTextFromValue (s.getValue()), r.reduced (9.0f, 0.0f), juce::Justification::centredRight);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = accentOf (b);
        const bool on = b.getToggleState();
        const auto shape = chamfer (r, std::min (7.0f, r.getHeight() * 0.32f));

        if (on)
        {
            g.setColour (accent.withAlpha (0.20f));
            g.strokePath (shape, juce::PathStrokeType (3.5f));
        }
        juce::ColourGradient grad (on ? accent.withAlpha (0.42f) : juce::Colour (0xe0120a30), 0, r.getY(),
                                   on ? accent.withAlpha (0.14f) : juce::Colour (0xe006031a), 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (shape);
        g.setColour (on ? accent : (over ? accent.withAlpha (0.6f) : col::line.brighter (0.35f)));
        g.strokePath (shape, juce::PathStrokeType (1.1f));
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.fillPath (shape);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto accent = accentOf (b);
        const float h = (float) b.getHeight();
        auto f = bb::fonts::display (std::min (14.0f, h * 0.5f), true);
        f.setExtraKerningFactor (0.08f);
        g.setFont (f);
        g.setColour (b.getToggleState() ? accent.brighter (0.6f) : col::inkDim);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        const auto shape = chamfer (r, 8.0f, true, true);
        g.setColour (juce::Colour (0xe00a0620));
        g.fillPath (shape);
        g.setColour (box.hasKeyboardFocus (false) || box.isMouseOver (true) ? col::slicer : col::frame);
        g.strokePath (shape, juce::PathStrokeType (1.1f));
        juce::Path arrow;
        const float ax = (float) w - 15.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (col::slicer);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return bb::fonts::display (14.0f, true); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
    juce::Font getPopupMenuFont() override { return bb::fonts::display (15.0f); }
};

} // namespace bcui
