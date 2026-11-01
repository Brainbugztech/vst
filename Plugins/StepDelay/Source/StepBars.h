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

    // Zeichnet das Balkenfeld der Vorlage (Komponente = Feld, 260 Vorlage-Pixel hoch)
    void paint (juce::Graphics& g) override
    {
        using namespace sd;
        const float u = getHeight() / 260.0f;
        g.fillAll (col::field);

        // Scanner-Lichtbalken läuft auch durchs Feld (hinter den Balken)
        if (scanX > -9000.0f)
        {
            const float bw = 160.0f * u;
            juce::ColourGradient band (col::magenta.withAlpha (0.0f), scanX - bw, 0, col::magenta.withAlpha (0.0f), scanX + bw * 0.25f, 0, false);
            band.addColour (0.8, col::magenta.withAlpha (0.2f));
            g.setGradientFill (band);
            g.fillRect (juce::Rectangle<float> (scanX - bw, 0, bw * 1.25f, (float) getHeight()));
            g.setColour (col::magenta.withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> (scanX - 1.0f * u, 0, 2.0f * u, (float) getHeight()));
        }

        const auto area = barArea();
        const int n = numSteps();
        const float w = area.getWidth() / (float) n;
        const float base = area.getBottom();

        // Spalten mit feinen lila Trennlinien
        for (int k = 0; k < n; ++k)
        {
            auto slot = juce::Rectangle<float> (area.getX() + w * (float) k, 0.0f, w, base + 1.0f * u);
            juce::ColourGradient sg (col::slotTop.withAlpha (k == hoverStep && isMouseOver() ? 1.0f : 0.8f), 0, 0,
                                     col::slotBot, 0, base, false);
            g.setGradientFill (sg);
            g.fillRect (slot);
        }
        g.setColour (col::sep);
        for (int k = 0; k <= n; ++k)
            g.fillRect (juce::Rectangle<float> (area.getX() + w * (float) k - 0.6f * u, 0.0f, 1.2f * u, base));

        for (int k = 0; k < n; ++k)
        {
            const float x0 = area.getX() + w * (float) k;
            const float pad = std::max (1.5f * u, w * 0.085f);
            auto slot = juce::Rectangle<float> (x0, area.getY(), w, area.getHeight()).reduced (pad, 0.0f);
            const float lv = proc.getLevel (k);
            const float gl = glow[(size_t) k];

            if (lv > 0.001f)
            {
                auto bar = slot.withTop (base - area.getHeight() * lv).withBottom (base);
                const auto top = col::greenHi.interpolatedWith (col::magenta, gl);
                const auto mid = col::green.interpolatedWith (col::magenta, gl);
                const auto bot = col::greenDk.interpolatedWith (col::magentaDk, gl);

                // Leuchten (beim klingenden Step stark)
                for (int i = 3; i >= 1; --i)
                {
                    g.setColour (mid.withAlpha ((0.05f + 0.10f * gl) * (float) (4 - i) / 3.0f));
                    g.fillRect (bar.expanded ((float) i * (2.0f + 4.0f * gl) * u));
                }
                juce::ColourGradient grad (top, 0, bar.getY(), bot, 0, bar.getBottom(), false);
                grad.addColour (0.3, mid);
                g.setGradientFill (grad);
                g.fillRect (bar);
                // helle Kante und Schimmer oben
                g.setColour (top.brighter (0.3f).withAlpha (0.9f));
                g.drawRect (bar, 1.4f * u);
                juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.28f + 0.2f * gl), 0, bar.getY(),
                                            juce::Colours::white.withAlpha (0.0f), 0, bar.getY() + std::min (40.0f * u, bar.getHeight()), false);
                g.setGradientFill (sheen);
                g.fillRect (bar.reduced (1.4f * u, 0).withTrimmedTop (1.4f * u));
            }
            else
            {
                const float cx = slot.getCentreX(), cy = base - 12.0f * u, s2 = std::min (6.0f * u, w * 0.2f);
                g.setColour (col::inkDim.withAlpha (0.35f));
                g.drawLine (cx - s2, cy - s2, cx + s2, cy + s2, 2.0f * u);
                g.drawLine (cx - s2, cy + s2, cx + s2, cy - s2, 2.0f * u);
            }

            // pinker Unterstrich beim klingenden Step
            if (gl > 0.1f)
            {
                auto line = juce::Rectangle<float> (slot.getX(), base + 2.0f * u, slot.getWidth(), 3.0f * u);
                g.setColour (col::magenta.withAlpha (0.4f * gl));
                g.fillRect (line.expanded (0, 3.0f * u));
                g.setColour (col::magenta.brighter (0.3f).withAlpha (gl));
                g.fillRect (line);
            }

            // Step-Nummer
            if (w > 22.0f * u || k % 4 == 0)
            {
                g.setFont (font (std::min (30.0f * u, w * 0.6f + 6.0f * u)));
                g.setColour (col::lilac.interpolatedWith (col::magenta, juce::jlimit (0.0f, 1.0f, gl * 1.5f)));
                g.drawText (juce::String (k + 1), juce::Rectangle<float> (x0 - 10 * u, base + 8.0f * u, w + 20 * u, 30.0f * u),
                            juce::Justification::centred, false);
            }
        }

        // Wert beim Ziehen/Hovern oben rechts im Feld
        if (isMouseOver() && juce::isPositiveAndBelow (hoverStep, n))
        {
            const float lv = proc.getLevel (hoverStep);
            const auto txt = "STEP " + juce::String (hoverStep + 1) + ": " + (lv < 0.001f ? juce::String ("STUMM") : juce::String (juce::roundToInt (lv * 100.0f)) + "%");
            g.setFont (font (26.0f * u));
            const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 24 * u;
            auto box = juce::Rectangle<float> (area.getRight() - tw, 4 * u, tw, 30 * u);
            g.setColour (juce::Colour (0xe0070409));
            g.fillPath (chamfer (box, 6 * u));
            g.setColour (col::green);
            g.drawText (txt, box, juce::Justification::centred, false);
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
        const float u = getHeight() / 260.0f;
        return getLocalBounds().toFloat().withTrimmedTop (8.0f * u).withTrimmedBottom (40.0f * u).reduced (2.0f * u, 0.0f);
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
public:
    float scanX = -10000.0f;   // Position des Scanners in lokalen Pixeln (vom Editor gesetzt)
private:
    std::array<float, StepDelayProcessor::kMaxSteps> glow {}, shown {}, remembered {};
    int hoverStep = -1, lastN = -1;
    bool dragging = false;
    juce::Point<float> lastPos;
};
