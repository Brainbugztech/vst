#include "BandView.h"

namespace
{
    constexpr float fMin = 20.0f, fMax = 20000.0f;
    const char* bandNames[4]  { "LOW", "LO-MID", "HI-MID", "HIGH" };
    const char* bandShort[4]  { "LO", "LM", "HM", "HI" };

    juce::Colour bandColour (int b)
    {
        // Brain-Bugz-Verlauf: Gift-Grün -> Gehirn-Pink
        return bb::col::tox.interpolatedWith (bb::col::brain, (float) b / 3.0f);
    }

    juce::String hzText (float v)
    {
        if (v >= 10000.0f) return juce::String (v / 1000.0f, 1) + " kHz";
        if (v >= 1000.0f)  return juce::String (v / 1000.0f, 2) + " kHz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }
}

BandView::BandView (BugFuzzProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setTitle ("Baender und Weichen");
    startTimerHz (30);
}

BandView::~BandView() { stopTimer(); }

void BandView::timerCallback()
{
    for (int b = 0; b < 4; ++b)
    {
        const float target = std::min (1.0f, proc.vizBand[(size_t) b].load() * 3.5f);
        level[(size_t) b] += (target - level[(size_t) b]) * (target > level[(size_t) b] ? 0.6f : 0.15f);
    }
    ++frame;
    repaint();
}

juce::Rectangle<float> BandView::plot() const
{
    auto r = getLocalBounds().toFloat();
    return r.withTrimmedTop (r.getHeight() * 0.16f).withTrimmedBottom (r.getHeight() * 0.09f).reduced (r.getWidth() * 0.012f, 0);
}

float BandView::freqToX (float hz) const
{
    const auto p = plot();
    return p.getX() + p.getWidth() * std::log (hz / fMin) / std::log (fMax / fMin);
}

float BandView::xToFreq (float x) const
{
    const auto p = plot();
    return fMin * std::pow (fMax / fMin, juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}

//==============================================================================
void BandView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float u = r.getHeight() / 290.0f; // Entwurf bei 290 px Höhe
    const auto p = plot();
    const auto prm = proc.readParams();
    const auto x = bb::BugFuzzEngine::sanitiseXover (prm.xover);

    // Grundfläche
    g.setColour (juce::Colour (0xff0b0810));
    g.fillRoundedRectangle (r, 12 * u);
    g.setColour (bb::col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 12 * u, 1.0f);

    // Raster mit Frequenz-Beschriftung
    g.setFont (bb::fonts::mono (11 * u));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float gx = freqToX (f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawVerticalLine ((int) gx, p.getY(), p.getBottom());
        g.setColour (bb::col::inkDim.withAlpha (0.7f));
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (gx - 30 * u, p.getBottom() + 4 * u, 60 * u, 14 * u), juce::Justification::centred);
    }
    for (int k = 1; k < 4; ++k)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine ((int) (p.getBottom() - p.getHeight() * k / 4.0f), p.getX(), p.getRight());
    }

    // Bänder
    const float edges[5] { p.getX(), freqToX (x[0]), freqToX (x[1]), freqToX (x[2]), p.getRight() };
    juce::Random rnd ((juce::int64) frame * 7919);
    for (int b = 0; b < 4; ++b)
    {
        const auto col = bandColour (b);
        const float x0 = edges[b] + 2 * u, x1 = edges[b + 1] - 2 * u;
        if (x1 - x0 < 2) continue;
        const float v = prm.band[(size_t) b];
        const float top = p.getBottom() - p.getHeight() * v;
        const bool hot = hover == b || dragItem == b || (focusItem == b && hasKeyboardFocus (false));

        // Oberkante: zittert ("fuzzt") mit Pegel und Drive
        const float amp = level[(size_t) b] * (2.0f + 10.0f * prm.drive) * u * (v > 0.01f ? 1.0f : 0.0f);
        juce::Path edge, fill;
        const int steps = std::max (2, (int) ((x1 - x0) / (4 * u)));
        for (int i = 0; i <= steps; ++i)
        {
            const float xx = x0 + (x1 - x0) * i / steps;
            const float yy = juce::jmin (p.getBottom(), top + (i == 0 || i == steps ? 0.0f : (rnd.nextFloat() - 0.5f) * 2.0f * amp));
            if (i == 0) { edge.startNewSubPath (xx, yy); fill.startNewSubPath (xx, p.getBottom()); fill.lineTo (xx, yy); }
            else        { edge.lineTo (xx, yy); fill.lineTo (xx, yy); }
        }
        fill.lineTo (x1, p.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (col.withAlpha (hot ? 0.5f : 0.38f), 0, top,
                                                 col.withAlpha (0.04f), 0, p.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col.withAlpha (0.25f + 0.3f * level[(size_t) b]));
        g.strokePath (edge, juce::PathStrokeType (7 * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (edge, juce::PathStrokeType (2.2f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Beschriftung oben im Band
        const float bw = x1 - x0;
        const auto labelArea = juce::Rectangle<float> (x0, r.getY() + 8 * u, bw, 18 * u);
        g.setFont (bb::fonts::display ((bw > 90 * u ? 14.0f : 12.0f) * u, true));
        g.setColour (hot ? col : col.withAlpha (0.85f));
        g.drawText (bw > 90 * u ? bandNames[b] : bandShort[b], labelArea, juce::Justification::centred);
        g.setFont (bb::fonts::mono (12 * u));
        g.setColour (bb::col::ink.withAlpha (hot ? 1.0f : 0.75f));
        const juce::String val = juce::String (juce::roundToInt (v * 100.0f)) + "%";
        const juce::String db = "+" + juce::String (juce::roundToInt (bb::BugFuzzEngine::bandGainDb (prm.drive, v))) + " dB";
        g.drawText (bw > 110 * u ? val + "  " + db : val, labelArea.translated (0, 18 * u), juce::Justification::centred);
    }

    // Weichen mit Griff
    for (int i = 0; i < 3; ++i)
    {
        const float gx = edges[i + 1];
        const bool hot = hover == 4 + i || dragItem == 4 + i || (focusItem == 4 + i && hasKeyboardFocus (false));
        const auto col = bandColour (i).interpolatedWith (bandColour (i + 1), 0.5f);
        g.setColour (hot ? col : bb::col::ink.withAlpha (0.35f));
        g.fillRect (gx - (hot ? 1.5f : 1.0f) * u, p.getY(), (hot ? 3.0f : 2.0f) * u, p.getHeight());

        const auto grip = juce::Rectangle<float> (gx - 34 * u, p.getY() - 1 * u, 68 * u, 20 * u);
        g.setColour (juce::Colour (0xff17111c));
        g.fillRoundedRectangle (grip, 10 * u);
        g.setColour (hot ? col : bb::col::line.brighter (0.4f));
        g.drawRoundedRectangle (grip, 10 * u, 1.2f * u);
        g.setFont (bb::fonts::mono (11.5f * u));
        g.setColour (hot ? col : bb::col::ink);
        g.drawText (hzText (x[(size_t) i]), grip, juce::Justification::centred);
    }
}

//==============================================================================
int BandView::itemAt (juce::Point<float> pt) const
{
    const auto x = bb::BugFuzzEngine::sanitiseXover (proc.readParams().xover);
    const float u = (float) getHeight() / 290.0f;
    // Weichen haben Vorrang (Griff oder Linie, ±8 px)
    const bool inGripRow = pt.y >= plot().getY() - 2 * u && pt.y <= plot().getY() + 20 * u;
    int best = -1;
    float bestD = (inGripRow ? 34.0f : 9.0f) * u;
    for (int i = 0; i < 3; ++i)
    {
        const float d = std::abs (pt.x - freqToX (x[(size_t) i]));
        if (d < bestD) { bestD = d; best = 4 + i; }
    }
    if (best >= 0) return best;
    if (! getLocalBounds().toFloat().contains (pt)) return -1;
    for (int b = 0; b < 4; ++b)
    {
        const float lo = b == 0 ? -1e9f : freqToX (x[(size_t) b - 1]);
        const float hi = b == 3 ? 1e9f : freqToX (x[(size_t) b]);
        if (pt.x >= lo && pt.x < hi) return b;
    }
    return -1;
}

void BandView::mouseMove (const juce::MouseEvent& e)
{
    hover = itemAt (e.position);
    setMouseCursor (hover >= 4 ? juce::MouseCursor::LeftRightResizeCursor
                    : hover >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void BandView::mouseExit (const juce::MouseEvent&) { hover = -1; }

void BandView::mouseDown (const juce::MouseEvent& e)
{
    dragItem = itemAt (e.position);
    if (dragItem < 0) return;
    focusItem = dragItem;
    const auto prm = proc.readParams();
    startValue = dragItem < 4 ? prm.band[(size_t) dragItem]
                              : bb::BugFuzzEngine::sanitiseXover (prm.xover)[(size_t) (dragItem - 4)];
}

void BandView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragItem < 0) return;
    const bool fine = e.mods.isShiftDown();
    if (dragItem < 4)
    {
        const float dy = (float) e.getDistanceFromDragStartY() / plot().getHeight();
        setBand (dragItem, startValue - dy * (fine ? 0.25f : 1.0f));
    }
    else if (fine)
    {
        const float dx = (float) e.getDistanceFromDragStartX();
        setXover (dragItem - 4, startValue * std::pow (2.0f, dx * 0.004f));
    }
    else
        setXover (dragItem - 4, xToFreq (e.position.x));
}

void BandView::mouseUp (const juce::MouseEvent&) { dragItem = -1; }

void BandView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int it = itemAt (e.position);
    if (it >= 0 && it < 4) setBand (it, BugFuzzProcessor::kBandDefaults[it]);
    else if (it >= 4)      setXover (it - 4, BugFuzzProcessor::kXoverDefaults[it - 4]);
}

void BandView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int it = itemAt (e.position);
    if (it >= 0) step (it, w.deltaY > 0 ? 1.0f : -1.0f, e.mods.isShiftDown());
}

bool BandView::keyPressed (const juce::KeyPress& k)
{
    const bool fine = k.getModifiers().isShiftDown();
    if (k.isKeyCode (juce::KeyPress::upKey))    { step (focusItem, 1.0f, fine); return true; }
    if (k.isKeyCode (juce::KeyPress::downKey))  { step (focusItem, -1.0f, fine); return true; }
    if (k.isKeyCode (juce::KeyPress::rightKey)) { focusItem = (focusItem + 1) % 7; return true; }
    if (k.isKeyCode (juce::KeyPress::leftKey))  { focusItem = (focusItem + 6) % 7; return true; }
    return false;
}

void BandView::step (int it, float dir, bool fine)
{
    const auto prm = proc.readParams();
    if (it >= 0 && it < 4) setBand (it, prm.band[(size_t) it] + dir * (fine ? 0.01f : 0.05f));
    else if (it >= 4)      setXover (it - 4, bb::BugFuzzEngine::sanitiseXover (prm.xover)[(size_t) (it - 4)]
                                                 * std::pow (2.0f, dir * (fine ? 1.0f / 24.0f : 1.0f / 6.0f)));
}

void BandView::setBand (int b, float v)
{
    proc.setParam (ParamIDs::band[b], juce::jlimit (0.0f, 1.0f, v));
}

void BandView::setXover (int i, float hz)
{
    // Nachbarn berücksichtigen, damit die Weichen in Reihenfolge bleiben
    const auto x = bb::BugFuzzEngine::sanitiseXover (proc.readParams().xover);
    float lo = bb::BugFuzzEngine::kXMin[i], hi = bb::BugFuzzEngine::kXMax[i];
    if (i > 0) lo = std::max (lo, x[(size_t) i - 1] * 1.5f);
    if (i < 2) hi = std::min (hi, x[(size_t) i + 1] / 1.5f);
    proc.setParam (ParamIDs::xover[i], juce::jlimit (lo, std::max (lo, hi), hz));
}
