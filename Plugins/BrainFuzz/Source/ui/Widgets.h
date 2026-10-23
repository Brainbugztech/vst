#pragma once

// Brainfuzz – Bedienelemente: Regler mit Beschriftung und tippbarem Wert, Schalter, Auswahl-Leisten

#include "Theme.h"
#include "../PluginProcessor.h"

namespace bfz::ui
{

inline void setAccent (juce::Component& c, juce::Colour a) { c.getProperties().set ("accent", (juce::int64) a.getARGB()); }

// Regler: Beschriftung oben, Wert unten (anklicken = eintippen), Doppelklick = Standard
class Knob : public juce::Component
{
public:
    Knob (BrainfuzzProcessor& p, int paramIndex, const juce::String& caption, juce::Colour accent, bool bipolar = false)
        : index (paramIndex), text (caption)
    {
        const auto& spec = paramSpecs()[(size_t) paramIndex];
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 15);
        slider.setTextBoxIsEditable (true);
        slider.setMouseDragSensitivity (220);
        slider.setVelocityBasedMode (false);
        setAccent (slider, accent);
        slider.getProperties().set ("bipolar", bipolar);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, spec.id, slider);
        slider.setDoubleClickReturnValue (true, spec.def);
        slider.setTooltip (juce::String (spec.name) + "  (Doppelklick = Standard, Wert anklicken = eintippen)");
    }

    void setCaption (const juce::String& c) { text = c; repaint(); }
    void setValueBoxHeight (int h) { valueH = h; resized(); }
    juce::Slider slider;

    void paint (juce::Graphics& g) override
    {
        g.setColour (isEnabled() ? col::inkDim : col::inkMute);
        g.setFont (fonts::display (captionH * 0.95f, true));
        g.drawText (text.toUpperCase(), getLocalBounds().removeFromTop (captionH), juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        captionH = juce::jlimit (9, 13, getHeight() / 7);
        r.removeFromTop (captionH);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), valueH);
        slider.setBounds (r);
    }

private:
    int index;
    juce::String text;
    int captionH = 11, valueH = 15;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// An/Aus-Schalter fuer einen Bool-Parameter
class Toggle : public juce::TextButton
{
public:
    Toggle (BrainfuzzProcessor& p, int paramIndex, const juce::String& label, juce::Colour accent)
        : juce::TextButton (label)
    {
        setClickingTogglesState (true);
        setAccent (*this, accent);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, paramSpecs()[(size_t) paramIndex].id, *this);
        setTooltip (paramSpecs()[(size_t) paramIndex].name);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Leiste mit Knoepfen fuer einen Auswahl-Parameter
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (BrainfuzzProcessor& p, int paramIndex, juce::StringArray labels, juce::Colour accent, int columns = 0)
        : cols (columns)
    {
        auto* prm = p.param (paramIndex);
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            setAccent (*b, accent);
            b->setClickingTogglesState (false);
            b->onClick = [this, i] { if (attachment) attachment->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { show (juce::roundToInt (v)); }, &p.undo);
        attachment->sendInitialUpdate();
        setTooltip (paramSpecs()[(size_t) paramIndex].name);
    }

    void setTooltip (const juce::String& t) { for (auto* b : buttons) b->setTooltip (t); }
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;
    juce::TextButton* button (int i) { return buttons[i]; }

    void resized() override
    {
        const int n = buttons.size();
        if (n == 0) return;
        const int c = cols > 0 ? cols : n;
        const int rows = (n + c - 1) / c;
        const int gap = 3;
        const float bw = (getWidth() - gap * (c - 1)) / (float) c;
        const float bh = (getHeight() - gap * (rows - 1)) / (float) rows;
        for (int i = 0; i < n; ++i)
        {
            const int row = i / c, colI = i % c;
            const int inRow = std::min (c, n - row * c);
            const float rowW = inRow * bw + (inRow - 1) * gap;
            const float x0 = (getWidth() - rowW) * 0.5f;
            buttons[i]->setBounds (juce::Rectangle<float> (x0 + colI * (bw + gap), row * (bh + gap), bw, bh).toNearestInt());
        }
    }

private:
    void show (int v)
    {
        selected = v;
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == v, juce::dontSendNotification);
        if (onChange) onChange (v);
    }

    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int cols = 0, selected = 0;
};

// Aufklappliste fuer einen Auswahl-Parameter
class ChoiceBox : public juce::ComboBox
{
public:
    ChoiceBox (BrainfuzzProcessor& p, int paramIndex)
    {
        const auto& spec = paramSpecs()[(size_t) paramIndex];
        int id = 1;
        for (const auto& c : spec.choices) addItem (c, id++);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, spec.id, *this);
        setTooltip (spec.name);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

} // namespace bfz::ui
