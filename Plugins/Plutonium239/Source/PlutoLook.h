#pragma once

// Optik im Stil des Reaktor-Originals: grau, tuerkise Regler-Boegen, helle Wertekaesten

#include <juce_gui_basics/juce_gui_basics.h>

namespace pluto
{
namespace col
{
    inline const juce::Colour outer      { 0xff2b2b2b };
    inline const juce::Colour section    { 0xff454545 };
    inline const juce::Colour header     { 0xff333333 };
    inline const juce::Colour panel      { 0xff4c4c4c };
    inline const juce::Colour border     { 0xff262626 };
    inline const juce::Colour text       { 0xffd8d8d8 };
    inline const juce::Colour textDark   { 0xff101010 };
    inline const juce::Colour cyan       { 0xff3ef2f2 };
    inline const juce::Colour cyanBox    { 0xff8cfcfc };
    inline const juce::Colour teal       { 0xff2e6b6b };
    inline const juce::Colour valueBox   { 0xffc4c4c4 };
    inline const juce::Colour knob       { 0xff2c2c2c };
    inline const juce::Colour combo      { 0xff5b5b5b };
    inline const juce::Colour red        { 0xffff3030 };
}

class PlutoLook : public juce::LookAndFeel_V4
{
public:
    PlutoLook()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, col::outer);
        setColour (Slider::textBoxBackgroundColourId, col::valueBox);
        setColour (Slider::textBoxTextColourId, col::textDark);
        setColour (Slider::textBoxOutlineColourId, col::border);
        setColour (Slider::textBoxHighlightColourId, col::cyan.withAlpha (0.5f));
        setColour (Label::textColourId, col::text);
        setColour (Label::textWhenEditingColourId, col::textDark);
        setColour (TextEditor::backgroundColourId, col::valueBox);
        setColour (TextEditor::textColourId, col::textDark);
        setColour (TextEditor::highlightColourId, col::cyan.withAlpha (0.5f));
        setColour (ComboBox::backgroundColourId, col::combo);
        setColour (ComboBox::textColourId, col::text);
        setColour (ComboBox::outlineColourId, col::border);
        setColour (ComboBox::arrowColourId, col::text);
        setColour (PopupMenu::backgroundColourId, col::panel);
        setColour (PopupMenu::textColourId, col::text);
        setColour (PopupMenu::highlightedBackgroundColourId, col::valueBox);
        setColour (PopupMenu::highlightedTextColourId, col::textDark);
        setColour (TextButton::buttonColourId, col::combo);
        setColour (TextButton::buttonOnColourId, col::teal);
        setColour (TextButton::textColourOffId, col::text);
        setColour (TextButton::textColourOnId, col::cyan);
        setColour (ToggleButton::textColourId, col::text);
        setColour (TooltipWindow::backgroundColourId, col::header);
        setColour (TooltipWindow::textColourId, col::text);
        setColour (BubbleComponent::backgroundColourId, col::header);
        setColour (BubbleComponent::outlineColourId, col::border);
    }

    static juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font getLabelFont (juce::Label&) override { return font (11.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11.0f); }
    juce::Font getPopupMenuFont() override { return font (12.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (11.0f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return font (11.0f); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (10.0f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        using namespace juce;
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);

        // Bogen-Hintergrund
        Path bg;
        bg.addCentredArc (c.x, c.y, r - 1.5f, r - 1.5f, 0, a0, a1, true);
        g.setColour (col::border);
        g.strokePath (bg, PathStrokeType (3.0f));

        // tuerkiser Wert-Bogen (bipolar ab Mitte)
        const bool bip = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bip ? (a0 + a1) * 0.5f : a0;
        Path arc;
        arc.addCentredArc (c.x, c.y, r - 1.5f, r - 1.5f, 0, jmin (from, ang), jmax (from, ang), true);
        g.setColour (s.isEnabled() ? col::cyan : col::cyan.withAlpha (0.3f));
        g.strokePath (arc, PathStrokeType (3.0f));

        // Knopfkoerper
        const float kr = r - 4.5f;
        g.setGradientFill (ColourGradient (Colour (0xff4a4a4a), c.x, c.y - kr, col::knob, c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (col::border);
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

        // Zeiger
        Path p;
        p.addRoundedRectangle (-1.2f, -kr + 1.0f, 2.4f, kr * 0.65f, 1.0f);
        g.setColour (col::cyan);
        g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        using namespace juce;
        if (style == Slider::LinearBar || style == Slider::LinearBarVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
            return;
        }
        const bool vert = style == Slider::LinearVertical;
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        g.setColour (col::border);
        if (vert)
        {
            g.fillRoundedRectangle (b.getCentreX() - 2.0f, b.getY(), 4.0f, b.getHeight(), 2.0f);
            auto t = Rectangle<float> (0, 0, jmin (16.0f, b.getWidth()), 10.0f).withCentre ({ b.getCentreX(), pos });
            g.setColour (Colour (0xff6e6e6e));
            g.fillRoundedRectangle (t, 2.0f);
            g.setColour (col::cyan);
            g.fillRect (t.reduced (2.0f, 3.5f));
            g.setColour (col::border);
            g.drawRoundedRectangle (t, 2.0f, 1.0f);
        }
        else
        {
            g.fillRoundedRectangle (b.getX(), b.getCentreY() - 2.0f, b.getWidth(), 4.0f, 2.0f);
            g.setColour (col::cyan.withAlpha (0.55f));
            g.fillRoundedRectangle (b.getX(), b.getCentreY() - 1.0f, pos - b.getX(), 2.0f, 1.0f);
            auto t = Rectangle<float> (10.0f, jmin (14.0f, b.getHeight())).withCentre ({ pos, b.getCentreY() });
            g.setColour (Colour (0xff6e6e6e));
            g.fillRoundedRectangle (t, 2.0f);
            g.setColour (col::cyan);
            g.fillRect (t.reduced (3.5f, 2.0f));
            g.setColour (col::border);
            g.drawRoundedRectangle (t, 2.0f, 1.0f);
        }
    }

    int getSliderThumbRadius (juce::Slider& s) override
    {
        return s.isHorizontal() ? 5 : 5;
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto b = Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (box.isEnabled() ? col::combo : col::combo.darker (0.4f));
        g.fillRect (b);
        auto ab = b.removeFromRight ((float) h);
        g.setColour (Colour (0xff6c6c6c));
        g.fillRect (ab.reduced (1.0f));
        g.setColour (col::border);
        g.drawRect (Rectangle<float> (0, 0, (float) w, (float) h), 1.0f);
        Path tri;
        auto c = ab.getCentre();
        tri.addTriangle (c.x - 3.5f, c.y - 2.0f, c.x + 3.5f, c.y - 2.0f, c.x, c.y + 2.5f);
        g.setColour (col::text);
        g.fillPath (tri);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 0, box.getWidth() - box.getHeight() - 2, box.getHeight());
        label.setFont (font (11.0f));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        auto sw = r.removeFromLeft (jmin (26.0f, r.getWidth())).reduced (1.0f, jmax (1.0f, (r.getHeight() - 12.0f) * 0.5f));
        g.setColour (col::border);
        g.fillRoundedRectangle (sw, 2.0f);
        auto knobR = sw.reduced (1.5f);
        knobR = on ? knobR.removeFromRight (knobR.getWidth() * 0.5f) : knobR.removeFromLeft (knobR.getWidth() * 0.5f);
        g.setColour (on ? col::cyan : Colour (0xff7a7a7a).withMultipliedBrightness (hover ? 1.2f : 1.0f));
        g.fillRoundedRectangle (knobR, 1.5f);
        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (col::text);
            g.setFont (font (11.0f));
            g.drawText (b.getButtonText(), r.withTrimmedLeft (3.0f), Justification::centredLeft);
        }
    }
};

// Rahmen mit Titelleiste (Sektion) bzw. eingelassenes Feld (Panel)
inline void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    using namespace juce;
    g.setColour (col::section);
    g.fillRect (r);
    g.setColour (col::border);
    g.drawRect (r, 1);
    auto hb = r.removeFromTop (16).reduced (1, 1);
    g.setColour (col::header);
    g.fillRect (hb);
    g.setColour (col::text);
    g.setFont (PlutoLook::font (12.0f));
    g.drawText (title, hb, Justification::centred);
}

inline void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    using namespace juce;
    g.setColour (col::panel);
    g.fillRect (r);
    g.setColour (col::border);
    g.drawRect (r, 1);
    g.setColour (Colour (0xff5e5e5e));
    g.drawLine ((float) r.getX() + 1, (float) r.getY() + 1, (float) r.getRight() - 1, (float) r.getY() + 1, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (col::text);
        g.setFont (PlutoLook::font (11.5f));
        g.drawText (title, r.withHeight (14), Justification::centred);
    }
}
} // namespace pluto
