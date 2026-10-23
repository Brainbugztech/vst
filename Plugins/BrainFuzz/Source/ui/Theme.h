#pragma once

// Brainfuzz – Retro-Synthwave-Look: Nachtviolett, Neon-Pink, Cyan, Sonnenuntergang

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzLook.h"
#include "BrainfuzzAssets.h"

namespace bfz::ui
{
namespace col
{
    inline const juce::Colour bg0     { 0xff0a0416 };
    inline const juce::Colour bg1     { 0xff130926 };
    inline const juce::Colour bg2     { 0xff1c0f38 };
    inline const juce::Colour bg3     { 0xff281650 };
    inline const juce::Colour line    { 0xff3a2268 };
    inline const juce::Colour grid    { 0xff2b1752 };
    inline const juce::Colour ink     { 0xfff4eaff };
    inline const juce::Colour inkDim  { 0xffad97d2 };
    inline const juce::Colour inkMute { 0xff6f5b95 };
    inline const juce::Colour pink    { 0xffff3fd1 };
    inline const juce::Colour cyan    { 0xff2de2e6 };
    inline const juce::Colour violet  { 0xff9b5cff };
    inline const juce::Colour orange  { 0xffff8a3d };
    inline const juce::Colour yellow  { 0xffffd23f };
    inline const juce::Colour red     { 0xffff4060 };

    inline juce::Colour band (int b)
    {
        static const juce::Colour c[4] { juce::Colour (0xffff3fd1), juce::Colour (0xffff8a3d),
                                         juce::Colour (0xff2de2e6), juce::Colour (0xff9b5cff) };
        return c[juce::jlimit (0, 3, b)];
    }

    // Sonnenuntergangs-Verlauf (gelb -> orange -> pink -> violett)
    inline juce::ColourGradient sunset (float x1, float y1, float x2, float y2)
    {
        juce::ColourGradient g (yellow, x1, y1, violet, x2, y2, false);
        g.addColour (0.35, orange);
        g.addColour (0.68, pink);
        return g;
    }
}

namespace fonts
{
    inline juce::Font title (float h)
    {
        static juce::Typeface::Ptr tf = []
        {
            int size = 0;
            if (auto* d = BrainfuzzAssets::getNamedResource ("AudiowideRegular_ttf", size))
                return juce::Typeface::createSystemTypefaceFor (d, (size_t) size);
            return juce::Typeface::Ptr();
        }();
        if (tf != nullptr) return juce::Font (juce::FontOptions (tf).withHeight (h));
        return bb::fonts::display (h, true);
    }
    inline juce::Font display (float h, bool bold = false) { return bb::fonts::display (h, bold); }
    inline juce::Font mono (float h) { return bb::fonts::mono (h); }
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::bg1);
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::headerTextColourId, col::pink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::bg3);
        setColour (juce::PopupMenu::highlightedTextColourId, col::ink);
        setColour (juce::ComboBox::backgroundColourId, col::bg0);
        setColour (juce::ComboBox::textColourId, col::ink);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::arrowColourId, col::pink);
        setColour (juce::Label::textColourId, col::ink);
        setColour (juce::Label::textWhenEditingColourId, col::ink);
        setColour (juce::Label::backgroundWhenEditingColourId, col::bg0);
        setColour (juce::Label::outlineWhenEditingColourId, col::pink);
        setColour (juce::TextEditor::backgroundColourId, col::bg0);
        setColour (juce::TextEditor::textColourId, col::ink);
        setColour (juce::TextEditor::highlightColourId, col::pink.withAlpha (0.35f));
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::pink);
        setColour (juce::CaretComponent::caretColourId, col::pink);
        setColour (juce::Slider::textBoxTextColourId, col::ink);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, col::pink.withAlpha (0.35f));
        setColour (juce::AlertWindow::backgroundColourId, col::bg1);
        setColour (juce::AlertWindow::textColourId, col::ink);
        setColour (juce::AlertWindow::outlineColourId, col::pink);
        setColour (juce::TextButton::buttonColourId, col::bg2);
        setColour (juce::TextButton::textColourOffId, col::ink);
        setColour (juce::TooltipWindow::backgroundColourId, col::bg1);
        setColour (juce::TooltipWindow::textColourId, col::ink);
        setColour (juce::TooltipWindow::outlineColourId, col::line);
    }

    static juce::Colour accentOf (const juce::Component& c, juce::Colour fallback = col::pink)
    {
        const auto v = c.getProperties()["accent"];
        return v.isVoid() ? fallback : juce::Colour ((juce::uint32) (juce::int64) v);
    }

    static constexpr float kStart = juce::MathConstants<float>::pi * 1.22f;
    static constexpr float kEnd   = juce::MathConstants<float>::pi * 2.78f;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override
    {
        const float size = (float) std::min (w, h);
        const juce::Point<float> c (x + w * 0.5f, y + h * 0.5f);
        const float r = size * 0.5f - 3.0f;
        const float thick = std::max (2.5f, size * 0.075f);
        const auto accent = accentOf (s);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const bool enabled = s.isEnabled();
        const float a = kStart + pos * (kEnd - kStart);

        // Spur
        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, kStart, kEnd, true);
        g.setColour (col::bg0);
        g.strokePath (track, juce::PathStrokeType (thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::line);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Wert-Bogen mit Glow
        float from = kStart, to = a;
        if (bipolar) { const float mid = (kStart + kEnd) * 0.5f; from = std::min (mid, a); to = std::max (mid, a); }
        if (to - from > 0.002f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
            const auto ac = enabled ? accent : col::inkMute;
            g.setColour (ac.withAlpha (0.18f));
            g.strokePath (arc, juce::PathStrokeType (thick * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ac.withAlpha (0.35f));
            g.strokePath (arc, juce::PathStrokeType (thick * 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ac);
            g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Kappe
        const float cr = r - thick * 1.6f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - cr, c.y - cr + 2.0f, cr * 2.0f, cr * 2.0f);
        juce::ColourGradient cap (col::bg3.brighter (0.15f), c.x, c.y - cr, col::bg0, c.x, c.y + cr, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (accent.withAlpha (enabled ? 0.45f : 0.15f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);

        // Zeiger
        juce::Path p;
        p.startNewSubPath (0.0f, -cr * 0.25f);
        p.lineTo (0.0f, -cr * 0.88f);
        g.setColour (enabled ? col::ink : col::inkMute);
        g.strokePath (p, juce::PathStrokeType (std::max (1.6f, size * 0.045f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation (a).translated (c));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const auto accent = accentOf (s);
        const float cy = y + h * 0.5f;
        juce::Rectangle<float> tr ((float) x, cy - 2.5f, (float) w, 5.0f);
        g.setColour (col::bg0); g.fillRoundedRectangle (tr.expanded (1.0f), 3.0f);
        g.setColour (col::line); g.fillRoundedRectangle (tr, 2.5f);
        auto on = tr.withRight (pos);
        g.setColour (accent.withAlpha (s.isEnabled() ? 1.0f : 0.3f)); g.fillRoundedRectangle (on, 2.5f);
        g.setColour (col::ink);
        g.fillEllipse (pos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        g.setColour (accent); g.drawEllipse (pos - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.5f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (fonts::mono (12.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, col::ink);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        return l;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float rad = std::min (5.0f, r.getHeight() * 0.35f);
        const auto accent = accentOf (b);
        const bool on = b.getToggleState();
        juce::ColourGradient grad (on ? accent.withAlpha (0.32f) : col::bg3, 0, r.getY(),
                                   on ? accent.withAlpha (0.16f) : col::bg1, 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, rad);
        if (on)
        {
            g.setColour (accent.withAlpha (0.25f));
            g.drawRoundedRectangle (r.expanded (1.0f), rad + 1.0f, 2.0f);
        }
        g.setColour (on ? accent : over ? col::inkMute : col::line);
        g.drawRoundedRectangle (r, rad, 1.1f);
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (r, rad); }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto accent = accentOf (b);
        const float h = (float) b.getHeight();
        g.setFont (fonts::display (std::min (14.0f, h * 0.55f), true));
        g.setColour (b.getToggleState() ? accent.brighter (0.4f) : (b.isEnabled() ? col::inkDim : col::inkMute));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::bg0);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (box.hasKeyboardFocus (false) ? col::pink : col::line);
        g.drawRoundedRectangle (r, 5.0f, 1.1f);
        juce::Path arrow;
        const float ax = w - 13.0f, ay = h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (col::pink);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return fonts::display (13.0f, true); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
    juce::Font getPopupMenuFont() override { return fonts::display (15.0f); }
    juce::Font getAlertWindowMessageFont() override { return fonts::display (15.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (col::bg1);
        g.setColour (col::line);
        g.drawRect (0, 0, w, h);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (col::bg1);
        g.setColour (col::pink.withAlpha (0.6f));
        g.drawRect (0, 0, w, h);
        g.setColour (col::ink);
        g.setFont (fonts::display (14.0f));
        g.drawFittedText (text, 6, 2, w - 12, h - 4, juce::Justification::centredLeft, 4);
    }
};

// Panel-Hintergrund mit Titel
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {}, juce::Colour accent = col::pink)
{
    juce::ColourGradient bg (col::bg2, r.getX(), r.getY(), col::bg1, r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    // obere Neon-Kante
    juce::ColourGradient edge (accent.withAlpha (0.0f), r.getX(), r.getY(), accent.withAlpha (0.0f), r.getRight(), r.getY(), false);
    edge.addColour (0.5, accent.withAlpha (0.55f));
    g.setGradientFill (edge);
    g.fillRect (r.getX() + 10.0f, r.getY(), r.getWidth() - 20.0f, 1.2f);
    if (title.isNotEmpty())
    {
        g.setColour (accent);
        g.setFont (fonts::display (13.0f, true));
        g.drawText (title, r.reduced (12.0f, 0.0f).withHeight (26.0f).translated (0.0f, 2.0f), juce::Justification::centredLeft);
    }
}

} // namespace bfz::ui
