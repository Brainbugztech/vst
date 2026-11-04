#pragma once

// Bausteine der Braincut-Oberfläche: Regler, Wertebalken und die Slice-Anzeige

#include <juce_audio_processors/juce_audio_processors.h>
#include "Look.h"
#include "PluginProcessor.h"

namespace bcui
{

// Drehregler mit Wert und Name darunter
class KnobBox : public juce::Component
{
public:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    KnobBox (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& label, juce::Colour accentColour)
        : name (label), accent (accentColour)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (bb::LookAndFeel::startAngle, bb::LookAndFeel::endAngle, true);
        slider.setMouseDragSensitivity (220);
        slider.setVelocityBasedMode (false);
        slider.setTitle (label);
        setAccent (slider, accentColour);
        addAndMakeVisible (slider);
        attachment = std::make_unique<SA> (apvts, paramId, slider);
        slider.onValueChange = [this] { repaint(); };
        setTitle (label);
    }

    void resized() override
    {
        const int k = juce::jmin (getWidth(), getHeight() - kLabelH);
        slider.setBounds ((getWidth() - k) / 2, 0, k, k);
    }

    void paint (juce::Graphics& g) override
    {
        g.setFont (bb::fonts::mono (12.0f));
        g.setColour (col::ink);
        g.drawText (slider.getTextFromValue (slider.getValue()), 0, getHeight() - kLabelH, getWidth(), 14, juce::Justification::centred);
        g.setFont (bb::fonts::display (12.5f, true));
        g.setColour (accent.withAlpha (0.92f));
        g.drawText (name, 0, getHeight() - kLabelH + 14, getWidth(), 16, juce::Justification::centred);
    }

    static constexpr int kLabelH = 30;
    juce::Slider slider;

private:
    juce::String name;
    juce::Colour accent;
    std::unique_ptr<SA> attachment;
};

// Schmaler Wertebalken (z. B. MAX PHRASE 4 BAR); Klick/Ziehen setzt den Wert
class ValueBar : public juce::Slider
{
public:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    ValueBar (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& label,
              juce::Colour accent, const juce::String& unit)
    {
        setSliderStyle (juce::Slider::LinearBar);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setTitle (label);
        setAccent (*this, accent);
        getProperties().set ("lbl", label);
        attachment = std::make_unique<SA> (apvts, paramId, *this);
        // ohne Einheit zeigt der Balken den Text des Parameters (z. B. "1 Bar", "1/16")
        if (unit.isNotEmpty())
            textFromValueFunction = [unit] (double v) { return juce::String ((int) std::lround (v)) + " " + unit; };
        onValueChange = [this] { repaint(); };
    }

private:
    std::unique_ptr<SA> attachment;
};

// Step-Raster: 4 Zeilen = 4 Takte, jede Spalte ein Schritt (Subdivision). Jeder Balken zeigt, von welcher Stelle
// im Loop gerade gelesen wurde (Höhe und Farbe = Position im Loop): unverändertes Abspielen ergibt eine
// gleichmäßige Treppe, Schnitte brechen sie. Außerhalb der Dichte-Zonen bleiben die Balken dunkel (Original).
// Merker: Dreieck = rückwärts, Querstreifen = gestreckt, Punkt unten = Repeat, heller Block oben = Glitch.
// Unter jeder Zeile zeigt ein dünner Streifen, wo die Dichte gerade Effekte erlaubt.
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (BraincutProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        using E = bc::BraincutEngine;
        auto& e = proc.engine;
        auto b = getLocalBounds().toFloat();

        // Glasfläche
        const auto shape = chamfer (b, 12.0f, true, true);
        g.setColour (col::glass.withAlpha (0.66f));
        g.fillPath (shape);
        g.setColour (col::frame.withAlpha (0.8f));
        g.strokePath (shape, juce::PathStrokeType (1.0f));

        const bool playing = e.vizPlaying.load (std::memory_order_relaxed);
        const int cols = std::max (1, e.vizCols.load (std::memory_order_relaxed));
        const int sb = std::max (1, e.vizSteps.load (std::memory_order_relaxed));
        const int M = std::max (1, e.vizLoopSteps.load (std::memory_order_relaxed));
        const float bpb = e.vizBpb.load (std::memory_order_relaxed);
        const float cellPos = e.vizCellPos.load (std::memory_order_relaxed);
        const int curCell = (int) cellPos;
        const double winStart = e.vizWindowStart.load (std::memory_order_relaxed);

        const auto prm = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const int dens = (int) std::lround (prm (ParamIDs::density));
        const int seed = (int) std::lround (prm (ParamIDs::seed));
        const bool lock = prm (ParamIDs::lock) > 0.5f;

        // Kopfzeile
        static const char* densNames[3] { "DECENT", "MEDIUM", "NUTS" };
        static const char* densInfo[3] { "FILL EVERY 4 BARS", "FILL EVERY 2 BARS + SHOTS", "EVERY BEAT" };
        const int di = juce::jlimit (0, 2, dens);
        g.setFont (bb::fonts::display (11.0f, true));
        g.setColour (col::ink);
        g.drawText (densNames[di], (int) b.getX() + 14, (int) b.getY() + 6, 70, 16, juce::Justification::centredLeft);
        g.setFont (bb::fonts::mono (10.5f));
        g.setColour (col::inkDim);
        g.drawText (densInfo[di], (int) b.getX() + 72, (int) b.getY() + 6, (int) b.getWidth() - 84, 16, juce::Justification::centredRight);

        // Raster
        const float gx = b.getX() + 22.0f, gw = b.getWidth() - 34.0f;
        const float gy = b.getY() + 28.0f, rowH = (b.getHeight() - 28.0f - 30.0f) / (float) E::kVizRows;
        const float cw = gw / (float) cols;
        const float gap = cw > 5.0f ? 1.0f : 0.0f;
        const float barH = rowH - 10.0f;

        for (int row = 0; row < E::kVizRows; ++row)
        {
            const float ry = gy + (float) row * rowH;
            g.setFont (bb::fonts::mono (10.5f));
            g.setColour (col::inkDim.withAlpha (0.8f));
            g.drawText (juce::String (row + 1), (int) b.getX() + 6, (int) ry, 14, (int) barH, juce::Justification::centred);

            // Takt-Gitter (Beats)
            for (int c = 0; c <= cols; c += sb)
            {
                g.setColour (col::frame.withAlpha (c % (sb * 4) == 0 ? 0.35f : 0.18f));
                g.fillRect (gx + (float) c * cw - 0.5f, ry, 1.0f, barH);
            }

            for (int c = 0; c < cols; ++c)
            {
                const int idx = row * cols + c;
                const uint8_t f = e.vizCellFlags[(size_t) idx].load (std::memory_order_relaxed);
                if ((f & E::cfPlayed) == 0)
                    continue;
                const int src = e.vizCellSrc[(size_t) idx].load (std::memory_order_relaxed);
                const float t = juce::jlimit (0.0f, 1.0f, ((float) src + 0.5f) / (float) M);
                const bool zone = (f & E::cfZone) != 0;
                const bool old = playing ? idx > curCell : true;   // noch nicht gespielt: Stand der letzten Runde
                const float fade = old ? 0.38f : 1.0f;

                const float x = gx + (float) c * cw + gap * 0.5f, w = std::max (1.0f, cw - gap);
                const float h = barH * (0.16f + 0.84f * t);
                juce::Rectangle<float> bar (x, ry + barH - h, w, h);
                const auto colr = spectrum (t);

                if (! zone)
                {
                    g.setColour (col::frame.withAlpha (0.22f * fade));
                    g.fillRect (bar);
                    continue;
                }
                const bool moved = (f & E::cfMoved) != 0;
                g.setColour (colr.withAlpha ((moved ? 0.95f : 0.55f) * fade));
                g.fillRect (bar);
                if (moved && ! old)
                {
                    g.setColour (colr.withAlpha (0.25f));
                    g.fillRect (bar.expanded (1.0f, 1.0f));
                }
                if (f & E::cfStretch)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.55f * fade));
                    const float step = (f & E::cfFast) ? 3.0f : 6.0f;   // schneller = dichtere Streifen
                    for (float yy = bar.getBottom() - 3.0f; yy > bar.getY(); yy -= step)
                        g.fillRect (bar.getX(), yy, bar.getWidth(), 1.0f);
                }
                if (f & E::cfRev)
                {
                    juce::Path tri;
                    const float cx = bar.getCentreX(), tw = std::min (5.0f, w * 0.5f + 1.0f);
                    tri.addTriangle (cx - tw, bar.getY() + 1.0f, cx + tw, bar.getY() + 1.0f, cx, bar.getY() + 1.0f + tw * 1.4f);
                    g.setColour (juce::Colours::white.withAlpha (0.9f * fade));
                    g.fillPath (tri);
                }
                if (f & E::cfGlitch)
                {
                    g.setColour (col::glitch.brighter (0.5f).withAlpha (0.9f * fade));
                    g.fillRect (bar.getX() - 0.5f, ry + 1.0f, bar.getWidth() + 1.0f, 4.0f);
                }
                if (f & E::cfRepeat)
                {
                    g.setColour (col::repeat.brighter (0.4f).withAlpha (fade));
                    const float d = std::min (4.0f, w);
                    g.fillEllipse (bar.getCentreX() - d * 0.5f, ry + barH + 1.5f, d, d);
                }
            }

            // Zonen-Streifen aus der Dichte (für das ganze Raster bekannt, auch voraus)
            const float zy = ry + barH + 6.0f;
            g.setColour (col::frame.withAlpha (0.35f));
            g.fillRect (gx, zy, gw, 1.0f);
            const int segs = std::min (cols, 64);
            for (int k = 0; k < segs; ++k)
            {
                const double beat = winStart + ((double) row + ((double) k + 0.5) / (double) segs) * (double) bpb;
                if (bc::densityZone (dens, beat, (double) bpb, seed, lock).on)
                {
                    g.setColour (col::slicer.withAlpha (0.95f));
                    g.fillRect (gx + gw * (float) k / (float) segs, zy - 1.0f, gw / (float) segs + 0.5f, 3.0f);
                }
            }
        }

        // Abspielposition
        if (playing)
        {
            const int row = juce::jlimit (0, E::kVizRows - 1, curCell / cols);
            const float px = gx + (cellPos - (float) (row * cols)) * cw;
            const float ry = gy + (float) row * rowH;
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillRect (px - 2.0f, ry - 2.0f, 4.0f, barH + 4.0f);
            g.setColour (juce::Colours::white);
            g.fillRect (px - 0.75f, ry - 2.0f, 1.5f, barH + 4.0f);
        }
        else
        {
            g.setColour (col::glass.withAlpha (0.55f));
            g.fillRect (gx, gy, gw, rowH * (float) E::kVizRows - 4.0f);
            g.setFont (bb::fonts::display (13.0f, true));
            g.setColour (col::inkDim);
            g.drawText ("START HOST TRANSPORT", getLocalBounds().withTrimmedTop (28).withTrimmedBottom (30), juce::Justification::centred);
        }

        // Legende; REP und GLITCH leuchten, solange sie gerade arbeiten
        const float ly = b.getBottom() - 22.0f;
        const bool repNow = e.vizRepeat.load (std::memory_order_relaxed), gliNow = e.vizGlitch.load (std::memory_order_relaxed);
        const auto lf = bb::fonts::display (9.5f, true);
        auto textW = [&] (const char* t) { return juce::GlyphArrangement::getStringWidth (lf, t); };
        auto item = [&] (float x, const char* text, int kind, bool lit)
        {
            const auto c = kind == 2 ? col::repeat : kind == 3 ? col::glitch : col::slicer;
            juce::Rectangle<float> ic (x, ly + 3.0f, 9.0f, 10.0f);
            if (lit)
            {
                g.setColour (c.withAlpha (0.3f));
                g.fillRoundedRectangle (juce::Rectangle<float> (x - 3.0f, ly, 18.0f + textW (text), 16.0f), 3.0f);
            }
            g.setColour (lit ? c.brighter (0.6f) : col::inkDim);
            if (kind == 0) { juce::Path tri; tri.addTriangle (ic.getX(), ic.getY(), ic.getRight(), ic.getY(), ic.getCentreX(), ic.getBottom()); g.fillPath (tri); }
            if (kind == 1) for (int k = 0; k < 3; ++k) g.fillRect (ic.getX(), ic.getY() + 1.0f + (float) k * 3.5f, ic.getWidth(), 1.5f);
            if (kind == 2) g.fillEllipse (ic.getX() + 1.0f, ic.getY() + 2.0f, 6.0f, 6.0f);
            if (kind == 3) g.fillRect (ic.getX(), ic.getY() + 1.0f, ic.getWidth(), 4.0f);
            g.setFont (lf);
            g.drawText (text, juce::Rectangle<float> (x + 12.0f, ly, textW (text) + 4.0f, 16.0f), juce::Justification::centredLeft);
        };
        // Einträge gleichmäßig nach ihrer Breite verteilen
        const char* names[4] { "REV", "STRETCH", "REPEAT", "GLITCH" };
        const bool lit[4] { false, false, repNow, gliNow };
        float sum = 0.0f;
        for (auto* n : names) sum += 12.0f + textW (n);
        const float space = (b.getWidth() - 28.0f - sum) / 3.0f;
        float lx = b.getX() + 14.0f;
        for (int k = 0; k < 4; ++k)
        {
            item (lx, names[k], k, lit[k]);
            lx += 12.0f + textW (names[k]) + space;
        }
    }

private:
    BraincutProcessor& proc;
};

} // namespace bcui
