#pragma once

// Bausteine der Braincut-Oberfläche: Regler mit Beschriftung und die Slice-Balkenanzeige

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

// Balken für jeden Schritt der Phrase: Höhe = von wo der Schritt liest, Farbe = Original (grün) oder umgestellt (pink)
class SliceView : public juce::Component
{
public:
    explicit SliceView (BraincutProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::card.withAlpha (0.6f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (bb::col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        auto inner = r.reduced (14.0f, 12.0f);
        const bool playing = proc.vizPlaying.load (std::memory_order_relaxed);
        auto& sl = proc.engine.slicer;
        const int n = sl.vizSteps.load (std::memory_order_relaxed);

        if (! playing || n <= 0)
        {
            // Ruhezustand: schwache Deko-Balken und ein Hinweis
            const int dummy = 48;
            const float bw = inner.getWidth() / (float) dummy;
            for (int i = 0; i < dummy; ++i)
            {
                const float hh = 0.18f + 0.55f * (0.5f + 0.5f * std::sin ((float) i * 0.45f));
                g.setColour (col::green.withAlpha (0.10f));
                g.fillRoundedRectangle (inner.getX() + (float) i * bw + 1.0f, inner.getBottom() - inner.getHeight() * hh, bw - 2.0f, inner.getHeight() * hh, 2.0f);
            }
            g.setFont (bb::fonts::display (15.0f, true));
            g.setColour (bb::col::inkDim);
            g.drawText (juce::String::fromUTF8 ("Host-Transport starten \xe2\x80\x93 Braincut schneidet im Takt des Hosts"), getLocalBounds(), juce::Justification::centred);
            return;
        }

        const float bw = inner.getWidth() / (float) n;
        const float gap = bw > 8.0f ? 2.5f : (bw > 4.0f ? 1.5f : (bw > 2.5f ? 0.8f : 0.0f));
        const float pos = proc.engine.vizPos.load (std::memory_order_relaxed);
        const int cur = juce::jlimit (0, n - 1, (int) pos);

        for (int i = 0; i < n; ++i)
        {
            const int src = sl.vizSrc[(size_t) i].load (std::memory_order_relaxed);
            const bool moved = sl.vizMoved[(size_t) i].load (std::memory_order_relaxed) != 0;
            const float hh = 0.16f + 0.84f * ((float) src + 0.5f) / (float) n;
            const auto colour = moved ? col::pink : col::green;
            juce::Rectangle<float> bar (inner.getX() + (float) i * bw + gap * 0.5f, inner.getBottom() - inner.getHeight() * hh,
                                        std::max (1.0f, bw - gap), inner.getHeight() * hh);
            if (i == cur)
            {
                g.setColour (colour.withAlpha (0.22f));
                g.fillRoundedRectangle (bar.expanded (3.0f), 4.0f);
            }
            g.setColour (colour.withAlpha (i == cur ? 1.0f : (i < cur ? 0.5f : 0.78f)));
            g.fillRoundedRectangle (bar, std::min (2.5f, bar.getWidth() * 0.4f));
        }

        // Abspiel-Linie
        const float px = inner.getX() + juce::jlimit (0.0f, (float) n, pos) / (float) n * inner.getWidth();
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRect (px - 2.0f, inner.getY() - 4.0f, 4.0f, inner.getHeight() + 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (px - 0.75f, inner.getY() - 4.0f, 1.5f, inner.getHeight() + 8.0f);

        // Ereignis-Lichter
        auto chip = [&] (juce::Rectangle<float> cr, const char* text, bool lit, juce::Colour c)
        {
            g.setColour (lit ? c.withAlpha (0.3f) : juce::Colours::black.withAlpha (0.25f));
            g.fillRoundedRectangle (cr, 6.0f);
            g.setColour (lit ? c : bb::col::line);
            g.drawRoundedRectangle (cr, 6.0f, 1.0f);
            g.setFont (bb::fonts::display (11.5f, true));
            g.setColour (lit ? c.brighter (0.4f) : bb::col::inkDim.withAlpha (0.6f));
            g.drawText (text, cr, juce::Justification::centred);
        };
        chip ({ r.getRight() - 150.0f, r.getY() + 8.0f, 68.0f, 20.0f }, "REPEAT", proc.engine.vizRepeat.load (std::memory_order_relaxed), col::pink);
        chip ({ r.getRight() - 76.0f, r.getY() + 8.0f, 64.0f, 20.0f }, "GLITCH", proc.engine.vizGlitch.load (std::memory_order_relaxed), col::violet);
    }

private:
    BraincutProcessor& proc;
};

} // namespace bcui
