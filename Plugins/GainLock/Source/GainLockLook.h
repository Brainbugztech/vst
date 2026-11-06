#pragma once

// Gain Lock: Look "Studio-Messgerät" – Graphit, Bernstein, Cyan

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzLook.h"

namespace gl
{
namespace col
{
    inline const juce::Colour bg       { 0xff0c0d0f };
    inline const juce::Colour panel    { 0xff15171a };
    inline const juce::Colour panel2   { 0xff1c1f23 };
    inline const juce::Colour line     { 0xff2a2e34 };
    inline const juce::Colour lineHi   { 0xff3a3f46 };
    inline const juce::Colour face     { 0xff111316 };
    inline const juce::Colour amber    { 0xffffae1f };
    inline const juce::Colour amberHi  { 0xffffd27a };
    inline const juce::Colour amberDim { 0xff7a520f };
    inline const juce::Colour cyan     { 0xff2ed3ff };
    inline const juce::Colour cyanDim  { 0xff11566b };
    inline const juce::Colour red      { 0xffff4b3a };
    inline const juce::Colour ink      { 0xffe8e5de };
    inline const juce::Colour inkDim   { 0xff8b8881 };
    inline const juce::Colour inkFaint { 0xff55534f };
}

class GainLockLook : public juce::LookAndFeel_V4
{
public:
    GainLockLook()
    {
        setColour (juce::ComboBox::backgroundColourId, col::panel2);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::textColourId, col::ink);
        setColour (juce::ComboBox::arrowColourId, col::amber);
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amberDim);
        setColour (juce::PopupMenu::highlightedTextColourId, col::ink);
        setColour (juce::Slider::textBoxTextColourId, col::ink);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TooltipWindow::backgroundColourId, col::panel2);
        setColour (juce::TooltipWindow::textColourId, col::ink);
    }

    static constexpr float startA = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float endA   = juce::MathConstants<float>::pi * 2.75f;

    // Gefräster Graphit-Knopf mit Bernstein-Zeiger und feiner Skala
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float, float, juce::Slider& s) override
    {
        const float size = (float) std::min (w, h);
        const juce::Point<float> c (x + w * 0.5f, y + h * 0.5f);
        const float R = size * 0.5f - 1.0f;
        const float a = startA + pos * (endA - startA);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

        // Skalenstriche
        for (int i = 0; i <= 20; ++i)
        {
            const float t = (float) i / 20.0f;
            const float ang = startA + t * (endA - startA);
            const bool major = i % 5 == 0;
            const float r0 = R * (major ? 0.86f : 0.90f), r1 = R * 0.98f;
            g.setColour (major ? col::inkDim : col::inkFaint);
            g.drawLine (c.x + r0 * std::sin (ang), c.y - r0 * std::cos (ang),
                        c.x + r1 * std::sin (ang), c.y - r1 * std::cos (ang), major ? 1.2f : 0.8f);
        }

        // Wertebogen
        const float ar = R * 0.79f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, ar, ar, 0.0f, startA, endA, true);
        g.setColour (col::line);
        g.strokePath (track, juce::PathStrokeType (2.0f));
        const float a0 = bipolar ? (startA + endA) * 0.5f : startA;
        if (std::abs (a - a0) > 0.01f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, ar, ar, 0.0f, std::min (a0, a), std::max (a0, a), true);
            g.setColour (col::amber);
            g.strokePath (val, juce::PathStrokeType (2.0f));
        }

        // Körper
        const float kr = R * 0.66f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - kr, c.y - kr + kr * 0.10f, kr * 2, kr * 2);
        juce::ColourGradient body (juce::Colour (0xff3b3f45), c.x - kr * 0.6f, c.y - kr,
                                   juce::Colour (0xff15171a), c.x + kr * 0.5f, c.y + kr, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);

        // Rändelung
        g.setColour (juce::Colour (0xff0b0c0e).withAlpha (0.7f));
        for (int i = 0; i < 48; ++i)
        {
            const float ang = i * juce::MathConstants<float>::twoPi / 48.0f;
            g.drawLine (c.x + kr * 0.88f * std::sin (ang), c.y - kr * 0.88f * std::cos (ang),
                        c.x + kr * 0.99f * std::sin (ang), c.y - kr * 0.99f * std::cos (ang), 0.8f);
        }
        // Kappe
        const float cr = kr * 0.78f;
        juce::ColourGradient cap (juce::Colour (0xff2a2d32), c.x, c.y - cr,
                                  juce::Colour (0xff1a1c20), c.x, c.y + cr, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.0f);

        // Zeiger
        const float p0 = cr * 0.25f, p1 = cr * 0.95f;
        g.setColour (col::amber);
        g.drawLine (c.x + p0 * std::sin (a), c.y - p0 * std::cos (a),
                    c.x + p1 * std::sin (a), c.y - p1 * std::cos (a), 2.4f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (bb::fonts::mono (13.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, col::ink);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineWhenEditingColourId, col::amber);
        l->setColour (juce::TextEditor::highlightColourId, col::amberDim);
        return l;
    }

    // Taster: dunkel, beim Einrasten leuchtet die Akzentfarbe von innen
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("accent", (juce::int64) col::amber.getARGB()));
        const bool on = b.getToggleState();
        const float rad = 4.0f;

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0, 1.5f), rad);

        juce::ColourGradient fill (on ? accent.withMultipliedBrightness (0.42f) : col::panel2.brighter (over ? 0.08f : 0.0f),
                                   r.getX(), r.getY(),
                                   on ? accent.withMultipliedBrightness (0.22f) : col::panel,
                                   r.getX(), r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, rad);

        if (on)
        {
            // Lämpchen-Streifen oben
            auto lamp = r.reduced (r.getWidth() * 0.22f, 0).withHeight (3.0f).translated (0, 4.0f);
            g.setColour (accent.withAlpha (0.35f));
            g.fillRoundedRectangle (lamp.expanded (3.0f, 2.0f), 3.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (lamp, 1.5f);
        }
        g.setColour (on ? accent.withAlpha (0.8f) : (down ? col::lineHi.brighter() : col::lineHi));
        g.drawRoundedRectangle (r, rad, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (r.getX() + rad, r.getY() + 1.0f, r.getRight() - rad, r.getY() + 1.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto accent = juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("accent", (juce::int64) col::amber.getARGB()));
        g.setFont (bb::fonts::display (std::min (15.0f, b.getHeight() * 0.42f), true));
        g.setColour (b.getToggleState() ? accent.brighter (0.3f) : col::ink.withAlpha (b.isEnabled() ? 0.85f : 0.35f));
        auto r = b.getLocalBounds();
        if (b.getToggleState()) r = r.withTrimmedTop (5);
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::panel2);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.hasKeyboardFocus (false) ? col::amberDim : col::lineHi);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = w - 14.0f, ay = h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (col::amber);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return bb::fonts::display (14.0f, true); }
    juce::Font getPopupMenuFont() override { return bb::fonts::display (14.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 24, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }
};
} // namespace gl
