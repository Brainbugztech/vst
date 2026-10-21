#include "AlignView.h"
#include "BrainBugzLook.h"

namespace
{
    constexpr float baseW = 860.0f;
}

void AlignView::update()
{
    auto& e = proc.engine;
    const int s = e.snap.serial.load (std::memory_order_acquire);
    if (s != serial)
    {
        serial = s;
        for (size_t i = 0; i < kick.size(); ++i)
        {
            kick[i]   = e.snap.kick[i].load (std::memory_order_relaxed);
            before[i] = e.snap.before[i].load (std::memory_order_relaxed);
            after[i]  = e.snap.after[i].load (std::memory_order_relaxed);
        }
        corrBefore = e.snap.corrBefore.load();
        corrAfter  = e.snap.corrAfter.load();
        detected   = e.snap.thetaDeg.load();
        hasData = s > 0;
        freshness = 1.0f;
    }
    applied = e.appliedDeg.load();

    const int kc = e.kickCount.load();
    if (kc != lastKicks) { lastKicks = kc; kickFlash = 1.0f; }
    kickFlash *= 0.8f;
    freshness *= 0.94f;
    repaint();
}

void AlignView::paint (juce::Graphics& g)
{
    const float k = (float) getWidth() / baseW;
    auto R = [k] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * k, y * k, w * k, h * k); };

    drawScope (g, R (0, 0, 560, 230));
    drawDial (g, R (572, 0, 150, 230));
    drawMeters (g, R (734, 0, 126, 230));
}

void AlignView::drawScope (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float k = (float) getWidth() / baseW;
    g.setColour (bb::col::panel);
    g.fillRoundedRectangle (r, 8 * k);
    g.setColour (bb::col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8 * k, 1.0f);

    g.setFont (bb::fonts::mono (11 * k));
    g.setColour (bb::col::inkDim);
    g.drawText (juce::String::fromUTF8 ("PUNCH-FENSTER \xc2\xb7 letzte Kick"), r.withTrimmedLeft (12 * k).withHeight (24 * k),
                juce::Justification::centredLeft);

    // Legende
    struct L { const char* t; juce::Colour c; };
    float lx = r.getRight() - 12 * k;
    for (const auto& l : { L { "NACHHER", bb::col::brain }, L { "VORHER", bb::col::inkDim }, L { "KICK", bb::col::tox } })
    {
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), l.t);
        lx -= tw;
        g.setColour (l.c);
        g.drawText (l.t, juce::Rectangle<float> (lx, r.getY(), tw + 2, 24 * k), juce::Justification::centredLeft);
        g.fillRoundedRectangle (lx - 16 * k, r.getY() + 11 * k, 11 * k, 3 * k, 1.5f * k);
        lx -= 30 * k;
    }

    auto plot = r.reduced (12 * k).withTrimmedTop (16 * k).withTrimmedBottom (14 * k);
    const float mid = plot.getCentreY();

    // Raster
    g.setColour (bb::col::line.withAlpha (0.6f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 8.0f), plot.getY(), plot.getBottom());
    g.setColour (bb::col::line);
    g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());

    const auto win = proc.apvts.getParameter (ParamIDs::window)->getCurrentValueAsText();
    g.setFont (bb::fonts::mono (10 * k));
    g.setColour (bb::col::inkDim.withAlpha (0.7f));
    g.drawText ("0 ms", juce::Rectangle<float> (plot.getX(), plot.getBottom(), 60 * k, 14 * k), juce::Justification::centredLeft);
    g.drawText (win, juce::Rectangle<float> (plot.getRight() - 80 * k, plot.getBottom(), 80 * k, 14 * k), juce::Justification::centredRight);

    if (! hasData)
    {
        const bool sc = proc.sidechainConnected.load() && proc.engine.kickLevel.load() > 1.0e-4f;
        g.setFont (bb::fonts::display (15 * k));
        g.setColour (bb::col::ink.withAlpha (0.8f));
        g.drawText (sc ? juce::String::fromUTF8 ("Warte auf Kick + Bass \xe2\x80\xa6") : juce::String ("Kein Kick am Sidechain"),
                    plot.withTrimmedBottom (plot.getHeight() * 0.35f), juce::Justification::centred);
        if (! sc)
        {
            g.setFont (bb::fonts::mono (11 * k));
            g.setColour (bb::col::inkDim);
            g.drawText (juce::String::fromUTF8 ("Plugin-Fenster aufklappen \xe2\x86\x92 Sidechain \xe2\x86\x92 Audio From: Kick-Spur"),
                        plot.withTrimmedTop (plot.getHeight() * 0.4f), juce::Justification::centredTop);
        }
        return;
    }

    float peak = 1.0e-6f;
    for (size_t i = 0; i < kick.size(); ++i)
        peak = std::max ({ peak, std::abs (kick[i]), std::abs (before[i]), std::abs (after[i]) });
    const float sy = plot.getHeight() * 0.46f / peak;
    const int nPts = (int) kick.size();

    auto makePath = [&] (const std::array<float, bb::AlignEngine::kSnapPoints>& d)
    {
        juce::Path p;
        for (int i = 0; i < nPts; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (nPts - 1);
            const float y = mid - d[(size_t) i] * sy;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };

    // Kick: Fläche + Linie
    auto kp = makePath (kick);
    juce::Path kf (kp);
    kf.lineTo (plot.getRight(), mid);
    kf.lineTo (plot.getX(), mid);
    kf.closeSubPath();
    g.setColour (bb::col::tox.withAlpha (0.12f));
    g.fillPath (kf);
    g.setColour (bb::col::tox.withAlpha (0.85f));
    g.strokePath (kp, juce::PathStrokeType (1.6f * k));

    // Bass vorher (gestrichelt)
    auto bp = makePath (before);
    juce::Path dashed;
    const float dash[] { 5.0f * k, 4.0f * k };
    juce::PathStrokeType (1.3f * k).createDashedStroke (dashed, bp, dash, 2);
    g.setColour (bb::col::inkDim.withAlpha (0.75f));
    g.fillPath (dashed);

    // Bass nachher mit Glow
    auto ap = makePath (after);
    g.setColour (bb::col::brain.withAlpha (0.18f + 0.25f * freshness));
    g.strokePath (ap, juce::PathStrokeType (6.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (bb::col::brain);
    g.strokePath (ap, juce::PathStrokeType (2.2f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void AlignView::drawDial (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float k = (float) getWidth() / baseW;
    g.setColour (bb::col::panel);
    g.fillRoundedRectangle (r, 8 * k);
    g.setColour (bb::col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8 * k, 1.0f);

    g.setFont (bb::fonts::mono (11 * k));
    g.setColour (bb::col::inkDim);
    g.drawText ("PHASE", r.withHeight (24 * k), juce::Justification::centred);

    const juce::Point<float> c (r.getCentreX(), r.getY() + 104 * k);
    const float rad = 52 * k;

    g.setColour (bb::col::voidBg);
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
    g.setColour (bb::col::line);
    g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 2.0f * k);
    for (int i = 0; i < 24; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
        const float inner = i % 6 == 0 ? rad - 9 * k : rad - 4 * k;
        g.drawLine (c.x + std::sin (a) * inner, c.y - std::cos (a) * inner, c.x + std::sin (a) * rad, c.y - std::cos (a) * rad, 1.0f * k);
    }

    auto pointer = [&] (float deg, juce::Colour col, float w, float len)
    {
        const float a = juce::degreesToRadians (deg);
        const juce::Point<float> tip (c.x + std::sin (a) * len, c.y - std::cos (a) * len);
        g.setColour (col.withAlpha (0.25f));
        g.drawLine ({ c, tip }, w * 2.6f);
        g.setColour (col);
        g.drawLine ({ c, tip }, w);
    };
    if (hasData) pointer (detected, bb::col::inkDim.withAlpha (0.6f), 1.4f * k, rad - 6 * k);
    pointer (applied, bb::col::brain, 3.0f * k, rad - 10 * k);
    g.setColour (bb::col::ink);
    g.fillEllipse (c.x - 4 * k, c.y - 4 * k, 8 * k, 8 * k);

    g.setFont (bb::fonts::display (22 * k, true));
    g.setColour (bb::col::ink);
    g.drawText ((applied > 0.5f ? "+" : "") + juce::String (juce::roundToInt (applied)) + juce::String::fromUTF8 ("\xc2\xb0"),
                juce::Rectangle<float> (r.getX(), c.y + rad + 8 * k, r.getWidth(), 26 * k), juce::Justification::centred);
    g.setFont (bb::fonts::mono (10 * k));
    g.setColour (bb::col::inkDim);
    g.drawText (proc.readParams().bypass ? juce::String ("A/B: VORHER") : juce::String ("gedreht"),
                juce::Rectangle<float> (r.getX(), c.y + rad + 34 * k, r.getWidth(), 14 * k), juce::Justification::centred);
}

void AlignView::drawMeters (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float k = (float) getWidth() / baseW;
    g.setColour (bb::col::panel);
    g.fillRoundedRectangle (r, 8 * k);
    g.setColour (bb::col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8 * k, 1.0f);

    g.setFont (bb::fonts::mono (11 * k));
    g.setColour (bb::col::inkDim);
    g.drawText ("KORRELATION", r.withHeight (24 * k), juce::Justification::centred);

    auto bar = [&] (float y, const char* label, float v, juce::Colour col)
    {
        auto b = juce::Rectangle<float> (r.getX() + 12 * k, y, r.getWidth() - 24 * k, 12 * k);
        g.setFont (bb::fonts::mono (10 * k));
        g.setColour (bb::col::inkDim);
        g.drawText (label, b.translated (0, -15 * k).withHeight (14 * k), juce::Justification::centredLeft);
        g.setColour (hasData ? col : bb::col::inkDim);
        g.drawText (hasData ? juce::String (v, 2) : juce::String ("--"), b.translated (0, -15 * k).withHeight (14 * k),
                    juce::Justification::centredRight);
        g.setColour (bb::col::voidBg);
        g.fillRoundedRectangle (b, 3 * k);
        if (hasData)
        {
            const float cx = b.getCentreX();
            const float x2 = cx + juce::jlimit (-1.0f, 1.0f, v) * b.getWidth() * 0.5f;
            g.setColour (col);
            g.fillRoundedRectangle (juce::Rectangle<float> (std::min (cx, x2), b.getY() + 2 * k, std::abs (x2 - cx), b.getHeight() - 4 * k), 2 * k);
        }
        g.setColour (bb::col::ink.withAlpha (0.5f));
        g.drawVerticalLine ((int) b.getCentreX(), b.getY(), b.getBottom());
    };
    bar (r.getY() + 52 * k, "VORHER", corrBefore, bb::col::inkDim);
    bar (r.getY() + 92 * k, "NACHHER", corrAfter, bb::col::brain);

    // Kick-LED
    const auto led = juce::Rectangle<float> (r.getX() + 14 * k, r.getY() + 128 * k, 12 * k, 12 * k);
    g.setColour (bb::col::toxDim.withAlpha (0.35f));
    g.fillEllipse (led);
    if (kickFlash > 0.02f)
    {
        g.setColour (bb::col::tox.withAlpha (kickFlash));
        g.fillEllipse (led);
        g.setColour (bb::col::tox.withAlpha (kickFlash * 0.3f));
        g.fillEllipse (led.expanded (5 * k));
    }
    g.setFont (bb::fonts::mono (11 * k));
    g.setColour (bb::col::ink);
    g.drawText ("KICK", juce::Rectangle<float> (led.getRight() + 8 * k, led.getY() - 2 * k, 80 * k, 16 * k), juce::Justification::centredLeft);

    const auto p = proc.readParams();
    g.setFont (bb::fonts::mono (10 * k));
    g.setColour (p.hold ? bb::col::tox : bb::col::inkDim);
    g.drawText (p.hold ? "HOLD aktiv" : "folgt der Kick", juce::Rectangle<float> (r.getX(), r.getY() + 156 * k, r.getWidth(), 14 * k),
                juce::Justification::centred);
    g.setColour (bb::col::inkDim);
    g.drawText ("Latenz " + juce::String (juce::roundToInt (proc.latencyMs.load())) + " ms",
                juce::Rectangle<float> (r.getX(), r.getY() + 176 * k, r.getWidth(), 14 * k), juce::Justification::centred);
    g.drawText ("(Ableton gleicht aus)", juce::Rectangle<float> (r.getX(), r.getY() + 192 * k, r.getWidth(), 14 * k),
                juce::Justification::centred);
}
