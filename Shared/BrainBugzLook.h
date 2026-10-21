#pragma once

// Gemeinsamer Look für alle Brain-Bugz-Plugins: Farben, Schriften, Regler, Taster

#include <juce_gui_basics/juce_gui_basics.h>
#include "BrainBugzAssets.h"

namespace bb
{
namespace col
{
    inline const juce::Colour voidBg   { 0xff07050a };
    inline const juce::Colour panel    { 0xff110c16 };
    inline const juce::Colour panel2   { 0xff1a1222 };
    inline const juce::Colour line     { 0xff2d2136 };
    inline const juce::Colour tox      { 0xffb6ff1f };
    inline const juce::Colour toxDim   { 0xff5d8a12 };
    inline const juce::Colour brain    { 0xffff2f8e };
    inline const juce::Colour ink      { 0xffece6f2 };
    inline const juce::Colour inkDim   { 0xff9a8ea8 };
}

namespace fonts
{
    inline juce::Typeface::Ptr load (const char* resourceName)
    {
        int size = 0;
        if (auto* data = BBAssets::getNamedResource (resourceName, size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        return nullptr;
    }

    inline juce::Font display (float height, bool bold = false)
    {
        static juce::Typeface::Ptr semi = load ("ChakraPetchSemiBold_ttf");
        static juce::Typeface::Ptr heavy = load ("ChakraPetchBold_ttf");
        auto tf = bold && heavy != nullptr ? heavy : semi;
        if (tf != nullptr)
            return juce::Font (juce::FontOptions (tf).withHeight (height));
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font mono (float height)
    {
        static juce::Typeface::Ptr tf = load ("ShareTechMonoRegular_ttf");
        if (tf != nullptr)
            return juce::Font (juce::FontOptions (tf).withHeight (height));
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
    }
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::TextButton::buttonColourId, col::panel2);
        setColour (juce::TextButton::textColourOffId, col::ink);
        setColour (juce::TextButton::textColourOnId, col::tox);
        setColour (juce::Slider::textBoxTextColourId, col::ink);
    }

    static constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float endAngle   = juce::MathConstants<float>::pi * 2.75f;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float, float, juce::Slider& s) override
    {
        const float size = (float) std::min (w, h);
        const float u = size / 100.0f; // Entwurf in 100er-Einheiten wie im Prototyp
        const juce::Point<float> c ((float) x + w * 0.5f, (float) y + h * 0.5f);
        const float r = 40.0f * u, thick = 7.0f * u;
        const float a = startAngle + pos * (endAngle - startAngle);

        // Schatten
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - 30.0f * u, c.y - 26.0f * u, 60.0f * u, 64.0f * u);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
        g.setColour (col::line);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto accent = s.getProperties().getWithDefault ("accent", "tox").toString();
        const bool krass  = (bool) s.getProperties().getWithDefault ("krass", false);
        auto arc = [&] (float from, float to, juce::Colour colour)
        {
            if (to - from < 0.003f) return;
            juce::Path p;
            p.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle + from * (endAngle - startAngle),
                             startAngle + to * (endAngle - startAngle), true);
            g.setColour (colour.withAlpha (0.25f));
            g.strokePath (p, juce::PathStrokeType (thick * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colour);
            g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        if (krass)
        {
            arc (0.0f, std::min (pos, 0.5f), col::tox);
            if (pos > 0.5f) arc (0.5f, pos, col::brain);
        }
        else
            arc (0.0f, pos, accent == "brain" ? col::brain : col::tox);

        g.setColour (col::panel2);
        g.fillEllipse (c.x - 28.0f * u, c.y - 28.0f * u, 56.0f * u, 56.0f * u);
        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - 28.0f * u, c.y - 28.0f * u, 56.0f * u, 56.0f * u, 2.0f * u);
        juce::ColourGradient cap (juce::Colour (0xff1d1524), c.x, c.y - 22.0f * u, juce::Colour (0xff0d0911), c.x, c.y + 22.0f * u, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - 22.0f * u, c.y - 22.0f * u, 44.0f * u, 44.0f * u);

        juce::Path pointer;
        pointer.startNewSubPath (0.0f, 0.0f);
        pointer.lineTo (0.0f, -22.0f * u);
        g.setColour (col::ink);
        g.strokePath (pointer, juce::PathStrokeType (4.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation (a).translated (c));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float rad = std::min (6.0f, r.getHeight() * 0.3f);
        juce::ColourGradient grad (juce::Colour (0xff332c3a), 0, r.getY(), juce::Colour (0xff1b1720), 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, rad);
        const auto accent = b.getProperties().getWithDefault ("accent", "tox").toString() == "brain" ? col::brain : col::tox;
        juce::Colour border = juce::Colour (0xff0a080c);
        if (b.getToggleState()) border = accent;
        else if (over) border = col::toxDim;
        g.setColour (border);
        g.drawRoundedRectangle (r, rad, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (down ? 0.04f : 0.12f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + rad, r.getRight() - rad);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto accent = b.getProperties().getWithDefault ("accent", "tox").toString() == "brain" ? col::brain : col::tox;
        const bool small = (bool) b.getProperties().getWithDefault ("mono", false);
        g.setFont (small ? fonts::mono ((float) b.getHeight() * 0.62f) : fonts::display ((float) b.getHeight() * 0.42f));
        g.setColour (b.getToggleState() ? accent : col::ink);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }
};

} // namespace bb
