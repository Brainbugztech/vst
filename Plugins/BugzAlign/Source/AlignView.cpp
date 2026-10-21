#include "AlignView.h"
#include "BrainBugzLook.h"

AlignView::AlignView (BugzAlignProcessor& p) : proc (p)
{
    for (auto* b : { &zoomIn, &zoomOut, &pauseBtn, &beforeBtn })
    {
        b->getProperties().set ("mono", true);
        addAndMakeVisible (*b);
    }
    beforeBtn.setClickingTogglesState (true);
    beforeBtn.setToggleState (true, juce::dontSendNotification);
    beforeBtn.getProperties().set ("accent", "tox");
    pauseBtn.getProperties().set ("accent", "brain");
    pauseBtn.onClick = [this] { setPaused (! paused); };
    zoomIn.onClick  = [this] { zoomBy (0.5f, waveArea().getRight()); };
    zoomOut.onClick = [this] { zoomBy (2.0f, waveArea().getRight()); };
    beforeBtn.onClick = [this] { repaint(); };
    zoomIn.setTooltip ("Reinzoomen (Mausrad geht auch)");
    zoomOut.setTooltip ("Rauszoomen");
    pauseBtn.setTooltip ("Anhalten und mit der Maus zurueckscrollen, Doppelklick = Live");
    beforeBtn.setTooltip ("Bass vorher ein-/ausblenden");
}

void AlignView::resized()
{
    k = (float) getWidth() / baseW;
    beforeBtn.setBounds (R (520, 6, 78, 21).toNearestInt());
    zoomOut.setBounds   (R (610, 6, 26, 21).toNearestInt());
    zoomIn.setBounds    (R (716, 6, 26, 21).toNearestInt());
    pauseBtn.setBounds  (R (752, 6, 96, 21).toNearestInt());
}

// ------------------------------------------------------------------------------------------------
void AlignView::setPaused (bool shouldPause)
{
    if (shouldPause == paused) return;
    paused = shouldPause;
    pauseBtn.setToggleState (paused, juce::dontSendNotification);
    pauseBtn.setButtonText (paused ? "LIVE" : "PAUSE");

    if (paused)
    {
        auto& h = proc.engine.hist;
        const int len = h.length.load();
        pausedWritten = h.written.load (std::memory_order_acquire);
        auto copy = [len] (std::vector<float>& dst, const std::unique_ptr<std::atomic<float>[]>& src)
        {
            dst.resize ((size_t) len);
            for (int i = 0; i < len; ++i) dst[(size_t) i] = src[(size_t) i].load (std::memory_order_relaxed);
        };
        copy (pKick, h.kick); copy (pBefore, h.before); copy (pAfter, h.after);
        copy (pTheta, h.theta); copy (pMark, h.marker);
    }
    else
        endOffset = 0.0;
    repaint();
}

void AlignView::zoomBy (float factor, float anchorX)
{
    const auto wa = waveArea();
    const double newSpan = juce::jlimit (0.01, bb::AlignEngine::kHistorySeconds, spanSec * factor);
    if (paused)
    {
        const double frac = juce::jlimit (0.0, 1.0, (double) (wa.getRight() - anchorX) / wa.getWidth());
        endOffset = std::max (0.0, endOffset + frac * (spanSec - newSpan) * rate);
    }
    spanSec = newSpan;
    repaint();
}

void AlignView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) < 1.0e-4f) return;
    zoomBy (w.deltaY > 0 ? 0.8f : 1.25f, paused ? (float) e.x : waveArea().getRight());
}

void AlignView::mouseDown (const juce::MouseEvent& e)
{
    dragStartX = (float) e.x;
    dragStartOffset = endOffset;
}

void AlignView::mouseDrag (const juce::MouseEvent& e)
{
    const auto wa = waveArea();
    if (! wa.getUnion (phaseArea()).contains (e.mouseDownPosition)) return;
    if (std::abs (e.getDistanceFromDragStartX()) < 3) return;
    if (! paused) { setPaused (true); dragStartOffset = 0.0; }
    const double dx = (double) ((float) e.x - dragStartX);
    endOffset = std::max (0.0, dragStartOffset + dx / wa.getWidth() * spanSec * rate);
    repaint();
}

void AlignView::mouseDoubleClick (const juce::MouseEvent&)
{
    setPaused (false);
}

// ------------------------------------------------------------------------------------------------
void AlignView::update()
{
    auto& e = proc.engine;
    applied = e.appliedDeg.load();
    const int ac = e.analysisCount.load (std::memory_order_acquire);
    if (ac != lastAnalyses)
    {
        lastAnalyses = ac;
        corrBefore = e.corrBefore.load();
        corrAfter = e.corrAfter.load();
        detected = e.detectedDeg.load();
        hasData = true;
    }
    const int kc = e.kickCount.load();
    if (kc != lastKicks) { lastKicks = kc; kickFlash = 1.0f; }
    kickFlash *= 0.8f;
    repaint();
}

void AlignView::fetchVisible()
{
    auto& h = proc.engine.hist;
    rate = h.rate;
    const int len = std::max (1, h.length.load());
    const long long written = paused ? pausedWritten : h.written.load (std::memory_order_acquire);
    const long long oldest = std::max (0LL, written - len);
    const int P = juce::jlimit (4, len, (int) std::lround (spanSec * rate));

    const double maxOffset = std::max (0.0, (double) (written - oldest - P));
    endOffset = juce::jlimit (0.0, maxOffset, endOffset);
    const long long newest = written - 1 - (paused ? (long long) std::lround (endOffset) : 0);

    for (auto* v : { &vKick, &vBefore, &vAfter, &vTheta, &vMark }) v->resize ((size_t) P);
    for (int j = 0; j < P; ++j)
    {
        const long long idx = newest - (P - 1) + j;
        const size_t u = (size_t) j;
        if (idx < oldest || idx < 0)
        {
            vKick[u] = vBefore[u] = vAfter[u] = vTheta[u] = vMark[u] = 0.0f;
            continue;
        }
        const size_t r = (size_t) (idx % len);
        if (paused && r < pKick.size())
        {
            vKick[u] = pKick[r]; vBefore[u] = pBefore[r]; vAfter[u] = pAfter[r]; vTheta[u] = pTheta[r]; vMark[u] = pMark[r];
        }
        else
        {
            vKick[u]   = h.kick[r].load (std::memory_order_relaxed);
            vBefore[u] = h.before[r].load (std::memory_order_relaxed);
            vAfter[u]  = h.after[r].load (std::memory_order_relaxed);
            vTheta[u]  = h.theta[r].load (std::memory_order_relaxed);
            vMark[u]   = h.marker[r].load (std::memory_order_relaxed);
        }
    }
}

// ------------------------------------------------------------------------------------------------
void AlignView::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    g.setColour (bb::col::panel);
    g.fillRoundedRectangle (all, 8 * k);
    g.setColour (bb::col::line);
    g.drawRoundedRectangle (all.reduced (0.5f), 8 * k, 1.0f);

    // Kopf der Anzeige
    g.setFont (bb::fonts::mono (11 * k));
    g.setColour (bb::col::inkDim);
    g.drawText (juce::String::fromUTF8 ("VERLAUF \xc2\xb7 Sub von Kick & Bass, so wie er rauskommt"), R (12, 4, 330, 24),
                juce::Justification::centredLeft);
    struct L { const char* t; juce::Colour c; float x; };
    for (const auto& l : { L { "KICK", bb::col::tox, 360 }, L { "NACHHER", bb::col::brain, 420 } })
    {
        g.setColour (l.c);
        g.fillRoundedRectangle (R (l.x, 14.5f, 11, 3), 1.5f * k);
        g.drawText (l.t, R (l.x + 15, 4, 80, 24), juce::Justification::centredLeft);
    }

    // Zoom-Anzeige zwischen - und +
    auto spanText = [] (double s)
    {
        if (s < 1.0) return juce::String (juce::roundToInt (s * 1000.0)) + " ms";
        return juce::String (s, s < 10.0 ? 1 : 0) + " s";
    };
    g.setColour (bb::col::ink);
    g.drawText (spanText (spanSec), R (636, 4, 80, 24), juce::Justification::centred);

    fetchVisible();
    drawWave (g);
    drawPhase (g);
    drawInfo (g);
}

void AlignView::drawWave (juce::Graphics& g)
{
    const auto a = waveArea();
    g.setColour (bb::col::voidBg);
    g.fillRoundedRectangle (a, 5 * k);

    const int P = (int) vKick.size();
    const int W = std::max (1, (int) a.getWidth());
    const float mid = a.getCentreY();

    // Zeitraster
    static constexpr double steps[] { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0 };
    double step = 2.0;
    for (double s : steps) if (spanSec / s <= 8.0) { step = s; break; }
    g.setFont (bb::fonts::mono (9.5f * k));
    for (int m = 0; m * step <= spanSec + 1.0e-9; ++m)
    {
        const float x = a.getRight() - (float) (m * step / spanSec) * a.getWidth();
        g.setColour (bb::col::line.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        juce::String lbl;
        if (m == 0) lbl = paused ? "PAUSE" : "jetzt";
        else
        {
            const double tsec = m * step + (paused ? endOffset / rate : 0.0);
            lbl = tsec < 1.0 ? "-" + juce::String (juce::roundToInt (tsec * 1000.0)) + " ms" : "-" + juce::String (tsec, 2) + " s";
        }
        g.setColour (bb::col::inkDim.withAlpha (0.8f));
        g.drawText (lbl, juce::Rectangle<float> (x - 70 * k, a.getBottom() - 14 * k, 66 * k, 13 * k), juce::Justification::centredRight);
    }
    g.setColour (bb::col::line);
    g.drawHorizontalLine ((int) mid, a.getX(), a.getRight());

    if (P < 2) return;

    // Kick-Marker
    const float ppx = (float) P / (float) W;
    auto colRange = [&] (int c, int& j0, int& j1)
    {
        j0 = juce::jlimit (0, P - 1, (int) std::floor (c * ppx));
        j1 = juce::jlimit (j0 + 1, P, (int) std::floor ((c + 1) * ppx));
    };
    for (int c = 0; c < W; ++c)
    {
        int j0, j1; colRange (c, j0, j1);
        bool mark = false;
        for (int j = j0; j < j1 && ! mark; ++j) mark = vMark[(size_t) j] > 0.5f;
        if (mark)
        {
            g.setColour (bb::col::tox.withAlpha (0.45f));
            g.drawVerticalLine ((int) a.getX() + c, a.getY(), phaseArea().getBottom());
        }
    }

    // Höhe automatisch (sanft nachgeführt)
    float peak = 1.0e-4f;
    for (int j = 0; j < P; ++j)
        peak = std::max ({ peak, std::abs (vKick[(size_t) j]), std::abs (vAfter[(size_t) j]),
                           beforeBtn.getToggleState() ? std::abs (vBefore[(size_t) j]) : 0.0f });
    shownPeak = peak > shownPeak ? peak : shownPeak + (peak - shownPeak) * 0.15f;
    const float sy = a.getHeight() * 0.44f / std::max (1.0e-4f, shownPeak);

    const bool lines = ppx <= 1.5f;
    auto xAt = [&] (int j) { return a.getX() + a.getWidth() * (float) j / (float) (P - 1); };

    auto linePath = [&] (const std::vector<float>& d)
    {
        juce::Path p;
        for (int j = 0; j < P; ++j)
        {
            const float x = xAt (j), y = mid - d[(size_t) j] * sy;
            if (j == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };
    // Hüllkurve (min/max pro Pixel), als geschlossene Fläche
    auto envPath = [&] (const std::vector<float>& d, juce::Path* top, juce::Path* bottom)
    {
        juce::Path area;
        std::vector<float> mn ((size_t) W), mx ((size_t) W);
        for (int c = 0; c < W; ++c)
        {
            int j0, j1; colRange (c, j0, j1);
            float lo = d[(size_t) j0], hi = lo;
            for (int j = j0 + 1; j < j1; ++j) { lo = std::min (lo, d[(size_t) j]); hi = std::max (hi, d[(size_t) j]); }
            mn[(size_t) c] = mid - lo * sy; mx[(size_t) c] = mid - hi * sy;
        }
        for (int c = 0; c < W; ++c)
        {
            const float x = a.getX() + (float) c + 0.5f;
            if (c == 0) area.startNewSubPath (x, mx[0]); else area.lineTo (x, mx[(size_t) c]);
            if (top != nullptr) { if (c == 0) top->startNewSubPath (x, mx[0]); else top->lineTo (x, mx[(size_t) c]); }
            if (bottom != nullptr) { if (c == 0) bottom->startNewSubPath (x, mn[0]); else bottom->lineTo (x, mn[(size_t) c]); }
        }
        for (int c = W - 1; c >= 0; --c) area.lineTo (a.getX() + (float) c + 0.5f, mn[(size_t) c]);
        area.closeSubPath();
        return area;
    };

    g.saveState();
    g.reduceClipRegion (a.toNearestInt());

    if (lines)
    {
        auto kp = linePath (vKick);
        juce::Path kf (kp);
        kf.lineTo (xAt (P - 1), mid); kf.lineTo (xAt (0), mid); kf.closeSubPath();
        g.setColour (bb::col::tox.withAlpha (0.12f));
        g.fillPath (kf);
        g.setColour (bb::col::tox.withAlpha (0.9f));
        g.strokePath (kp, juce::PathStrokeType (1.6f * k));

        if (beforeBtn.getToggleState())
        {
            juce::Path dashed;
            const float dash[] { 5.0f * k, 4.0f * k };
            juce::PathStrokeType (1.3f * k).createDashedStroke (dashed, linePath (vBefore), dash, 2);
            g.setColour (bb::col::inkDim.withAlpha (0.8f));
            g.fillPath (dashed);
        }

        auto ap = linePath (vAfter);
        g.setColour (bb::col::brain.withAlpha (0.25f));
        g.strokePath (ap, juce::PathStrokeType (5.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (bb::col::brain);
        g.strokePath (ap, juce::PathStrokeType (2.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (bb::col::tox.withAlpha (0.28f));
        g.fillPath (envPath (vKick, nullptr, nullptr));

        if (beforeBtn.getToggleState())
        {
            juce::Path top, bottom;
            envPath (vBefore, &top, &bottom);
            g.setColour (bb::col::inkDim.withAlpha (0.75f));
            g.strokePath (top, juce::PathStrokeType (1.0f * k));
            g.strokePath (bottom, juce::PathStrokeType (1.0f * k));
        }

        juce::Path top, bottom;
        auto af = envPath (vAfter, &top, &bottom);
        g.setColour (bb::col::brain.withAlpha (0.35f));
        g.fillPath (af);
        g.setColour (bb::col::brain);
        g.strokePath (top, juce::PathStrokeType (1.3f * k));
        g.strokePath (bottom, juce::PathStrokeType (1.3f * k));
    }
    g.restoreState();

    // Hinweis ohne Kick
    const bool sc = proc.sidechainConnected.load() && proc.engine.kickLevel.load() > 1.0e-4f;
    if (! sc && ! paused)
    {
        g.setFont (bb::fonts::display (15 * k));
        g.setColour (bb::col::ink.withAlpha (0.85f));
        g.drawText ("Kein Kick am Sidechain", a.withTrimmedBottom (a.getHeight() * 0.3f), juce::Justification::centred);
        g.setFont (bb::fonts::mono (11 * k));
        g.setColour (bb::col::inkDim);
        g.drawText (juce::String::fromUTF8 ("Plugin-Fenster aufklappen \xe2\x86\x92 Sidechain \xe2\x86\x92 Audio From: Kick-Spur"),
                    a.withTrimmedTop (a.getHeight() * 0.52f), juce::Justification::centredTop);
    }
}

void AlignView::drawPhase (juce::Graphics& g)
{
    const auto a = phaseArea();
    g.setColour (bb::col::voidBg);
    g.fillRoundedRectangle (a, 5 * k);
    auto yOf = [&] (float deg) { return a.getCentreY() - deg / 180.0f * (a.getHeight() * 0.45f); };
    g.setColour (bb::col::line);
    g.drawHorizontalLine ((int) yOf (0), a.getX(), a.getRight());
    g.setColour (bb::col::line.withAlpha (0.5f));
    g.drawHorizontalLine ((int) yOf (90), a.getX(), a.getRight());
    g.drawHorizontalLine ((int) yOf (-90), a.getX(), a.getRight());
    g.setFont (bb::fonts::mono (9.5f * k));
    g.setColour (bb::col::inkDim.withAlpha (0.8f));
    g.drawText ("PHASE", a.withTrimmedLeft (6 * k).withHeight (13 * k), juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("+90\xc2\xb0"), juce::Rectangle<float> (a.getX() + 6 * k, yOf (90) - 6 * k, 40 * k, 12 * k), juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("-90\xc2\xb0"), juce::Rectangle<float> (a.getX() + 6 * k, yOf (-90) - 6 * k, 40 * k, 12 * k), juce::Justification::centredLeft);

    const int P = (int) vTheta.size();
    const int W = std::max (1, (int) a.getWidth());
    if (P < 2) return;
    juce::Path p;
    bool started = false;
    float prev = 0.0f;
    for (int c = 0; c < W; ++c)
    {
        const int j = juce::jlimit (0, P - 1, (int) ((c + 0.5f) * (float) P / (float) W));
        const float v = vTheta[(size_t) j];
        const float x = a.getX() + (float) c + 0.5f;
        if (! started || std::abs (v - prev) > 180.0f) { p.startNewSubPath (x, yOf (v)); started = true; }
        else p.lineTo (x, yOf (v));
        prev = v;
    }
    g.setColour (bb::col::brain.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (4.0f * k));
    g.setColour (bb::col::brain);
    g.strokePath (p, juce::PathStrokeType (1.6f * k));
}

void AlignView::drawInfo (juce::Graphics& g)
{
    const float y0 = 300.0f;

    // Kick-LED
    const auto led = R (16, y0 + 16, 12, 12);
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
    g.drawText ("KICK", R (34, y0 + 12, 50, 20), juce::Justification::centredLeft);

    // Phase jetzt / erkannt
    auto deg = [] (float d) { return (d > 0.5f ? "+" : "") + juce::String (juce::roundToInt (d)) + juce::String::fromUTF8 ("\xc2\xb0"); };
    g.setFont (bb::fonts::mono (10 * k));
    g.setColour (bb::col::inkDim);
    g.drawText ("PHASE JETZT", R (96, y0 + 2, 100, 14), juce::Justification::centredLeft);
    g.setFont (bb::fonts::display (20 * k, true));
    g.setColour (bb::col::brain);
    g.drawText (deg (applied), R (96, y0 + 16, 90, 26), juce::Justification::centredLeft);
    g.setFont (bb::fonts::mono (10 * k));
    g.setColour (bb::col::inkDim);
    g.drawText (hasData ? "erkannt " + deg (detected) : juce::String ("erkannt --"), R (186, y0 + 22, 100, 16), juce::Justification::centredLeft);

    // Korrelation
    auto bar = [&] (float x, const char* label, float v, juce::Colour col)
    {
        g.setFont (bb::fonts::mono (10 * k));
        g.setColour (bb::col::inkDim);
        g.drawText (label, R (x, y0 + 2, 120, 14), juce::Justification::centredLeft);
        g.setColour (hasData ? col : bb::col::inkDim);
        g.drawText (hasData ? juce::String (v, 2) : juce::String ("--"), R (x, y0 + 2, 150, 14), juce::Justification::centredRight);
        const auto b = R (x, y0 + 20, 150, 12);
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
    bar (300, "KORRELATION VORHER", corrBefore, bb::col::inkDim);
    bar (470, "NACHHER", corrAfter, bb::col::brain);

    // Status rechts
    const auto prm = proc.readParams();
    g.setFont (bb::fonts::mono (10 * k));
    g.setColour (prm.hold ? bb::col::tox : prm.bypass ? bb::col::brain : bb::col::inkDim);
    g.drawText (prm.hold ? "HOLD aktiv" : prm.bypass ? "A/B: Korrektur aus" : prm.subCheck ? "SUB CHECK" : "folgt der Kick",
                R (640, y0 + 2, 208, 14), juce::Justification::centredRight);
    g.setColour (bb::col::inkDim);
    g.drawText ("Latenz " + juce::String (juce::roundToInt (proc.latencyMs.load())) + " ms (Ableton gleicht aus)",
                R (600, y0 + 18, 248, 14), juce::Justification::centredRight);
    g.setColour (bb::col::inkDim.withAlpha (0.6f));
    g.drawText (juce::String::fromUTF8 ("Mausrad = Zoom \xc2\xb7 Ziehen = zur\xc3\xbc" "ck \xc2\xb7 Doppelklick = Live"),
                R (560, y0 + 32, 288, 14), juce::Justification::centredRight);
}
