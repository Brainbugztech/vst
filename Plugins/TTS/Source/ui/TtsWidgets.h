#pragma once

// Bedienelemente im Look des Entwurfs: Regler mit Leuchtring, unsichtbare Taster mit Glow, Dropdowns ohne Rahmen

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzLook.h"

namespace ttsui
{
namespace col
{
    inline const juce::Colour cyan   { 0xff2ff3ff };
    inline const juce::Colour cyan2  { 0xff19c9e6 };
    inline const juce::Colour purple { 0xff9b3cff };
    inline const juce::Colour violet { 0xffc9b8ff };
    inline const juce::Colour text   { 0xffd9d0ff };
    inline const juce::Colour dim    { 0xff8a80b8 };
    inline const juce::Colour label  { 0xff9db4ef };
    inline const juce::Colour panel  { 0xf20b0718 };
}

inline juce::Font mono (float h) { return bb::fonts::mono (h); }

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::ComboBox::textColourId, col::violet);
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, col::purple);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf50b0718));
        setColour (juce::PopupMenu::textColourId, col::violet);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::purple.withAlpha (0.3f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, col::cyan2);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::TextEditor::highlightColourId, col::purple.withAlpha (0.45f));
        setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::CaretComponent::caretColourId, col::cyan);
        setColour (juce::ScrollBar::thumbColourId, col::purple.withAlpha (0.55f));
        setColour (juce::Slider::trackColourId, col::cyan2);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1b1336));
        setColour (juce::Slider::thumbColourId, col::cyan);
    }

    juce::Font getComboBoxFont (juce::ComboBox& b) override { return mono ((float) b.getHeight() * 0.55f); }
    juce::Font getPopupMenuFont() override { return mono (20.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return mono ((float) l.getHeight() * 0.6f); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        // Rahmen ist im Hintergrundbild, hier nur der Pfeil
        juce::Path arrow;
        const float ax = (float) w - 34.0f, ay = (float) h * 0.5f - 5.0f;
        arrow.addTriangle (ax, ay, ax + 20.0f, ay, ax + 10.0f, ay + 12.0f);
        g.setColour (col::purple.withAlpha (0.35f));
        g.fillPath (arrow, juce::AffineTransform::scale (1.15f, 1.15f, ax + 10.0f, ay + 6.0f));
        g.setColour (box.isMouseOver (true) ? col::cyan : col::purple);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 50, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (col::purple);
        g.drawRect (0, 0, w, h, 2);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (juce::Colour (0xff1b1336));
        g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) w, 6.0f, 3.0f);
        juce::ColourGradient grad (col::cyan2, (float) x, cy, col::purple, (float) (x + w), cy, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle ((float) x, cy - 3.0f, pos - (float) x, 6.0f, 3.0f);
        g.setColour (col::cyan);
        g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool) override
    {
        juce::ignoreUnused (x, y, w, h);
        if (thumbSize <= 0) return;
        auto r = vertical ? juce::Rectangle<float> ((float) x + 2.0f, (float) thumbStart, (float) w - 4.0f, (float) thumbSize)
                          : juce::Rectangle<float> ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) h - 4.0f);
        g.setColour (col::purple.withAlpha (over ? 0.8f : 0.5f));
        g.fillRoundedRectangle (r, 3.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = b.getProperties().getWithDefault ("purple", false) ? col::purple : col::cyan2;
        g.setColour (juce::Colour (0xff0b0718).withAlpha (0.9f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (accent.withAlpha (down ? 0.35f : (over ? 0.22f : 0.12f)));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (accent);
        g.drawRoundedRectangle (r, 6.0f, 2.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (mono ((float) b.getHeight() * 0.48f));
        g.setColour (b.isEnabled() ? col::text : col::dim);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1);
    }
};

// Unsichtbarer Taster über einem im Hintergrund gemalten Button: zeigt nur Glow bei Hover/Klick
class GlowButton : public juce::Button
{
public:
    GlowButton (const juce::String& name, juce::Colour c, float radius = 6.0f) : juce::Button (name), accent (c), rad (radius)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    bool busy = false;
    float busyPhase = 0.0f;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        float a = down ? 0.35f : (over ? 0.2f : 0.0f);
        if (busy) a = juce::jmax (a, 0.15f + 0.2f * (0.5f + 0.5f * std::sin (busyPhase)));
        if (a <= 0.0f) return;
        g.setColour (accent.withAlpha (a * 0.45f));
        g.fillRoundedRectangle (r, rad);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (accent.withAlpha (a * 0.25f / (float) i));
            g.drawRoundedRectangle (r.expanded ((float) i * 1.5f), rad + (float) i, 2.0f);
        }
    }

private:
    juce::Colour accent;
    float rad;
};

// Regler: dunkler Ring über dem gemalten Ring, Leuchtbogen, drehende Kappe aus dem Entwurf
class Knob : public juce::Slider
{
public:
    Knob (const juce::Image& capImage, float capRadius, float ringRadius, float strokeWidth, bool bipolar, bool pointer)
        : cap (capImage), capR (capRadius), ringR (ringRadius), sw (strokeWidth), bi (bipolar), drawPointer (pointer)
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (220);
        setVelocityModeParameters (0.6, 1, 0.08, true, juce::ModifierKeys::shiftModifier);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
        const float n = (float) valueToProportionOfLength (getValue());
        auto ring = [&] (float from, float to, juce::Colour colour, float width)
        {
            juce::Path p;
            p.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, from, to, true);
            g.setColour (colour);
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        };
        // gemalten Ring abdecken
        ring (0.0f, juce::MathConstants<float>::twoPi, juce::Colour (0xff0a0716), sw + 4.0f);
        ring (start, end, juce::Colour (0xff1b1336), sw);
        const float from = bi ? 0.5f : 0.0f;
        const float a = juce::jmin (from, n), b = juce::jmax (from, n);
        if (b - a > 0.002f)
        {
            const float aa = start + a * (end - start), bb = start + b * (end - start);
            ring (aa, bb, col::cyan.withAlpha (0.22f), sw * 2.2f);
            // Farbverlauf Cyan -> Lila entlang des Bogens
            const int steps = juce::jmax (2, (int) ((bb - aa) * 18.0f));
            for (int i = 0; i < steps; ++i)
            {
                const float t0 = (float) i / (float) steps, t1 = (float) (i + 1) / (float) steps;
                const float pos = (a + (b - a) * (t0 + t1) * 0.5f);
                ring (aa + (bb - aa) * t0, aa + (bb - aa) * t1 + 0.01f,
                      col::cyan2.interpolatedWith (col::purple, juce::jlimit (0.0f, 1.0f, pos)), sw);
            }
        }
        // Kappe
        const float angle = start + n * (end - start);
        if (cap.isValid())
        {
            const auto t = juce::AffineTransform::translation (-(float) cap.getWidth() * 0.5f, -(float) cap.getHeight() * 0.5f)
                               .scaled (capR * 2.0f / (float) cap.getWidth())
                               .rotated (angle - juce::MathConstants<float>::twoPi)
                               .translated (c);
            g.drawImageTransformed (cap, t);
        }
        if (drawPointer)
        {
            juce::Path p;
            p.addRoundedRectangle (-1.5f, -capR * 0.92f, 3.0f, capR * 0.62f, 1.5f);
            g.setColour (col::cyan.withAlpha (0.35f));
            g.strokePath (p, juce::PathStrokeType (3.0f), juce::AffineTransform::rotation (angle).translated (c));
            g.setColour (col::cyan);
            g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
        }
    }

private:
    juce::Image cap;
    float capR, ringR, sw;
    bool bi, drawPointer;
};
} // namespace ttsui
