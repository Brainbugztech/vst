#pragma once

// Bedienelemente im Look des Entwurfs: Regler mit Leuchtring, Glow-Taster, Panels, Tabs

#include <juce_audio_processors/juce_audio_processors.h>
#include "BrainBugzLook.h"

namespace ttsui
{
namespace col
{
    inline const juce::Colour cyan   { 0xff2ff3ff };
    inline const juce::Colour cyan2  { 0xff19c9e6 };
    inline const juce::Colour purple { 0xff9b3cff };
    inline const juce::Colour pink   { 0xffff5fc8 };
    inline const juce::Colour violet { 0xffc9b8ff };
    inline const juce::Colour text   { 0xffd9d0ff };
    inline const juce::Colour dim    { 0xff8a80b8 };
    inline const juce::Colour label  { 0xff9db4ef };
    inline const juce::Colour panel  { 0xf20b0718 };
    inline const juce::Colour inner  { 0xff0e0a1c };
}

inline juce::Font mono (float h) { return bb::fonts::mono (h); }

// ---------------------------------------------------------------- Panel im Stil des Entwurfs (doppelte Neonlinie, abgeschrägte Ecken)

inline juce::Path chamfered (juce::Rectangle<float> r, float c)
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

inline void drawNeonPanel (juce::Graphics& g, juce::Rectangle<float> r, float chamfer = 24.0f, bool fill = true)
{
    auto outer = chamfered (r, chamfer);
    if (fill)
    {
        juce::ColourGradient bgGrad (juce::Colour (0xff120c22), r.getX(), r.getY(), juce::Colour (0xff0a0714), r.getX(), r.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillPath (outer);
    }
    juce::ColourGradient line (col::purple, r.getX(), r.getY(), juce::Colour (0xff3a6bff), r.getRight(), r.getBottom(), false);
    line.addColour (0.5, juce::Colour (0xff7a3cff));
    // Glow
    g.setColour (col::purple.withAlpha (0.18f));
    g.strokePath (outer, juce::PathStrokeType (7.0f));
    g.setGradientFill (line);
    g.strokePath (outer, juce::PathStrokeType (2.4f));
    auto in = chamfered (r.reduced (9.0f), chamfer - 6.0f);
    g.setColour (col::purple.withAlpha (0.55f));
    g.strokePath (in, juce::PathStrokeType (1.3f));
}

inline void drawSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool on)
{
    g.setColour (juce::Colour (0x66000000));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour ((on ? col::cyan2 : col::dim).withAlpha (on ? 0.55f : 0.3f));
    g.drawRoundedRectangle (r, 8.0f, 1.4f);
    g.setFont (mono (22.0f));
    g.setColour (on ? col::cyan : col::dim);
    g.drawText (title, r.withHeight (40.0f).withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);
}

// ---------------------------------------------------------------- LookAndFeel

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
    }

    juce::Font getComboBoxFont (juce::ComboBox& b) override { return mono (juce::jmin (24.0f, (float) b.getHeight() * 0.58f)); }
    juce::Font getPopupMenuFont() override { return mono (20.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return mono ((float) l.getHeight() * 0.6f); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const bool framed = (bool) box.getProperties().getWithDefault ("framed", false);
        if (framed)
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
            g.setColour (juce::Colour (0xff0b0718));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour ((box.isMouseOver (true) ? col::cyan2 : col::purple).withAlpha (0.8f));
            g.drawRoundedRectangle (r, 5.0f, 1.5f);
        }
        const float s = framed ? 0.6f : 1.0f;
        juce::Path arrow;
        const float aw = 20.0f * s, ah = 12.0f * s;
        const float ax = (float) w - (framed ? 22.0f : 34.0f), ay = (float) h * 0.5f - ah * 0.45f;
        arrow.addTriangle (ax, ay, ax + aw, ay, ax + aw * 0.5f, ay + ah);
        g.setColour (box.isMouseOver (true) ? col::cyan : col::purple);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        const bool framed = (bool) box.getProperties().getWithDefault ("framed", false);
        label.setBounds (framed ? 6 : 8, 0, box.getWidth() - (framed ? 30 : 50), box.getHeight());
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
        if (thumbSize <= 0) return;
        auto r = vertical ? juce::Rectangle<float> ((float) x + 2.0f, (float) thumbStart, (float) w - 4.0f, (float) thumbSize)
                          : juce::Rectangle<float> ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) h - 4.0f);
        g.setColour (col::purple.withAlpha (over ? 0.8f : 0.5f));
        g.fillRoundedRectangle (r, 3.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool purple = (bool) b.getProperties().getWithDefault ("purple", false);
        const bool tab = (bool) b.getProperties().getWithDefault ("tab", false);
        const auto accent = purple ? col::purple : col::cyan2;
        const bool on = b.getToggleState();
        const float rad = tab ? 4.0f : 6.0f;
        g.setColour (juce::Colour (0xff0b0718).withAlpha (0.92f));
        g.fillRoundedRectangle (r, rad);
        float a = down ? 0.35f : (over ? 0.2f : 0.08f);
        if (on) a = juce::jmax (a, 0.32f);
        g.setColour (accent.withAlpha (a));
        g.fillRoundedRectangle (r, rad);
        if (on)
        {
            g.setColour (accent.withAlpha (0.25f));
            g.drawRoundedRectangle (r.expanded (2.0f), rad + 2.0f, 3.0f);
        }
        g.setColour (on ? accent : accent.withAlpha (0.55f));
        g.drawRoundedRectangle (r, rad, on ? 2.0f : 1.4f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const float scale = (float) b.getProperties().getWithDefault ("textScale", 0.48);
        g.setFont (mono ((float) b.getHeight() * scale));
        g.setColour (! b.isEnabled() ? col::dim : (b.getToggleState() ? juce::Colours::white : col::text));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
    }
};

// ---------------------------------------------------------------- Taster

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

// Ein/Aus-Taster mit Power-Symbol (an einen Bool-Parameter gebunden)
class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ("power")
    {
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        const bool on = getToggleState();
        const auto c = on ? col::cyan : col::dim.withAlpha (0.7f);
        const auto ctr = r.getCentre();
        const float rad = r.getWidth() * 0.32f;
        juce::Path arc;
        arc.addCentredArc (ctr.x, ctr.y, rad, rad, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        juce::Path stem;
        stem.startNewSubPath (ctr.x, ctr.y - rad * 1.25f);
        stem.lineTo (ctr.x, ctr.y - rad * 0.15f);
        const float w = juce::jmax (2.5f, r.getWidth() * 0.09f);
        if (on || over)
        {
            g.setColour (c.withAlpha (on ? 0.3f : 0.15f));
            g.strokePath (arc, juce::PathStrokeType (w * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.strokePath (stem, juce::PathStrokeType (w * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (c);
        g.strokePath (arc, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (stem, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

// ---------------------------------------------------------------- Regler

// Regler: dunkler Ring, Leuchtbogen, drehende Kappe aus dem Entwurf
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

    void setBipolar (bool b) { bi = b; repaint(); }
    bool coverRing = true;

    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
        const float n = (float) valueToProportionOfLength (getValue());
        const float alpha = isEnabled() ? 1.0f : 0.4f;
        auto ring = [&] (float from, float to, juce::Colour colour, float width)
        {
            juce::Path p;
            p.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, from, to, true);
            g.setColour (colour.withMultipliedAlpha (alpha));
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        };
        if (coverRing) ring (0.0f, juce::MathConstants<float>::twoPi, juce::Colour (0xff0a0716), sw + 4.0f);
        ring (start, end, juce::Colour (0xff1b1336), sw);
        const float from = bi ? 0.5f : 0.0f;
        const float a = juce::jmin (from, n), b = juce::jmax (from, n);
        if (b - a > 0.002f)
        {
            const float aa = start + a * (end - start), bb = start + b * (end - start);
            ring (aa, bb, col::cyan.withAlpha (0.22f), sw * 2.2f);
            const int steps = juce::jmax (2, (int) ((bb - aa) * 18.0f));
            for (int i = 0; i < steps; ++i)
            {
                const float t0 = (float) i / (float) steps, t1 = (float) (i + 1) / (float) steps;
                const float pos = (a + (b - a) * (t0 + t1) * 0.5f);
                ring (aa + (bb - aa) * t0, aa + (bb - aa) * t1 + 0.01f,
                      col::cyan2.interpolatedWith (col::purple, juce::jlimit (0.0f, 1.0f, pos)), sw);
            }
        }
        const float angle = start + n * (end - start);
        if (cap.isValid())
        {
            const auto t = juce::AffineTransform::translation (-(float) cap.getWidth() * 0.5f, -(float) cap.getHeight() * 0.5f)
                               .scaled (capR * 2.0f / (float) cap.getWidth())
                               .rotated (angle - juce::MathConstants<float>::twoPi)
                               .translated (c);
            g.setOpacity (alpha);
            g.drawImageTransformed (cap, t);
            g.setOpacity (1.0f);
        }
        if (drawPointer)
        {
            juce::Path p;
            p.addRoundedRectangle (-1.5f, -capR * 0.92f, 3.0f, capR * 0.62f, 1.5f);
            g.setColour (col::cyan.withAlpha (0.35f * alpha));
            g.strokePath (p, juce::PathStrokeType (3.0f), juce::AffineTransform::rotation (angle).translated (c));
            g.setColour (col::cyan.withMultipliedAlpha (alpha));
            g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
        }
    }

private:
    juce::Image cap;
    float capR, ringR, sw;
    bool bi, drawPointer;
};

// Regler mit Beschriftung oben und Wert unten, an einen Parameter gebunden
class ParamKnob : public juce::Component
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& labelText,
               const juce::Image& capImage, bool bipolar = false, float capRadius = 22.0f)
        : param (s.getParameter (id)), label (labelText),
          knob (capImage, capRadius, capRadius + 3.0f, 5.0f, bipolar, true)
    {
        knob.coverRing = false;
        addAndMakeVisible (knob);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, knob);
        if (param != nullptr)
            knob.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        knob.onValueChange = [this] { repaint(); };
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (20);
        r.removeFromBottom (22);
        knob.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        const float a = isEnabled() ? 1.0f : 0.4f;
        g.setFont (mono (15.0f));
        g.setColour (col::label.withMultipliedAlpha (a));
        g.drawText (label, getLocalBounds().removeFromTop (20), juce::Justification::centred, false);
        g.setFont (mono (15.0f));
        g.setColour (col::violet.withMultipliedAlpha (a));
        g.drawText (param != nullptr ? param->getCurrentValueAsText() : juce::String(),
                    getLocalBounds().removeFromBottom (22), juce::Justification::centred, false);
    }

    juce::RangedAudioParameter* param;
    juce::String label;
    Knob knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};
} // namespace ttsui
