#pragma once

// Balken-Editor: ein Balken pro Echo-Step. Ziehen/Wischen setzt die Lautstärke,
// Rechtsklick schaltet stumm/an, Doppelklick setzt auf 80 %.

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"

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
        using namespace bb;
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        const auto area = barArea();
        const int n = numSteps();
        const float w = area.getWidth() / (float) n;

        // Hilfslinien bei 25/50/75 %
        g.setColour (col::line.withAlpha (0.6f));
        for (float f : { 0.25f, 0.5f, 0.75f })
        {
            const float y = area.getBottom() - area.getHeight() * f;
            for (float x = area.getX(); x < area.getRight(); x += 6.0f)
                g.drawLine (x, y, std::min (x + 2.0f, area.getRight()), y, 1.0f);
        }

        const float u = getHeight() / 280.0f;
        for (int k = 0; k < n; ++k)
        {
            const float x0 = area.getX() + w * (float) k;
            auto slot = juce::Rectangle<float> (x0, area.getY(), w, area.getHeight()).reduced (std::min (4.0f, w * 0.14f), 0.0f);
            const float lv = proc.getLevel (k);
            const float gl = glow[(size_t) k];
            const bool hover = isMouseOver() && k == hoverStep;

            // Slot-Hintergrund, jeder 4. Step etwas heller (Taktgefühl)
            g.setColour ((k % 4 == 0 ? col::panel2.brighter (0.08f) : col::panel2).withAlpha (hover ? 1.0f : 0.85f));
            g.fillRoundedRectangle (slot, 3.0f);

            if (lv > 0.001f)
            {
                auto bar = slot.withTop (slot.getBottom() - slot.getHeight() * lv);
                const auto c = col::tox.interpolatedWith (col::brain, gl);
                g.setColour (c.withAlpha (0.18f + 0.35f * gl));
                g.fillRoundedRectangle (bar.expanded (2.0f + 4.0f * gl, 2.0f + 4.0f * gl), 4.0f);
                juce::ColourGradient grad (c.brighter (0.25f), 0, bar.getY(), c.darker (0.55f), 0, bar.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (juce::Colours::white.withAlpha (0.55f + 0.45f * gl));
                g.fillRect (bar.withHeight (std::min (2.5f, bar.getHeight())));
            }
            else
            {
                // stummer Step: kleines X
                const float cx = slot.getCentreX(), cy = slot.getBottom() - 10.0f * u, s = std::min (4.0f * u, w * 0.2f);
                g.setColour (col::inkDim.withAlpha (0.5f));
                g.drawLine (cx - s, cy - s, cx + s, cy + s, 1.2f);
                g.drawLine (cx - s, cy + s, cx + s, cy - s, 1.2f);
            }

            // Step-Nummer
            if (w > 14.0f || k % 4 == 0)
            {
                g.setFont (fonts::mono (std::min (13.0f * u, w * 0.62f + 4.0f)));
                g.setColour (k % 4 == 0 ? col::ink : col::inkDim);
                g.drawText (juce::String (k + 1), juce::Rectangle<float> (x0 - 6.0f, area.getBottom() + 3.0f, w + 12.0f, 16.0f * u),
                            juce::Justification::centredTop, false);
            }
        }

        // Wert beim Ziehen/Hovern
        if (isMouseOver() && juce::isPositiveAndBelow (hoverStep, n))
        {
            const float lv = proc.getLevel (hoverStep);
            const auto txt = "Step " + juce::String (hoverStep + 1) + ": " + (lv < 0.001f ? juce::String ("stumm") : juce::String (juce::roundToInt (lv * 100.0f)) + "%");
            g.setFont (fonts::mono (14.0f * u));
            g.setColour (col::tox);
            g.drawText (txt, juce::Rectangle<float> (area.getX(), 4.0f, area.getWidth(), 18.0f * u), juce::Justification::centredRight, false);
        }
        g.setFont (fonts::display (14.0f * u, true));
        g.setColour (col::inkDim);
        g.drawText ("ECHO-STEPS", juce::Rectangle<float> (area.getX(), 4.0f, 200.0f, 18.0f * u), juce::Justification::centredLeft, false);
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
        const float u = getHeight() / 280.0f;
        return getLocalBounds().toFloat().withTrimmedTop (28.0f * u).withTrimmedBottom (24.0f * u).reduced (12.0f * u, 0.0f);
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
