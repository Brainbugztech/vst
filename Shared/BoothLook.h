#pragma once

// Booth-Reihe: gemeinsame GUI-Vorlage.
//
// Jedes Booth-Plugin ist wie eine Spalte der Novation Launch Control XL aufgebaut, in Hardware-Reihenfolge:
//   Kopf (Logo, "BOOTH", Plugin-Name)  ->  Anzeige (grid::screen)  ->  Knopf 1/2/3  ->  Fader-Platz  ->  Taster 1/2  ->  Fußleiste
// Kleine Markierungen K1 K2 K3 / F / B1 B2 zeigen, wohin der Regler auf der XL gehört.
// Anthrazit-Grundton, je Plugin eine eigene Akzentfarbe aus dem Bereich Cyan bis Lila (ähnlich, nicht gleich).
// Hilfe: englische Hinweise nur beim Drüberfahren, mit "?" ein-/ausschaltbar, im Projekt gespeichert.
// CPU: Hintergrund einmal vorberechnet (HiDPI-scharf), Neuzeichnen nur bei Änderungen, keine Timer-Animation.

#include <juce_audio_processors/juce_audio_processors.h>
#include "BrainBugzLook.h"

namespace booth
{
namespace col
{
    inline const juce::Colour bgTop    { 0xff25272c };
    inline const juce::Colour bgBottom { 0xff18191d };
    inline const juce::Colour well     { 0xff121316 };
    inline const juce::Colour line     { 0xff34373e };
    inline const juce::Colour lineSoft { 0xff2a2c32 };
    inline const juce::Colour ink      { 0xffeae8f0 };
    inline const juce::Colour inkDim   { 0xff8f8c99 };
    inline const juce::Colour inkFaint { 0xff5b5964 };
}

// Akzentfarbe je Plugin
struct Accent
{
    juce::Colour main, hi, lo;
    static Accent violet() { return { juce::Colour (0xffa46bff), juce::Colour (0xffd2b2ff), juce::Colour (0xff563892) }; }   // EQ
    static Accent cyan()   { return { juce::Colour (0xff2fd6e8), juce::Colour (0xffa6f4fb), juce::Colour (0xff17707a) }; }   // Filter
    static Accent azure()  { return { juce::Colour (0xff4f8dff), juce::Colour (0xffb3cdff), juce::Colour (0xff27478a) }; }   // Repeat
    static Accent orchid() { return { juce::Colour (0xffd06bff), juce::Colour (0xffeab8ff), juce::Colour (0xff6c3290) }; }   // Motion
    static Accent teal()   { return { juce::Colour (0xff35e0b4), juce::Colour (0xffa8f6e2), juce::Colour (0xff177a60) }; }   // Master
};

// Raster (Pixel bei 100 %)
namespace grid
{
    inline constexpr int width = 300, height = 640;
    inline constexpr int gutter = 16;
    inline const juce::Rectangle<int> header  { 0, 0, width, 64 };
    inline const juce::Rectangle<int> screen { 12, 70, width - 24, 104 };
    inline constexpr int knobTop = 184, knobRowH = 92, knobSize = 80;
    inline const juce::Rectangle<int> faderRow { 16, knobTop + 3 * knobRowH + 6, width - 32, 26 };
    inline const juce::Rectangle<int> pad1 { 16, knobTop + 3 * knobRowH + 42, (width - 40) / 2, 64 };
    inline const juce::Rectangle<int> pad2 { width / 2 + 4, knobTop + 3 * knobRowH + 42, (width - 40) / 2, 64 };
    inline constexpr int footerTop = knobTop + 3 * knobRowH + 118;

    inline juce::Rectangle<int> knobRow (int i) { return { 0, knobTop + i * knobRowH, width, knobRowH }; }
    inline juce::Rectangle<int> knob (int i) { return juce::Rectangle<int> (knobSize, knobSize).withCentre (knobRow (i).getCentre().translated (0, 0)); }
    inline juce::Rectangle<int> knobValue (int i) { return { width - 104, knobRow (i).getCentreY() + 4, 88, 20 }; }
}

inline juce::Font display (float h, bool bold = false) { return bb::fonts::display (h, bold); }
inline juce::Font mono (float h) { return bb::fonts::mono (h); }

//==============================================================================
class Look : public juce::LookAndFeel_V4
{
public:
    explicit Look (Accent a) : accent (a)
    {
        setColour (juce::Label::textColourId, col::ink);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0141518));
        setColour (juce::TooltipWindow::textColourId, col::ink);
        setColour (juce::TooltipWindow::outlineColourId, a.lo);
    }

    Accent accent;

    juce::Font getLabelFont (juce::Label&) override { return mono (15.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return display (13.0f, true); }

    // Drehregler. Property "bipolar" = Wertbogen ab 12 Uhr mit Raste, "chip" = kleines Wertfeld (Expertenwerte)
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        if (s.getProperties().contains ("chip")) { drawChip (g, area, s); return; }

        const bool bipolar = s.getProperties().contains ("bipolar");
        const float size = juce::jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float rTrack = size * 0.46f, rCap = size * 0.33f;
        const float centreA = (startAngle + endAngle) * 0.5f;
        const float a = startAngle + pos * (endAngle - startAngle);
        const bool atRest = bipolar ? std::abs (pos - 0.5f) <= 0.02f : pos <= 0.0005f;

        for (int i = 0; i <= 10; ++i)
        {
            const float ta = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            const bool mark = bipolar ? i == 5 : i == 0;
            const float r0 = rTrack + size * 0.035f, r1 = r0 + size * (mark ? 0.05f : 0.03f);
            g.setColour (mark ? (atRest ? accent.hi : col::inkDim) : col::inkFaint);
            g.drawLine ({ c.getPointOnCircumference (r0, ta), c.getPointOnCircumference (r1, ta) }, mark ? 1.6f : 1.0f);
        }

        juce::Path track;
        track.addCentredArc (c.x, c.y, rTrack, rTrack, 0.0f, startAngle, endAngle, true);
        g.setColour (col::line);
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (! atRest)
        {
            const float from = bipolar ? centreA : startAngle;
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, rTrack, rTrack, 0.0f, juce::jmin (a, from), juce::jmax (a, from), true);
            g.setColour (accent.main.withAlpha (0.16f));
            g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent.main);
            g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Kappe: Schatten, Metallkörper, Lichtkante
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (juce::Rectangle<float> (rCap * 2.1f, rCap * 2.1f).withCentre (c.translated (0.0f, size * 0.03f)));
        auto cap = juce::Rectangle<float> (rCap * 2.0f, rCap * 2.0f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff46484f), c.x - rCap * 0.6f, c.y - rCap * 0.8f,
                                                 juce::Colour (0xff1b1c20), c.x + rCap * 0.5f, c.y + rCap, true));
        g.fillEllipse (cap);
        auto inner = cap.reduced (rCap * 0.16f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34363c), inner.getX(), inner.getY(),
                                                 juce::Colour (0xff222327), inner.getRight(), inner.getBottom(), false));
        g.fillEllipse (inner);
        juce::Path rim;
        rim.addCentredArc (c.x, c.y, rCap - 0.8f, rCap - 0.8f, 0.0f, -1.1f, 1.1f, true);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.strokePath (rim, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colour (0xff0d0e10));
        g.drawEllipse (cap, 1.0f);

        const auto p0 = c.getPointOnCircumference (rCap * 0.30f, a);
        const auto tip = c.getPointOnCircumference (rCap * 0.86f, a);
        g.setColour (col::ink.withAlpha (0.92f));
        g.drawLine ({ p0, tip }, 2.2f);
        g.setColour (accent.main.withAlpha (s.isMouseOverOrDragging() ? 0.40f : 0.25f));
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (tip));
        g.setColour (accent.hi);
        g.fillEllipse (juce::Rectangle<float> (3.6f, 3.6f).withCentre (tip));
    }

    // Kleine Umschalter (z. B. Kurve EQ/ISO)
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent.main.withAlpha (0.18f) : (over ? col::lineSoft.brighter (0.08f) : col::lineSoft));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? accent.main : col::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.getToggleState() ? col::ink : col::inkDim);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (240, juce::GlyphArrangement::getStringWidthInt (mono (12.0f), text) + 20);
        const int lines = juce::jmax (1, juce::GlyphArrangement::getStringWidthInt (mono (12.0f), text) / 220 + 1);
        const int h = 10 + lines * 16;
        auto r = juce::Rectangle<int> (pos.x - w / 2, pos.y + 18, w, h);
        return r.constrainedWithin (parent);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        auto r = juce::Rectangle<float> ((float) w, (float) h);
        g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (accent.lo);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (col::ink);
        g.setFont (mono (12.0f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (10, 4), juce::Justification::centredLeft, 4);
    }

private:
    void drawChip (juce::Graphics& g, juce::Rectangle<float> r, juce::Slider& s)
    {
        const bool active = s.isMouseOverOrDragging();
        g.setColour (active ? col::lineSoft.brighter (0.1f) : col::lineSoft);
        g.fillRoundedRectangle (r.reduced (0.5f), 4.0f);
        g.setColour (active ? accent.lo : col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (active ? col::ink : col::inkDim);
        g.setFont (mono (13.0f));
        g.drawFittedText (s.getTextFromValue (s.getValue()), r.toNearestInt(), juce::Justification::centred, 1);
    }
};

//==============================================================================
// Großer Taster (Hold): solange gedrückt = an, Loslassen = aus. Zeigt auch MIDI/Automation (XL-Taster).
class HoldPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    HoldPad (juce::RangedAudioParameter& p, juce::String text, juce::String xlTag, Accent a)
        : param (p), label (std::move (text)), tag (std::move (xlTag)), accent (a),
          attachment (p, [this] (float v) { setLit (v > 0.5f); })
    {
        attachment.sendInitialUpdate();
        setRepaintsOnMouseActivity (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        // Sockel
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 7.0f);
        g.setGradientFill (juce::ColourGradient (lit ? accent.main.withAlpha (0.30f) : juce::Colour (0xff2e3036), 0.0f, r.getY(),
                                                 lit ? accent.lo.withAlpha (0.45f) : juce::Colour (0xff222327), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (lit ? accent.main : (isMouseOver() ? col::inkFaint : col::line));
        g.drawRoundedRectangle (r, 7.0f, lit ? 1.6f : 1.0f);

        // LED-Streifen oben, gut sichtbar
        auto led = juce::Rectangle<float> (r.getX() + 12.0f, r.getY() + 9.0f, r.getWidth() - 24.0f, 5.0f);
        if (lit)
        {
            g.setColour (accent.main.withAlpha (0.35f));
            g.fillRoundedRectangle (led.expanded (3.0f), 4.0f);
            g.setColour (accent.hi);
        }
        else
            g.setColour (juce::Colour (0xff3a3c43));
        g.fillRoundedRectangle (led, 2.5f);

        g.setColour (lit ? col::ink : col::inkDim);
        g.setFont (display (17.0f, true));
        g.drawText (label, r.withTrimmedTop (16.0f).toNearestInt(), juce::Justification::centred);
        g.setColour (col::inkFaint);
        g.setFont (mono (10.0f));
        g.drawText (tag, r.reduced (8.0f, 5.0f).toNearestInt(), juce::Justification::bottomRight);
    }

    void mouseDown (const juce::MouseEvent&) override { attachment.beginGesture(); attachment.setValueAsPartOfGesture (1.0f); }
    void mouseUp (const juce::MouseEvent&) override   { attachment.setValueAsPartOfGesture (0.0f); attachment.endGesture(); }

private:
    void setLit (bool on) { if (on != lit) { lit = on; repaint(); } }

    juce::RangedAudioParameter& param;
    juce::String label, tag;
    Accent accent;
    juce::ParameterAttachment attachment;
    bool lit = false;
};

//==============================================================================
// Hilfe-Umschalter "?" (englische Hover-Hinweise), Zustand im Plugin-State gespeichert
class HelpToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    HelpToggle (juce::ValueTree st, juce::Component& owner, Accent a) : state (st), parent (owner), accent (a)
    {
        apply (state.getProperty ("help", true));
        setTooltip ("Hover help on/off");
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = tips != nullptr;
        g.setColour (on ? accent.main.withAlpha (0.18f) : col::lineSoft);
        g.fillEllipse (r);
        g.setColour (on ? accent.main : col::line);
        g.drawEllipse (r, 1.0f);
        g.setColour (on ? col::ink : col::inkDim);
        g.setFont (display (13.0f, true));
        g.drawText ("?", getLocalBounds(), juce::Justification::centred);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        const bool on = tips == nullptr;
        state.setProperty ("help", on, nullptr);
        apply (on);
    }

private:
    void apply (bool on)
    {
        if (on && tips == nullptr) tips = std::make_unique<juce::TooltipWindow> (&parent, 500);
        if (! on) tips.reset();
        repaint();
    }

    juce::ValueTree state;
    juce::Component& parent;
    Accent accent;
    std::unique_ptr<juce::TooltipWindow> tips;
};

//==============================================================================
// Hintergrund der Reihe (einmal gerendert). Pluginseitige Texte kommen über Strip.
struct Strip
{
    juce::String name;                   // z. B. "EQ"
    juce::String subtitle;               // z. B. "3-BAND  |  ISOLATOR"
    juce::StringArray knobNames, knobHints;   // je 3
    juce::String faderNote;              // leer = Fader belegt, sonst Hinweis, warum frei
    juce::String footerLeft, footerRight;
};

inline void loadLogo (const char* svgData, int svgSize, juce::Path& outline, juce::Path& letters)
{
    if (auto xml = juce::parseXML (juce::String::fromUTF8 (svgData, svgSize)))
        for (auto* e : xml->getChildIterator())
            if (e->hasTagName ("path"))
            {
                auto p = juce::Drawable::parseSVGPath (e->getStringAttribute ("d"));
                p.setUsingNonZeroWinding (false);
                (e->getStringAttribute ("id") == "outline" ? outline : letters) = p;
            }
}

inline juce::Image renderBackground (const Strip& s, Accent a, const juce::Path& logoOutline, const juce::Path& logoLetters, float scale)
{
    constexpr int width = grid::width, height = grid::height, footerTop = grid::footerTop;
    const auto& faderRow = grid::faderRow;
    auto knobRow = [] (int i) { return grid::knobRow (i); };
    juce::Image img (juce::Image::ARGB, juce::roundToInt (width * scale), juce::roundToInt (height * scale), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));

    g.setGradientFill (juce::ColourGradient (col::bgTop, 0.0f, 0.0f, col::bgBottom, 0.0f, (float) height, false));
    g.fillAll();
    juce::Random rnd (7);
    for (int y = 0; y < height; y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (0.006f + rnd.nextFloat() * 0.010f));
        g.drawHorizontalLine (y, 0.0f, (float) width);
    }
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (0, 0.0f, (float) width);
    g.setColour (a.main);   // Akzentkante oben links: Wiedererkennung der Reihe
    g.fillRect (0.0f, 0.0f, 3.0f, 64.0f);

    // Kopf: Logo links, BOOTH + Name rechts
    if (! logoLetters.isEmpty())
    {
        auto all = logoLetters.getBounds().getUnion (logoOutline.getBounds());
        auto t = juce::RectanglePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid)
                     .getTransformToFit (all, { 16.0f, 14.0f, 96.0f, 36.0f });
        g.setColour (a.main.withAlpha (0.85f));
        g.fillPath (logoOutline, t);
        g.setColour (col::ink.withAlpha (0.92f));
        g.fillPath (logoLetters, t);
    }
    g.setColour (a.main);
    g.setFont (mono (11.0f));
    g.drawText ("B O O T H", juce::Rectangle<int> (120, 11, width - 136, 14), juce::Justification::right);
    g.setColour (col::ink);
    g.setFont (display (26.0f, true));
    g.drawText (s.name, juce::Rectangle<int> (120, 23, width - 136, 28), juce::Justification::right);
    g.setColour (col::inkFaint);
    g.setFont (mono (10.0f));
    g.drawText (s.subtitle, juce::Rectangle<int> (120, 50, width - 136, 12), juce::Justification::right);

    // Anzeige-Mulde
    auto well = grid::screen.toFloat();
    g.setColour (col::well);
    g.fillRoundedRectangle (well, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (well.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) well.getBottom(), well.getX() + 6.0f, well.getRight() - 6.0f);

    // Knopfzeilen mit XL-Markierung
    for (int i = 0; i < 3; ++i)
    {
        auto row = knobRow (i);
        if (i > 0)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawHorizontalLine (row.getY(), 24.0f, (float) width - 24.0f);
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.drawHorizontalLine (row.getY() + 1, 24.0f, (float) width - 24.0f);
        }
        g.setColour (col::ink);
        g.setFont (display (22.0f, true));
        g.drawText (s.knobNames[i], juce::Rectangle<int> (20, row.getCentreY() - 22, 90, 26), juce::Justification::left);
        g.setColour (col::inkDim);
        g.setFont (mono (12.0f));
        g.drawText (s.knobHints[i], juce::Rectangle<int> (20, row.getCentreY() + 4, 90, 16), juce::Justification::left);
        g.setColour (col::inkFaint);
        g.setFont (mono (10.0f));
        g.drawText ("K" + juce::String (i + 1), juce::Rectangle<int> (width - 40, row.getY() + 8, 24, 12), juce::Justification::right);
    }

    // Fader-Platz
    auto fr = faderRow.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawHorizontalLine ((int) fr.getY() - 6, 16.0f, (float) width - 16.0f);
    if (s.faderNote.isNotEmpty())
    {
        g.setColour (col::lineSoft.withAlpha (0.6f));
        g.fillRoundedRectangle (fr, 4.0f);
        float dash[] = { 3.0f, 3.0f };
        juce::Path outline;
        outline.addRoundedRectangle (fr.reduced (0.5f), 4.0f);
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dash, 2);
        g.setColour (col::line);
        g.fillPath (dashed);
        g.setColour (col::inkFaint);
        g.setFont (mono (11.0f));
        g.drawText ("F", fr.reduced (10.0f, 0.0f).toNearestInt(), juce::Justification::left);
        g.drawText (s.faderNote, fr.toNearestInt(), juce::Justification::centred);
    }

    // Fußleiste
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawHorizontalLine (footerTop - 8, 16.0f, (float) width - 16.0f);
    g.setFont (mono (10.0f));
    g.setColour (col::inkFaint);
    g.drawText (s.footerLeft, juce::Rectangle<int> (16, footerTop, 130, 14), juce::Justification::left);
    g.drawText (s.footerRight, juce::Rectangle<int> (width - 186, footerTop, 170, 14), juce::Justification::right);
    return img;
}
} // namespace booth
