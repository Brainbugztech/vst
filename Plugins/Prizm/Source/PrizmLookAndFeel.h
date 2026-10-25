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
    inline const juce::Colour tox        { 0xffb6ff1f };
    inline const juce::Colour toxInk     { 0xff0b1200 };
    inline const juce::Colour pink       { 0xffff2f8e };
    inline const juce::Colour amber      { 0xffffaa28 };
    inline const juce::Colour cyan       { 0xff2fe6ff };
    inline const juce::Colour violet     { 0xff9b5cff };

    inline juce::ColourGradient spectrum (float x1, float y, float x2, float alpha = 1.0f)
    {
        juce::ColourGradient g (pink.withAlpha (alpha), x1, y, violet.withAlpha (alpha), x2, y, false);
        g.addColour (0.25, amber.withAlpha (alpha));
        g.addColour (0.5, tox.withAlpha (alpha));
        g.addColour (0.75, cyan.withAlpha (alpha));
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
        const auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float valueAngle = startAngle + pos * (endAngle - startAngle);
        const bool hasOverdrive = (bool) slider.getProperties().getWithDefault ("overdrive", false);
        const float splitAngle = startAngle + 0.5f * (endAngle - startAngle);
        const bool hot = slider.isMouseOverOrDragging();

        // --- holo highlight: thin value ring around the knob ---------------------
        const float ringR = radius - 4.0f;
        Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (track, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Col::holoDim.withAlpha (0.55f));
        g.strokePath (track, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));

        auto glowArc = [&] (float from, float to, Colour col)
        {
            if (to <= from + 0.0005f)
                return;
            Path p;
            p.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, from, to, true);
            g.setColour (col.withAlpha (hot ? 0.20f : 0.12f));
            g.strokePath (p, PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (col.withAlpha (hot ? 0.40f : 0.28f));
            g.strokePath (p, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (col.brighter (0.2f));
            g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        };
        if (hasOverdrive)
        {
            glowArc (startAngle, jmin (valueAngle, splitAngle), Col::tox);
            glowArc (splitAngle, valueAngle, Col::pink);
        }
        else
        {
            glowArc (startAngle, valueAngle, Col::tox);
        }

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

        // bevel between skirt and cap
        ColourGradient bevel (Colour (0xff6a7480), c.x - capR, c.y - capR, Colour (0xff050608), c.x + capR * 0.6f, c.y + capR, false);
        g.setGradientFill (bevel);
        g.fillEllipse (cap.expanded (1.6f));

        // brushed cap: conic sheen (two highlights opposite each other)
        g.setColour (Colour (0xff1a1f24));
        g.fillEllipse (cap);
        const int wedges = 90;
        for (int i = 0; i < wedges; ++i)
        {
            const float a0 = MathConstants<float>::twoPi * (float) i / (float) wedges;
            const float a1 = MathConstants<float>::twoPi * (float) (i + 1) / (float) wedges;
            const float sheen = std::pow (std::abs (std::cos ((a0 + a1) * 0.5f + 0.75f)), 6.0f);
            Path w;
            w.addPieSegment (cap, a0, a1 + 0.004f, 0.0f);
            g.setColour (Colour (0xff1a1f24).interpolatedWith (Colour (0xff5f6a74), sheen * 0.9f));
            g.fillPath (w);
        }
        // fine circular brushing
        for (float rr = capR * 0.15f; rr < capR; rr += 1.6f)
        {
            g.setColour ((((int) (rr * 3.0f)) % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
            g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 0.8f);
        }
        // depth + wet gloss
        ColourGradient depth (juce::Colours::transparentBlack, c.x, c.y - capR * 0.2f, juce::Colours::black.withAlpha (0.45f), c.x, c.y + capR * 1.2f, true);
        g.setGradientFill (depth);
        g.fillEllipse (cap);
        ColourGradient gloss (juce::Colours::white.withAlpha (0.22f), c.x - capR * 0.35f, c.y - capR * 0.55f,
                              juce::Colours::white.withAlpha (0.0f), c.x - capR * 0.35f, c.y + capR * 0.1f, true);
        g.setGradientFill (gloss);
        g.fillEllipse (cap.reduced (capR * 0.12f).translated (-capR * 0.12f, -capR * 0.2f).withHeight (capR * 0.9f));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (cap, 0.8f);

        // tiny water drops on the cap
        auto drop = [&] (float ax, float ay, float rr)
        {
            const Point<float> p (c.x + ax * capR, c.y + ay * capR);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (p.translated (0.4f, 0.6f)));
            g.setColour (Colour (0xff3a444d).withAlpha (0.9f));
            g.fillEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (p));
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.fillEllipse (Rectangle<float> (rr * 0.8f, rr * 0.8f).withCentre (p.translated (-rr * 0.35f, -rr * 0.35f)));
        };
        drop (0.42f, 0.38f, 1.7f);
        drop (-0.30f, 0.52f, 1.2f);

        // --- holo highlight: glowing indicator in a machined groove --------------
        const auto tip = c.getPointOnCircumference (capR * 0.86f, valueAngle);
        const auto inner = c.getPointOnCircumference (capR * 0.30f, valueAngle);
        const auto pc = (hasOverdrive && valueAngle > splitAngle) ? Col::pink : Col::holoBright;
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
                g.setFont (bb::fonts::mono (16.0f));
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
        return layout;
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        const auto r = Rectangle<int> (0, 0, width, height).toFloat();
        holo::box (g, r, 1.0f, box.isMouseOver (true) || box.isPopupActive());

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
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (Col::pink.withAlpha (0.10f / (float) i));
                g.fillRoundedRectangle (r.expanded ((float) i * 2.5f), 3.0f + (float) i * 2.5f);
            }
            ColourGradient fill (Col::pink.brighter (0.15f), 0.0f, r.getY(), Col::pink.darker (0.35f), 0.0f, r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
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

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return bb::fonts::display (14.0f, true).withExtraKerningFactor (0.08f);
    }
};
} // namespace prizm
