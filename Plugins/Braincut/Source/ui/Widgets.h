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
        g.setColour (bb::col::ink);
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

// Zeigt die aktuelle Phrase: jeder Block ist ein Stück der Quelle (Farbe = Stelle im Loop). Umgestellte Stücke leuchten,
// rückwärts gespielte tragen Pfeile. Darunter der Loop als Farbband mit Marke, woher gerade gelesen wird.
class SliceView : public juce::Component
{
public:
    explicit SliceView (BraincutProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        juce::ColourGradient bgGrad (juce::Colour (0xff130c1f).withAlpha (0.92f), 0, r.getY(), juce::Colour (0xff0a0612).withAlpha (0.92f), 0, r.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

        const bool playing = proc.vizPlaying.load (std::memory_order_relaxed);
        auto& sl = proc.engine.slicer;
        const int n = sl.vizSteps.load (std::memory_order_relaxed);
        const int M = std::max (1, sl.vizLoop.load (std::memory_order_relaxed));

        auto area = r.reduced (18.0f, 0.0f);
        const float top = r.getY() + 34.0f, bottom = r.getBottom() - 46.0f;
        juce::Rectangle<float> lane (area.getX(), top, area.getWidth(), bottom - top);
        juce::Rectangle<float> strip (area.getX(), r.getBottom() - 20.0f, area.getWidth(), 7.0f);

        // Loop-Farbband (immer sichtbar)
        {
            juce::ColourGradient cg (spectrum (0.0f), strip.getX(), 0, spectrum (1.0f), strip.getRight(), 0, false);
            for (int k = 1; k < 8; ++k) cg.addColour (k / 8.0, spectrum ((float) k / 8.0f));
            g.setGradientFill (cg);
            g.setOpacity (playing ? 0.40f : 0.22f);
            g.fillRoundedRectangle (strip, 3.5f);
        }

        g.setFont (bb::fonts::display (11.5f, true));
        g.setColour (bb::col::inkDim);
        g.drawText ("OUT", (int) area.getX(), 10, 60, 16, juce::Justification::centredLeft);
        g.drawText ("LOOP", (int) area.getX(), (int) strip.getY() - 15, 60, 12, juce::Justification::centredLeft);

        if (! playing || n <= 0)
        {
            // Ruhezustand: weiche Deko-Blöcke und ein Hinweis
            const int blocks = 12;
            const float bw = lane.getWidth() / (float) blocks;
            for (int i = 0; i < blocks; ++i)
            {
                const auto c = spectrum ((float) i / (float) (blocks - 1));
                auto b = juce::Rectangle<float> (lane.getX() + (float) i * bw + 2.0f, lane.getY() + lane.getHeight() * 0.18f, bw - 4.0f, lane.getHeight() * 0.64f);
                g.setColour (c.withAlpha (0.13f));
                g.fillRoundedRectangle (b, 6.0f);
            }
            g.setFont (bb::fonts::display (14.5f, true));
            g.setColour (bb::col::inkDim);
            g.drawText (juce::String::fromUTF8 ("Host-Transport starten \xe2\x80\x93 Braincut schneidet im Takt des Hosts"), lane, juce::Justification::centred);
            return;
        }

        const float pos = proc.engine.vizPos.load (std::memory_order_relaxed);
        const int cur = juce::jlimit (0, n - 1, (int) pos);
        const float pb = sl.vizPhraseBeats.load (std::memory_order_relaxed);

        // Kopfzeile: Länge der Phrase, Schritte und Lichter
        g.setFont (bb::fonts::mono (11.5f));
        g.setColour (bb::col::inkDim);
        g.drawText (juce::String (n) + " STEPS  \xc2\xb7  " + juce::String (pb, pb == std::floor (pb) ? 0 : 1) + " BEATS", 50, 10, 200, 16, juce::Justification::centredLeft);
        auto chip = [&] (juce::Rectangle<float> cr, const char* text, bool lit, juce::Colour c)
        {
            g.setColour (lit ? c.withAlpha (0.30f) : juce::Colours::black.withAlpha (0.25f));
            g.fillRoundedRectangle (cr, 6.0f);
            g.setColour (lit ? c : bb::col::line);
            g.drawRoundedRectangle (cr, 6.0f, 1.0f);
            g.setFont (bb::fonts::display (11.0f, true));
            g.setColour (lit ? c.brighter (0.4f) : bb::col::inkDim.withAlpha (0.6f));
            g.drawText (text, cr, juce::Justification::centred);
        };
        chip ({ r.getRight() - 162.0f, r.getY() + 8.0f, 72.0f, 20.0f }, "REPEAT", proc.engine.vizRepeat.load (std::memory_order_relaxed), col::pink);
        chip ({ r.getRight() - 84.0f, r.getY() + 8.0f, 66.0f, 20.0f }, "GLITCH", proc.engine.vizGlitch.load (std::memory_order_relaxed), col::violet);

        // Blöcke: zusammenhängende Schritte (Quelle läuft vorwärts weiter) bilden einen Block
        const float sw = lane.getWidth() / (float) n;
        auto srcAt = [&] (int i) { return (int) sl.vizSrc[(size_t) i].load (std::memory_order_relaxed); };
        auto movedAt = [&] (int i) { return sl.vizMoved[(size_t) i].load (std::memory_order_relaxed) != 0; };
        auto revAt = [&] (int i) { return sl.vizRev[(size_t) i].load (std::memory_order_relaxed) != 0; };

        int a = 0;
        while (a < n)
        {
            int b = a + 1;
            const bool rev = revAt (a), mv = movedAt (a);
            while (b < n && revAt (b) == rev && movedAt (b) == mv)
            {
                const int expect = rev ? srcAt (b - 1) - 1 : srcAt (b - 1) + 1;
                if (((srcAt (b) - expect) % M + M) % M != 0) break;
                ++b;
            }
            const float x0 = lane.getX() + (float) a * sw, x1 = lane.getX() + (float) b * sw;
            juce::Rectangle<float> blk (x0 + 1.5f, lane.getY(), std::max (2.0f, x1 - x0 - 3.0f), lane.getHeight());
            const auto c0 = spectrum ((float) srcAt (a) / (float) M), c1 = spectrum ((float) srcAt (b - 1) / (float) M);
            const bool isCur = cur >= a && cur < b;

            if (mv)
            {
                g.setColour (c0.interpolatedWith (c1, 0.5f).withAlpha (isCur ? 0.34f : 0.20f));
                g.fillRoundedRectangle (blk.expanded (4.0f), 10.0f);
            }
            juce::ColourGradient cg (c0.withAlpha (mv ? 0.95f : 0.50f), blk.getX(), 0, c1.withAlpha (mv ? 0.95f : 0.50f), blk.getRight(), 0, false);
            g.setGradientFill (cg);
            g.fillRoundedRectangle (blk, 7.0f);
            // Glanz oben
            juce::ColourGradient shine (juce::Colours::white.withAlpha (mv ? 0.22f : 0.10f), 0, blk.getY(), juce::Colours::white.withAlpha (0.0f), 0, blk.getY() + blk.getHeight() * 0.6f, false);
            g.setGradientFill (shine);
            g.fillRoundedRectangle (blk, 7.0f);
            if (isCur)
            {
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawRoundedRectangle (blk, 7.0f, 1.6f);
            }

            // Schritt-Teilung (dezent)
            if (sw > 7.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.22f));
                for (int k = a + 1; k < b; ++k)
                    g.fillRect (lane.getX() + (float) k * sw - 0.5f, blk.getY() + 6.0f, 1.0f, blk.getHeight() - 12.0f);
            }
            // Rückwärts: Pfeile
            if (rev)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                const float cy = blk.getCentreY();
                for (float px = blk.getX() + 10.0f; px < blk.getRight() - 6.0f; px += 16.0f)
                {
                    juce::Path ar;
                    ar.startNewSubPath (px + 5.0f, cy - 6.0f);
                    ar.lineTo (px, cy);
                    ar.lineTo (px + 5.0f, cy + 6.0f);
                    g.strokePath (ar, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }
            a = b;
        }

        // Verbindung zur Quelle im Loop
        const float within = juce::jlimit (0.0f, 0.999f, pos - (float) cur);
        const float px = lane.getX() + juce::jlimit (0.0f, (float) n, pos) * sw;
        const float srcPos = ((float) srcAt (cur) + within) / (float) M;
        const float sx = strip.getX() + juce::jlimit (0.0f, 1.0f, srcPos) * strip.getWidth();
        const auto sc = spectrum (srcPos);

        juce::Path link;
        link.startNewSubPath (px, lane.getBottom());
        link.cubicTo (px, lane.getBottom() + 12.0f, sx, strip.getY() - 14.0f, sx, strip.getY());
        g.setColour (sc.withAlpha (0.35f));
        g.strokePath (link, juce::PathStrokeType (4.0f));
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.strokePath (link, juce::PathStrokeType (1.2f));

        const float segW = std::max (4.0f, strip.getWidth() / (float) M);
        g.setColour (sc.withAlpha (0.35f));
        g.fillRoundedRectangle (juce::Rectangle<float> (sx - segW * 0.5f - 3.0f, strip.getY() - 3.0f, segW + 6.0f, strip.getHeight() + 6.0f), 5.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (juce::Rectangle<float> (sx - segW * 0.5f, strip.getY() - 1.0f, segW, strip.getHeight() + 2.0f), 3.0f);

        // Abspiel-Linie
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.fillRect (px - 2.5f, lane.getY() - 5.0f, 5.0f, lane.getHeight() + 10.0f);
        g.setColour (juce::Colours::white);
        g.fillRect (px - 0.8f, lane.getY() - 5.0f, 1.6f, lane.getHeight() + 10.0f);
    }

private:
    BraincutProcessor& proc;
};

} // namespace bcui
