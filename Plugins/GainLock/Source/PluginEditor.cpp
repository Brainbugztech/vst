#include "PluginEditor.h"

using namespace gl;

namespace
{
    constexpr float kBaseW = 780.0f, kBaseH = 480.0f;

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
void NeedleMeter::setValues (float gainDb, float targetDb, float rangeDb, bool holding, bool active)
{
    value = gainDb; target = targetDb; range = std::max (3.0f, rangeDb); hold = holding; live = active;
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
    g.drawText (U ("LEISER"), juce::Rectangle<float> (90.0f * u, 14.0f * u).withCentre (pt (-maxAng * 0.62f, R * 0.70f)), juce::Justification::centred);
    g.drawText (U ("LAUTER"), juce::Rectangle<float> (90.0f * u, 14.0f * u).withCentre (pt (maxAng * 0.62f, R * 0.70f)), juce::Justification::centred);

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

        g.setFont (bb::fonts::mono (11.0f * u));
        g.setColour (col::inkFaint);
        g.drawText (U ("KORREKTUR"), juce::Rectangle<float> (sheet.getRight() - 120.0f * u, sheet.getBottom() - 26.0f * u, 106.0f * u, 16.0f * u),
                    juce::Justification::centredRight);
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
    g.drawText (sourceMode ? U ("EINGANG") : U ("LAUTHEIT"), header, juce::Justification::centredLeft);
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
        list.push_back ({ barsArea.removeFromLeft (barW), ref, refPeakShown, col::cyan, U ("QUELLE"), true });
        barsArea.removeFromLeft (10.0f * u);
    }
    else
    {
        list.push_back ({ barsArea.removeFromLeft (barW), ref, refPeakShown, col::cyan, U ("QUELLE"), refOn });
        barsArea.removeFromLeft (10.0f * u);
    }
    auto scale = barsArea.removeFromLeft (scaleW);
    if (! sourceMode)
    {
        barsArea.removeFromLeft (10.0f * u);
        list.push_back ({ barsArea.removeFromLeft (barW), out, outPeakShown, col::amber, U ("AUSGANG"), true });
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
void HistoryGraph::push (float db, float rangeDb, bool holding)
{
    data[(size_t) head] = db;
    held[(size_t) head] = holding;
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
    g.drawText (U ("VERLAUF"), cap, juce::Justification::centredLeft);
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
SegmentSwitch::SegmentSwitch (juce::RangedAudioParameter& p, juce::StringArray l, juce::String c)
    : param (p), labels (std::move (l)), caption (std::move (c)),
      attachment (p, [this] (float v) { index = juce::jlimit (0, labels.size() - 1, (int) std::lround (v)); repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SegmentSwitch::mouseDown (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty()) r.removeFromTop (r.getHeight() * 0.36f);
    const int i = juce::jlimit (0, labels.size() - 1, (int) ((e.position.x - r.getX()) / r.getWidth() * labels.size()));
    attachment.setValueAsCompleteGesture ((float) i);
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
GainLockEditor::GainLockEditor (GainLockProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      roleSwitch (*p.apvts.getParameter (ParamIDs::role), { "QUELLE", "LOCK" }),
      speedSwitch (*p.apvts.getParameter (ParamIDs::speed), { "SCHNELL", "MITTEL", "LANGSAM" }, "TEMPO"),
      metricSwitch (*p.apvts.getParameter (ParamIDs::metric), { "LUFS", "RMS" }, "MESSUNG")
{
    setLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (GainLockAssets::gainlock_logo_png, GainLockAssets::gainlock_logo_pngSize);

    for (auto* c : std::initializer_list<juce::Component*> { &needle, &bars, &history, &roleSwitch, &speedSwitch,
                                                             &metricSwitch, &groupBox, &trimKnob, &rangeKnob, &holdButton, &abButton })
        addAndMakeVisible (c);

    for (int i = 1; i <= gl::LinkHub::kNumGroups; ++i)
        groupBox.addItem ("GRUPPE " + juce::String (i), i);
    groupAtt = std::make_unique<CA> (proc.apvts, ParamIDs::group, groupBox);
    groupBox.setTooltip (U ("Quelle und Lock mit derselben Gruppe gehören zusammen"));

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
    abButton.setClickingTogglesState (true);
    holdButton.getProperties().set ("accent", (juce::int64) col::amber.getARGB());
    abButton.getProperties().set ("accent", (juce::int64) col::cyan.getARGB());
    holdAtt = std::make_unique<BA> (proc.apvts, ParamIDs::hold, holdButton);
    abAtt = std::make_unique<BA> (proc.apvts, ParamIDs::listen, abButton);
    holdButton.setTooltip (U ("Korrektur einfrieren"));
    abButton.setTooltip (U ("Vergleich: Original (vor der Kette, latenzbereinigt) gegen bearbeitet, gleich laut"));

    setResizable (true, true);
    setResizeLimits ((int) (kBaseW * 0.75f), (int) (kBaseH * 0.75f), (int) (kBaseW * 2.0f), (int) (kBaseH * 2.0f));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (kBaseW / kBaseH);
    setSize ((int) kBaseW, (int) kBaseH);

    updateVisibility();
    startTimerHz (30);
}

GainLockEditor::~GainLockEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GainLockEditor::updateVisibility()
{
    const bool lock = proc.status.role.load() == GainLockProcessor::roleLock;
    for (auto* c : std::initializer_list<juce::Component*> { &needle, &history, &speedSwitch, &metricSwitch,
                                                             &trimKnob, &rangeKnob, &holdButton, &abButton })
        c->setVisible (lock);
    bars.setSourceMode (! lock);
    shownRole = lock ? 1 : 0;
    resized();
    repaint();
}

void GainLockEditor::timerCallback()
{
    auto& s = proc.status;
    const int role = s.role.load();
    if (role != shownRole) updateVisibility();

    const bool lufs = proc.apvts.getRawParameterValue (ParamIDs::metric)->load() < 0.5f;
    const float range = proc.apvts.getRawParameterValue (ParamIDs::range)->load();
    const bool hold = proc.apvts.getRawParameterValue (ParamIDs::hold)->load() > 0.5f;
    const bool refOn = s.refKind.load() != GainLockProcessor::refNone;

    const float refPk = s.refPeak.exchange (0.0f);
    const float outPk = s.outPeak.exchange (0.0f);

    if (role == GainLockProcessor::roleLock)
    {
        needle.setValues (s.gainDb.load(), s.targetDb.load(), range, hold, refOn);
        needle.tick (1.0f / 30.0f);
        bars.setValues (s.refLufs.load(), s.postLufs.load(), s.outLufs.load(), refPk, outPk, refOn, lufs);
        if (nameTimer % 2 == 0) history.push (s.gainDb.load(), range, hold || ! refOn);
    }
    else
    {
        bars.setValues (s.refLufs.load(), -180, -180, refPk, 0, true, true);
    }

    if (++nameTimer % 15 == 0) linkedName = proc.getLinkedTrackName();
    repaint (0, getHeight() - (int) (30.0f * uiScale), getWidth(), (int) (30.0f * uiScale));
}

juce::String GainLockEditor::statusText() const
{
    auto& s = proc.status;
    const juce::String grp = "Gruppe " + juce::String (proc.getGroupIndex() + 1);
    const juce::String name = linkedName.isNotEmpty() ? U (" \xc2\xb7 ") + linkedName : juce::String();

    if (s.role.load() == GainLockProcessor::roleSource)
    {
        if (s.groupTaken.load()) return grp + U (" hat schon eine Quelle \xe2\x80\x93 w\xc3\xa4hl eine andere Gruppe");
        if (s.lockSeen.load())   return U ("\xe2\x97\x8f Lock verbunden \xc2\xb7 ") + grp;
        return U ("Warte auf Lock in ") + grp + U (" \xe2\x80\x93 zweites Gain Lock ans Ende der Kette, Rolle LOCK");
    }

    switch (s.refKind.load())
    {
        case GainLockProcessor::refSource:   return U ("\xe2\x97\x8f Gekoppelt mit Quelle \xc2\xb7 ") + grp + name;
        case GainLockProcessor::refSidechain: return U ("\xe2\x97\x8f Referenz: Sidechain-Eingang");
        default: break;
    }
    if (s.rateMismatch.load()) return U ("Quelle in ") + grp + U (" l\xc3\xa4uft mit anderer Samplerate");
    if (s.sourceStale.load())  return U ("Quelle in ") + grp + U (" spielt gerade nicht \xe2\x80\x93 Korrektur wird gehalten");
    return U ("Keine Quelle in ") + grp + U (" \xe2\x80\x93 Gain Lock an den Anfang der Kette, Rolle QUELLE");
}

void GainLockEditor::paint (juce::Graphics& g)
{
    const float u = uiScale;
    auto r = getLocalBounds().toFloat();

    // Grund: Graphit mit feiner Bürstung
    g.fillAll (col::bg);
    {
        juce::ColourGradient vg (juce::Colour (0xff17191c), r.getCentreX(), r.getY(),
                                 col::bg, r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (vg);
        g.fillRect (r);
        juce::Random rnd (7);
        for (int i = 0; i < 160; ++i)
        {
            const float y = rnd.nextFloat() * r.getHeight();
            g.setColour (juce::Colours::white.withAlpha (0.008f + rnd.nextFloat() * 0.012f));
            g.drawHorizontalLine ((int) y, 0.0f, r.getWidth());
        }
    }

    // Kopfzeile
    auto head = r.removeFromTop (64.0f * u);
    if (logo.isValid())
    {
        const float lh = 38.0f * u;
        const float lw = lh * (float) logo.getWidth() / (float) logo.getHeight();
        g.setOpacity (1.0f);
        g.drawImage (logo, juce::Rectangle<float> (18.0f * u, (head.getHeight() - lh) * 0.5f, lw, lh), juce::RectanglePlacement::centred);
        const float tx = 18.0f * u + lw + 14.0f * u;
        g.setColour (col::line);
        g.drawVerticalLine ((int) (tx - 7.0f * u), 16.0f * u, 48.0f * u);
        g.setFont (bb::fonts::display (25.0f * u, true));
        g.setColour (col::ink);
        g.drawText ("GAIN LOCK", juce::Rectangle<float> (tx, 12.0f * u, 220.0f * u, 26.0f * u), juce::Justification::centredLeft);
        g.setFont (bb::fonts::mono (11.0f * u));
        g.setColour (col::amber.withAlpha (0.85f));
        g.drawText (U ("AUTO GAIN MATCH \xc2\xb7 A/B"), juce::Rectangle<float> (tx + 1.0f * u, 37.0f * u, 220.0f * u, 14.0f * u), juce::Justification::centredLeft);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawHorizontalLine ((int) (64.0f * u), 0.0f, (float) getWidth());
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) (65.0f * u), 0.0f, (float) getWidth());

    const bool lock = shownRole == 1;
    if (lock)
    {
        // Knopf-Beschriftungen
        g.setFont (bb::fonts::display (12.0f * u, true));
        g.setColour (col::inkDim);
        g.drawText ("TRIM", trimKnob.getBounds().toFloat().withHeight (14.0f * u).translated (0, -14.0f * u), juce::Justification::centred);
        g.drawText (U ("BEREICH"), rangeKnob.getBounds().toFloat().withHeight (14.0f * u).translated (0, -14.0f * u), juce::Justification::centred);
    }
    else
    {
        // Quelle: Infofeld
        auto info = juce::Rectangle<float> (16.0f * u, 76.0f * u, 484.0f * u, 374.0f * u);
        drawPanel (g, info);
        auto in = info.reduced (28.0f * u, 26.0f * u);
        g.setFont (bb::fonts::mono (12.0f * u));
        g.setColour (col::amber);
        g.drawText (U ("ROLLE"), in.removeFromTop (16.0f * u), juce::Justification::centredLeft);
        g.setFont (bb::fonts::display (46.0f * u, true));
        g.setColour (col::ink);
        g.drawText ("QUELLE", in.removeFromTop (52.0f * u), juce::Justification::centredLeft);
        in.removeFromTop (8.0f * u);
        g.setColour (col::line);
        g.drawHorizontalLine ((int) in.getY(), in.getX(), in.getRight());
        in.removeFromTop (16.0f * u);
        g.setFont (bb::fonts::display (16.0f * u));
        g.setColour (col::ink.withAlpha (0.85f));
        juce::AttributedString as;
        as.setJustification (juce::Justification::topLeft);
        as.setLineSpacing (5.0f * u);
        as.append (U ("Diese Instanz sitzt am Anfang der Kette und reicht das Signal unver\xc3\xa4ndert durch.\n\n"),
                   bb::fonts::display (16.0f * u), col::ink.withAlpha (0.85f));
        as.append (U ("Ans Ende der Kette kommt ein zweites Gain Lock mit Rolle LOCK und derselben Gruppe. "
                      "Es gleicht die Lautheit automatisch an und spielt auf A/B das Original \xe2\x80\x93 latenzbereinigt."),
                   bb::fonts::display (16.0f * u), col::inkDim);
        as.draw (g, in.removeFromTop (190.0f * u));

        // Verbindungsanzeige
        auto st = in.removeFromBottom (40.0f * u);
        drawRecess (g, st, 4.0f * u);
        const bool ok = proc.status.lockSeen.load();
        const bool taken = proc.status.groupTaken.load();
        auto dot = st.withWidth (st.getHeight()).reduced (14.0f * u);
        const auto c = taken ? col::red : (ok ? col::cyan : col::amberDim);
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (dot.expanded (4.0f * u));
        g.setColour (c);
        g.fillEllipse (dot);
        g.setFont (bb::fonts::display (15.0f * u, true));
        g.setColour (taken ? col::red : (ok ? col::ink : col::inkDim));
        g.drawText (taken ? U ("GRUPPE BELEGT") : (ok ? U ("LOCK VERBUNDEN") : U ("WARTE AUF LOCK")),
                    st.withTrimmedLeft (st.getHeight() + 4.0f * u), juce::Justification::centredLeft);
    }

    // Statuszeile
    auto statusR = juce::Rectangle<float> (18.0f * u, getHeight() - 26.0f * u, getWidth() - 36.0f * u, 20.0f * u);
    g.setFont (bb::fonts::mono (12.0f * u));
    auto& s = proc.status;
    const bool linkedSrc = lock && s.refKind.load() == GainLockProcessor::refSource;
    juce::String right;
    if (linkedSrc)
    {
        right = s.latencyKnown.load() ? U ("LATENZ ") + juce::String (s.latencyMs.load(), 1) + " ms"
                                      : U ("LATENZ WIRD GESUCHT\xe2\x80\xa6");
        if (s.latencyKnown.load() && s.polarityFlip.load()) right = U ("PHASE GEDREHT \xc2\xb7 ") + right;
    }
    const bool warn = s.refKind.load() == GainLockProcessor::refNone && lock;
    g.setColour (warn || s.groupTaken.load() ? col::amber.withAlpha (0.85f) : col::inkDim);
    g.drawText (statusText(), statusR.withTrimmedRight (right.isNotEmpty() ? 220.0f * u : 0.0f), juce::Justification::centredLeft, true);
    g.setColour (s.latencyKnown.load() ? col::cyan.withAlpha (0.85f) : col::inkDim);
    g.drawText (right, statusR, juce::Justification::centredRight);
}

void GainLockEditor::resized()
{
    const float u = uiScale = getWidth() / kBaseW;
    auto R = [u] (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (x * u, y * u, w * u, h * u).toNearestInt();
    };

    roleSwitch.setBounds (R (462, 16, 178, 32));
    groupBox.setBounds (R (652, 16, 110, 32));

    needle.setBounds (R (16, 76, 484, 254));
    bars.setBounds (R (512, 76, 252, shownRole == 1 ? 254 : 374));
    history.setBounds (R (16, 342, 220, 108));
    speedSwitch.setBounds (R (248, 342, 200, 50));
    metricSwitch.setBounds (R (248, 400, 200, 50));
    trimKnob.setBounds (R (456, 358, 92, 92));
    rangeKnob.setBounds (R (552, 358, 92, 92));
    holdButton.setBounds (R (656, 346, 108, 46));
    abButton.setBounds (R (656, 400, 108, 50));

    for (auto* k : { &trimKnob, &rangeKnob })
        k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, (int) (80 * u), (int) (18 * u));
}
