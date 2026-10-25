#include "PitchMatrix.h"

#include <cmath>

using namespace prizm;

namespace
{
    juce::String de (const char* text) { return juce::String::fromUTF8 (text); }
    juce::String noteName (int pc) { return kNoteNames[(size_t) (((pc % 12) + 12) % 12)]; }

    juce::Colour kindColour (PitchMatrix::Route::Kind k)
    {
        switch (k)
        {
            case PitchMatrix::Route::pass:     return Col::lit;
            case PitchMatrix::Route::shift:    return Col::cyan;
            case PitchMatrix::Route::map:      return Col::pink;
            case PitchMatrix::Route::mute:     return Col::pink.withMultipliedSaturation (0.6f);
            case PitchMatrix::Route::filtered: return Col::muted;
        }
        return Col::muted;
    }

    void cross (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col, float thickness)
    {
        g.setColour (col);
        g.drawLine (c.x - r, c.y - r, c.x + r, c.y + r, thickness);
        g.drawLine (c.x - r, c.y + r, c.x + r, c.y - r, thickness);
    }
} // namespace

//==============================================================================
juce::Rectangle<float> PitchMatrix::laneBounds (int pc) const
{
    // gleiche Formel wie KeyStrip::cellBounds (Rand 10 px, Abstand 7 px)
    const auto area = getLocalBounds().toFloat().reduced (10.0f, 0.0f);
    const float gap = 7.0f;
    const float w = (area.getWidth() - gap * 11.0f) / 12.0f;
    return { area.getX() + (float) pc * (w + gap), area.getY(), w, area.getHeight() };
}

float PitchMatrix::laneX (float pos) const
{
    const auto a = laneBounds (0), b = laneBounds (1);
    return a.getCentreX() + pos * (b.getX() - a.getX());
}

int PitchMatrix::laneAt (float x) const
{
    for (int pc = 0; pc < 12; ++pc)
    {
        const auto r = laneBounds (pc);
        if (x >= r.getX() - 3.5f && x < r.getRight() + 3.5f)
            return pc;
    }
    return -1;
}

juce::Rectangle<float> PitchMatrix::outNode (int pc) const
{
    const auto lane = laneBounds (pc);
    return { lane.getX(), outTop() + 2.0f, lane.getWidth(), 26.0f };
}

juce::Path PitchMatrix::beamPath (float x0, float x1) const
{
    const float y0 = portY() + 4.0f, y1 = outTop() + 1.0f;
    const float dy = y1 - y0;
    juce::Path p;
    p.startNewSubPath (x0, y0);
    p.cubicTo (x0, y0 + dy * 0.55f, x1, y1 - dy * 0.55f, x1, y1);
    return p;
}

void PitchMatrix::setRoutes (const std::array<Route, 12>& newRoutes, bool isHighQuality, PitchMask allowedMask, PitchMask scaleMaskToUse)
{
    routes = newRoutes;
    hq = isHighQuality;
    allowed = allowedMask;
    scaleMask = scaleMaskToUse;
}

void PitchMatrix::setLevels (const std::array<float, 12>& in, const std::array<float, 12>& out, float animPhase)
{
    inLevel = in;
    outLevel = out;
    phase = animPhase;
    repaint();
}

//==============================================================================
void PitchMatrix::paint (juce::Graphics& g)
{
    using namespace juce;
    const float y0 = portY() + 4.0f, y1 = outTop() + 1.0f;

    // Spuren der Spalten
    for (int pc = 0; pc < 12; ++pc)
    {
        const float x = laneX ((float) pc);
        ColourGradient lane (Col::holo.withAlpha (pc == hoverLane ? 0.16f : 0.06f), x, y0, Col::holo.withAlpha (0.0f), x, y1, false);
        g.setGradientFill (lane);
        g.fillRect (x - 0.5f, y0, 1.0f, y1 - y0);
    }

    // Ankommende Noten pro Ausgang: die Farbe kommt vom Strahl, der gerade am stärksten klingt.
    // Ist alles still, gewinnt die eigene Note (geradeaus) vor Shift und Map.
    std::array<int, 12> arriving;
    arriving.fill (-1);
    std::array<float, 12> arriveWeight {}, arriveScore {};
    arriveScore.fill (-1.0f);
    for (int pc = 0; pc < 12; ++pc)
    {
        const auto& r = routes[(size_t) pc];
        if (r.kind == Route::mute || r.kind == Route::filtered || r.weight < 0.02f)
            continue;
        if (std::abs (r.targetPos - (float) r.target) > 0.05f && r.kind == Route::shift)
            continue; // Shift < 100 %: landet zwischen den Noten
        const int t = r.target;
        const float score = inLevel[(size_t) pc] + (r.kind == Route::pass ? 0.002f : 0.0f);
        if (score > arriveScore[(size_t) t])
        {
            arriveScore[(size_t) t] = score;
            arriving[(size_t) t] = (int) r.kind;
        }
        arriveWeight[(size_t) t] = jmax (arriveWeight[(size_t) t], r.weight);
    }

    // Strahlen: zuerst die schwachen, dann die starken obendrauf
    std::array<int, 12> order;
    for (int i = 0; i < 12; ++i)
        order[(size_t) i] = i;
    std::sort (order.begin(), order.end(), [this] (int a, int b) { return inLevel[(size_t) a] < inLevel[(size_t) b]; });

    for (int pc : order)
    {
        const auto& r = routes[(size_t) pc];
        const float e = jlimit (0.0f, 1.0f, inLevel[(size_t) pc]);
        const float x0 = laneX ((float) pc);
        const bool hot = pc == hoverLane || pc == dragFrom;

        // Original, das zusätzlich unverändert durchgeht (Strength / Mix unter 100 %)
        if (r.kind != Route::pass && r.dry > 0.03f)
        {
            g.setColour (Col::muted.withAlpha (jmin (0.5f, r.dry * 0.45f) * (0.4f + 0.6f * e)));
            float dashes[] = { 3.0f, 4.0f };
            g.drawDashedLine ({ x0, y0, x0, y1 }, dashes, 2, 1.0f);
        }

        // Map gesetzt, wirkt aber nicht (Zero Latency): gestrichelter Geist
        if (r.ghost >= 0)
        {
            Path ghostPath = beamPath (x0, laneX ((float) r.ghost));
            Path dashed;
            float dashes[] = { 4.0f, 4.0f };
            PathStrokeType (1.2f).createDashedStroke (dashed, ghostPath, dashes, 2);
            g.setColour (Col::pink.withAlpha (hot ? 0.55f : 0.3f));
            g.fillPath (dashed);
        }

        const float strength = (0.22f + 0.78f * e) * (0.35f + 0.65f * r.weight);
        const auto col = kindColour (r.kind);

        if (r.kind == Route::filtered || r.kind == Route::mute)
        {
            // verblasst auf halbem Weg, × markiert das Ende
            const float yEnd = y0 + (y1 - y0) * 0.5f;
            ColourGradient fade (juce::Colours::white.withAlpha (0.55f * strength), x0, y0, col.withAlpha (0.0f), x0, yEnd, false);
            g.setGradientFill (fade);
            g.fillRect (x0 - 1.0f, y0, 2.0f, yEnd - y0);
            const float crossA = r.kind == Route::mute ? 0.9f : 0.35f + 0.4f * e;
            cross (g, { x0, yEnd + 5.0f }, 3.5f, (r.kind == Route::mute ? Col::pink : Col::muted).withAlpha (crossA), 1.4f);
            if (r.kind == Route::mute)
            {
                g.setFont (bb::fonts::mono (9.5f));
                g.setColour (Col::pink.withAlpha (0.85f));
                g.drawText ("OFF", Rectangle<float> (x0 - 20.0f, yEnd + 11.0f, 40.0f, 12.0f), Justification::centred);
            }
            continue;
        }

        const float x1 = laneX (r.targetPos);
        const Path beam = beamPath (x0, x1);

        // Licht kommt weiß rein und wird zur Farbe der Aktion "gebrochen"
        ColourGradient grad (juce::Colours::white.withAlpha (0.9f * strength), x0, y0, col.withAlpha (strength), x1, y1, false);
        grad.addColour (0.35, col.interpolatedWith (juce::Colours::white, 0.45f).withAlpha (0.95f * strength));

        g.setColour (col.withAlpha (0.10f * strength + (hot ? 0.10f : 0.0f)));
        g.strokePath (beam, PathStrokeType (8.0f + 4.0f * e, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col.withAlpha (0.20f * strength));
        g.strokePath (beam, PathStrokeType (4.0f + 2.0f * e, PathStrokeType::curved, PathStrokeType::rounded));
        g.setGradientFill (grad);
        g.strokePath (beam, PathStrokeType (1.3f + 1.4f * e * (0.5f + 0.5f * r.weight), PathStrokeType::curved, PathStrokeType::rounded));

        // Lichtteilchen fließen nach unten, solange die Note klingt
        if (e > 0.04f)
        {
            const float len = beam.getLength();
            for (int k = 0; k < 3; ++k)
            {
                float t = std::fmod (phase * 0.55f + (float) k / 3.0f + (float) pc * 0.137f, 1.0f);
                const auto pt = beam.getPointAlongPath (len * t);
                const float a = e * (0.4f + 0.6f * r.weight) * std::sin (t * MathConstants<float>::pi);
                g.setColour (col.withAlpha (0.25f * a));
                g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (pt));
                g.setColour (juce::Colours::white.withAlpha (0.9f * a));
                g.fillEllipse (Rectangle<float> (3.2f, 3.2f).withCentre (pt));
            }
        }

        // Shift < 100 %: Ende zwischen zwei Noten markieren
        if (r.kind == Route::shift && std::abs (r.targetPos - (float) r.target) > 0.05f)
        {
            g.setColour (Col::cyan.withAlpha (0.8f * strength + 0.2f));
            g.drawEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ x1, y1 - 2.0f }), 1.4f);
        }
    }

    // Vorschau beim Ziehen
    if (dragging && dragFrom >= 0)
    {
        const float x0 = laneX ((float) dragFrom);
        Path p;
        p.startNewSubPath (x0, y0);
        const auto end = dragTarget >= 0 ? Point<float> (laneX ((float) dragTarget), y1) : dragPos;
        p.cubicTo (x0, y0 + (end.y - y0) * 0.55f, end.x, end.y - (end.y - y0) * 0.55f, end.x, end.y);
        Path dashed;
        float dashes[] = { 6.0f, 4.0f };
        PathStrokeType (2.0f).createDashedStroke (dashed, p, dashes, 2);
        g.setColour (Col::pink.withAlpha (0.25f));
        g.strokePath (p, PathStrokeType (8.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Col::pink);
        g.fillPath (dashed);
    }

    // Eingänge (oben): kleiner Ring, leuchtet mit dem was reinkommt
    for (int pc = 0; pc < 12; ++pc)
    {
        const float e = jlimit (0.0f, 1.0f, inLevel[(size_t) pc]);
        const auto c = Point<float> (laneX ((float) pc), portY() + 3.0f);
        const auto col = kindColour (routes[(size_t) pc].kind);
        if (e > 0.02f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f * e));
            g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (c));
        }
        g.setColour (Col::glassDeep);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c));
        g.setColour ((pc == hoverLane || pc == dragFrom) ? Col::holoBright : col.withAlpha (0.5f + 0.5f * e));
        g.drawEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c), 1.3f);
    }

    // Ausgänge (unten)
    for (int pc = 0; pc < 12; ++pc)
    {
        const auto r = outNode (pc);
        const float e = jlimit (0.0f, 1.0f, outLevel[(size_t) pc]);
        const int kind = arriving[(size_t) pc];
        const bool target = dragging && pc == dragTarget;

        if (e > 0.02f && kind >= 0)
            for (int i = 3; i >= 1; --i)
            {
                const float grow = (float) i * 2.4f;
                g.setColour (kindColour ((Route::Kind) kind).withAlpha (e * 0.12f / (float) i));
                g.fillRoundedRectangle (r.expanded (grow), 3.0f + grow);
            }

        const float shimmer = std::fmod (phase * 0.07f + (float) pc * 0.083f + 0.5f, 1.0f);
        if (kind >= 0)
        {
            const auto col = kindColour ((Route::Kind) kind);
            holo::glassTile (g, r, col, 0.25f + 0.45f * arriveWeight[(size_t) pc] + 0.3f * e, 0.15f + 0.6f * e, shimmer, 1.0f, 3.5f);
        }
        else
        {
            holo::glassTile (g, r, Col::holo, 0.0f, 0.0f, shimmer, 1.0f, 3.5f);
            if (! isAllowed (allowed, pc))
            {
                g.setColour (juce::Colours::black.withAlpha (0.3f));
                g.fillRoundedRectangle (r, 3.5f);
            }
        }

        if (target)
        {
            g.setColour (Col::pink);
            g.drawRoundedRectangle (r.expanded (1.5f), 4.0f, 2.0f);
        }

        g.setFont (bb::fonts::display (14.0f, true));
        g.setColour (kind >= 0 ? juce::Colours::white : Col::dim);
        g.drawText (noteName (pc), r, Justification::centred);
    }
}

//==============================================================================
void PitchMatrix::mouseMove (const juce::MouseEvent& e)
{
    const int lane = laneAt (e.position.x);
    if (lane != hoverLane)
    {
        hoverLane = lane;
        repaint();
    }
    setMouseCursor (lane >= 0 && e.position.y < outTop() ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void PitchMatrix::mouseExit (const juce::MouseEvent&)
{
    hoverLane = -1;
    repaint();
}

void PitchMatrix::mouseDown (const juce::MouseEvent& e)
{
    const int lane = laneAt (e.position.x);
    if (lane < 0)
        return;
    if (e.mods.isPopupMenu())
    {
        showMenu (lane);
        return;
    }
    if (e.position.y < outTop())
    {
        dragFrom = lane;
        dragTarget = -1;
        dragPos = e.position;
        dragging = false;
    }
}

void PitchMatrix::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0)
        return;
    if (! dragging && e.getDistanceFromDragStart() < 5)
        return;
    dragging = true;
    dragPos = e.position;
    dragTarget = e.position.y > outTop() - 14.0f ? laneAt (e.position.x) : -1;
    repaint();
}

void PitchMatrix::mouseUp (const juce::MouseEvent&)
{
    if (dragging && dragFrom >= 0 && dragTarget >= 0 && onSetMap)
        onSetMap (dragFrom, dragTarget == dragFrom ? 0 : dragTarget + 1);
    dragFrom = dragTarget = -1;
    dragging = false;
    repaint();
}

void PitchMatrix::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int lane = laneAt (e.position.x);
    if (lane >= 0 && onSetMap)
        onSetMap (lane, 0);
}

void PitchMatrix::showMenu (int pc)
{
    juce::PopupMenu menu;
    menu.addSectionHeader (noteName (pc) + de (" umleiten"));
    menu.addItem (1, de ("Normal (") + noteName (pc) + de (" bleibt ") + noteName (pc) + ")");
    menu.addSeparator();
    for (int t = 0; t < 12; ++t)
        if (t != pc)
            menu.addItem (t + 2, de ("→ ") + noteName (t) + (hq ? juce::String() : de ("   (braucht High Quality)")));
    menu.addSeparator();
    menu.addItem (14, de ("OFF (stummschalten)"));
    menu.addSeparator();
    menu.addItem (100, de ("Alle Noten zurücksetzen"));

    juce::Component::SafePointer<PitchMatrix> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (190),
                        [safe, pc] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result == 100)
                            {
                                if (safe->onResetAll)
                                    safe->onResetAll();
                                return;
                            }
                            if (safe->onSetMap)
                                safe->onSetMap (pc, result == 1 ? 0 : (result == 14 ? 13 : result - 1));
                        });
}

//==============================================================================
juce::String PitchMatrix::describe (int pc) const
{
    const auto& r = routes[(size_t) pc];
    const auto n = noteName (pc), t = noteName (r.target);
    juce::String title, body;
    switch (r.kind)
    {
        case Route::pass:
            title = n + de (" BLEIBT ") + n;
            body = n + (isAllowed (scaleMask, pc) ? de (" ist in der Tonart und kommt durch")
                                                  : de (" ist das Ziel einer Umleitung und darf deshalb auch selbst durch"))
                   + (r.weight < 0.99f ? de (", mit ") + juce::String (juce::roundToInt (r.weight * 100.0f)) + de (" % Gewichtung.") : de ("."));
            break;
        case Route::shift:
            if (std::abs (r.targetPos - (float) r.target) > 0.05f)
            {
                title = n + de (" → HALB RICHTUNG ") + t + de ("  (SHIFT)");
                body = n + de (" ist nicht in der Tonart. SHIFT zieht die Note ein Stück Richtung ") + t
                       + de (", sie landet zwischen den Noten. Mit SHIFT 100 % sitzt sie genau auf ") + t + ".";
            }
            else
            {
                title = n + de (" → ") + t + de ("  (SHIFT)");
                body = n + de (" ist nicht in der Tonart. SHIFT zieht die Note samt Obertönen auf die nächste erlaubte Note ") + t + ".";
            }
            break;
        case Route::map:
            title = n + de (" → ") + t + de ("  (MAP)");
            body = de ("Die gespielte Note ") + n + de (" landet auf ") + t + de (". Ihre Obertöne wandern mit, Obertöne anderer Noten bleiben stehen.");
            break;
        case Route::mute:
            title = n + de ("  OFF");
            body = n + de (" ist stummgeschaltet, in beiden Modi.");
            break;
        case Route::filtered:
            title = n + de (" WIRD GEFILTERT");
            body = r.weight < 0.02f && isAllowed (allowed, pc)
                       ? n + de (" ist erlaubt, steht aber auf 0 % Gewichtung.")
                       : n + de (" ist nicht in der Tonart und wird leiser gemacht.")
                             + (hq ? de (" Mit SHIFT wird die Note stattdessen auf die nächste erlaubte gezogen.")
                                   : de (" Im High-Quality-Modus kann SHIFT sie stattdessen auf die nächste erlaubte Note ziehen."));
            break;
    }
    if (r.ghost >= 0)
        body << de ("\nMap → ") << noteName (r.ghost) << de (" ist gesetzt, wirkt aber nur im High-Quality-Modus.");
    if (r.dry > 0.03f && r.kind != Route::pass)
        body << de ("\nDas Original klingt zu ") << juce::String (juce::roundToInt (r.dry * 100.0f)) << de (" % mit (STRENGTH / MIX).");
    body << de ("\nZiehen zu einer Note unten: umleiten. Rechtsklick: Menü. Doppelklick: normal.");
    return title + "\n" + body;
}

juce::String PitchMatrix::getTooltip()
{
    if (dragging)
        return {};
    const auto pos = getMouseXYRelative().toFloat();
    const int lane = laneAt (pos.x);
    if (lane < 0)
        return {};
    if (pos.y >= outTop())
    {
        const float e = outLevel[(size_t) lane];
        return noteName (lane) + de ("  AUSGANG\n")
               + (e > 0.05f ? de ("Diese Note kommt gerade raus. ") : juce::String())
               + de ("Hier landen die Strahlen von oben: grün = bleibt, cyan = SHIFT, pink = MAP. Zieh einen Strahl hierher, um eine Note umzuleiten.");
    }
    return describe (lane);
}
