#pragma once

// Balken-Editor: ein Balken pro Echo-Step. Ziehen/Wischen setzt die Lautstärke,
// Rechtsklick schaltet stumm/an, Doppelklick setzt auf 80 %.

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "CyberLook.h"

class StepBars : public juce::Component
{
public:
    explicit StepBars (StepDelayProcessor& p) : proc (p)
    {
        setRepaintsOnMouseActivity (true);
        remembered.fill (0.8f);
    }

    // vom Editor-Timer aufgerufen
    void tick()
    {
        bool changed = false;
        for (int k = 0; k < StepDelayProcessor::kMaxSteps; ++k)
        {
            const float target = juce::jlimit (0.0f, 1.0f, proc.vizTap[(size_t) k].load (std::memory_order_relaxed) * 1.6f);
            auto& gl = glow[(size_t) k];
            const float nv = target > gl ? target : gl * 0.82f;
            if (std::abs (nv - gl) > 0.002f) changed = true;
            gl = nv;

            const float lv = proc.getLevel (k);
            if (std::abs (lv - shown[(size_t) k]) > 0.0001f) { shown[(size_t) k] = lv; changed = true; }
        }
        const int n = numSteps();
        if (n != lastN) { lastN = n; changed = true; }
        if (changed) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace sd;
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        const float u = getHeight() / 172.0f;   // Entwurf: Panel ist 172 Einheiten hoch
        neonPanel (g, r, u, col::green, 1.0f, 16.0f);

        const auto area = barArea();
        const int n = numSteps();
        const float w = area.getWidth() / (float) n;
        const float base = area.getBottom();

        // Kopfzeile: "/// 16 STEP PATTERN" links, "1/16 GRID ///" rechts
        const float ty = 8.0f * u, th = 14.0f * u;
        slashes (g, r.getX() + 22 * u, ty + 2 * u, th - 4 * u, 3, col::magenta, u);
        g.setFont (bb::fonts::display (13.5f * u, true));
        g.setColour (col::ink);
        const auto title = juce::String (n) + (n == 1 ? " STEP PATTERN" : " STEP PATTERN");
        g.drawText (title, juce::Rectangle<float> (r.getX() + 48 * u, ty, 300 * u, th), juce::Justification::centredLeft, false);
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title);
        const auto grid = proc.gridName().toUpperCase() + (proc.getParam (ParamIDs::sync) > 0.5f ? " GRID" : "");
        const float gw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), grid);
        const float gx = r.getRight() - 52 * u - gw;
        g.drawText (grid, juce::Rectangle<float> (gx, ty, gw + 4, th), juce::Justification::centredLeft, false);
        slashes (g, r.getRight() - 46 * u, ty + 2 * u, th - 4 * u, 3, col::magenta, u);
        g.setColour (col::magenta.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (r.getX() + 56 * u + tw, ty + th * 0.5f, gx - (r.getX() + 56 * u + tw) - 10 * u, 1.0f * u));

        // Wert beim Ziehen/Hovern mittig in der Linie
        if (isMouseOver() && juce::isPositiveAndBelow (hoverStep, n))
        {
            const float lv = proc.getLevel (hoverStep);
            const auto txt = " STEP " + juce::String (hoverStep + 1) + ": " + (lv < 0.001f ? juce::String ("STUMM") : juce::String (juce::roundToInt (lv * 100.0f)) + "% ");
            g.setFont (bb::fonts::mono (13.0f * u));
            const float vw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 10 * u;
            auto box = juce::Rectangle<float> (r.getCentreX() - vw * 0.5f, ty - 1 * u, vw, th + 2 * u);
            g.setColour (col::panel);
            g.fillRect (box);
            g.setColour (col::green);
            g.drawText (txt, box, juce::Justification::centred, false);
        }

        // Feld mit Spalten
        g.setColour (juce::Colour (0xff0a0510));
        g.fillRect (area);
        for (int k = 0; k < n; ++k)
        {
            const float x0 = area.getX() + w * (float) k;
            auto slot = juce::Rectangle<float> (x0, area.getY(), w, area.getHeight());
            juce::ColourGradient sg (juce::Colour (0xff1d1028), 0, slot.getY(), juce::Colour (0xff110818), 0, slot.getBottom(), false);
            g.setGradientFill (sg);
            g.fillRect (slot.reduced (1.0f * u, 0));
            if (k == hoverStep && isMouseOver())
            {
                g.setColour (juce::Colours::white.withAlpha (0.04f));
                g.fillRect (slot);
            }
        }
        g.setColour (juce::Colour (0xff050208));
        for (int k = 1; k < n; ++k)
            g.fillRect (juce::Rectangle<float> (area.getX() + w * (float) k - 1.0f * u, area.getY(), 2.0f * u, area.getHeight()));

        // Balken
        for (int k = 0; k < n; ++k)
        {
            const float x0 = area.getX() + w * (float) k;
            const float pad = std::max (1.5f * u, std::min (5.0f * u, w * 0.08f));
            auto slot = juce::Rectangle<float> (x0, area.getY(), w, area.getHeight()).reduced (pad, 0.0f);
            const float lv = proc.getLevel (k);
            const float gl = glow[(size_t) k];
            const bool hot = gl > 0.15f;

            if (lv > 0.001f)
            {
                auto bar = slot.withTop (base - area.getHeight() * lv).withBottom (base - 1.0f * u);
                const auto top = col::green.interpolatedWith (col::magenta, gl);
                const auto bot = col::greenDk.interpolatedWith (juce::Colour (0xffb0109c), gl);
                // Leuchten
                g.setColour (top.withAlpha (0.10f + 0.30f * gl));
                g.fillRect (bar.expanded (3.0f * u, 3.0f * u));
                juce::ColourGradient grad (top.brighter (0.15f), 0, bar.getY(), bot, 0, bar.getBottom(), false);
                grad.addColour (0.35, top);
                g.setGradientFill (grad);
                g.fillRect (bar);
                g.setColour (juce::Colours::white.withAlpha (0.35f + 0.4f * gl));
                g.fillRect (bar.withHeight (std::min (2.0f * u, bar.getHeight())));
                g.setColour (top.brighter (0.4f).withAlpha (0.6f));
                g.drawRect (bar, 1.0f);
            }
            else
            {
                const float cx = slot.getCentreX(), cy = base - 9.0f * u, s2 = std::min (3.5f * u, w * 0.2f);
                g.setColour (col::inkDim.withAlpha (0.45f));
                g.drawLine (cx - s2, cy - s2, cx + s2, cy + s2, 1.2f * u);
                g.drawLine (cx - s2, cy + s2, cx + s2, cy - s2, 1.2f * u);
            }

            // pinker Unterstrich beim gerade klingenden Step
            if (hot)
            {
                g.setColour (col::magenta.withAlpha (gl));
                g.fillRect (juce::Rectangle<float> (slot.getX(), base + 2.0f * u, slot.getWidth(), 2.0f * u));
            }

            // Step-Nummer
            if (w > 16.0f * u || k % 4 == 0)
            {
                g.setFont (bb::fonts::display (std::min (13.0f * u, w * 0.55f + 4.0f), true));
                g.setColour (hot ? col::magenta : col::inkDim);
                g.drawText (juce::String (k + 1), juce::Rectangle<float> (x0 - 4 * u, base + 6.0f * u, w + 8 * u, 15.0f * u),
                            juce::Justification::centred, false);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { hoverStep = stepAt (e.position.x); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hoverStep = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int k = stepAt (e.position.x);
        hoverStep = k;
        if (! juce::isPositiveAndBelow (k, numSteps())) return;

        if (e.mods.isPopupMenu())
        {
            // Rechtsklick: stumm <-> letzter Wert
            const float lv = proc.getLevel (k);
            if (lv > 0.001f) { remembered[(size_t) k] = lv; proc.setLevel (k, 0.0f); }
            else proc.setLevel (k, remembered[(size_t) k] > 0.001f ? remembered[(size_t) k] : 0.8f);
            dragging = false;
            return;
        }
        dragging = true;
        lastPos = e.position;
        setAt (e.position, e.mods);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        hoverStep = stepAt (e.position.x);
        if (! dragging) return;

        // Wischen: alle Steps zwischen letzter und aktueller Position mitnehmen
        const auto from = lastPos, to = e.position;
        const auto area = barArea();
        const float w = area.getWidth() / (float) numSteps();
        const int a = stepAt (from.x), b = stepAt (to.x);
        if (a != b && std::abs (to.x - from.x) > 0.5f)
        {
            const int dir = b > a ? 1 : -1;
            for (int k = a + dir; k != b; k += dir)
            {
                const float cx = area.getX() + w * ((float) k + 0.5f);
                const float t = (cx - from.x) / (to.x - from.x);
                setAt ({ cx, from.y + (to.y - from.y) * t }, e.mods);
            }
        }
        setAt (to, e.mods);
        lastPos = to;
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int k = stepAt (e.position.x);
        if (juce::isPositiveAndBelow (k, numSteps()))
            proc.setLevel (k, 0.8f);
    }

private:
    int numSteps() const { return juce::jlimit (1, StepDelayProcessor::kMaxSteps, (int) proc.getParam (ParamIDs::steps)); }

    juce::Rectangle<float> barArea() const
    {
        const float u = getHeight() / 172.0f;
        return getLocalBounds().toFloat().reduced (4.0f).withTrimmedTop (28.0f * u).withTrimmedBottom (28.0f * u).reduced (18.0f * u, 0.0f);
    }

    int stepAt (float x) const
    {
        const auto area = barArea();
        const int n = numSteps();
        return juce::jlimit (0, n - 1, (int) std::floor ((x - area.getX()) / area.getWidth() * (float) n));
    }

    void setAt (juce::Point<float> p, const juce::ModifierKeys& mods)
    {
        const auto area = barArea();
        const int k = stepAt (p.x);
        float v = juce::jlimit (0.0f, 1.0f, (area.getBottom() - p.y) / area.getHeight());
        if (v < 0.02f || mods.isAltDown()) v = 0.0f;               // ganz unten oder Alt = stumm
        if (mods.isShiftDown()) v = std::round (v * 8.0f) / 8.0f;  // Shift = in 12,5-%-Schritten
        if (std::abs (proc.getLevel (k) - v) > 0.0005f)
            proc.setLevel (k, v);
    }

    StepDelayProcessor& proc;
    std::array<float, StepDelayProcessor::kMaxSteps> glow {}, shown {}, remembered {};
    int hoverStep = -1, lastN = -1;
    bool dragging = false;
    juce::Point<float> lastPos;
};
