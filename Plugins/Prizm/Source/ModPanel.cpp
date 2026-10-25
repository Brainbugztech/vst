#include "ModPanel.h"

#include <cmath>

using namespace prizm;

namespace
{
    juce::String de (const char* text) { return juce::String::fromUTF8 (text); }
    juce::Font display (float size, bool bold = false) { return bb::fonts::display (size, bold); }
    juce::Font mono (float size) { return bb::fonts::mono (size); }

    // feste Pseudo-Zufallsfolge, damit die Anzeige von S&H / Smooth Random ruhig bleibt
    float demoRandom (int i)
    {
        const float r = std::sin ((float) i * 12.9898f + 4.1414f) * 43758.5453f;
        return (r - std::floor (r)) * 2.0f - 1.0f;
    }
} // namespace

//==============================================================================
SegmentSwitch::SegmentSwitch (juce::RangedAudioParameter& param, juce::StringArray segmentLabels, bool cycles, juce::String prefixText)
    : labels (std::move (segmentLabels)), cycle (cycles), prefix (std::move (prefixText)),
      attachment (std::make_unique<juce::ParameterAttachment> (param, [this] (float v) { value = juce::jlimit (0, labels.size() - 1, juce::roundToInt (v)); repaint(); }))
{
    attachment->sendInitialUpdate();
}

SegmentSwitch::SegmentSwitch (juce::StringArray segmentLabels, bool cycles, juce::String prefixText, int initial, std::function<void (int)> changed)
    : labels (std::move (segmentLabels)), cycle (cycles), prefix (std::move (prefixText)), value (initial), onChange (std::move (changed))
{
}

int SegmentSwitch::segmentAt (int x) const
{
    return juce::jlimit (0, labels.size() - 1, x * labels.size() / juce::jmax (1, getWidth()));
}

void SegmentSwitch::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool hot = isMouseOver();
    g.setColour (Col::glassDeep.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Col::holo.withAlpha (hot ? 0.8f : 0.45f));
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    g.setFont (bb::fonts::mono (juce::jlimit (10.5f, 12.5f, (float) getHeight() * 0.46f)));
    if (cycle)
    {
        g.setColour (hot ? Col::ink : Col::holoBright);
        g.drawText (prefix + labels[value], r, Justification::centred);
        return;
    }
    const float w = r.getWidth() / (float) labels.size();
    const int hover = hot ? segmentAt (getMouseXYRelative().x) : -1;
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto seg = Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        if (i == value)
        {
            g.setColour (Col::holo.withAlpha (0.25f));
            g.fillRoundedRectangle (seg.reduced (1.5f), 1.5f);
            g.setColour (Col::holoBright);
            g.fillRect (seg.getX() + 4.0f, seg.getBottom() - 2.5f, seg.getWidth() - 8.0f, 1.2f);
        }
        else if (i > 0 && i != value + 1)
        {
            g.setColour (Col::holoDim.withAlpha (0.6f));
            g.fillRect (seg.getX(), seg.getY() + 4.0f, 1.0f, seg.getHeight() - 8.0f);
        }
        g.setColour (i == value ? Col::ink : (i == hover ? Col::ink.withAlpha (0.8f) : Col::muted));
        g.drawText (labels[i], seg, Justification::centred);
    }
}

void SegmentSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int next = cycle ? (value + 1) % labels.size() : segmentAt (e.x);
    if (next == value)
        return;
    if (attachment != nullptr)
        attachment->setValueAsCompleteGesture ((float) next);
    else
    {
        value = next;
        repaint();
        if (onChange)
            onChange (next);
    }
}

//==============================================================================
void ModSourceTab::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = modSourceColour (source);
    holo::box (g, r, 1.0f, selected || isMouseOver());
    if (flashAmount > 0.0f) // Retrig/Envelope ausgelöst
    {
        g.setColour (col.withAlpha (0.35f * flashAmount));
        g.drawRoundedRectangle (r.expanded (0.5f), 3.0f, 5.0f);
        g.setColour (col.withAlpha (flashAmount));
        g.drawRoundedRectangle (r, 3.0f, 1.5f);
    }
    if (selected)
    {
        g.setColour (col.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (col);
        g.fillRect (r.getX() + 1.0f, r.getY() + 4.0f, 3.0f, r.getHeight() - 8.0f);
    }

    g.setFont (display (16.0f, true).withExtraKerningFactor (0.06f));
    g.setColour (selected ? Col::ink : Col::muted);
    g.drawText (modSourceName (source), r.withTrimmedLeft (12.0f).withHeight (r.getHeight() * 0.6f), Justification::centredLeft);

    // Griff: zeigt, dass man den Reiter ziehen kann
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
        {
            g.setColour (col.withAlpha (0.8f));
            g.fillEllipse (r.getRight() - 16.0f + (float) j * 5.0f, r.getY() + 9.0f + (float) i * 5.0f, 2.4f, 2.4f);
        }

    // Mini-Pegel der Quelle
    auto bar = Rectangle<float> (r.getX() + 12.0f, r.getBottom() - 13.0f, r.getWidth() - 24.0f, 4.0f);
    g.setColour (Col::glassDeep);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (col);
    if (source == srcEnv)
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, level)), 2.0f);
    else
    {
        const float c = bar.getCentreX(), w = bar.getWidth() * 0.5f * jlimit (-1.0f, 1.0f, level);
        g.fillRoundedRectangle (Rectangle<float> (jmin (c, c + w), bar.getY(), std::abs (w) + 1.0f, bar.getHeight()), 2.0f);
    }
}

void ModSourceTab::mouseDown (const juce::MouseEvent&)
{
    if (onSelect)
        onSelect (source);
}

void ModSourceTab::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 5)
        return;
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr || container->isDragAndDropActive())
        return;

    // Ziehbild: leuchtende Pille mit dem Namen der Quelle
    const auto col = modSourceColour (source);
    juce::Image img (juce::Image::ARGB, 96, 30, true);
    {
        juce::Graphics g (img);
        const auto r = juce::Rectangle<float> (4.0f, 4.0f, 88.0f, 22.0f);
        g.setColour (col.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (3.0f), 14.0f);
        g.setColour (Col::glassDeep.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 11.0f);
        g.setColour (col);
        g.drawRoundedRectangle (r, 11.0f, 1.6f);
        g.setFont (display (14.0f, true));
        g.setColour (Col::ink);
        g.drawText (modSourceName (source), r, juce::Justification::centred);
    }
    container->startDragging ("mod:" + juce::String (source), this, juce::ScaledImage (img), false);
}

//==============================================================================
juce::Rectangle<float> CurveEditor::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (32.0f).withTrimmedBottom (14.0f).reduced (16.0f, 0.0f);
}

juce::Point<float> CurveEditor::toScreen (float x, float y) const
{
    const auto a = plotArea();
    return { a.getX() + x * a.getWidth(), a.getCentreY() - y * a.getHeight() * 0.5f };
}

juce::Point<float> CurveEditor::toCurve (juce::Point<float> s) const
{
    const auto a = plotArea();
    return { juce::jlimit (0.0f, 1.0f, (s.x - a.getX()) / a.getWidth()),
             juce::jlimit (-1.0f, 1.0f, (a.getCentreY() - s.y) / (a.getHeight() * 0.5f)) };
}

int CurveEditor::currentShape() const
{
    if (! isLfo())
        return 0;
    return (int) processor.apvts.getRawParameterValue (ParamIDs::lfoShape (lfoIndex() + 1))->load();
}

float CurveEditor::valueAt (float x) const
{
    const int shape = currentShape();
    if (shape == kShapeCustom)
        return processor.getLfoCurve (lfoIndex()).evaluatePoints (x);
    if (shape == 5) // S&H: vier Stufen als Beispiel
        return demoRandom ((int) (x * 4.0f) + 3) * 0.9f;
    if (shape == 6)
    {
        const float pos = x * 4.0f;
        const int i = (int) pos;
        const float t = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (pos - (float) i));
        return (demoRandom (i + 3) + (demoRandom (i + 4) - demoRandom (i + 3)) * t) * 0.9f;
    }
    return lfoShape (shape, x, 0.0f, 0.0f);
}

std::vector<CurveEditor::Point> CurveEditor::editablePoints()
{
    const int shape = currentShape();
    if (shape == kShapeCustom)
        return processor.getLfoCurve (lfoIndex()).getPoints();
    return LfoCurve::pointsForShape (shape);
}

void CurveEditor::commit (std::vector<Point> pts)
{
    processor.setLfoCurve (lfoIndex(), std::move (pts));
    if (currentShape() != kShapeCustom)
        if (auto* param = processor.apvts.getParameter (ParamIDs::lfoShape (lfoIndex() + 1)))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) kShapeCustom));
            param->endChangeGesture();
        }
    repaint();
}

std::vector<CurveEditor::Point> CurveEditor::shownPoints() const
{
    // Custom: die gezeichnete Kurve. Standardformen: ihre Punkte, damit man sie direkt weiterziehen kann.
    const int shape = currentShape();
    if (! isLfo() || shape == 5 || shape == 6)
        return {};
    if (shape == kShapeCustom)
        return processor.getLfoCurve (lfoIndex()).getPoints();
    return LfoCurve::pointsForShape (shape);
}

int CurveEditor::pointAt (juce::Point<float> pos) const
{
    const auto pts = shownPoints();
    for (int i = 0; i < (int) pts.size(); ++i)
        if (toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (pos) < 11.0f)
            return i;
    return -1;
}

int CurveEditor::segmentAt (juce::Point<float> pos) const
{
    const auto pts = shownPoints();
    const float x = toCurve (pos).x;
    for (int i = 0; i + 1 < (int) pts.size(); ++i)
        if (x >= pts[(size_t) i].x && x <= pts[(size_t) i + 1].x)
        {
            const float y = LfoCurve::evaluate (pts, x);
            return std::abs (toScreen (x, y).y - pos.y) < 16.0f ? i : -1;
        }
    return -1;
}

int CurveEditor::segmentIndexAtX (float x) const
{
    const auto pts = shownPoints();
    for (int i = 0; i + 1 < (int) pts.size(); ++i)
        if (x >= pts[(size_t) i].x && x <= pts[(size_t) i + 1].x)
            return i;
    return -1;
}

juce::Point<float> CurveEditor::snap (juce::Point<float> c, const juce::ModifierKeys& mods) const
{
    // Raster an (GRID), Strg/Cmd gedrückt = frei
    if (gridColumns > 0 && ! mods.isCommandDown())
    {
        c.x = std::round (c.x * (float) gridColumns) / (float) gridColumns;
        c.y = std::round (c.y * 4.0f) / 4.0f;
    }
    return c;
}

void CurveEditor::tick()
{
    if (source == srcEnv)
    {
        envHistory.push_back (processor.getEnvValue());
        while (envHistory.size() > 180)
            envHistory.pop_front();
    }
    repaint();
}

void CurveEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto bounds = getLocalBounds().toFloat();
    const auto col = modSourceColour (source);
    {
        // dunkles Glas ohne Glanzstreifen, damit die Kurve gut lesbar bleibt
        const auto r = bounds.reduced (0.5f);
        ColourGradient fill (Col::glassDeep.withAlpha (0.92f), 0.0f, r.getY(), Colour (0xff03070c).withAlpha (0.95f), 0.0f, r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Col::holo.withAlpha (0.06f));
        g.fillRect (r.getX() + 1.0f, r.getY() + 1.0f, r.getWidth() - 2.0f, 18.0f);
        g.setColour (Col::holo.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        holo::cornerBrackets (g, r, 6.0f, Col::holoBright.withAlpha (0.8f), 1.4f);
    }
    const auto a = plotArea();

    // Raster (GRID), Viertel hervorgehoben, Mittellinie
    {
        const int cols = isLfo() && gridColumns > 0 ? gridColumns : 16;
        for (int i = 0; i <= cols; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / (float) cols;
            const bool quarter = (i * 4) % cols == 0;
            g.setColour (Col::holo.withAlpha (quarter ? 0.16f : (gridColumns > 0 || ! isLfo() ? 0.07f : 0.03f)));
            g.fillRect (x - 0.5f, a.getY(), 1.0f, a.getHeight());
        }
    }
    for (float yy : { -1.0f, -0.75f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        g.setColour (Col::holo.withAlpha (yy == 0.0f ? 0.22f : (std::abs (yy) == 0.5f || std::abs (yy) == 1.0f ? 0.07f : 0.035f)));
        const float y = toScreen (0.0f, yy).y;
        g.fillRect (a.getX(), y - 0.5f, a.getWidth(), 1.0f);
    }

    g.setFont (mono (11.0f));
    if (! isLfo())
    {
        // Envelope Follower: Verlauf der letzten Sekunden
        Path p;
        const int n = (int) envHistory.size();
        if (n > 1)
        {
            p.startNewSubPath (a.getRight() - (float) (n - 1) / 179.0f * a.getWidth(), a.getBottom());
            for (int i = 0; i < n; ++i)
            {
                const float x = a.getRight() - (float) (n - 1 - i) / 179.0f * a.getWidth();
                p.lineTo (x, a.getBottom() - envHistory[(size_t) i] * a.getHeight());
            }
            p.lineTo (a.getRight(), a.getBottom());
            p.closeSubPath();
            ColourGradient fill (col.withAlpha (0.35f), 0.0f, a.getY(), col.withAlpha (0.02f), 0.0f, a.getBottom(), false);
            g.setGradientFill (fill);
            g.fillPath (p);
            g.setColour (col);
            g.strokePath (p, PathStrokeType (1.6f));
        }
        g.setColour (Col::muted);
        auto& apvts = processor.apvts;
        g.drawText ("ENV  //  ATTACK " + String (roundToInt (apvts.getRawParameterValue (ParamIDs::envAttack)->load())) + " MS  //  RELEASE "
                        + String (roundToInt (apvts.getRawParameterValue (ParamIDs::envRelease)->load())) + " MS",
                    Rectangle<float> (10.0f, 4.0f, bounds.getWidth() - 20.0f, 14.0f), Justification::centredLeft);
        return;
    }

    // LFO-Form
    const int shape = currentShape();
    Path curve;
    const int steps = 240;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        const auto pt = toScreen (x, valueAt (x));
        if (i == 0)
            curve.startNewSubPath (pt);
        else
            curve.lineTo (pt);
    }
    Path fillPath (curve);
    fillPath.lineTo (toScreen (1.0f, 0.0f));
    fillPath.lineTo (toScreen (0.0f, 0.0f));
    fillPath.closeSubPath();
    g.setColour (col.withAlpha (0.12f));
    g.fillPath (fillPath);
    g.setColour (col.withAlpha (0.25f));
    g.strokePath (curve, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (curve, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    // Punkte und Biege-Griffe: bei Custom die Kurve, bei Standardformen deren Punkte (Ziehen macht daraus Custom)
    {
        const auto pts = shownPoints();
        const float handleAlpha = shape == kShapeCustom ? 1.0f : 0.6f;
        for (int i = 0; i + 1 < (int) pts.size(); ++i)
        {
            const float mx = 0.5f * (pts[(size_t) i].x + pts[(size_t) i + 1].x);
            const auto c = toScreen (mx, LfoCurve::evaluate (pts, mx));
            const bool hot = i == hoverSegment || i == dragSegment;
            Path d;
            d.addPolygon (c, 4, hot ? 6.5f : 4.5f, MathConstants<float>::pi * 0.25f);
            g.setColour (hot ? Col::ink : col.withAlpha (0.6f * handleAlpha));
            g.fillPath (d);
        }
        for (int i = 0; i < (int) pts.size(); ++i)
        {
            const auto c = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
            const bool hot = i == hoverPoint || i == selectedPoint;
            g.setColour (col.withAlpha (0.3f * handleAlpha));
            g.fillEllipse (Rectangle<float> (hot ? 19.0f : 14.0f, hot ? 19.0f : 14.0f).withCentre (c));
            g.setColour (Col::glassDeep);
            g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (c));
            g.setColour (hot ? juce::Colours::white : col.withAlpha (handleAlpha));
            g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (c), 1.8f);
        }
    }

    // Abspielposition mit leuchtender Spur
    const float phase = processor.getLfoPhase (lfoIndex());
    {
        Path trail;
        const float len = 0.12f;
        for (int i = 0; i <= 24; ++i)
        {
            float x = phase - len + len * (float) i / 24.0f;
            if (x < 0.0f)
                continue;
            const auto pt = toScreen (x, valueAt (x));
            if (trail.isEmpty())
                trail.startNewSubPath (pt);
            else
                trail.lineTo (pt);
        }
        if (! trail.isEmpty())
        {
            g.setColour (col.withAlpha (0.25f));
            g.strokePath (trail, PathStrokeType (8.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (col.brighter (0.6f));
            g.strokePath (trail, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
    const auto head = toScreen (phase, valueAt (phase));
    g.setColour (Col::ink.withAlpha (0.25f));
    g.fillRect (head.x - 0.5f, a.getY(), 1.0f, a.getHeight());
    g.setColour (col.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (head));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (head));

    // Kopfzeile
    auto& apvts = processor.apvts;
    const int n = lfoIndex() + 1;
    const bool sync = apvts.getRawParameterValue (ParamIDs::lfoSync (n))->load() > 0.5f;
    const String rate = sync ? String (kSyncDivisions[(size_t) (int) apvts.getRawParameterValue (ParamIDs::lfoDiv (n))->load()].name)
                             : apvts.getParameter (ParamIDs::lfoRate (n))->getCurrentValueAsText();
    const int modeIndex = (int) apvts.getRawParameterValue (ParamIDs::lfoMode (n))->load();
    g.setColour (Col::muted);
    g.drawText ("LFO " + String (n) + "  //  " + rate.toUpperCase() + "  //  " + String (kLfoModeNames[(size_t) modeIndex]).toUpperCase(),
                Rectangle<float> (10.0f, 4.0f, bounds.getWidth() - 20.0f, 14.0f), Justification::centredLeft);
    g.setColour (Col::dim);
    g.drawText (de ("DOPPELKLICK = PUNKT  //  ALT+KLICK = LÖSCHEN  //  ALT+ZIEHEN = BIEGEN"), Rectangle<float> (10.0f, 4.0f, bounds.getWidth() - 20.0f, 14.0f), Justification::centredRight);

    // Wert beim Ziehen
    if (dragging && (selectedPoint >= 0 || dragSegment >= 0))
    {
        const auto pts = shownPoints();
        String text;
        juce::Point<float> at;
        if (selectedPoint >= 0 && selectedPoint < (int) pts.size())
        {
            const auto& p = pts[(size_t) selectedPoint];
            text = "X " + String (p.x, 3) + "   Y " + (p.y >= 0.0f ? "+" : "") + String (p.y, 2);
            at = toScreen (p.x, p.y);
        }
        else if (dragSegment >= 0 && dragSegment + 1 < (int) pts.size())
        {
            const float c = pts[(size_t) dragSegment].curve;
            text = "CURVE " + String (c >= 0.0f ? "+" : "") + String (roundToInt (c * 100.0f));
            const float mx = 0.5f * (pts[(size_t) dragSegment].x + pts[(size_t) dragSegment + 1].x);
            at = toScreen (mx, LfoCurve::evaluate (pts, mx));
        }
        if (text.isNotEmpty())
        {
            g.setFont (mono (11.5f));
            const float w = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 16.0f;
            auto box = Rectangle<float> (w, 20.0f).withCentre (at.translated (0.0f, at.y > a.getCentreY() ? -22.0f : 22.0f));
            box = box.constrainedWithin (bounds.reduced (4.0f));
            g.setColour (Col::glassDeep.withAlpha (0.92f));
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (col);
            g.drawRoundedRectangle (box, 3.0f, 1.0f);
            g.setColour (Col::ink);
            g.drawText (text, box, Justification::centred);
        }
    }
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const int p = pointAt (e.position);
    const int s = p < 0 ? (e.mods.isAltDown() ? segmentIndexAtX (toCurve (e.position).x) : segmentAt (e.position)) : -1;
    if (p != hoverPoint || s != hoverSegment)
    {
        hoverPoint = p;
        hoverSegment = s;
        repaint();
    }
    setMouseCursor (p >= 0 ? juce::MouseCursor::DraggingHandCursor
                           : (s >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::CrosshairCursor));
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! isLfo())
        return;
    if (e.mods.isPopupMenu())
    {
        showMenu();
        return;
    }
    selectedPoint = pointAt (e.position);
    dragPoints = editablePoints();
    if (e.mods.isAltDown() && selectedPoint >= 0)
    {
        // Alt+Klick auf einen Punkt: löschen (erster und letzter Punkt bleiben)
        if (selectedPoint > 0 && selectedPoint < (int) dragPoints.size() - 1)
        {
            dragPoints.erase (dragPoints.begin() + selectedPoint);
            commit (dragPoints);
        }
        selectedPoint = -1;
        hoverPoint = -1;
        dragSegment = -1;
        return;
    }
    // Alt+Ziehen irgendwo im Segment biegt es, ohne Alt nur direkt auf der Linie
    dragSegment = selectedPoint >= 0 ? -1 : (e.mods.isAltDown() ? segmentIndexAtX (toCurve (e.position).x) : segmentAt (e.position));
    if (dragSegment >= (int) dragPoints.size() - 1)
        dragSegment = -1;
    if (dragSegment >= 0)
        dragStartCurve = dragPoints[(size_t) dragSegment].curve;
    dragging = selectedPoint >= 0 || dragSegment >= 0;
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! isLfo() || e.mods.isPopupMenu())
        return;
    if (selectedPoint >= 0 && selectedPoint < (int) dragPoints.size())
    {
        const auto c = snap (toCurve (e.position), e.mods);
        auto& pt = dragPoints[(size_t) selectedPoint];
        const bool edge = selectedPoint == 0 || selectedPoint == (int) dragPoints.size() - 1;
        if (! edge)
        {
            const float lo = dragPoints[(size_t) selectedPoint - 1].x + 0.001f;
            const float hi = dragPoints[(size_t) selectedPoint + 1].x - 0.001f;
            pt.x = juce::jlimit (lo, hi, c.x);
        }
        pt.y = c.y;
        commit (dragPoints);
    }
    else if (dragSegment >= 0 && dragSegment + 1 < (int) dragPoints.size())
    {
        const auto& a = dragPoints[(size_t) dragSegment];
        const auto& b = dragPoints[(size_t) dragSegment + 1];
        const float dir = b.y >= a.y ? 1.0f : -1.0f;
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        dragPoints[(size_t) dragSegment].curve = juce::jlimit (-1.0f, 1.0f, dragStartCurve + dir * (float) e.getDistanceFromDragStartY() / 90.0f * fine);
        commit (dragPoints);
    }
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    dragSegment = -1;
    dragging = false;
    repaint();
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! isLfo())
        return;
    auto pts = editablePoints();
    const int hit = pointAt (e.position);
    if (hit > 0 && hit < (int) pts.size() - 1)
    {
        pts.erase (pts.begin() + hit); // Punkt löschen
        selectedPoint = -1;
    }
    else if (hit < 0)
    {
        auto c = snap (toCurve (e.position), e.mods);
        c.x = juce::jlimit (0.001f, 0.999f, c.x);
        for (const auto& p : pts)
            if (std::abs (p.x - c.x) < 0.0005f)
                return; // gleiche Stelle
        pts.push_back ({ c.x, c.y, 0.0f });
    }
    commit (pts);
}

void CurveEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("LFO " + juce::String (lfoIndex() + 1));
    juce::PopupMenu shapes;
    for (int s : { 0, 1, 2, 3, 4 })
        shapes.addItem (10 + s, kLfoShapeNames[(size_t) s]);
    menu.addSubMenu (de ("Form laden"), shapes);
    menu.addItem (1, de ("Spiegeln (links/rechts)"));
    menu.addItem (2, de ("Umdrehen (oben/unten)"));
    menu.addItem (3, de ("Alle Biegungen gerade"));
    menu.addSeparator();
    menu.addItem (4, de ("Zurücksetzen"));
    if (hoverPoint > 0 && hoverPoint < (int) processor.getLfoCurve (lfoIndex()).getPoints().size() - 1)
        menu.addItem (5, de ("Punkt löschen"));

    const int pointToDelete = hoverPoint;
    juce::Component::SafePointer<CurveEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, pointToDelete] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        auto pts = safe->editablePoints();
        if (result >= 10)
            pts = LfoCurve::pointsForShape (result - 10);
        else if (result == 1)
        {
            for (auto& p : pts)
                p.x = 1.0f - p.x;
            std::reverse (pts.begin(), pts.end());
            // Biegung gehört zum Segment davor: nach dem Spiegeln eins weiterschieben und umkehren
            for (size_t i = 0; i + 1 < pts.size(); ++i)
                pts[i].curve = -pts[i + 1].curve;
            pts.back().curve = 0.0f;
        }
        else if (result == 2)
            for (auto& p : pts)
                p.y = -p.y;
        else if (result == 3)
            for (auto& p : pts)
                p.curve = 0.0f;
        else if (result == 4)
            pts = LfoCurve::defaultPoints();
        else if (result == 5 && pointToDelete > 0 && pointToDelete < (int) pts.size() - 1)
            pts.erase (pts.begin() + pointToDelete);
        safe->commit (pts);
    });
}

juce::String CurveEditor::getTooltip()
{
    if (! isLfo())
        return de ("ENVELOPE FOLLOWER\nZeigt die Lautstärke des Eingangs. Zieh ENV links auf einen Regler, dann bewegen lautere Stellen ihn stärker.");
    if (hoverPoint >= 0)
        return de ("PUNKT\nZiehen: verschieben (rastet im GRID ein, Strg/Cmd = frei). Doppelklick oder Alt+Klick: löschen.");
    if (hoverSegment >= 0)
        return de ("KURVE\nHoch/runter ziehen (oder mit Alt irgendwo im Abschnitt): Segment biegen, Shift = fein. Doppelklick: neuer Punkt.");
    if (currentShape() == 5 || currentShape() == 6)
        return de ("LFO-FORM\nS&H und Smooth Random sind Zufall und lassen sich nicht zeichnen. Doppelklick oder Rechtsklick > Form laden: zeichenbare Form.");
    return de ("LFO-FORM\nJede Form ist zeichenbar: Punkte ziehen, Segmente hoch/runter biegen, Doppelklick setzt oder löscht einen Punkt. "
               "Rechtsklick: Formen laden, spiegeln, umdrehen. Sobald du eine Standardform änderst, wird daraus Custom.");
}

//==============================================================================
std::vector<int> ModSlotList::activeSlots() const
{
    std::vector<int> out;
    for (int i = 0; i < kNumModSlots; ++i)
        if ((int) processor.apvts.getRawParameterValue (ParamIDs::modSrc (i))->load() != srcOff)
            out.push_back (i);
    return out;
}

// Zwei Spalten mit je vier Zeilen
juce::Rectangle<float> ModSlotList::rowBounds (int row) const
{
    const float colW = ((float) getWidth() - 24.0f) * 0.5f;
    return { 8.0f + (float) (row / 4) * (colW + 8.0f), 26.0f + (float) (row % 4) * 24.0f, colW, 22.0f };
}

juce::Rectangle<float> ModSlotList::polarityBounds (int row) const
{
    const auto r = rowBounds (row);
    return { r.getRight() - 152.0f, r.getY() + 3.0f, 34.0f, 16.0f };
}

juce::Rectangle<float> ModSlotList::barBounds (int row) const
{
    const auto r = rowBounds (row);
    return { r.getRight() - 110.0f, r.getY() + 7.5f, 58.0f, 7.0f };
}

juce::Rectangle<float> ModSlotList::removeBounds (int row) const
{
    const auto r = rowBounds (row);
    return { r.getRight() - 16.0f, r.getY(), 16.0f, r.getHeight() };
}

int ModSlotList::rowAt (juce::Point<float> p) const
{
    const int n = (int) activeSlots().size();
    for (int row = 0; row < n; ++row)
        if (rowBounds (row).contains (p))
            return row;
    return -1;
}

void ModSlotList::paint (juce::Graphics& g)
{
    using namespace juce;
    holo::box (g, getLocalBounds().toFloat().reduced (0.5f), 1.0f, false);
    const auto slots = activeSlots();
    auto& apvts = processor.apvts;

    g.setFont (mono (11.0f));
    g.setColour (Col::holoBright);
    g.drawText ("// ROUTING", 10, 5, 120, 14, Justification::centredLeft);
    g.setColour (Col::dim);
    g.drawText (String ((int) slots.size()) + " / " + String (kNumModSlots), getWidth() - 70, 5, 60, 14, Justification::centredRight);

    if (slots.empty())
    {
        g.setFont (display (13.0f));
        g.setColour (Col::muted);
        g.drawFittedText (de ("Zieh LFO 1, LFO 2 oder ENV\nauf einen Regler."), getLocalBounds().withTrimmedTop (24), Justification::centred, 2);
        return;
    }

    for (int row = 0; row < (int) slots.size(); ++row)
    {
        const int slot = slots[(size_t) row];
        const int src = (int) apvts.getRawParameterValue (ParamIDs::modSrc (slot))->load();
        const int dst = (int) apvts.getRawParameterValue (ParamIDs::modDst (slot))->load();
        const float amt = apvts.getRawParameterValue (ParamIDs::modAmt (slot))->load();
        const auto col = modSourceColour (src);
        const auto r = rowBounds (row);

        if (row == hoverRow)
        {
            g.setColour (Col::holo.withAlpha (0.08f));
            g.fillRoundedRectangle (r, 2.0f);
        }
        g.setColour (col);
        g.fillEllipse (r.getX() + 2.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);
        g.setFont (display (12.5f, true));
        g.setColour (Col::ink);
        g.drawFittedText (modSourceName (src) + String::fromUTF8 (" → ") + String (kModTargetNames[(size_t) dst]).toUpperCase(),
                          r.withTrimmedLeft (14.0f).withTrimmedRight (156.0f).toNearestInt(), Justification::centredLeft, 1, 0.7f);

        // UNI / BI
        {
            const bool uni = (int) apvts.getRawParameterValue (ParamIDs::modPol (slot))->load() == 1;
            const auto pb = polarityBounds (row);
            const bool hot = pb.contains (getMouseXYRelative().toFloat()) && isMouseOver();
            g.setColour (col.withAlpha (hot ? 0.3f : 0.14f));
            g.fillRoundedRectangle (pb, 3.0f);
            g.setColour (col.withAlpha (hot ? 1.0f : 0.6f));
            g.drawRoundedRectangle (pb, 3.0f, 1.0f);
            g.setFont (mono (10.5f));
            g.setColour (Col::ink);
            g.drawText (uni ? "UNI" : "BI", pb, Justification::centred);
        }

        // Tiefe als bipolarer Balken
        const auto bar = barBounds (row);
        g.setColour (Col::glassDeep);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (Col::holoDim);
        g.fillRect (bar.getCentreX() - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
        const float w = bar.getWidth() * 0.5f * amt / 100.0f;
        g.setColour (col);
        g.fillRoundedRectangle (Rectangle<float> (jmin (bar.getCentreX(), bar.getCentreX() + w), bar.getY(), std::abs (w), bar.getHeight()), 2.0f);

        g.setFont (mono (11.0f));
        g.setColour (Col::muted);
        g.drawText ((amt > 0 ? "+" : "") + String (roundToInt (amt)), Rectangle<float> (bar.getRight() + 2.0f, r.getY(), 32.0f, r.getHeight()), Justification::centredRight);

        const auto x = removeBounds (row);
        g.setColour (row == hoverRow ? Col::pink : Col::dim);
        g.drawLine (x.getCentreX() - 3.5f, x.getCentreY() - 3.5f, x.getCentreX() + 3.5f, x.getCentreY() + 3.5f, 1.4f);
        g.drawLine (x.getCentreX() - 3.5f, x.getCentreY() + 3.5f, x.getCentreX() + 3.5f, x.getCentreY() - 3.5f, 1.4f);
    }
}

void ModSlotList::mouseDown (const juce::MouseEvent& e)
{
    const int row = rowAt (e.position);
    if (row < 0)
        return;
    const int slot = activeSlots()[(size_t) row];
    if (removeBounds (row).expanded (3.0f).contains (e.position) || e.mods.isPopupMenu())
    {
        processor.clearModSlot (slot);
        repaint();
        return;
    }
    if (polarityBounds (row).expanded (2.0f).contains (e.position))
    {
        if (auto* p = processor.apvts.getParameter (ParamIDs::modPol (slot)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
        repaint();
        return;
    }
    dragSlot = slot;
    dragStartAmt = processor.apvts.getRawParameterValue (ParamIDs::modAmt (slot))->load();
    if (auto* p = processor.apvts.getParameter (ParamIDs::modAmt (slot)))
        p->beginChangeGesture();
}

void ModSlotList::mouseDrag (const juce::MouseEvent& e)
{
    if (dragSlot < 0)
        return;
    const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
    const float amt = juce::jlimit (-100.0f, 100.0f, dragStartAmt + (float) e.getDistanceFromDragStartX() * 200.0f / 120.0f * fine);
    if (auto* p = processor.apvts.getParameter (ParamIDs::modAmt (dragSlot)))
        p->setValueNotifyingHost (p->convertTo0to1 (amt));
    repaint();
}

void ModSlotList::mouseUp (const juce::MouseEvent&)
{
    if (dragSlot >= 0)
        if (auto* p = processor.apvts.getParameter (ParamIDs::modAmt (dragSlot)))
            p->endChangeGesture();
    dragSlot = -1;
}

void ModSlotList::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int row = rowAt (e.position);
    if (row < 0 || polarityBounds (row).expanded (2.0f).contains (e.position))
        return;
    if (auto* p = processor.apvts.getParameter (ParamIDs::modAmt (activeSlots()[(size_t) row])))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (50.0f));
        p->endChangeGesture();
    }
    repaint();
}

juce::String ModSlotList::getTooltip()
{
    const auto mouse = getMouseXYRelative().toFloat();
    const int row = rowAt (mouse);
    if (row < 0)
        return de ("ROUTING\nAlle Zuweisungen. Neue Zuweisung: LFO 1, LFO 2 oder ENV auf einen Regler ziehen. Die Tiefe geht auch direkt am Regler über den Griff am farbigen Bogen.");
    if (polarityBounds (row).expanded (2.0f).contains (mouse))
        return de ("UNI / BI\nBipolar: die Quelle schwingt um den Reglerwert, nach oben und unten.\nUnipolar: nur in eine Richtung, ab dem Reglerwert (bei positiver Tiefe nach oben). Klick schaltet um.");
    return de ("ZUWEISUNG\nIrgendwo in der Zeile nach links/rechts ziehen: Tiefe (Shift = fein). Negative Werte drehen die Richtung um. "
               "Doppelklick: +50 %. × oder Rechtsklick: entfernen.");
}

//==============================================================================
ModPanel::ModPanel (PrizmAudioProcessor& p) : processor (p), editor (p), slots (p)
{
    auto& apvts = processor.apvts;
    for (int i = 0; i < 3; ++i)
    {
        tabs[(size_t) i] = std::make_unique<ModSourceTab> (srcLfo1 + i);
        tabs[(size_t) i]->onSelect = [this] (int s) { select (s); };
        addAndMakeVisible (*tabs[(size_t) i]);
    }
    tabs[0]->setTooltip (de ("LFO 1\nKlick: anzeigen und bearbeiten. Auf einen Regler ziehen: diesen Regler mit LFO 1 bewegen."));
    tabs[1]->setTooltip (de ("LFO 2\nKlick: anzeigen und bearbeiten. Auf einen Regler ziehen: diesen Regler mit LFO 2 bewegen."));
    tabs[2]->setTooltip (de ("ENV\nEnvelope Follower: folgt der Lautstärke des Eingangs. Auf einen Regler ziehen: lautere Stellen bewegen ihn stärker."));

    addAndMakeVisible (editor);
    addAndMakeVisible (slots);

    // Lauf-Modus je LFO und Trigger-Quelle, oben rechts in der Kurvenanzeige
    for (int k = 0; k < 2; ++k)
    {
        modeSwitch[(size_t) k] = std::make_unique<SegmentSwitch> (*apvts.getParameter (ParamIDs::lfoMode (k + 1)),
                                                                  juce::StringArray { "FREE", "RETRIG", "ENV" }, false);
        modeSwitch[(size_t) k]->setTooltip (de ("MODE\nFree: läuft durch, mit SYNC fest an der Songposition.\n"
                                                "Retrig: startet bei jedem Trigger von vorn und läuft weiter.\n"
                                                "Env: läuft bei jedem Trigger einmal durch und bleibt am Ende stehen, wie eine Hüllkurve.\n"
                                                "Was auslöst, stellt TRIG daneben ein."));
        addChildComponent (*modeSwitch[(size_t) k]);
    }
    trigSwitch = std::make_unique<SegmentSwitch> (*apvts.getParameter (ParamIDs::lfoTrig), juce::StringArray { "AUDIO", "MIDI", "BOTH" }, true, "TRIG ");
    trigSwitch->setTooltip (de ("TRIG\nWas Retrig und Env auslöst (gilt für beide LFOs). Klick schaltet weiter.\n"
                                "AUDIO: Anschläge im Eingang, z. B. jeder Bass- oder Drum-Hit.\n"
                                "MIDI: Noten am MIDI-Eingang von Prizm.\nBOTH: beides."));
    addChildComponent (*trigSwitch);

    // Raster der Kurvenanzeige (Bedien-Einstellung, wird im Projekt gemerkt)
    {
        const int saved = juce::jlimit (0, 4, (int) apvts.state.getProperty ("lfoGrid", 3));
        editor.setGrid (CurveEditor::kGridOptions[(size_t) saved]);
        gridSwitch = std::make_unique<SegmentSwitch> (juce::StringArray { "OFF", "1/4", "1/8", "1/16", "1/32" }, true, "GRID ", saved, [this] (int v)
        {
            editor.setGrid (CurveEditor::kGridOptions[(size_t) v]);
            processor.apvts.state.setProperty ("lfoGrid", v, nullptr);
        });
        gridSwitch->setTooltip (de ("GRID\nRaster zum Zeichnen: Punkte rasten waagrecht auf diese Teilung und senkrecht in Achteln ein. "
                                    "Klick schaltet weiter. Strg/Cmd beim Ziehen = frei."));
        addChildComponent (*gridSwitch);
    }

    auto setupKnob = [] (juce::Slider& s, const juce::String& tip)
    {
        s.setTooltip (tip);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 20);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        s.getProperties().set ("small", true);
    };

    for (int k = 0; k < 2; ++k)
    {
        auto& c = lfo[(size_t) k];
        const int n = k + 1;
        for (auto* s : kLfoShapeNames)
            c.shape.addItem (s, c.shape.getNumItems() + 1);
        c.shape.setTooltip (de ("SHAPE\nForm des LFO. Custom = deine gezeichnete Kurve, zeichnen direkt in der Anzeige."));
        c.shapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::lfoShape (n), c.shape);
        c.sync.setClickingTogglesState (true);
        c.sync.setTooltip (de ("SYNC\nAn: Rate im Songtempo (4 Takte bis 1/32) und fest an der Songposition. Aus: frei in Hz."));
        c.syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamIDs::lfoSync (n), c.sync);
        setupKnob (c.rate, de ("RATE\nGeschwindigkeit in Hz (SYNC aus)."));
        c.rateAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamIDs::lfoRate (n), c.rate);
        setupKnob (c.division, de ("RATE\nLänge eines Durchlaufs im Songtempo. T = Triole, Punkt = punktiert."));
        c.divAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamIDs::lfoDiv (n), c.division);
        for (juce::Component* comp : { (juce::Component*) &c.shape, (juce::Component*) &c.sync, (juce::Component*) &c.rate, (juce::Component*) &c.division })
            addChildComponent (comp);
    }

    setupKnob (envAttack, de ("ATTACK\nWie schnell ENV auf lautere Stellen reagiert."));
    setupKnob (envRelease, de ("RELEASE\nWie schnell ENV wieder fällt, wenn es leiser wird."));
    setupKnob (envSens, de ("SENS\nEmpfindlichkeit: höher = ENV schlägt schon bei leisen Signalen aus."));
    envAttackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamIDs::envAttack, envAttack);
    envReleaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamIDs::envRelease, envRelease);
    envSensAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamIDs::envSens, envSens);
    for (auto* s : { &envAttack, &envRelease, &envSens })
        addChildComponent (s);

    select (srcLfo1);
}

void ModPanel::select (int source)
{
    selected = source;
    for (int i = 0; i < 3; ++i)
        tabs[(size_t) i]->setSelected (srcLfo1 + i == source);
    editor.setSource (source);
    for (int k = 0; k < 2; ++k)
    {
        const bool show = source == srcLfo1 + k;
        lfo[(size_t) k].shape.setVisible (show);
        lfo[(size_t) k].sync.setVisible (show);
    }
    for (auto* s : { &envAttack, &envRelease, &envSens })
        s->setVisible (source == srcEnv);
    for (int k = 0; k < 2; ++k)
        modeSwitch[(size_t) k]->setVisible (source == srcLfo1 + k);
    trigSwitch->setVisible (source != srcEnv);
    gridSwitch->setVisible (source != srcEnv);
    updateRateVisibility();
    repaint();
}

void ModPanel::updateRateVisibility()
{
    for (int k = 0; k < 2; ++k)
    {
        auto& c = lfo[(size_t) k];
        const bool show = selected == srcLfo1 + k;
        const bool sync = c.sync.getToggleState();
        if (c.rate.isVisible() != (show && ! sync))
            c.rate.setVisible (show && ! sync);
        if (c.division.isVisible() != (show && sync))
            c.division.setVisible (show && sync);
        c.sync.setButtonText (sync ? "SYNC" : "FREE");
    }
}

void ModPanel::tick()
{
    tabs[0]->setLevel (processor.getLfoValue (0));
    tabs[1]->setLevel (processor.getLfoValue (1));
    tabs[2]->setLevel (processor.getEnvValue());
    for (int k = 0; k < 2; ++k)
    {
        const auto count = processor.getTrigCount (k);
        tabs[(size_t) k]->decayFlash();
        if (count != lastTrig[(size_t) k])
            tabs[(size_t) k]->flash();
        lastTrig[(size_t) k] = count;
    }
    if (selected != srcEnv) // TRIG nur wichtig, wenn der gezeigte LFO nicht frei läuft
    {
        const float alpha = modeSwitch[(size_t) (selected - srcLfo1)]->getValue() == lfoFree ? 0.45f : 1.0f;
        if (trigSwitch->getAlpha() != alpha)
            trigSwitch->setAlpha (alpha);
    }
    editor.tick();
    slots.repaint();
    updateRateVisibility();
}

void ModPanel::resized()
{
    // oben: Quellen-Reiter links, große Kurvenanzeige rechts; darunter Bedienung; unten Routing in zwei Spalten
    const int w = getWidth();
    const int tabW = 96, gap = 12, editorH = 180;
    for (int i = 0; i < 3; ++i)
        tabs[(size_t) i]->setBounds (0, i * 62, tabW, 56);

    const int cx = tabW + gap, cw = w - cx;
    editor.setBounds (cx, 0, cw, editorH);
    controlsRow = { cx, editorH + 12, cw, 44 };
    const int y = controlsRow.getY() + 16;
    for (auto& c : lfo)
    {
        c.shape.setBounds (cx, y, 140, 28);
        c.sync.setBounds (cx + 320, y, 64, 28);
        c.rate.setBounds (cx + 396, y - 3, 112, 34);
        c.division.setBounds (cx + 396, y - 3, 112, 34);
    }
    for (auto& m : modeSwitch)
        m->setBounds (cx + 152, y, 156, 28);
    trigSwitch->setBounds (w - 104, y, 104, 28);
    gridSwitch->setBounds (0, y, tabW, 28);

    const int third = cw / 3;
    envAttack.setBounds (cx, y - 3, third - 12, 34);
    envRelease.setBounds (cx + third, y - 3, third - 12, 34);
    envSens.setBounds (cx + 2 * third, y - 3, third - 12, 34);

    slots.setBounds (0, controlsRow.getBottom() + 10, w, getHeight() - controlsRow.getBottom() - 10);
}

void ModPanel::paint (juce::Graphics& g)
{
    using namespace juce;
    g.setFont (display (12.0f, true).withExtraKerningFactor (0.1f));
    g.setColour (Col::muted);
    const int x = controlsRow.getX(), y = controlsRow.getY();
    if (selected == srcEnv)
    {
        const int third = controlsRow.getWidth() / 3;
        g.drawText ("ATTACK", x, y, third, 14, Justification::centredLeft);
        g.drawText ("RELEASE", x + third, y, third, 14, Justification::centredLeft);
        g.drawText ("SENS", x + 2 * third, y, third, 14, Justification::centredLeft);
    }
    else
    {
        g.drawText ("SHAPE", x, y, 140, 14, Justification::centredLeft);
        g.drawText ("MODE", x + 152, y, 156, 14, Justification::centredLeft);
        g.drawText ("RATE", x + 320, y, 180, 14, Justification::centredLeft);
        g.drawText ("TRIGGER", getWidth() - 104, y, 104, 14, Justification::centredLeft);
        g.drawText ("DRAW", 0, y, 96, 14, Justification::centredLeft);
    }
}
