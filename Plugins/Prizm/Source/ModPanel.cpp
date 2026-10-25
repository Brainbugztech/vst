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
void ModSourceTab::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = modSourceColour (source);
    holo::box (g, r, 1.0f, selected || isMouseOver());
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
    return getLocalBounds().toFloat().reduced (12.0f, 22.0f).withTrimmedBottom (-10.0f);
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

int CurveEditor::pointAt (juce::Point<float> pos) const
{
    if (! isLfo() || currentShape() != kShapeCustom)
        return -1;
    const auto& pts = processor.getLfoCurve (lfoIndex()).getPoints();
    for (int i = 0; i < (int) pts.size(); ++i)
        if (toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (pos) < 8.0f)
            return i;
    return -1;
}

int CurveEditor::segmentAt (juce::Point<float> pos) const
{
    if (! isLfo() || currentShape() != kShapeCustom)
        return -1;
    const auto& pts = processor.getLfoCurve (lfoIndex()).getPoints();
    const float x = toCurve (pos).x;
    for (int i = 0; i + 1 < (int) pts.size(); ++i)
        if (x >= pts[(size_t) i].x && x <= pts[(size_t) i + 1].x)
        {
            const float y = processor.getLfoCurve (lfoIndex()).evaluatePoints (x);
            return std::abs (toScreen (x, y).y - pos.y) < 14.0f ? i : -1;
        }
    return -1;
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
    holo::box (g, bounds.reduced (0.5f), 1.0f, false);
    const auto a = plotArea();

    // Raster: 16 Spalten (Viertel hervorgehoben), Mittellinie
    for (int i = 0; i <= 16; ++i)
    {
        const float x = a.getX() + a.getWidth() * (float) i / 16.0f;
        g.setColour (Col::holo.withAlpha (i % 4 == 0 ? 0.16f : 0.06f));
        g.fillRect (x - 0.5f, a.getY(), 1.0f, a.getHeight());
    }
    for (float yy : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
    {
        g.setColour (Col::holo.withAlpha (yy == 0.0f ? 0.2f : 0.06f));
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

    // Punkte und Biege-Griffe (nur bei gezeichneter Kurve)
    if (shape == kShapeCustom)
    {
        const auto& pts = processor.getLfoCurve (lfoIndex()).getPoints();
        for (int i = 0; i + 1 < (int) pts.size(); ++i)
        {
            const float mx = 0.5f * (pts[(size_t) i].x + pts[(size_t) i + 1].x);
            const auto c = toScreen (mx, processor.getLfoCurve (lfoIndex()).evaluatePoints (mx));
            const bool hot = i == hoverSegment || i == dragSegment;
            Path d;
            d.addPolygon (c, 4, hot ? 5.0f : 3.5f, MathConstants<float>::pi * 0.25f);
            g.setColour (hot ? Col::ink : col.withAlpha (0.6f));
            g.fillPath (d);
        }
        for (int i = 0; i < (int) pts.size(); ++i)
        {
            const auto c = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
            const bool hot = i == hoverPoint || i == selectedPoint;
            g.setColour (col.withAlpha (0.3f));
            g.fillEllipse (Rectangle<float> (hot ? 14.0f : 10.0f, hot ? 14.0f : 10.0f).withCentre (c));
            g.setColour (Col::glassDeep);
            g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c));
            g.setColour (hot ? Col::ink : col);
            g.drawEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c), 1.6f);
        }
    }

    // Abspielposition
    const float phase = processor.getLfoPhase (lfoIndex());
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
    g.setColour (Col::muted);
    g.drawText ("LFO " + String (n) + "  //  " + String (kLfoShapeNames[(size_t) shape]).toUpperCase() + "  //  " + rate.toUpperCase(),
                Rectangle<float> (10.0f, 4.0f, bounds.getWidth() - 20.0f, 14.0f), Justification::centredLeft);
    g.setColour (Col::dim);
    g.drawText (shape == kShapeCustom ? de ("RECHTSKLICK = MENÜ") : de ("ZUM ZEICHNEN DOPPELKLICKEN"),
                Rectangle<float> (10.0f, 4.0f, bounds.getWidth() - 20.0f, 14.0f), Justification::centredRight);
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const int p = pointAt (e.position);
    const int s = p < 0 ? segmentAt (e.position) : -1;
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
    dragSegment = selectedPoint < 0 ? segmentAt (e.position) : -1;
    dragPoints = editablePoints();
    if (dragSegment >= 0)
        dragStartCurve = dragPoints[(size_t) dragSegment].curve;
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! isLfo() || e.mods.isPopupMenu())
        return;
    if (selectedPoint >= 0 && selectedPoint < (int) dragPoints.size())
    {
        auto c = toCurve (e.position);
        if (! e.mods.isAltDown()) // Raster: 1/16 waagrecht, Viertel senkrecht
        {
            c.x = std::round (c.x * 16.0f) / 16.0f;
            c.y = std::round (c.y * 4.0f) / 4.0f;
        }
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
        auto c = toCurve (e.position);
        if (! e.mods.isAltDown())
        {
            c.x = std::round (c.x * 16.0f) / 16.0f;
            c.y = std::round (c.y * 4.0f) / 4.0f;
        }
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
        return de ("PUNKT\nZiehen: verschieben (rastet ein, Alt = frei). Doppelklick: löschen.");
    if (hoverSegment >= 0)
        return de ("KURVE\nHoch/runter ziehen: Segment biegen (Shift = fein). Doppelklick: neuer Punkt.");
    return de ("LFO-FORM\nDoppelklick: Punkt setzen. Punkte ziehen, Segmente biegen. Rechtsklick: Formen laden, spiegeln, umdrehen. "
               "Sobald du zeichnest, springt SHAPE auf Custom.");
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

juce::Rectangle<float> ModSlotList::rowBounds (int row) const
{
    return { 8.0f, 24.0f + (float) row * 19.0f, (float) getWidth() - 16.0f, 17.0f };
}

juce::Rectangle<float> ModSlotList::barBounds (int row) const
{
    const auto r = rowBounds (row);
    return { r.getRight() - 100.0f, r.getY() + 5.0f, 50.0f, 7.0f };
}

juce::Rectangle<float> ModSlotList::removeBounds (int row) const
{
    const auto r = rowBounds (row);
    return { r.getRight() - 14.0f, r.getY(), 14.0f, r.getHeight() };
}

int ModSlotList::rowAt (int y) const
{
    const int row = (int) std::floor (((float) y - 24.0f) / 19.0f);
    return row >= 0 && row < (int) activeSlots().size() ? row : -1;
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
                          r.withTrimmedLeft (14.0f).withTrimmedRight (104.0f).toNearestInt(), Justification::centredLeft, 1, 0.7f);

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
        g.drawText ((amt > 0 ? "+" : "") + String (roundToInt (amt)), Rectangle<float> (bar.getRight() + 2.0f, r.getY(), 34.0f, r.getHeight()), Justification::centredRight);

        const auto x = removeBounds (row);
        g.setColour (row == hoverRow ? Col::pink : Col::dim);
        g.drawLine (x.getCentreX() - 3.5f, x.getCentreY() - 3.5f, x.getCentreX() + 3.5f, x.getCentreY() + 3.5f, 1.4f);
        g.drawLine (x.getCentreX() - 3.5f, x.getCentreY() + 3.5f, x.getCentreX() + 3.5f, x.getCentreY() - 3.5f, 1.4f);
    }
}

void ModSlotList::mouseDown (const juce::MouseEvent& e)
{
    const int row = rowAt (e.y);
    if (row < 0)
        return;
    const int slot = activeSlots()[(size_t) row];
    if (removeBounds (row).expanded (3.0f).contains (e.position) || e.mods.isPopupMenu())
    {
        processor.clearModSlot (slot);
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
    const int row = rowAt (e.y);
    if (row < 0)
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
    const int row = rowAt (getMouseXYRelative().y);
    if (row < 0)
        return de ("ROUTING\nAlle Zuweisungen. Neue Zuweisung: LFO 1, LFO 2 oder ENV links auf einen Regler ziehen.");
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
    editor.tick();
    slots.repaint();
    updateRateVisibility();
}

void ModPanel::resized()
{
    const int w = getWidth();
    const int tabW = 96, listW = 262, gap = 12;
    for (int i = 0; i < 3; ++i)
        tabs[(size_t) i]->setBounds (0, i * 58, tabW, 52);

    const int cx = tabW + gap, cw = w - tabW - listW - 2 * gap;
    editor.setBounds (cx, 0, cw, 110);
    controlsRow = { cx, 116, cw, 58 };
    slots.setBounds (w - listW, 0, listW, 174);

    const int y = controlsRow.getY() + 16;
    for (auto& c : lfo)
    {
        c.shape.setBounds (cx, y + 4, 132, 28);
        c.sync.setBounds (cx + 140, y + 4, 66, 28);
        c.rate.setBounds (cx + 216, y - 2, cw - 216, 42);
        c.division.setBounds (cx + 216, y - 2, cw - 216, 42);
    }
    const int third = cw / 3;
    envAttack.setBounds (cx, y - 2, third - 6, 42);
    envRelease.setBounds (cx + third, y - 2, third - 6, 42);
    envSens.setBounds (cx + 2 * third, y - 2, third - 6, 42);
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
        g.drawText ("SHAPE", x, y, 130, 14, Justification::centredLeft);
        g.drawText ("RATE", x + 216, y, 120, 14, Justification::centredLeft);
    }
}
