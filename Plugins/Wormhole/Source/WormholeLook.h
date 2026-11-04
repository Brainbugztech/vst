#pragma once

// Wormhole-Look: Anthrazit, Akzent Violett -> Cyan, feine Chromkanten.
// Alles wird als Vektor gezeichnet und bleibt in jeder Skalierung scharf.

#include <juce_gui_basics/juce_gui_basics.h>
#include "WormholeAssets.h"
#include "LogoPaths.h"

namespace whl
{
namespace col
{
    inline const juce::Colour bgCentre { 0xff1a1b23 };
    inline const juce::Colour bgMid    { 0xff121319 };
    inline const juce::Colour bgEdge   { 0xff0b0c10 };
    inline const juce::Colour well     { 0xff0e0f13 };
    inline const juce::Colour track    { 0xff23252e };
    inline const juce::Colour ink      { 0xffeceef4 };
    inline const juce::Colour dim      { 0xff8a8f9d };
    inline const juce::Colour faint    { 0xff4a4e5a };
    inline const juce::Colour vio      { 0xff8b3dff };
    inline const juce::Colour mag      { 0xffc03cff };
    inline const juce::Colour cyan     { 0xff00e0ff };
    inline const juce::Colour ice      { 0xffbff6ff };
}

inline juce::ColourGradient accent (float x1, float y1, float x2, float y2)
{
    return juce::ColourGradient (col::vio, x1, y1, col::cyan, x2, y2, false);
}

// ---------------------------------------------------------------------------
//  Schriften (Inter, OFL)
// ---------------------------------------------------------------------------
namespace fonts
{
    inline juce::Typeface::Ptr load (const char* originalFile)
    {
        for (int i = 0; i < WormholeAssets::namedResourceListSize; ++i)
            if (juce::String (WormholeAssets::originalFilenames[i]) == originalFile)
            {
                int size = 0;
                if (auto* d = WormholeAssets::getNamedResource (WormholeAssets::namedResourceList[i], size))
                    return juce::Typeface::createSystemTypefaceFor (d, (size_t) size);
            }
        return nullptr;
    }
    inline juce::Font make (juce::Typeface::Ptr tf, float h, float tracking)
    {
        juce::Font f = tf != nullptr ? juce::Font (juce::FontOptions (tf).withHeight (h))
                                     : juce::Font (juce::FontOptions (h));
        return f.withExtraKerningFactor (tracking);
    }
    inline juce::Font light (float h, float tracking = 0.0f)    { static auto tf = load ("Inter-Light.ttf");        return make (tf, h, tracking); }
    inline juce::Font medium (float h, float tracking = 0.0f)   { static auto tf = load ("Inter-Medium.ttf");       return make (tf, h, tracking); }
    inline juce::Font semi (float h, float tracking = 0.0f)     { static auto tf = load ("Inter-SemiBold.ttf");     return make (tf, h, tracking); }
    inline juce::Font display (float h, float tracking = 0.0f)  { static auto tf = load ("InterDisplay-Light.ttf"); return make (tf, h, tracking); }
}

// Kleine Großbuchstaben-Beschriftung mit Sperrung (wie TEMPO, PITCH)
inline void caps (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size = 9.5f,
                  juce::Colour c = col::dim, juce::Justification j = juce::Justification::centred, float tracking = 0.34f)
{
    g.setColour (c);
    g.setFont (fonts::semi (size, tracking));
    g.drawText (t, r, j, false);
}

// ---------------------------------------------------------------------------
//  Chrom-Ring (konischer Verlauf, angenähert durch Segmente)
// ---------------------------------------------------------------------------
inline juce::Colour chromeAt (float t)
{
    static const float pos[] = { 0.0f, 0.12f, 0.28f, 0.45f, 0.62f, 0.80f, 1.0f };
    static const juce::uint32 c[] = { 0xfff2f4f8, 0xff7d828d, 0xffd9dce3, 0xff4d515b, 0xffc9cdd5, 0xff5d616b, 0xfff2f4f8 };
    for (int i = 0; i < 6; ++i)
        if (t <= pos[i + 1])
            return juce::Colour (c[i]).interpolatedWith (juce::Colour (c[i + 1]), (t - pos[i]) / (pos[i + 1] - pos[i]));
    return juce::Colour (c[6]);
}

inline void chromeDisc (juce::Graphics& g, juce::Point<float> c, float r)
{
    const int N = 180;
    for (int k = 0; k < N; ++k)
    {
        const float a0 = juce::MathConstants<float>::twoPi * k / N, a1 = juce::MathConstants<float>::twoPi * (k + 1.05f) / N;
        juce::Path p;
        p.startNewSubPath (c);
        p.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, false);
        p.closeSubPath();
        g.setColour (chromeAt ((k + 0.5f) / N));
        g.fillPath (p);
    }
}

// ---------------------------------------------------------------------------
//  Look & Feel: Regler und Schalter
// ---------------------------------------------------------------------------
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::Label::textColourId, col::ink);
        setColour (juce::Label::textWhenEditingColourId, col::ink);
        setColour (juce::TextEditor::textColourId, col::ink);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::highlightColourId, col::vio.withAlpha (0.45f));
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::CaretComponent::caretColourId, col::cyan);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const float S = (float) juce::jmin (w, h);
        const juce::Point<float> c (x + w * 0.5f, y + h * 0.5f);
        const bool big = S > 100.0f, bipolar = (bool) s.getProperties()["bipolar"];
        const float alpha = s.isEnabled() ? 1.0f : 0.35f;
        const float R = S * 0.46f, tw = juce::jmax (2.0f, S * 0.026f);
        const float a = a0 + pos * (a1 - a0);

        // Spur
        juce::Path trackP; trackP.addCentredArc (c.x, c.y, R, R, 0.0f, a0, a1, true);
        g.setColour (col::track.withMultipliedAlpha (alpha));
        g.strokePath (trackP, juce::PathStrokeType (tw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Wertebogen mit sanftem Glow
        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        if (std::abs (a - from) > 0.002f)
        {
            juce::Path arc; arc.addCentredArc (c.x, c.y, R, R, 0.0f, juce::jmin (from, a), juce::jmax (from, a), true);
            for (int pass = 0; pass < 2; ++pass)
            {
                auto gl = accent (c.x - R, c.y, c.x + R, c.y);
                gl.multiplyOpacity (alpha * (pass == 0 ? 0.07f : 0.13f));
                g.setGradientFill (gl);
                g.strokePath (arc, juce::PathStrokeType (tw * (pass == 0 ? 3.4f : 2.0f), juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            }
            auto gr = accent (c.x - R, c.y, c.x + R, c.y); gr.multiplyOpacity (alpha);
            g.setGradientFill (gr);
            g.strokePath (arc, juce::PathStrokeType (tw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Skala
        if (big)
            for (int i = 0; i <= 40; ++i)
            {
                const float t = a0 + i / 40.0f * (a1 - a0);
                const float r1 = R * 0.86f, r2 = R * (i % 10 == 0 ? 0.80f : 0.83f);
                g.setColour (juce::Colours::white.withAlpha ((i % 10 == 0 ? 0.28f : 0.10f) * alpha));
                g.drawLine (c.x + std::sin (t) * r1, c.y - std::cos (t) * r1, c.x + std::sin (t) * r2, c.y - std::cos (t) * r2, 1.0f);
            }

        // Körper: Schatten, Chromrand, Gesicht
        const float br = R * (big ? 0.70f : 0.62f);
        for (int i = 6; i >= 1; --i)
        {
            const float e = br + S * 0.012f * i;
            g.setColour (juce::Colours::black.withAlpha (0.10f * alpha));
            g.fillEllipse (c.x - e, c.y - e + S * 0.035f, e * 2, e * 2);
        }
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.setOpacity (alpha);
            chromeDisc (g, c, br);
        }
        const float fr = br * 0.9f;
        juce::ColourGradient face (juce::Colour (0xff2b2d37), c.x - br * 0.3f, c.y - br * 0.4f,
                                   juce::Colour (0xff121318), c.x - br * 0.3f + br * 1.1f, c.y - br * 0.4f + br * 1.1f, true);
        g.setGradientFill (face);
        g.fillEllipse (c.x - fr, c.y - fr, fr * 2, fr * 2);
        if (big)
            for (float r = br * 0.2f; r < br * 0.88f; r += 1.4f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.012f + 0.012f * std::sin (r)));
                g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.5f);
            }

        // Zeiger mit Glow
        const float ir1 = br * 0.55f, ir2 = br * 0.80f;
        const juce::Line<float> ln (c.x + std::sin (a) * ir1, c.y - std::cos (a) * ir1, c.x + std::sin (a) * ir2, c.y - std::cos (a) * ir2);
        const float lw = juce::jmax (1.5f, S * 0.018f);
        juce::Path ptr; ptr.startNewSubPath (ln.getStart()); ptr.lineTo (ln.getEnd());
        const auto rs = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
        g.setColour (col::cyan.withAlpha (0.10f * alpha)); g.strokePath (ptr, rs (lw * 3.2f));
        g.setColour (col::cyan.withAlpha (0.16f * alpha)); g.strokePath (ptr, rs (lw * 2.0f));
        g.setColour (col::ice.withAlpha (alpha));          g.strokePath (ptr, rs (lw));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        const auto r = b.getLocalBounds().toFloat();
        const juce::Rectangle<float> pill (r.getX(), r.getCentreY() - 8.0f, 30.0f, 16.0f);
        const bool on = b.getToggleState();
        const float al = b.isEnabled() ? 1.0f : 0.35f;
        if (on)
        {
            juce::ColourGradient gr (juce::Colour (0xff5a22b8), pill.getX(), 0, juce::Colour (0xff008fb0), pill.getRight(), 0, false);
            gr.multiplyOpacity (al);
            g.setGradientFill (gr);
        }
        else g.setColour (juce::Colour (0xff0c0d11).withMultipliedAlpha (al));
        g.fillRoundedRectangle (pill, 8.0f);
        g.setColour (juce::Colours::white.withAlpha ((hover ? 0.18f : 0.10f) * al));
        g.drawRoundedRectangle (pill.reduced (0.5f), 7.5f, 1.0f);
        const float kx = on ? pill.getX() + 16.0f : pill.getX() + 2.0f;
        if (on)
        {
            g.setColour (col::cyan.withAlpha (0.35f * al));
            g.fillEllipse (kx - 2.0f, pill.getY(), 16.0f, 16.0f);
        }
        juce::ColourGradient knob (juce::Colour (0xfff4f6fa), 0, pill.getY() + 2, juce::Colour (0xff9da2ad), 0, pill.getY() + 14, false);
        knob.multiplyOpacity (al);
        g.setGradientFill (knob);
        g.fillEllipse (kx, pill.getY() + 2.0f, 12.0f, 12.0f);
        caps (g, b.getButtonText(), r.withTrimmedLeft (42.0f), 9.5f, col::dim.withMultipliedAlpha (al),
              juce::Justification::centredLeft, 0.2f);
    }
};

// ---------------------------------------------------------------------------
//  Brain-Bugz-Logo: Chrom-Front, Anthrazit-Tiefe, Lichtkante Violett -> Cyan.
//  Wird als Bild in Gerätauflösung zwischengespeichert.
// ---------------------------------------------------------------------------
class Logo : public juce::Component
{
public:
    Logo()
    {
        letters = juce::Drawable::parseSVGPath (logo::letters);
        dot = juce::Drawable::parseSVGPath (logo::iDot);
        letters.setUsingNonZeroWinding (false);
        dot.setUsingNonZeroWinding (false);
        setInterceptsMouseClicks (false, false);
    }

    static constexpr float BX = 146.0f, BY = 106.0f, BW = 1060.0f, BH = 368.0f; // Ausschnitt im viewBox

    void paint (juce::Graphics& g) override
    {
        const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int iw = juce::jmax (1, juce::roundToInt (getWidth() * scale)), ih = juce::jmax (1, juce::roundToInt (getHeight() * scale));
        if (cache.getWidth() != iw || cache.getHeight() != ih)
        {
            cache = juce::Image (juce::Image::ARGB, iw, ih, true);
            juce::Graphics ig (cache);
            draw (ig, juce::Rectangle<float> (0, 0, (float) iw, (float) ih));
        }
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (cache, getLocalBounds().toFloat());
    }

    void draw (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const float s = juce::jmin (area.getWidth() / BW, area.getHeight() / BH);
        const auto base = juce::AffineTransform::translation (-BX, -BY).scaled (s).translated (area.getX(), area.getY());
        auto both = [&] (float dx, float dy)
        {
            juce::Path p (letters); p.addPath (dot);
            p.applyTransform (juce::AffineTransform::translation (dx, dy).followedBy (base));
            return p;
        };
        const juce::Path face = both (0, 0);

        // weicher Schatten (versetzt, halbtransparent)
        g.setColour (juce::Colour (0xff08090c).withAlpha (0.35f));
        g.fillPath (both (4.0f, 10.0f));

        // Tiefe
        const int depth = 16;
        const float x0 = area.getX() + 0.0f, x1 = area.getX() + BW * s;
        for (int i = depth; i >= 1; --i)
        {
            const float t = (float) i / depth, k = 0.25f + 0.45f * (1.0f - t);
            juce::Colour cl ((juce::uint8) (22 + 40 * k), (juce::uint8) (20 + 10 * k), (juce::uint8) (34 + 70 * k));
            juce::Colour cr ((juce::uint8) (16 + 5 * k), (juce::uint8) (30 + 40 * k), (juce::uint8) (40 + 55 * k));
            g.setGradientFill (juce::ColourGradient (cl, x0, 0, cr, x1, 0, false));
            g.fillPath (both (i * 0.25f, i * 1.0f));
        }

        // Chrom-Front
        const float yTop = area.getY(), yBot = area.getY() + 350.0f * s;
        juce::ColourGradient fg (juce::Colour (0xfff4f6fa), 0, yTop, juce::Colour (0xffe6e9ef), 0, yBot, false);
        fg.addColour (0.38, juce::Colour (0xffc9cdd6));
        fg.addColour (0.52, juce::Colour (0xff7e838f));
        fg.addColour (0.62, juce::Colour (0xffb8bcc6));
        g.setGradientFill (fg);
        g.fillPath (face);

        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (face);
            // Lichtkante unten
            g.setGradientFill (accent (x0, 0, x1, 0));
            g.strokePath (both (-1.0f, -4.0f), juce::PathStrokeType (7.0f * s));
            // Glanzkante oben
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.strokePath (both (1.0f, 3.0f), juce::PathStrokeType (4.0f * s));
        }
        g.setColour (juce::Colour (0xa60a0a10));
        g.strokePath (face, juce::PathStrokeType (2.0f * s));
    }

private:
    juce::Path letters, dot;
    juce::Image cache;
};
} // namespace whl
