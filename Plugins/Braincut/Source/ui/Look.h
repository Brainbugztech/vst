#pragma once

// Braincut: ruhiger, farbiger Look auf Basis des gemeinsamen Brain-Bugz-Looks.
// Jeder Sektion ist eine eigene Akzentfarbe zugeordnet; Regler und Taster lesen sie aus ihren Properties.

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzLook.h"

namespace bcui
{
namespace col
{
    inline const juce::Colour green  = bb::col::tox;           // Slicer
    inline const juce::Colour cyan   { 0xff2de2e6 };           // Stretch
    inline const juce::Colour pink   = bb::col::brain;         // Repeat
    inline const juce::Colour violet { 0xffa56bff };           // Glitch
    inline const juce::Colour orange { 0xffff8a3d };
    inline const juce::Colour bg     { 0xff0a0612 };
    inline const juce::Colour card   { 0xff0c0716 };
}

// Farbverlauf grün -> cyan -> violett -> pink -> orange für die Slice-Anzeige (t = 0..1)
inline juce::Colour spectrum (float t)
{
    const juce::Colour stops[5] { col::green, col::cyan, col::violet, col::pink, col::orange };
    t = juce::jlimit (0.0f, 1.0f, t) * 4.0f;
    const int i = juce::jmin (3, (int) t);
    return stops[i].interpolatedWith (stops[i + 1], t - (float) i);
}

inline void setAccent (juce::Component& c, juce::Colour colour)
{
    c.getProperties().set ("acc", (juce::int64) colour.getARGB());
}

inline juce::Colour accentOf (const juce::Component& c, juce::Colour fallback = bb::col::tox)
{
    const auto v = c.getProperties()["acc"];
    return v.isVoid() ? fallback : juce::Colour ((juce::uint32) (juce::int64) v);
}

class Look : public bb::LookAndFeel
{
public:
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff140d20));
        setColour (juce::PopupMenu::textColourId, bb::col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a1c40));
        setColour (juce::PopupMenu::highlightedTextColourId, bb::col::ink);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0e0818));
        setColour (juce::ComboBox::textColourId, bb::col::ink);
        setColour (juce::ComboBox::outlineColourId, bb::col::line);
        setColour (juce::ComboBox::arrowColourId, bb::col::tox);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override
    {
        const float size = (float) std::min (w, h);
        const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
        const float thick = std::max (2.5f, size * 0.075f);
        const float r = size * 0.5f - 2.0f - thick * 1.6f;
        const float a = startAngle + pos * (endAngle - startAngle);
        const auto accent = accentOf (s);
        const bool on = s.isEnabled();

        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (bb::col::line);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (pos > 0.003f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, a, true);
            const auto ac = on ? accent : bb::col::inkDim;
            g.setColour (ac.withAlpha (0.16f));
            g.strokePath (arc, juce::PathStrokeType (thick * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ac.withAlpha (0.34f));
            g.strokePath (arc, juce::PathStrokeType (thick * 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ac);
            g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float cr = r - thick * 1.6f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - cr, c.y - cr + 2.0f, cr * 2.0f, cr * 2.0f);
        juce::ColourGradient cap (juce::Colour (0xff2a1e3a), c.x, c.y - cr, juce::Colour (0xff0d0911), c.x, c.y + cr, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (accent.withAlpha (on ? 0.45f : 0.15f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);

        juce::Path p;
        p.startNewSubPath (0.0f, -cr * 0.25f);
        p.lineTo (0.0f, -cr * 0.88f);
        g.setColour (bb::col::ink);
        g.strokePath (p, juce::PathStrokeType (std::max (1.8f, size * 0.045f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation (a).translated (c));
    }

    // Wertebalken (Min/Max Phrase, Min/Max Rep): Füllung zeigt den Wert, Beschriftung links, Wert rechts
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
        const auto accent = accentOf (s);
        const float rad = 7.0f;
        g.setColour (juce::Colour (0x66000000));
        g.fillRoundedRectangle (r, rad);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (r, rad);
            g.reduceClipRegion (clip);
            juce::ColourGradient fill (accent.withAlpha (0.50f), r.getX(), 0, accent.withAlpha (0.20f), sliderPos, 0, false);
            g.setGradientFill (fill);
            g.fillRect (r.withRight (std::max (r.getX() + 2.0f, sliderPos)));
            g.setColour (accent);
            g.fillRect (std::max (r.getX(), sliderPos - 2.0f), r.getY(), 2.0f, r.getHeight());
        }
        g.setColour (s.isMouseOverOrDragging() ? accent.withAlpha (0.8f) : bb::col::line);
        g.drawRoundedRectangle (r, rad, 1.0f);
        g.setFont (bb::fonts::display (11.5f, true));
        g.setColour (bb::col::inkDim);
        g.drawText (s.getProperties()["lbl"].toString(), r.reduced (9.0f, 0.0f), juce::Justification::centredLeft);
        g.setFont (bb::fonts::mono (12.5f));
        g.setColour (bb::col::ink);
        g.drawText (s.getTextFromValue (s.getValue()), r.reduced (9.0f, 0.0f), juce::Justification::centredRight);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float rad = std::min (7.0f, r.getHeight() * 0.4f);
        const auto accent = accentOf (b);
        const bool on = b.getToggleState();

        juce::ColourGradient grad (on ? accent.withAlpha (0.34f) : juce::Colour (0xff2a2036), 0, r.getY(),
                                   on ? accent.withAlpha (0.14f) : juce::Colour (0xff150f1d), 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, rad);
        if (on)
        {
            g.setColour (accent.withAlpha (0.22f));
            g.drawRoundedRectangle (r.expanded (1.0f), rad + 1.0f, 2.0f);
        }
        g.setColour (on ? accent : (over ? bb::col::inkDim : bb::col::line));
        g.drawRoundedRectangle (r, rad, 1.1f);
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.2f));
            g.fillRoundedRectangle (r, rad);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto accent = accentOf (b);
        const float h = (float) b.getHeight();
        g.setFont (bb::fonts::display (std::min (14.0f, h * 0.5f), true));
        g.setColour (b.getToggleState() ? accent.brighter (0.35f) : bb::col::inkDim);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colour (0xff0e0818));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (box.hasKeyboardFocus (false) ? bb::col::tox : bb::col::line);
        g.drawRoundedRectangle (r, 7.0f, 1.1f);
        juce::Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (bb::col::tox);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return bb::fonts::display (14.0f, true); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
    juce::Font getPopupMenuFont() override { return bb::fonts::display (15.0f); }
};

} // namespace bcui
