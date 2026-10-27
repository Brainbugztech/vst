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
        textFromValueFunction = [unit] (double v) { return juce::String ((int) std::lround (v)) + " " + unit; };
        onValueChange = [this] { repaint(); };
    }

private:
    std::unique_ptr<SA> attachment;
};

// Rundes Display ("Auge"): außen die aktuelle Phrase als Ring (Farbe = Stelle im Loop, umgestellte Stücke leuchten,
// rückwärts gespielte haben eine gestrichelte Linie), innen der Loop als schmaler Ring. Eine Kurve verbindet den
// Abspielpunkt mit der Stelle, von der gerade gelesen wird.
class SliceView : public juce::Component
{
public:
    explicit SliceView (BraincutProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        using juce::MathConstants;
        const float twoPi = MathConstants<float>::twoPi;
        auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX(), cy = b.getY() + 120.0f;
        const float R = 112.0f, o0 = 74.0f, o1 = 110.0f, l0 = 55.0f, l1 = 65.0f;

        auto pt = [&] (float rad, float a) { return juce::Point<float> (cx + rad * std::sin (a), cy - rad * std::cos (a)); };
        auto wedge = [&] (float r0, float r1, float a0, float a1)
        {
            juce::Path p;
            p.addPieSegment (cx - r1, cy - r1, r1 * 2.0f, r1 * 2.0f, a0, a1, r0 / r1);
            return p;
        };

        // Scheibe und Skala
        {
            // dunkle Scheibe, die Mitte bleibt durchsichtig, damit der Vortex aus dem Bild durchscheint
            juce::ColourGradient d (col::glass.withAlpha (0.20f), cx, cy, col::glass.withAlpha (0.80f), cx + R + 6.0f, cy, true);
            g.setGradientFill (d);
            g.fillEllipse (cx - R - 6.0f, cy - R - 6.0f, (R + 6.0f) * 2.0f, (R + 6.0f) * 2.0f);
            g.setColour (col::frame.withAlpha (0.25f));
            g.drawEllipse (cx - R - 10.0f, cy - R - 10.0f, (R + 10.0f) * 2.0f, (R + 10.0f) * 2.0f, 5.0f);
            g.setColour (col::slicer.withAlpha (0.55f));
            g.drawEllipse (cx - R - 6.0f, cy - R - 6.0f, (R + 6.0f) * 2.0f, (R + 6.0f) * 2.0f, 1.0f);
            for (int k = 0; k < 64; ++k)
            {
                const float a = twoPi * (float) k / 64.0f;
                const float len = (k % 4 == 0) ? 6.0f : 3.0f;
                g.setColour ((k % 16 == 0) ? col::ink.withAlpha (0.75f) : col::slicer.withAlpha (0.40f));
                g.drawLine (juce::Line<float> (pt (R + 9.0f, a), pt (R + 9.0f + len, a)), 1.0f);
            }
        }

        const bool playing = proc.vizPlaying.load (std::memory_order_relaxed);
        auto& sl = proc.engine.slicer;
        const int n = sl.vizSteps.load (std::memory_order_relaxed);
        const int M = std::max (1, sl.vizLoop.load (std::memory_order_relaxed));

        if (! playing || n <= 0)
        {
            const float rot = (float) (juce::Time::getMillisecondCounter() % 24000) / 24000.0f * twoPi;
            const int blocks = 16;
            for (int i = 0; i < blocks; ++i)
            {
                const float a0 = rot + twoPi * (float) i / (float) blocks + 0.03f, a1 = rot + twoPi * (float) (i + 1) / (float) blocks - 0.03f;
                g.setColour (spectrum ((float) i / (float) blocks).withAlpha (0.16f));
                g.fillPath (wedge (o0, o1, a0, a1));
            }
            g.setColour (col::slicer.withAlpha (0.25f));
            g.drawEllipse (cx - l1, cy - l1, l1 * 2.0f, l1 * 2.0f, 1.0f);
            g.setFont (bb::fonts::display (13.0f, true));
            g.setColour (col::inkDim);
            g.drawText ("START HOST", (int) cx - 50, (int) cy - 16, 100, 16, juce::Justification::centred);
            g.drawText ("TRANSPORT", (int) cx - 50, (int) cy + 2, 100, 16, juce::Justification::centred);
            return;
        }

        const float pos = proc.engine.vizPos.load (std::memory_order_relaxed);
        const int cur = juce::jlimit (0, n - 1, (int) pos);
        const float pb = sl.vizPhraseBeats.load (std::memory_order_relaxed);
        auto srcAt = [&] (int i) { return (int) sl.vizSrc[(size_t) i].load (std::memory_order_relaxed); };
        auto movedAt = [&] (int i) { return sl.vizMoved[(size_t) i].load (std::memory_order_relaxed) != 0; };
        auto revAt = [&] (int i) { return sl.vizRev[(size_t) i].load (std::memory_order_relaxed) != 0; };

        // Loop-Ring
        for (int k = 0; k < M; ++k)
        {
            const float a0 = twoPi * (float) k / (float) M, a1 = twoPi * (float) (k + 1) / (float) M;
            g.setColour (spectrum ((float) k / (float) std::max (1, M - 1)).withAlpha (0.38f));
            g.fillPath (wedge (l0, l1, a0 + 0.004f, a1 - 0.004f));
        }

        // Phrase: zusammenhängende Schritte bilden einen Block
        const float stepA = twoPi / (float) n;
        int a = 0;
        while (a < n)
        {
            int e = a + 1;
            const bool rev = revAt (a), mv = movedAt (a);
            while (e < n && revAt (e) == rev && movedAt (e) == mv)
            {
                const int expect = rev ? srcAt (e - 1) - 1 : srcAt (e - 1) + 1;
                if (((srcAt (e) - expect) % M + M) % M != 0) break;
                ++e;
            }
            const float gap = (e - a) * stepA > 0.05f ? 0.012f : 0.004f;
            const float s0 = (float) a * stepA + gap, s1 = (float) e * stepA - gap;
            const bool isCur = cur >= a && cur < e;
            const auto mid = spectrum ((float) srcAt (a) / (float) M).interpolatedWith (spectrum ((float) srcAt (e - 1) / (float) M), 0.5f);

            if (mv)
            {
                g.setColour (mid.withAlpha (isCur ? 0.34f : 0.20f));
                g.fillPath (wedge (o0 - 4.0f, o1 + 5.0f, s0 - 0.01f, s1 + 0.01f));
            }
            for (int i = a; i < e; ++i)
            {
                const float t0 = s0 + (s1 - s0) * (float) (i - a) / (float) (e - a);
                const float t1 = s0 + (s1 - s0) * (float) (i - a + 1) / (float) (e - a);
                g.setColour (spectrum (((float) srcAt (i) + 0.5f) / (float) M).withAlpha (mv ? 0.95f : 0.50f));
                g.fillPath (wedge (o0, o1, t0, t1 + 0.003f));
            }
            if (isCur)
            {
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.strokePath (wedge (o0, o1, s0, s1), juce::PathStrokeType (1.5f));
            }
            if (rev)
            {
                juce::Path arc;
                arc.addCentredArc (cx, cy, (o0 + o1) * 0.5f, (o0 + o1) * 0.5f, 0.0f, s0 + 0.02f, s1 - 0.02f, true);
                juce::Path dashed;
                const float dl[2] { 3.0f, 4.0f };
                juce::PathStrokeType (2.0f).createDashedStroke (dashed, arc, dl, 2);
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.fillPath (dashed);
            }
            a = e;
        }

        // Abspielpunkt und Verbindung zur gelesenen Stelle im Loop
        const float within = juce::jlimit (0.0f, 0.999f, pos - (float) cur);
        const float pa = twoPi * juce::jlimit (0.0f, (float) n, pos) / (float) n;
        const float srcPos = ((float) srcAt (cur) + within) / (float) M;
        const float sa = twoPi * srcPos;
        const auto sc = spectrum (srcPos);

        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawLine (juce::Line<float> (pt (o0 - 6.0f, pa), pt (o1 + 8.0f, pa)), 4.0f);
        g.setColour (juce::Colours::white);
        g.drawLine (juce::Line<float> (pt (o0 - 6.0f, pa), pt (o1 + 8.0f, pa)), 1.6f);

        juce::Path link;
        link.startNewSubPath (pt (o0 - 6.0f, pa));
        const auto ctrl = juce::Point<float> (cx + (pt (o0, pa).x - cx + pt (l1, sa).x - cx) * 0.18f,
                                              cy + (pt (o0, pa).y - cy + pt (l1, sa).y - cy) * 0.18f);
        link.quadraticTo (ctrl, pt (l1 + 1.0f, sa));
        g.setColour (sc.withAlpha (0.35f));
        g.strokePath (link, juce::PathStrokeType (4.0f));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.strokePath (link, juce::PathStrokeType (1.2f));

        const float segA = twoPi / (float) M;
        g.setColour (sc.withAlpha (0.4f));
        g.fillPath (wedge (l0 - 4.0f, l1 + 4.0f, std::floor (srcPos * (float) M) * segA - 0.01f, std::floor (srcPos * (float) M) * segA + segA + 0.01f));
        g.setColour (juce::Colours::white);
        g.fillPath (wedge (l0 - 1.0f, l1 + 1.0f, std::floor (srcPos * (float) M) * segA, std::floor (srcPos * (float) M) * segA + segA));

        // Mitte: Zahl der Schritte
        g.setFont (bb::fonts::display (30.0f, true));
        g.setColour (col::ink);
        g.drawText (juce::String (n), (int) cx - 40, (int) cy - 34, 80, 34, juce::Justification::centred);
        g.setFont (bb::fonts::display (11.5f, true));
        g.setColour (col::inkDim);
        g.drawText ("STEPS", (int) cx - 40, (int) cy, 80, 14, juce::Justification::centred);
        g.setFont (bb::fonts::mono (11.0f));
        g.drawText (juce::String (pb, pb == std::floor (pb) ? 0 : 1) + " BEATS", (int) cx - 40, (int) cy + 15, 80, 14, juce::Justification::centred);

        // Lichter für Repeater und Glitcher: links und rechts unten neben dem Ring
        auto chip = [&] (juce::Rectangle<float> cr, const char* text, bool lit, juce::Colour c)
        {
            const auto shape = chamfer (cr, 5.0f);
            g.setColour (lit ? c.withAlpha (0.32f) : col::glass.withAlpha (0.7f));
            g.fillPath (shape);
            g.setColour (lit ? c : col::line.brighter (0.3f));
            g.strokePath (shape, juce::PathStrokeType (1.0f));
            g.setFont (bb::fonts::display (10.5f, true));
            g.setColour (lit ? c.brighter (0.6f) : col::inkDim.withAlpha (0.6f));
            g.drawText (text, cr, juce::Justification::centred);
        };
        chip ({ b.getX() + 2.0f, b.getBottom() - 22.0f, 58.0f, 18.0f }, "REPEAT", proc.engine.vizRepeat.load (std::memory_order_relaxed), col::repeat);
        chip ({ b.getRight() - 60.0f, b.getBottom() - 22.0f, 58.0f, 18.0f }, "GLITCH", proc.engine.vizGlitch.load (std::memory_order_relaxed), col::glitch);
    }

private:
    BraincutProcessor& proc;
};

} // namespace bcui
