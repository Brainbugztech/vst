#include "PluginEditor.h"

using namespace gl;

namespace
{
    constexpr float kBaseW = 780.0f, kBaseH = 560.0f;

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    juce::String fmtDb (float v, int decimals = 1)
    {
        if (std::abs (v) < 0.05f) v = 0.0f;
        return (v > 0.0f ? "+" : (v < 0.0f ? U ("\xe2\x88\x92") : juce::String())) + juce::String (std::abs (v), decimals);
    }

    juce::String fmtLufs (float v)
    {
        if (v < -99.0f) return U ("\xe2\x80\x93\xe2\x80\x93.\xe2\x80\x93");
        return (v < 0 ? U ("\xe2\x88\x92") : juce::String()) + juce::String (std::abs (v), 1);
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float rad = 6.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (0, 2.0f), rad);
        juce::ColourGradient grad (col::panel2, r.getX(), r.getY(), col::panel, r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, rad);
        g.setColour (col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawLine (r.getX() + rad, r.getY() + 1.0f, r.getRight() - rad, r.getY() + 1.0f, 1.0f);
    }

    // Vertiefung (Messfenster, Anzeigefelder)
    void drawRecess (juce::Graphics& g, juce::Rectangle<float> r, float rad = 4.0f)
    {
        g.setColour (col::face);
        g.fillRoundedRectangle (r, rad);
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(),
                                 juce::Colours::transparentBlack, r.getX(), r.getY() + 8.0f, false);
        g.setGradientFill (sh);
        g.fillRoundedRectangle (r, rad);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, rad, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (r.getX() + rad, r.getBottom() + 0.5f, r.getRight() - rad, r.getBottom() + 0.5f, 1.0f);
    }
}

//==============================================================================
void NeedleMeter::setValues (float gainDb, float targetDb, float rangeDb, bool holding, bool active, float chainDiffDb)
{
    value = gainDb; target = targetDb; range = std::max (3.0f, rangeDb); hold = holding; live = active; chainDiff = chainDiffDb;
}

void NeedleMeter::tick (float dt)
{
    // Zeiger mit leichter Masse (gedämpfte Feder), wirkt wie ein echtes Instrument
    const float k = 90.0f, d = 15.0f;
    const float acc = k * (value - shown) - d * vel;
    vel += acc * dt;
    shown += vel * dt;
    shown = juce::jlimit (-range * 1.08f, range * 1.08f, shown);

    glintTimer -= dt;
    if (glint >= 0.0f)
    {
        glint += dt * 0.55f;
        if (glint > 1.0f) glint = -1.0f;
    }
    else if (glintTimer <= 0.0f)
    {
        glint = 0.0f;
        glintTimer = 9.0f + juce::Random::getSystemRandom().nextFloat() * 5.0f;
    }
    repaint();
}

void NeedleMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float u = r.getHeight() / 254.0f;
    drawPanel (g, r);

    auto faceR = r.reduced (10.0f * u);
    drawRecess (g, faceR, 5.0f * u);

    // Skalenblatt: warmes, mattes Schwarz
    auto sheet = faceR.reduced (3.0f * u);
    juce::ColourGradient sheetG (juce::Colour (0xff1a1a1a), sheet.getCentreX(), sheet.getY() + sheet.getHeight() * 0.35f,
                                 juce::Colour (0xff0d0e10), sheet.getCentreX(), sheet.getBottom() + sheet.getHeight() * 0.3f, true);
    g.setGradientFill (sheetG);
    g.fillRoundedRectangle (sheet, 4.0f * u);

    g.saveState();
    g.reduceClipRegion (sheet.toNearestInt());

    const juce::Point<float> pivot (sheet.getCentreX(), sheet.getBottom() - 58.0f * u);
    const float R = std::min (sheet.getWidth() * 0.36f, (pivot.y - sheet.getY()) - 40.0f * u);
    const float maxAng = juce::degreesToRadians (54.0f);
    auto angOf = [&] (float db) { return juce::jlimit (-1.08f, 1.08f, db / range) * maxAng; };
    auto pt = [&] (float ang, float rad) { return juce::Point<float> (pivot.x + rad * std::sin (ang), pivot.y - rad * std::cos (ang)); };

    // Skalenbogen
    {
        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, -maxAng, maxAng, true);
        g.setColour (col::inkDim.withAlpha (0.8f));
        g.strokePath (arc, juce::PathStrokeType (1.2f * u));
        // Bereich zwischen 0 und Ziel als feines Band
        if (live && std::abs (target) > 0.05f)
        {
            juce::Path band;
            const float a0 = angOf (0.0f), a1 = angOf (target);
            band.addCentredArc (pivot.x, pivot.y, R - 6.0f * u, R - 6.0f * u, 0.0f, std::min (a0, a1), std::max (a0, a1), true);
            g.setColour (col::amber.withAlpha (0.35f));
            g.strokePath (band, juce::PathStrokeType (5.0f * u));
        }
    }

    // Striche und Zahlen
    float step = range <= 6.0f ? 1.0f : range <= 12.0f ? 2.0f : range <= 24.0f ? 3.0f : 6.0f;
    const float labelStep = step * (range > 12.0f ? 2.0f : 2.0f);
    for (float v = -std::floor (range / step) * step; v <= range + 0.001f; v += step)
    {
        const float a = angOf (v);
        const bool major = std::fmod (std::abs (v) + 0.001f, labelStep) < 0.01f;
        const float r0 = R + (major ? 1.0f : 1.0f) * u, r1 = R + (major ? 12.0f : 7.0f) * u;
        g.setColour (std::abs (v) < 0.01f ? col::amber : (major ? col::ink.withAlpha (0.85f) : col::inkDim));
        g.drawLine ({ pt (a, r0), pt (a, r1) }, (major ? 1.6f : 1.0f) * u);
        if (major)
        {
            g.setFont (bb::fonts::mono (13.0f * u));
            auto p = pt (a, R + 24.0f * u);
            g.setColour (std::abs (v) < 0.01f ? col::amber : col::ink.withAlpha (0.8f));
            const juce::String txt = std::abs (v) < 0.01f ? juce::String ("0") : fmtDb (v, 0);
            g.drawText (txt, juce::Rectangle<float> (60.0f * u, 16.0f * u).withCentre (p), juce::Justification::centred);
        }
    }
    // Feinstriche dazwischen
    for (float v = -range; v <= range + 0.001f; v += step * 0.5f)
    {
        const float a = angOf (v);
        g.setColour (col::inkFaint);
        g.drawLine ({ pt (a, R - 3.0f * u), pt (a, R + 1.0f * u) }, 0.8f * u);
    }

    // Beschriftung im Blatt
    g.setFont (bb::fonts::display (12.0f * u, true));
    g.setColour (col::inkDim);
    g.drawText ("dB", juce::Rectangle<float> (80.0f * u, 16.0f * u).withCentre (pt (0.0f, R * 0.52f)), juce::Justification::centred);
    g.setFont (bb::fonts::mono (10.5f * u));
    g.setColour (col::inkFaint);
    g.drawText (U ("QUIETER"), juce::Rectangle<float> (90.0f * u, 14.0f * u).withCentre (pt (-maxAng * 0.62f, R * 0.70f)), juce::Justification::centred);
    g.drawText (U ("LOUDER"), juce::Rectangle<float> (90.0f * u, 14.0f * u).withCentre (pt (maxAng * 0.62f, R * 0.70f)), juce::Justification::centred);

    // Ziel-Marke (Cyan-Dreieck außen)
    if (live)
    {
        const float a = angOf (target);
        juce::Path tri;
        auto tip = pt (a, R + 1.5f * u);
        auto b1 = pt (a - 0.025f, R - 8.0f * u), b2 = pt (a + 0.025f, R - 8.0f * u);
        tri.addTriangle (tip, b1, b2);
        g.setColour (col::cyan);
        g.fillPath (tri);
    }

    // Zeiger mit Schatten
    {
        const float a = angOf (shown);
        const auto tip = pt (a, R + 6.0f * u);
        const auto tail = pt (a + juce::MathConstants<float>::pi, 16.0f * u);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine (juce::Line<float> (tail.translated (1.5f * u, 2.5f * u), tip.translated (1.5f * u, 2.5f * u)), 2.6f * u);
        juce::Path needle;
        needle.startNewSubPath (tail);
        needle.lineTo (tip);
        g.setColour (live ? col::amberHi : col::inkDim);
        g.strokePath (needle, juce::PathStrokeType (1.8f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour ((live ? col::amber : col::inkDim).withAlpha (0.25f));
        g.strokePath (needle, juce::PathStrokeType (4.5f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // Achse
    {
        const float cr = 13.0f * u;
        juce::ColourGradient capG (juce::Colour (0xff3d4147), pivot.x - cr * 0.5f, pivot.y - cr,
                                   juce::Colour (0xff111316), pivot.x + cr * 0.4f, pivot.y + cr, false);
        g.setGradientFill (capG);
        g.fillEllipse (pivot.x - cr, pivot.y - cr, cr * 2, cr * 2);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (pivot.x - cr, pivot.y - cr, cr * 2, cr * 2, 1.0f);
        g.setColour (col::amber.withAlpha (0.7f));
        g.fillEllipse (pivot.x - 2.5f * u, pivot.y - 2.5f * u, 5.0f * u, 5.0f * u);
    }

    // Digitalanzeige unter der Achse
    {
        auto lcd = juce::Rectangle<float> (150.0f * u, 34.0f * u).withCentre ({ pivot.x, pivot.y + 33.0f * u });
        g.setColour (juce::Colour (0xff08090a));
        g.fillRoundedRectangle (lcd, 3.0f * u);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (lcd, 3.0f * u, 1.0f);
        g.setFont (bb::fonts::mono (25.0f * u));
        g.setColour (col::amberDim.withAlpha (0.25f));
        g.drawText ("+88.8", lcd.withTrimmedRight (34.0f * u), juce::Justification::centredRight);
        g.setColour (live ? col::amber : col::inkDim);
        g.drawText (fmtDb (value), lcd.withTrimmedRight (34.0f * u), juce::Justification::centredRight);
        g.setFont (bb::fonts::mono (12.0f * u));
        g.drawText ("dB", lcd.withTrimmedLeft (lcd.getWidth() - 30.0f * u), juce::Justification::centredLeft);
    }

    // Lämpchen: LIVE / HOLD
    {
        auto lampRow = juce::Rectangle<float> (sheet.getX() + 14.0f * u, sheet.getBottom() - 26.0f * u, 120.0f * u, 16.0f * u);
        auto lamp = [&] (juce::Rectangle<float> rr, const juce::String& txt, bool on, juce::Colour c)
        {
            auto dot = rr.withWidth (rr.getHeight()).reduced (4.0f * u);
            if (on)
            {
                g.setColour (c.withAlpha (0.25f));
                g.fillEllipse (dot.expanded (3.0f * u));
            }
            g.setColour (on ? c : juce::Colour (0xff26282c));
            g.fillEllipse (dot);
            g.setFont (bb::fonts::mono (11.0f * u));
            g.setColour (on ? c.brighter (0.2f) : col::inkFaint);
            g.drawText (txt, rr.withTrimmedLeft (rr.getHeight() + 2.0f * u), juce::Justification::centredLeft);
        };
        lamp (lampRow.withWidth (60.0f * u), "LIVE", live && ! hold, col::cyan);
        lamp (lampRow.withX (lampRow.getX() + 62.0f * u).withWidth (60.0f * u), "HOLD", hold, col::amber);

        // Zähler: was macht die Kette mit der Lautheit?
        auto cnt = juce::Rectangle<float> (sheet.getRight() - 148.0f * u, sheet.getBottom() - 40.0f * u, 136.0f * u, 32.0f * u);
        g.setFont (bb::fonts::mono (10.5f * u));
        g.setColour (col::inkFaint);
        g.drawText (U ("THE CHAIN IS"), cnt.removeFromTop (13.0f * u), juce::Justification::centredRight);
        juce::String txt;
        if (! live)                          txt = U ("\xe2\x80\x93");
        else if (std::abs (chainDiff) < 0.1f) txt = U ("EQUALLY LOUD");
        else txt = juce::String (std::abs (chainDiff), 1) + (chainDiff > 0 ? U (" dB LOUDER") : U (" dB QUIETER"));
        g.setFont (bb::fonts::display (13.5f * u, true));
        g.setColour (live ? col::amber : col::inkFaint);
        g.drawText (txt, cnt, juce::Justification::centredRight);
    }

    // Glas: feiner Reflex oben, seltener Lichtzug quer drüber
    {
        juce::ColourGradient glass (juce::Colours::white.withAlpha (0.045f), sheet.getX(), sheet.getY(),
                                    juce::Colours::transparentWhite, sheet.getX(), sheet.getY() + sheet.getHeight() * 0.45f, false);
        g.setGradientFill (glass);
        g.fillRoundedRectangle (sheet.withHeight (sheet.getHeight() * 0.45f), 4.0f * u);

        if (glint >= 0.0f)
        {
            const float x = sheet.getX() - sheet.getWidth() * 0.4f + glint * sheet.getWidth() * 1.8f;
            const float w = sheet.getWidth() * 0.18f;
            const float env = std::sin (glint * juce::MathConstants<float>::pi);
            juce::ColourGradient sweep (juce::Colours::transparentWhite, x - w, sheet.getY(),
                                        juce::Colours::transparentWhite, x + w, sheet.getY(), false);
            sweep.addColour (0.5, juce::Colours::white.withAlpha (0.06f * env));
            g.setGradientFill (sweep);
            juce::Path band;
            band.startNewSubPath (x - w, sheet.getY());
            band.lineTo (x + w, sheet.getY());
            band.lineTo (x + w - sheet.getHeight() * 0.35f, sheet.getBottom());
            band.lineTo (x - w - sheet.getHeight() * 0.35f, sheet.getBottom());
            band.closeSubPath();
            g.fillPath (band);
        }
    }
    g.restoreState();
}

//==============================================================================
void LevelBars::setValues (float refLufs, float postLufs, float outLufs, float refPk, float outPk, bool refActive, bool lufs)
{
    ref = refLufs; post = postLufs; out = outLufs; refOn = refActive; isLufs = lufs;
    refPeakShown = std::max (refPk, refPeakShown * 0.92f);
    outPeakShown = std::max (outPk, outPeakShown * 0.92f);
    repaint();
}

void LevelBars::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float u = std::min (r.getHeight() / 254.0f, r.getWidth() / 252.0f);
    drawPanel (g, r);

    const float lo = -48.0f, hi = 0.0f;
    auto inner = r.reduced (14.0f * u, 12.0f * u);
    auto header = inner.removeFromTop (18.0f * u);
    auto footer = inner.removeFromBottom (44.0f * u);

    g.setFont (bb::fonts::display (12.0f * u, true));
    g.setColour (col::inkDim);
    g.drawText (sourceMode ? singleCaption : U ("LOUDNESS"), header, juce::Justification::centredLeft);
    g.setFont (bb::fonts::mono (11.0f * u));
    g.drawText (isLufs ? "LUFS" : "RMS", header, juce::Justification::centredRight);

    // Skala in der Mitte
    const int nBars = sourceMode ? 1 : 2;
    const float scaleW = 30.0f * u;
    const float barW = sourceMode ? 84.0f * u : (inner.getWidth() - scaleW) / (float) nBars - 10.0f * u;
    auto barsArea = inner;
    if (sourceMode) barsArea = barsArea.withSizeKeepingCentre (barW + scaleW + 10.0f * u, inner.getHeight());
    auto yOf = [&] (float v) { return juce::jmap (juce::jlimit (lo, hi, v), lo, hi, inner.getBottom(), inner.getY()); };

    struct Bar { juce::Rectangle<float> rect; float value; float peak; juce::Colour c; juce::String name; bool on; };
    std::vector<Bar> list;
    if (sourceMode)
    {
        list.push_back ({ barsArea.removeFromLeft (barW), ref, refPeakShown, col::cyan, U ("LEVEL"), true });
        barsArea.removeFromLeft (10.0f * u);
    }
    else
    {
        list.push_back ({ barsArea.removeFromLeft (barW), ref, refPeakShown, col::cyan, U ("SOURCE"), refOn });
        barsArea.removeFromLeft (10.0f * u);
    }
    auto scale = barsArea.removeFromLeft (scaleW);
    if (! sourceMode)
    {
        barsArea.removeFromLeft (10.0f * u);
        list.push_back ({ barsArea.removeFromLeft (barW), out, outPeakShown, col::amber, U ("OUTPUT"), true });
    }

    // Skalenzahlen
    g.setFont (bb::fonts::mono (10.5f * u));
    for (float v = hi; v >= lo; v -= 6.0f)
    {
        const float y = yOf (v);
        g.setColour (std::abs (v + 14.0f) < 0.1f ? col::amber.withAlpha (0.8f) : col::inkDim);
        g.drawText (juce::String ((int) v), juce::Rectangle<float> (scale.getX(), y - 7.0f * u, scale.getWidth(), 14.0f * u),
                    juce::Justification::centred);
    }

    for (auto& b : list)
    {
        drawRecess (g, b.rect, 3.0f * u);
        auto well = b.rect.reduced (3.0f * u);
        // Rasterlinien
        for (float v = hi; v >= lo; v -= 6.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) yOf (v), well.getX(), well.getRight());
        }
        if (b.on && b.value > lo)
        {
            auto fill = well.withTop (yOf (b.value));
            juce::ColourGradient grad (b.c.withMultipliedBrightness (0.55f), fill.getX(), well.getBottom(),
                                       b.c, fill.getX(), well.getY() + well.getHeight() * 0.25f, false);
            g.setGradientFill (grad);
            g.fillRect (fill);
            g.setColour (b.c.brighter (0.4f));
            g.fillRect (fill.withHeight (1.5f * u));
            // feine Lamellen wie bei einer Hardware-Anzeige
            g.setColour (juce::Colour (0xff0b0c0e).withAlpha (0.55f));
            for (float y = well.getBottom() - 4.0f * u; y > fill.getY(); y -= 4.0f * u)
                g.drawHorizontalLine ((int) y, fill.getX(), fill.getRight());
        }
        // Spitze (dBFS-Peak, gleiche Skala) als dünner Strich
        if (b.on && b.peak > 1.0e-4f)
        {
            const float pkDb = 20.0f * std::log10 (b.peak);
            const float y = yOf (pkDb);
            g.setColour ((pkDb > -0.3f ? col::red : b.c.brighter (0.5f)).withAlpha (0.9f));
            g.fillRect (well.getX(), y - 1.0f, well.getWidth(), 1.6f * u);
        }

        // Beschriftung + Wert
        auto f = footer.withX (b.rect.getX()).withWidth (b.rect.getWidth());
        g.setFont (bb::fonts::mono (10.5f * u));
        g.setColour (b.on ? b.c : col::inkFaint);
        g.drawText (b.name, f.removeFromTop (16.0f * u), juce::Justification::centred);
        g.setFont (bb::fonts::mono (19.0f * u));
        g.setColour (b.on ? col::ink : col::inkFaint);
        g.drawText (b.on ? fmtLufs (b.value) : U ("\xe2\x80\x93\xe2\x80\x93.\xe2\x80\x93"), f, juce::Justification::centred);
    }

    // Marke: Pegel der Kette vor der Korrektur (auf dem Ausgangsbalken)
    if (! sourceMode && post > lo)
    {
        auto well = list.back().rect.reduced (3.0f * u);
        const float y = yOf (post);
        juce::Path tri;
        tri.addTriangle (well.getRight() + 2.0f * u, y, well.getRight() + 8.0f * u, y - 4.0f * u, well.getRight() + 8.0f * u, y + 4.0f * u);
        g.setColour (col::inkDim);
        g.fillPath (tri);
        g.setColour (col::ink.withAlpha (0.35f));
        g.drawHorizontalLine ((int) y, well.getX(), well.getRight());
    }
}

//==============================================================================
void HistoryGraph::push (float db, float rangeDb)
{
    data[(size_t) head] = db;
    head = (head + 1) % kLen;
    count = std::min (count + 1, kLen);
    range = std::max (3.0f, rangeDb);
    repaint();
}

void HistoryGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float u = r.getHeight() / 108.0f;
    drawPanel (g, r);
    auto inner = r.reduced (10.0f * u, 8.0f * u);
    auto cap = inner.removeFromTop (14.0f * u);
    g.setFont (bb::fonts::display (11.0f * u, true));
    g.setColour (col::inkDim);
    g.drawText (U ("HISTORY"), cap, juce::Justification::centredLeft);
    g.setFont (bb::fonts::mono (10.0f * u));
    g.drawText (U ("\xc2\xb1") + juce::String (range, 0) + " dB", cap, juce::Justification::centredRight);

    inner.removeFromTop (4.0f * u);
    drawRecess (g, inner, 3.0f * u);
    auto plot = inner.reduced (4.0f * u, 4.0f * u);
    const float midY = plot.getCentreY();
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * 0.25f), plot.getX(), plot.getRight());
    g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * 0.75f), plot.getX(), plot.getRight());
    g.setColour (col::amber.withAlpha (0.25f));
    g.drawHorizontalLine ((int) midY, plot.getX(), plot.getRight());

    if (count < 2) return;
    juce::Path p, fill;
    for (int i = 0; i < count; ++i)
    {
        const int idx = (head - count + i + kLen) % kLen;
        const float x = plot.getRight() - (float) (count - 1 - i) / (float) (kLen - 1) * plot.getWidth();
        const float y = midY - juce::jlimit (-1.0f, 1.0f, data[(size_t) idx] / range) * plot.getHeight() * 0.5f;
        if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, midY); fill.lineTo (x, y); }
        else { p.lineTo (x, y); fill.lineTo (x, y); }
        if (i == count - 1) { fill.lineTo (x, midY); fill.closeSubPath(); }
    }
    g.setColour (col::amber.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (col::amber);
    g.strokePath (p, juce::PathStrokeType (1.4f * u));
}

//==============================================================================
SegmentSwitch::SegmentSwitch (juce::RangedAudioParameter& p, juce::StringArray l, juce::String c, int valueOffset)
    : labels (std::move (l)), caption (std::move (c)), offset (valueOffset),
      attachment (p, [this] (float v) { index = juce::jlimit (0, labels.size() - 1, (int) std::lround (v) - offset); repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SegmentSwitch::mouseDown (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty()) r.removeFromTop (r.getHeight() * 0.36f);
    const int i = juce::jlimit (0, labels.size() - 1, (int) ((e.position.x - r.getX()) / r.getWidth() * labels.size()));
    attachment.setValueAsCompleteGesture ((float) (i + offset));
}

void SegmentSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
    {
        auto c = r.removeFromTop (r.getHeight() * 0.36f);
        g.setFont (bb::fonts::display (std::min (12.0f, c.getHeight() * 0.8f), true));
        g.setColour (col::inkDim);
        g.drawText (caption, c, juce::Justification::centredLeft);
    }
    r = r.reduced (0.5f);
    const float rad = 4.0f;
    drawRecess (g, r, rad);
    const float w = r.getWidth() / (float) labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto s = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f);
        const bool on = i == index;
        if (on)
        {
            juce::ColourGradient fill (juce::Colour (0xff3a2a0c), s.getX(), s.getY(), juce::Colour (0xff22190a), s.getX(), s.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (s, rad - 1.0f);
            g.setColour (col::amber.withAlpha (0.7f));
            g.drawRoundedRectangle (s, rad - 1.0f, 1.0f);
        }
        else if (i > 0 && i - 1 != index)
        {
            g.setColour (col::line);
            g.drawVerticalLine ((int) s.getX() - 1, s.getY() + 5.0f, s.getBottom() - 5.0f);
        }
        g.setFont (bb::fonts::display (std::min (13.0f, s.getHeight() * 0.48f), true));
        g.setColour (on ? col::amberHi : col::inkDim);
        g.drawFittedText (labels[i], s.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.8f);
    }
}

//==============================================================================
MonitorStrip::MonitorStrip (GainLockProcessor& p)
    : proc (p),
      attachment (*p.apvts.getParameter (ParamIDs::monitor), [this] (float v)
      {
          const int s = juce::jlimit (0, GainLockProcessor::kNumMon - 1, (int) std::lround (v));
          if (s != selected) { previous = selected; selected = s; }
          repaint();
      })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

// Anzeige-Reihenfolge: ORIGINAL  P1  P2  P3  FINAL  DELTA
static constexpr int kStripOrder[GainLockProcessor::kNumMon] = {
    GainLockProcessor::monOriginal, GainLockProcessor::monP1, GainLockProcessor::monP2,
    GainLockProcessor::monP3, GainLockProcessor::monFinal, GainLockProcessor::monDelta };

juce::Rectangle<float> MonitorStrip::slotBounds (int i) const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (r.getHeight() * 0.30f);
    const float w = r.getWidth() / (float) GainLockProcessor::kNumMon;
    // kleine Lücken zwischen Quellen-, Punkt- und Ergebnisgruppe
    return juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (3.0f, 0.0f);
}

int MonitorStrip::slotAt (juce::Point<float> pos) const
{
    for (int i = 0; i < GainLockProcessor::kNumMon; ++i)
        if (slotBounds (i).contains (pos)) return i;
    return -1;
}

void MonitorStrip::refresh()
{
    bool changed = false;
    for (int m = 0; m < GainLockProcessor::kNumMon; ++m)
    {
        const bool a = proc.isMonitorAvailable (m);
        changed |= a != avail[(size_t) m];
        avail[(size_t) m] = a;
    }
    const int act = proc.status.monitorActive.load();
    const bool br = proc.getBlindState().running;
    changed |= act != active || br != blindRunning;
    active = act; blindRunning = br;
    for (int t = 1; t < GainLockProcessor::kNumTaps; ++t)
    {
        auto n = proc.getTapName (t);
        changed |= n != names[(size_t) t];
        names[(size_t) t] = n;
    }
    if (changed) repaint();
}

void MonitorStrip::mouseDown (const juce::MouseEvent& e)
{
    if (blindRunning) return;
    const int i = slotAt (e.position);
    if (i < 0) return;
    const int m = kStripOrder[i];
    if (! avail[(size_t) m]) return;
    // Nochmal auf die leuchtende Taste: zurück zur vorherigen (schnelles A/B)
    const int target = (m == selected && previous != selected && avail[(size_t) previous]) ? previous : m;
    attachment.setValueAsCompleteGesture ((float) target);
}

void MonitorStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float u = r.getHeight() / 66.0f;
    auto cap = r.removeFromTop (r.getHeight() * 0.30f);
    g.setFont (bb::fonts::display (12.0f * u, true));
    g.setColour (col::inkDim);
    g.drawText ("MONITOR", cap, juce::Justification::centredLeft);
    g.setFont (bb::fonts::mono (10.5f * u));
    g.setColour (blindRunning ? col::amber : col::inkFaint);
    g.drawText (blindRunning ? U ("LOCKED DURING BLIND TEST") : U ("LEVEL-MATCHED \xc2\xb7 LATENCY-ALIGNED"), cap, juce::Justification::centredRight);

    for (int i = 0; i < GainLockProcessor::kNumMon; ++i)
    {
        const int m = kStripOrder[i];
        auto s = slotBounds (i);
        const bool on = m == active && ! blindRunning;
        const bool chosen = m == selected;
        const bool av = avail[(size_t) m] && ! blindRunning;
        const auto accent = m == GainLockProcessor::monFinal ? col::amber : (m == GainLockProcessor::monDelta ? col::red : col::cyan);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (s.translated (0, 1.5f), 4.0f);
        juce::ColourGradient fill (on ? accent.withMultipliedBrightness (0.40f) : col::panel2,
                                   s.getX(), s.getY(),
                                   on ? accent.withMultipliedBrightness (0.20f) : col::panel, s.getX(), s.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (s, 4.0f);
        g.setColour (on ? accent.withAlpha (0.85f) : (chosen && ! av ? accent.withAlpha (0.35f) : col::lineHi));
        g.drawRoundedRectangle (s, 4.0f, 1.0f);
        if (on)
        {
            auto lamp = s.reduced (s.getWidth() * 0.25f, 0).withHeight (3.0f * u).translated (0, 4.0f * u);
            g.setColour (accent.withAlpha (0.35f));
            g.fillRoundedRectangle (lamp.expanded (3.0f, 2.0f), 3.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (lamp, 1.5f);
        }

        juce::String label, sub;
        switch (m)
        {
            case GainLockProcessor::monOriginal: label = "ORIGINAL"; sub = "before chain"; break;
            case GainLockProcessor::monFinal:    label = "FINAL";    sub = "after chain"; break;
            case GainLockProcessor::monDelta:    label = "DELTA";    sub = "difference only"; break;
            default:
            {
                const int t = m - GainLockProcessor::monP1 + 1;
                label = "P" + juce::String (t);
                sub = names[(size_t) t].isNotEmpty() ? names[(size_t) t] : (avail[(size_t) m] ? "point " + juce::String (t) : "not placed");
            }
        }
        auto tr = s.withTrimmedTop (on ? 6.0f * u : 2.0f * u);
        g.setFont (bb::fonts::display (14.0f * u, true));
        g.setColour (on ? accent.brighter (0.35f) : (av ? col::ink.withAlpha (0.9f) : col::inkFaint));
        g.drawText (label, tr.removeFromTop (tr.getHeight() * 0.58f), juce::Justification::centredBottom);
        g.setFont (bb::fonts::mono (10.0f * u));
        g.setColour (av ? col::inkDim : col::inkFaint.withAlpha (0.7f));
        g.drawFittedText (sub, tr.toNearestInt().reduced (3, 0), juce::Justification::centredTop, 1, 0.75f);
    }
}

//==============================================================================
BlindPanel::BlindPanel (GainLockProcessor& p) : proc (p)
{
    for (auto* b : { &startBtn, &closeBtn, &xBtn, &yBtn, &prefX, &prefY, &noneBtn, &stopBtn, &againBtn, &clearBtn })
        addChildComponent (b);
    for (auto* c : { &aBox, &bBox, &roundsBox })
        addChildComponent (c);

    for (int n : { 5, 10, 15, 20 }) roundsBox.addItem (juce::String (n) + " ROUNDS", n);
    roundsBox.setSelectedId (10, juce::dontSendNotification);
    roundsBox.setTooltip ("More rounds = a more trustworthy verdict. 10 is a good default; use 15-20 for subtle differences.");
    aBox.setTooltip ("First contender (called A in the log).");
    bBox.setTooltip ("Second contender (called B in the log).");

    xBtn.setClickingTogglesState (false);
    yBtn.setClickingTogglesState (false);
    xBtn.getProperties().set ("accent", (juce::int64) col::cyan.getARGB());
    yBtn.getProperties().set ("accent", (juce::int64) col::cyan.getARGB());
    xBtn.setTooltip ("Listen to X. Switch between X and Y as often as you like.");
    yBtn.setTooltip ("Listen to Y. Switch between X and Y as often as you like.");
    prefX.setTooltip ("X sounds better to you.");
    prefY.setTooltip ("Y sounds better to you.");
    noneBtn.setTooltip ("You can't hear a reliable difference. That's a valid answer: if it happens often, the change isn't worth it.");
    startBtn.getProperties().set ("accent", (juce::int64) col::amber.getARGB());

    startBtn.onClick = [this]
    {
        const int a = aBox.getSelectedId() - 1, b = bBox.getSelectedId() - 1;
        if (a < 0 || b < 0 || a == b) return;
        proc.blindStart (a, b, roundsBox.getSelectedId());
        refresh();
    };
    xBtn.onClick = [this] { proc.blindListen (true); refresh(); };
    yBtn.onClick = [this] { proc.blindListen (false); refresh(); };
    prefX.onClick = [this] { proc.blindAnswer (0); refresh(); };
    prefY.onClick = [this] { proc.blindAnswer (1); refresh(); };
    noneBtn.onClick = [this] { proc.blindAnswer (2); refresh(); };
    stopBtn.onClick = [this] { proc.blindStop(); refresh(); };
    againBtn.onClick = [this] { proc.blindDismiss(); refresh(); };
    clearBtn.onClick = [this] { proc.clearBlindHistory(); refresh(); };
    closeBtn.onClick = [this] { proc.blindDismiss(); if (onClose) onClose(); };
}

void BlindPanel::fillSourceBoxes()
{
    juce::String key;
    std::vector<int> mons;
    for (int m : { GainLockProcessor::monOriginal, GainLockProcessor::monP1, GainLockProcessor::monP2,
                   GainLockProcessor::monP3, GainLockProcessor::monFinal })
        if (proc.isMonitorAvailable (m)) { mons.push_back (m); key << m << proc.monitorName (m) << ";"; }
    if (key == availKey) return;
    availKey = key;

    const int oldA = aBox.getSelectedId(), oldB = bBox.getSelectedId();
    aBox.clear (juce::dontSendNotification);
    bBox.clear (juce::dontSendNotification);
    for (int m : mons)
    {
        aBox.addItem (proc.monitorName (m).toUpperCase(), m + 1);
        bBox.addItem (proc.monitorName (m).toUpperCase(), m + 1);
    }
    auto pick = [] (juce::ComboBox& box, int old, int fallback)
    {
        if (box.indexOfItemId (old) >= 0) box.setSelectedId (old, juce::dontSendNotification);
        else if (box.indexOfItemId (fallback) >= 0) box.setSelectedId (fallback, juce::dontSendNotification);
        else if (box.getNumItems() > 0) box.setSelectedItemIndex (0, juce::dontSendNotification);
    };
    pick (aBox, oldA, GainLockProcessor::monOriginal + 1);
    pick (bBox, oldB, GainLockProcessor::monFinal + 1);
}

void BlindPanel::refresh()
{
    state = proc.getBlindState();
    history = proc.getBlindHistory();
    fillSourceBoxes();

    const bool setup = ! state.running && ! state.finished;
    for (auto* c : std::initializer_list<juce::Component*> { &aBox, &bBox, &roundsBox, &startBtn, &clearBtn })
        c->setVisible (setup);
    for (auto* c : std::initializer_list<juce::Component*> { &xBtn, &yBtn, &prefX, &prefY, &noneBtn, &stopBtn })
        c->setVisible (state.running);
    againBtn.setVisible (state.finished);
    closeBtn.setVisible (! state.running);
    clearBtn.setEnabled (! history.empty());
    startBtn.setEnabled (aBox.getNumItems() >= 2 && aBox.getSelectedId() != bBox.getSelectedId());

    xBtn.setToggleState (state.running && state.listenX, juce::dontSendNotification);
    yBtn.setToggleState (state.running && ! state.listenX, juce::dontSendNotification);
    repaint();
}

juce::String BlindPanel::verdictHeadline (const GainLockProcessor::BlindResult& r) const
{
    const auto v = gl::judgeBlind (r.prefA, r.prefB, r.none);
    const juce::String win = v.winner == 0 ? r.nameA : r.nameB;
    switch (v.kind)
    {
        case gl::BlindVerdict::reliable:     return "Reliable preference: " + win;
        case gl::BlindVerdict::trend:        return "Leaning towards " + win + U (" \xe2\x80\x93 not proven yet");
        case gl::BlindVerdict::noPreference: return "No reliable preference";
        case gl::BlindVerdict::inaudible:    return "Hard to tell apart";
        default:                             return "Too few rounds";
    }
}

juce::String BlindPanel::verdictDetail (const GainLockProcessor::BlindResult& r) const
{
    const auto v = gl::judgeBlind (r.prefA, r.prefB, r.none);
    const int decided = r.prefA + r.prefB;
    const int chance = (int) std::lround ((1.0 - std::min (1.0, v.p)) * 100.0);
    switch (v.kind)
    {
        case gl::BlindVerdict::reliable:
            return juce::String (std::max (r.prefA, r.prefB)) + " of " + juce::String (decided)
                 + U (" decisions, ") + juce::String (chance) + U ("% unlikely to be luck. Go with it.");
        case gl::BlindVerdict::trend:
            return U ("Could still be chance. Run more rounds before you commit.");
        case gl::BlindVerdict::noPreference:
            return U ("You picked both about equally. The change doesn't make it clearly better \xe2\x80\x93 keep the simpler option.");
        case gl::BlindVerdict::inaudible:
            return U ("Most rounds: no difference. The change is too small to matter \xe2\x80\x93 skip it.");
        default:
            return {};
    }
}

void BlindPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float u = r.getHeight() / 374.0f;
    drawPanel (g, r);
    auto in = r.reduced (24.0f * u, 18.0f * u);

    g.setFont (bb::fonts::mono (11.5f * u));
    g.setColour (col::amber);
    g.drawText ("BLIND TEST", in.removeFromTop (16.0f * u), juce::Justification::centredLeft);

    if (state.running)
    {
        g.setFont (bb::fonts::display (34.0f * u, true));
        g.setColour (col::ink);
        g.drawText ("ROUND " + juce::String (state.round + 1) + " OF " + juce::String (state.rounds),
                    in.removeFromTop (44.0f * u), juce::Justification::centredLeft);
        g.setFont (bb::fonts::display (15.0f * u));
        g.setColour (col::inkDim);
        g.drawText (U ("Switch between X and Y as long as you like, then decide. Meters are hidden so they can't give it away."),
                    in.removeFromTop (22.0f * u), juce::Justification::centredLeft, true);

        // Fortschritt
        auto prog = juce::Rectangle<float> (in.getX(), r.getBottom() - 30.0f * u, in.getWidth(), 6.0f * u);
        drawRecess (g, prog, 3.0f * u);
        g.setColour (col::amber);
        g.fillRoundedRectangle (prog.withWidth (prog.getWidth() * (float) state.round / (float) std::max (1, state.rounds)), 3.0f * u);

        g.setFont (bb::fonts::mono (11.0f * u));
        g.setColour (col::inkFaint);
        g.drawText ("NOW PLAYING", xBtn.getBounds().toFloat().withHeight (14.0f * u).translated (0, -16.0f * u)
                        .withWidth ((float) (yBtn.getRight() - xBtn.getX())), juce::Justification::centred);
        g.drawText ("YOUR VERDICT", prefX.getBounds().toFloat().withHeight (14.0f * u).translated (0, -16.0f * u)
                        .withWidth ((float) (noneBtn.getRight() - prefX.getX())), juce::Justification::centred);
        return;
    }

    auto drawLog = [&] (juce::Rectangle<float> area, int maxRows, int skip)
    {
        g.setFont (bb::fonts::mono (11.0f * u));
        g.setColour (col::inkDim);
        g.drawText ("LOG", area.removeFromTop (16.0f * u), juce::Justification::centredLeft);
        g.setColour (col::line);
        g.drawHorizontalLine ((int) area.getY(), area.getX(), area.getRight());
        area.removeFromTop (4.0f * u);
        if ((int) history.size() <= skip)
        {
            g.setColour (col::inkFaint);
            g.drawText ("No tests yet. Results are saved with the project.", area.removeFromTop (20.0f * u), juce::Justification::centredLeft);
            return;
        }
        for (int i = skip; i < std::min ((int) history.size(), skip + maxRows); ++i)
        {
            const auto& h = history[(size_t) i];
            auto row = area.removeFromTop (21.0f * u);
            const auto v = gl::judgeBlind (h.prefA, h.prefB, h.none);
            g.setFont (bb::fonts::mono (11.0f * u));
            g.setColour (col::inkFaint);
            g.drawText (h.when, row.removeFromLeft (92.0f * u), juce::Justification::centredLeft);
            g.setColour (col::inkDim);
            g.drawText (h.nameA.toUpperCase() + " vs " + h.nameB.toUpperCase(), row.removeFromLeft (190.0f * u), juce::Justification::centredLeft, true);
            g.drawText (juce::String (h.prefA) + " : " + juce::String (h.prefB) + " : " + juce::String (h.none), row.removeFromLeft (80.0f * u), juce::Justification::centredLeft);
            g.setFont (bb::fonts::display (12.5f * u, true));
            g.setColour (v.kind == gl::BlindVerdict::reliable ? col::amber : col::ink.withAlpha (0.75f));
            g.drawText (verdictHeadline (h), row, juce::Justification::centredLeft, true);
        }
    };

    if (state.finished && ! history.empty())
    {
        const auto& res = history.front();
        g.setFont (bb::fonts::display (28.0f * u, true));
        const auto v = gl::judgeBlind (res.prefA, res.prefB, res.none);
        g.setColour (v.kind == gl::BlindVerdict::reliable ? col::amber : col::ink);
        g.drawText (verdictHeadline (res), in.removeFromTop (38.0f * u), juce::Justification::centredLeft, true);
        g.setFont (bb::fonts::display (15.0f * u));
        g.setColour (col::inkDim);
        g.drawText (verdictDetail (res), in.removeFromTop (22.0f * u), juce::Justification::centredLeft, true);
        in.removeFromTop (10.0f * u);

        // Balken: A / B / kein Unterschied
        const int total = std::max (1, res.prefA + res.prefB + res.none);
        auto bar = [&] (const juce::String& name, int count, juce::Colour c)
        {
            auto row = in.removeFromTop (24.0f * u);
            g.setFont (bb::fonts::mono (11.5f * u));
            g.setColour (col::ink.withAlpha (0.85f));
            g.drawText (name.toUpperCase(), row.removeFromLeft (170.0f * u), juce::Justification::centredLeft, true);
            g.drawText (juce::String (count), row.removeFromRight (30.0f * u), juce::Justification::centredRight);
            auto well = row.reduced (6.0f * u, 6.0f * u);
            drawRecess (g, well, 3.0f * u);
            g.setColour (c);
            g.fillRoundedRectangle (well.withWidth (well.getWidth() * (float) count / (float) total), 3.0f * u);
        };
        bar (res.nameA + " (A)", res.prefA, col::cyan);
        bar (res.nameB + " (B)", res.prefB, col::amber);
        bar ("no difference", res.none, col::inkDim);
        in.removeFromTop (14.0f * u);
        drawLog (in.withTrimmedBottom (50.0f * u), 3, 1);
        return;
    }

    // Einrichtung
    g.setFont (bb::fonts::display (26.0f * u, true));
    g.setColour (col::ink);
    g.drawText ("Decide by ear, not by eye", in.removeFromTop (36.0f * u), juce::Justification::centredLeft);
    g.setFont (bb::fonts::display (14.5f * u));
    g.setColour (col::inkDim);
    juce::AttributedString as;
    as.setLineSpacing (3.0f * u);
    as.append (U ("Pick two points to compare. Each round Gain Lock hides them behind X and Y at random \xe2\x80\x93 "
                  "same loudness, same timing. You say which sounds better, or that you hear no difference. "
                  "At the end you get a verdict that tells you whether your preference is real or just chance."),
               bb::fonts::display (14.5f * u), col::inkDim);
    as.draw (g, in.removeFromTop (64.0f * u));

    g.setFont (bb::fonts::mono (11.0f * u));
    g.setColour (col::inkDim);
    g.drawText ("A", aBox.getBounds().toFloat().withHeight (14.0f * u).translated (0, -16.0f * u), juce::Justification::centredLeft);
    g.drawText ("B", bBox.getBounds().toFloat().withHeight (14.0f * u).translated (0, -16.0f * u), juce::Justification::centredLeft);
    g.drawText ("VS", juce::Rectangle<float> ((float) aBox.getRight(), (float) aBox.getY(), (float) (bBox.getX() - aBox.getRight()), (float) aBox.getHeight()),
                juce::Justification::centred);

    if (aBox.getNumItems() < 2)
    {
        g.setColour (col::amber);
        g.drawText (U ("Needs a SOURCE (or sidechain) in this group to compare against."),
                    juce::Rectangle<float> (in.getX(), (float) startBtn.getBottom() + 6.0f * u, in.getWidth(), 16.0f * u), juce::Justification::centredLeft);
    }
    auto logArea = juce::Rectangle<float> (in.getX(), (float) startBtn.getBottom() + 26.0f * u, in.getWidth(),
                                           r.getBottom() - startBtn.getBottom() - 26.0f * u - 60.0f * u);
    drawLog (logArea, 5, 0);
}

void BlindPanel::resized()
{
    const float u = getHeight() / 374.0f;
    auto R = [u] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * u, y * u, w * u, h * u).toNearestInt(); };
    const float W = getWidth() / u;

    // Einrichtung
    aBox.setBounds (R (24, 162, 196, 32));
    bBox.setBounds (R (256, 162, 196, 32));
    roundsBox.setBounds (R (468, 162, 120, 32));
    startBtn.setBounds (R (W - 24 - 120, 158, 120, 40));
    clearBtn.setBounds (R (24, 374 - 52, 110, 34));

    // Laufender Test
    xBtn.setBounds (R (24, 150, 150, 110));
    yBtn.setBounds (R (184, 150, 150, 110));
    prefX.setBounds (R (360, 150, 128, 52));
    prefY.setBounds (R (496, 150, 128, 52));
    noneBtn.setBounds (R (360, 208, 264, 52));
    stopBtn.setBounds (R (W - 24 - 90, 374 - 52 - 26, 90, 34));

    // Ergebnis / gemeinsam
    againBtn.setBounds (R (W - 24 - 120 - 100, 374 - 52, 110, 34));
    closeBtn.setBounds (R (W - 24 - 90, 374 - 52, 90, 34));
}

//==============================================================================
GainLockEditor::GainLockEditor (GainLockProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      monitor (p), blindPanel (p),
      roleSwitch (*p.apvts.getParameter (ParamIDs::role), { "SOURCE", "POINT", "LOCK" }),
      speedSwitch (*p.apvts.getParameter (ParamIDs::speed), { "FAST", "MEDIUM", "SLOW" }, "SPEED"),
      metricSwitch (*p.apvts.getParameter (ParamIDs::metric), { "LUFS", "RMS" }, "METERING"),
      pointSwitch (*p.apvts.getParameter (ParamIDs::point), { "P1", "P2", "P3" }, "POINT NUMBER", 1)
{
    setLookAndFeel (&look);
    if (auto xml = juce::parseXML (juce::String::fromUTF8 (GainLockAssets::bb_logo_svg, GainLockAssets::bb_logo_svgSize)))
        for (auto* e : xml->getChildIterator())
            if (e->hasTagName ("path"))
            {
                auto path = juce::Drawable::parseSVGPath (e->getStringAttribute ("d"));
                path.setUsingNonZeroWinding (false);
                (e->getStringAttribute ("id") == "outline" ? logoOutline : logoLetters) = path;
            }

    for (auto* c : std::initializer_list<juce::Component*> { &needle, &bars, &history, &monitor, &roleSwitch, &speedSwitch,
                                                             &metricSwitch, &pointSwitch, &groupBox, &trimKnob, &rangeKnob,
                                                             &holdButton, &blindButton, &helpButton, &pointName })
        addAndMakeVisible (c);
    addChildComponent (blindPanel);
    blindPanel.onClose = [this] { blindOpen = false; updateVisibility(); };

    for (int i = 1; i <= gl::LinkHub::kNumGroups; ++i)
        groupBox.addItem ("GROUP " + juce::String (i), i);
    groupAtt = std::make_unique<CA> (proc.apvts, ParamIDs::group, groupBox);

    for (auto* k : { &trimKnob, &rangeKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        k->setRotaryParameters (gl::GainLockLook::startA, gl::GainLockLook::endA, true);
    }
    trimKnob.getProperties().set ("bipolar", true);
    trimAtt = std::make_unique<SA> (proc.apvts, ParamIDs::trim, trimKnob);
    rangeAtt = std::make_unique<SA> (proc.apvts, ParamIDs::range, rangeKnob);
    trimKnob.setDoubleClickReturnValue (true, 0.0);
    rangeKnob.setDoubleClickReturnValue (true, 18.0);

    holdButton.setClickingTogglesState (true);
    holdButton.getProperties().set ("accent", (juce::int64) col::amber.getARGB());
    holdAtt = std::make_unique<BA> (proc.apvts, ParamIDs::hold, holdButton);

    blindButton.getProperties().set ("accent", (juce::int64) col::cyan.getARGB());
    blindButton.onClick = [this] { blindOpen = ! blindOpen || proc.getBlindState().running; if (! blindOpen) proc.blindDismiss(); updateVisibility(); };

    helpButton.setClickingTogglesState (true);
    helpButton.getProperties().set ("accent", (juce::int64) col::cyan.getARGB());
    helpButton.setToggleState (proc.tooltipsOn.load(), juce::dontSendNotification);
    helpButton.onClick = [this] { proc.tooltipsOn.store (helpButton.getToggleState()); applyTooltips(); };

    pointName.setFont (bb::fonts::display (16.0f));
    pointName.setTextToShowWhenEmpty ("e.g. after EQ", col::inkFaint);
    pointName.setText (proc.getPointName(), juce::dontSendNotification);
    pointName.setColour (juce::TextEditor::backgroundColourId, col::face);
    pointName.setColour (juce::TextEditor::outlineColourId, col::lineHi);
    pointName.setColour (juce::TextEditor::focusedOutlineColourId, col::amberDim);
    pointName.setColour (juce::TextEditor::textColourId, col::ink);
    pointName.setColour (juce::TextEditor::highlightColourId, col::amberDim);
    pointName.setInputRestrictions (24);
    pointName.onTextChange = [this] { proc.setPointName (pointName.getText()); };

    // Erklärtexte (erscheinen nach kurzem Hovern, mit "?" abschaltbar)
    roleSwitch.setTooltip ("Where this instance sits in the chain: SOURCE at the very start, POINT somewhere in between (optional, up to 3), LOCK at the end.");
    groupBox.setTooltip ("Instances with the same group belong to one chain. Use a different group for every chain you want to match.");
    needle.setTooltip ("Gain correction applied right now, so the end of the chain is exactly as loud as the source. The cyan mark is where it's heading.");
    bars.setTooltip ("Loudness of the source and of what you hear at the output. The small marker on the output shows the chain before correction.");
    history.setTooltip ("Correction over the last ~24 seconds. A wobbly line means the chain changes loudness with the music (e.g. compression).");
    speedSwitch.setTooltip ("How fast the correction follows. FAST: 0.4 s window, MEDIUM: 3 s, SLOW: 10 s. SLOW is steadiest for mastering decisions.");
    metricSwitch.setTooltip ("LUFS matches perceived loudness (ITU BS.1770, recommended). RMS matches plain signal energy.");
    trimKnob.setTooltip ("Extra offset on top of the automatic match, e.g. to hear the chain 1 dB louder on purpose. Double-click to reset.");
    rangeKnob.setTooltip ("Maximum correction in either direction. Double-click to reset to 18 dB.");
    holdButton.setTooltip ("Freezes the current correction instead of following the music.");
    monitor.setTooltip ("What you hear. ORIGINAL: before the chain. P1-P3: points in between. FINAL: after the chain. DELTA: only what the chain changes. "
                        "All equally loud and in time. Click the lit button again to jump back to the previous one.");
    blindButton.setTooltip ("Blind preference test: compare two points without knowing which is which, and get a verdict on whether your preference is real.");
    helpButton.setTooltip ("Show or hide these explanations.");
    pointSwitch.setTooltip ("Which comparison point this is. Each number can only be used once per group.");
    pointName.setTooltip ("Short name shown on the LOCK's monitor buttons, e.g. 'after EQ'.");

    setResizable (true, true);
    setResizeLimits ((int) (kBaseW * 0.75f), (int) (kBaseH * 0.75f), (int) (kBaseW * 2.0f), (int) (kBaseH * 2.0f));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (kBaseW / kBaseH);
    setSize ((int) kBaseW, (int) kBaseH);

    blindOpen = proc.getBlindState().running || proc.getBlindState().finished;
    applyTooltips();
    updateVisibility();
    startTimerHz (30);
}

GainLockEditor::~GainLockEditor()
{
    stopTimer();
    tips.reset();
    setLookAndFeel (nullptr);
}

void GainLockEditor::applyTooltips()
{
    if (proc.tooltipsOn.load())
    {
        if (tips == nullptr) tips = std::make_unique<juce::TooltipWindow> (this, 650);
        tips->setLookAndFeel (&look);
    }
    else
    {
        tips.reset();
    }
}

void GainLockEditor::updateVisibility()
{
    const int role = proc.status.role.load();
    const bool lock = role == GainLockProcessor::roleLock;
    const bool blindShown = lock && blindOpen;

    for (auto* c : std::initializer_list<juce::Component*> { &needle, &bars, &history, &speedSwitch, &metricSwitch,
                                                             &trimKnob, &rangeKnob, &holdButton })
        c->setVisible (lock && ! blindShown);
    bars.setVisible (! blindShown);
    for (auto* c : std::initializer_list<juce::Component*> { &monitor, &blindButton })
        c->setVisible (lock);
    blindPanel.setVisible (blindShown);
    pointSwitch.setVisible (role == GainLockProcessor::rolePoint);
    pointName.setVisible (role == GainLockProcessor::rolePoint);
    bars.setSourceMode (! lock, role == GainLockProcessor::rolePoint ? "AT THIS POINT" : "INPUT");
    blindButton.setToggleState (blindShown, juce::dontSendNotification);

    shownRole = role;
    shownBlind = blindShown;
    if (blindShown) blindPanel.refresh();
    resized();
    repaint();
}

void GainLockEditor::timerCallback()
{
    auto& s = proc.status;
    const int role = s.role.load();
    const auto bs = proc.getBlindState();
    if (bs.running && ! blindOpen) blindOpen = true;
    const bool wantBlind = role == GainLockProcessor::roleLock && blindOpen;
    if (role != shownRole || wantBlind != shownBlind) updateVisibility();

    const bool lufs = proc.apvts.getRawParameterValue (ParamIDs::metric)->load() < 0.5f;
    const float range = proc.apvts.getRawParameterValue (ParamIDs::range)->load();
    const bool hold = proc.apvts.getRawParameterValue (ParamIDs::hold)->load() > 0.5f;
    const bool refOn = s.refKind.load() != GainLockProcessor::refNone;

    const float refPk = s.refPeak.exchange (0.0f);
    const float outPk = s.outPeak.exchange (0.0f);
    ++frame;

    if (role == GainLockProcessor::roleLock)
    {
        // Zähler: Kette gegenüber Source (geglättet, damit er ruhig lesbar bleibt)
        const float diff = refOn ? s.postLufs.load() - s.tapLufs[0].load() : 0.0f;
        if (std::isfinite (diff) && std::abs (diff) < 60.0f) chainDiffShown += 0.08f * (diff - chainDiffShown);

        needle.setValues (s.gainDb.load(), s.targetDb.load(), range, hold, refOn, chainDiffShown);
        needle.tick (1.0f / 30.0f);
        bars.setValues (s.tapLufs[0].load(), s.postLufs.load(), s.outLufs.load(), refPk, outPk, refOn, lufs);
        if (frame % 2 == 0) history.push (s.gainDb.load(), range);
        monitor.refresh();
        if (shownBlind && frame % 3 == 0) blindPanel.refresh();
    }
    else
    {
        bars.setValues (s.ownLufs.load(), -180, -180, refPk, 0, true, true);
    }

    repaint (0, 0, getWidth(), (int) (64.0f * uiScale));
    repaint (0, getHeight() - (int) (30.0f * uiScale), getWidth(), (int) (30.0f * uiScale));
    if (role != GainLockProcessor::roleLock && frame % 10 == 0) repaint();
}

juce::String GainLockEditor::statusText() const
{
    auto& s = proc.status;
    const juce::String grp = "group " + juce::String (proc.getGroupIndex() + 1);
    const int role = s.role.load();

    if (role != GainLockProcessor::roleLock)
    {
        const juce::String what = role == GainLockProcessor::roleSource ? "a SOURCE" : "this point number";
        if (s.slotTaken.load()) return "Another instance is already " + what + " in " + grp + U (" \xe2\x80\x93 pick another one");
        if (s.lockSeen.load())   return U ("\xe2\x97\x8f LOCK connected \xc2\xb7 ") + grp;
        return "Waiting for a LOCK in " + grp + U (" \xe2\x80\x93 put another Gain Lock at the end of the chain");
    }

    juce::String txt;
    switch (s.refKind.load())
    {
        case GainLockProcessor::refSource:
        {
            auto n = const_cast<GainLockProcessor&> (proc).getTapName (0);
            txt = U ("\xe2\x97\x8f Linked to SOURCE \xc2\xb7 ") + grp + (n.isNotEmpty() ? U (" \xc2\xb7 ") + n : juce::String());
            int pts = 0;
            for (int t = 1; t < GainLockProcessor::kNumTaps; ++t) pts += s.tapLinked[(size_t) t].load() ? 1 : 0;
            if (pts > 0) txt << U (" \xc2\xb7 ") << pts << (pts == 1 ? " point" : " points");
            return txt;
        }
        case GainLockProcessor::refSidechain: return U ("\xe2\x97\x8f Reference: sidechain input");
        default: break;
    }
    if (s.rateMismatch.load()) return "SOURCE in " + grp + " runs at a different sample rate";
    if (s.sourceStale.load())  return "SOURCE in " + grp + U (" isn't playing \xe2\x80\x93 correction is held");
    return "No SOURCE in " + grp + U (" \xe2\x80\x93 put a Gain Lock set to SOURCE at the start of the chain");
}

// "GAIN L[Schloss]CK": das Schloss ist zu, wenn Gain Lock gekoppelt ist und regelt
void GainLockEditor::drawWordmark (juce::Graphics& g, juce::Rectangle<float> area, bool locked)
{
    const auto ink = juce::Colour (0xffd9d3c7);
    const float h = area.getHeight();
    auto font = bb::fonts::display (h * 1.12f, true);
    auto width = [&font] (const juce::String& t)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    };

    const float capH = h * 0.80f;
    const float base = area.getBottom() - (h - capH) * 0.5f;
    float x = area.getX();
    g.setFont (font);
    g.setColour (ink);
    const juce::String a = "GAIN L", c = "CK";
    g.drawText (a, juce::Rectangle<float> (x, area.getY() - h * 0.1f, width (a) + 4.0f, h * 1.2f), juce::Justification::centredLeft, false);
    x += width (a) + capH * 0.10f;

    // Schloss in O-Größe
    const float bw = capH * 0.80f, bh = capH * 0.60f;
    const float stroke = capH * 0.14f;
    auto body = juce::Rectangle<float> (x, base - bh, bw, bh);
    const float lift = locked ? 0.0f : capH * 0.20f;
    juce::Path shackle;
    const float sw = bw * 0.62f, sx = body.getCentreX() - sw * 0.5f;
    const float top = base - capH - capH * 0.02f - lift;
    shackle.startNewSubPath (sx, body.getY() + stroke * 0.3f - lift);
    shackle.lineTo (sx, top + sw * 0.5f);
    shackle.addCentredArc (body.getCentreX(), top + sw * 0.5f, sw * 0.5f, sw * 0.5f, 0.0f,
                           -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
    shackle.lineTo (sx + sw, locked ? body.getY() + stroke * 0.3f : top + sw * 0.9f);
    g.setColour (locked ? col::amber : ink.withAlpha (0.6f));
    g.strokePath (shackle, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    g.setColour (ink);
    g.fillRoundedRectangle (body, capH * 0.10f);
    // Schlüsselloch
    g.setColour (col::bg);
    const float kr = bh * 0.15f;
    g.fillEllipse (body.getCentreX() - kr, body.getY() + bh * 0.28f, kr * 2.0f, kr * 2.0f);
    g.fillRect (body.getCentreX() - kr * 0.45f, body.getY() + bh * 0.40f, kr * 0.9f, bh * 0.34f);
    x += bw + capH * 0.10f;

    g.setColour (ink);
    g.drawText (c, juce::Rectangle<float> (x, area.getY() - h * 0.1f, width (c) + 4.0f, h * 1.2f), juce::Justification::centredLeft, false);
}

void GainLockEditor::paint (juce::Graphics& g)
{
    const float u = uiScale;
    auto r = getLocalBounds().toFloat();

    // Grund: Graphit mit feiner Bürstung
    g.fillAll (col::bg);
    {
        juce::ColourGradient vg (juce::Colour (0xff17191c), r.getCentreX(), r.getY(), col::bg, r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (vg);
        g.fillRect (r);
        juce::Random rnd (7);
        for (int i = 0; i < 200; ++i)
        {
            const float y = rnd.nextFloat() * r.getHeight();
            g.setColour (juce::Colours::white.withAlpha (0.008f + rnd.nextFloat() * 0.012f));
            g.drawHorizontalLine ((int) y, 0.0f, r.getWidth());
        }
    }

    // Kopf: Siebdruck auf der Metallplatte
    if (! logoLetters.isEmpty())
    {
        auto all = logoLetters.getBounds().getUnion (logoOutline.getBounds());
        const float lh = 46.0f * u;
        const float lw = lh * all.getWidth() / all.getHeight();
        auto lr = juce::Rectangle<float> (14.0f * u, (64.0f * u - lh) * 0.5f, lw, lh);
        if (logoPrinted.getWidth() != (int) std::ceil (lw) || logoPrinted.getHeight() != (int) std::ceil (lh))
        {
            // Siebdruck einmal pro Größe vorbereiten: zwei Druckfarben + feine Körnung, wie Farbe auf Metall
            logoPrinted = juce::Image (juce::Image::ARGB, std::max (1, (int) std::ceil (lw)), std::max (1, (int) std::ceil (lh)), true);
            juce::Graphics lg (logoPrinted);
            const auto t = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                               .getTransformToFit (all, logoPrinted.getBounds().toFloat());
            lg.setColour (col::amber.withMultipliedBrightness (0.92f));
            lg.fillPath (logoOutline, t);
            lg.setColour (juce::Colour (0xffd9d3c7));
            lg.fillPath (logoLetters, t);
            juce::Random grain (11);
            juce::Image::BitmapData bd (logoPrinted, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < bd.height; ++y)
                for (int x = 0; x < bd.width; ++x)
                {
                    auto c = bd.getPixelColour (x, y);
                    if (c.getAlpha() == 0) continue;
                    bd.setPixelColour (x, y, c.withMultipliedAlpha (0.86f + 0.14f * grain.nextFloat()));
                }
        }
        g.setOpacity (1.0f);
        g.drawImageAt (logoPrinted, (int) lr.getX(), (int) lr.getY());

        const float tx = lr.getRight() + 16.0f * u;
        g.setColour (col::line);
        g.drawVerticalLine ((int) (tx - 8.0f * u), 16.0f * u, 48.0f * u);
        const bool locked = proc.status.role.load() == GainLockProcessor::roleLock
                          && proc.status.refKind.load() != GainLockProcessor::refNone;
        drawWordmark (g, juce::Rectangle<float> (tx, 15.0f * u, 230.0f * u, 21.0f * u), locked);
        g.setFont (bb::fonts::mono (10.5f * u));
        g.setColour (col::amber.withAlpha (0.85f));
        g.drawText (U ("AUTO GAIN MATCH \xc2\xb7 BLIND TEST"), juce::Rectangle<float> (tx + 1.0f * u, 39.0f * u, 230.0f * u, 14.0f * u),
                    juce::Justification::centredLeft);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawHorizontalLine ((int) (64.0f * u), 0.0f, (float) getWidth());
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) (65.0f * u), 0.0f, (float) getWidth());

    const int role = shownRole;
    if (role == GainLockProcessor::roleLock)
    {
        if (! shownBlind)
        {
            g.setFont (bb::fonts::display (12.0f * u, true));
            g.setColour (col::inkDim);
            g.drawText ("TRIM", trimKnob.getBounds().toFloat().withHeight (14.0f * u).translated (0, -14.0f * u), juce::Justification::centred);
            g.drawText ("RANGE", rangeKnob.getBounds().toFloat().withHeight (14.0f * u).translated (0, -14.0f * u), juce::Justification::centred);
        }
    }
    else
    {
        // Source / Point: Infofeld
        const bool isPoint = role == GainLockProcessor::rolePoint;
        auto info = juce::Rectangle<float> (16.0f * u, 76.0f * u, 484.0f * u, 452.0f * u);
        drawPanel (g, info);
        auto in = info.reduced (28.0f * u, 24.0f * u);
        g.setFont (bb::fonts::mono (12.0f * u));
        g.setColour (col::amber);
        g.drawText ("ROLE", in.removeFromTop (16.0f * u), juce::Justification::centredLeft);
        g.setFont (bb::fonts::display (44.0f * u, true));
        g.setColour (col::ink);
        g.drawText (isPoint ? "POINT " + juce::String ((int) std::lround (proc.apvts.getRawParameterValue (ParamIDs::point)->load())) : juce::String ("SOURCE"),
                    in.removeFromTop (50.0f * u), juce::Justification::centredLeft);
        in.removeFromTop (8.0f * u);
        g.setColour (col::line);
        g.drawHorizontalLine ((int) in.getY(), in.getX(), in.getRight());
        in.removeFromTop (16.0f * u);

        juce::AttributedString as;
        as.setLineSpacing (5.0f * u);
        if (isPoint)
        {
            as.append (U ("A comparison point in the middle of the chain. It passes audio through untouched.\n\n"),
                       bb::fonts::display (15.5f * u), col::ink.withAlpha (0.85f));
            as.append (U ("On the LOCK you can then listen to this point level-matched and in time, or put it into a blind test "
                          "against the original or the final result."),
                       bb::fonts::display (15.5f * u), col::inkDim);
            as.draw (g, in.removeFromTop (130.0f * u));
            g.setFont (bb::fonts::display (12.0f * u, true));
            g.setColour (col::inkDim);
            g.drawText ("NAME", pointName.getBounds().toFloat().withHeight (14.0f * u).translated (0, -16.0f * u), juce::Justification::centredLeft);
        }
        else
        {
            as.append (U ("This instance sits at the very start of the chain and passes audio through untouched.\n\n"),
                       bb::fonts::display (15.5f * u), col::ink.withAlpha (0.85f));
            as.append (U ("Put a second Gain Lock set to LOCK at the end of the chain, same group. It matches the loudness automatically "
                          "and lets you hear the original, any point in between and the difference \xe2\x80\x93 all latency-aligned."),
                       bb::fonts::display (15.5f * u), col::inkDim);
            as.draw (g, in.removeFromTop (180.0f * u));
        }

        // Verbindungsanzeige
        auto st = in.removeFromBottom (40.0f * u);
        drawRecess (g, st, 4.0f * u);
        const bool ok = proc.status.lockSeen.load();
        const bool taken = proc.status.slotTaken.load();
        auto dot = st.withWidth (st.getHeight()).reduced (14.0f * u);
        const auto c = taken ? col::red : (ok ? col::cyan : col::amberDim);
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (dot.expanded (4.0f * u));
        g.setColour (c);
        g.fillEllipse (dot);
        g.setFont (bb::fonts::display (15.0f * u, true));
        g.setColour (taken ? col::red : (ok ? col::ink : col::inkDim));
        g.drawText (taken ? "ALREADY IN USE" : (ok ? "LOCK CONNECTED" : "WAITING FOR LOCK"),
                    st.withTrimmedLeft (st.getHeight() + 4.0f * u), juce::Justification::centredLeft);
    }

    // Statuszeile
    auto statusR = juce::Rectangle<float> (18.0f * u, getHeight() - 26.0f * u, getWidth() - 36.0f * u, 20.0f * u);
    g.setFont (bb::fonts::mono (12.0f * u));
    auto& s = proc.status;
    const bool lock = role == GainLockProcessor::roleLock;
    juce::String right;
    bool rightOk = false;
    if (lock && s.refKind.load() == GainLockProcessor::refSource && ! shownBlind)
    {
        const int mon = s.monitorActive.load();
        int t = mon == GainLockProcessor::monFinal || mon == GainLockProcessor::monDelta ? 0
              : (mon == GainLockProcessor::monOriginal ? 0 : mon - GainLockProcessor::monP1 + 1);
        const juce::String who = t == 0 ? "SOURCE" : "P" + juce::String (t);
        if (s.tapLatencyKnown[(size_t) t].load())
        {
            right = who + " LATENCY " + juce::String (s.tapLatencyMs[(size_t) t].load(), 1) + " ms";
            if (s.tapPolarity[(size_t) t].load()) right = U ("POLARITY FLIPPED \xc2\xb7 ") + right;
            rightOk = true;
        }
        else right = U ("FINDING LATENCY\xe2\x80\xa6");
    }
    const bool warn = (s.refKind.load() == GainLockProcessor::refNone && lock) || s.slotTaken.load();
    g.setColour (warn ? col::amber.withAlpha (0.85f) : col::inkDim);
    g.drawText (statusText(), statusR.withTrimmedRight (right.isNotEmpty() ? 250.0f * u : 0.0f), juce::Justification::centredLeft, true);
    g.setColour (rightOk ? col::cyan.withAlpha (0.85f) : col::inkDim);
    g.drawText (right, statusR, juce::Justification::centredRight);
}

void GainLockEditor::resized()
{
    const float u = uiScale = getWidth() / kBaseW;
    auto R = [u] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * u, y * u, w * u, h * u).toNearestInt(); };

    helpButton.setBounds (R (414, 18, 30, 28));
    roleSwitch.setBounds (R (452, 16, 196, 32));
    groupBox.setBounds (R (656, 16, 108, 32));

    const bool lock = shownRole == GainLockProcessor::roleLock;
    needle.setBounds (R (16, 76, 484, 254));
    bars.setBounds (lock ? R (512, 76, 252, 254) : R (512, 76, 252, 452));
    blindPanel.setBounds (R (16, 76, 748, 374));
    history.setBounds (R (16, 342, 220, 108));
    speedSwitch.setBounds (R (248, 342, 200, 50));
    metricSwitch.setBounds (R (248, 400, 200, 50));
    trimKnob.setBounds (R (456, 358, 92, 92));
    rangeKnob.setBounds (R (552, 358, 92, 92));
    holdButton.setBounds (R (656, 346, 108, 46));
    blindButton.setBounds (R (656, 400, 108, 50));
    monitor.setBounds (R (16, 462, 748, 66));

    pointSwitch.setBounds (R (44, 330, 220, 54));
    pointName.setBounds (R (284, 350, 188, 34));
    pointName.setFont (bb::fonts::display (16.0f * u));

    for (auto* k : { &trimKnob, &rangeKnob })
        k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, (int) (80 * u), (int) (18 * u));
}
