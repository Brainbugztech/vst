#include "PluginEditor.h"

// Platzhalter-Farben, bitte an die Brain-Bugz-Farben anpassen
namespace Colours
{
    const juce::Colour background (0xff0e0e12);
    const juce::Colour panel      (0xff1a1a22);
    const juce::Colour accent     (0xff7cff6b);
    const juce::Colour text       (0xffe6e6ea);
    const juce::Colour dim        (0xff8a8a96);
}

BugWobbleEditor::BugWobbleEditor (BugWobbleProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (680, BUGWOBBLE_IS_SYNTH ? 540 : 440);

    auto& apvts = processor.apvts;

    auto addKnob = [this, &apvts] (const char* paramID, const char* name)
    {
        auto knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                    juce::Slider::TextBoxBelow);
        knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        knob->setColour (juce::Slider::rotarySliderFillColourId, Colours::accent);
        knob->setColour (juce::Slider::rotarySliderOutlineColourId, Colours::panel);
        knob->setColour (juce::Slider::thumbColourId, Colours::accent);
        knob->setColour (juce::Slider::textBoxTextColourId, Colours::text);
        knob->setColour (juce::Slider::textBoxOutlineColourId, Colours::panel);
        sliderAtts.push_back (std::make_unique<SliderAtt> (apvts, paramID, *knob));
        addCell (name, std::move (knob));
    };

    auto addCombo = [this, &apvts] (const char* paramID, const char* name)
    {
        auto box = std::make_unique<juce::ComboBox>();
        box->setColour (juce::ComboBox::backgroundColourId, Colours::panel);
        box->setColour (juce::ComboBox::textColourId, Colours::text);
        box->setColour (juce::ComboBox::outlineColourId, Colours::accent);
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID)))
            box->addItemList (choice->choices, 1);
        comboAtts.push_back (std::make_unique<ComboAtt> (apvts, paramID, *box));
        addCell (name, std::move (box));
    };

    // Reihenfolge = Gitter (4 Spalten x 5 Zeilen)
#if BUGWOBBLE_IS_SYNTH
    addCombo ("osc",       "Osc");
    addKnob  ("attack",    "Attack");
    addKnob  ("decay",     "Decay");
    addKnob  ("sustain",   "Sustain");
    addKnob  ("release",   "Release");
#endif
    addKnob  ("rate",      "Rate (Hz)");
    addCombo ("division",  "Division");

    // Tempo-Sync als Toggle
    auto sync = std::make_unique<juce::ToggleButton> ("Host Tempo");
    sync->setColour (juce::ToggleButton::textColourId, Colours::text);
    sync->setColour (juce::ToggleButton::tickColourId, Colours::accent);
    sync->setColour (juce::ToggleButton::tickDisabledColourId, Colours::dim);
    buttonAtts.push_back (std::make_unique<ButtonAtt> (apvts, "sync", *sync));
    addCell ("Sync", std::move (sync));

    addCombo ("shape",     "LFO Shape");
    addCombo ("type",      "Filter");
    addKnob  ("cutoff",    "Cutoff");

    addKnob  ("depth",     "Depth");
    addKnob  ("resonance", "Resonance");
    addKnob  ("drive",     "Drive");
    addCombo ("driveType", "Drive Type");

    addKnob  ("stereo",    "Stereo");
    addKnob  ("mix",       "Mix");
    addKnob  ("output",    "Output");

    // Preset-Auswahl, setzt alle Parameter
    auto presetBox = std::make_unique<juce::ComboBox>();
    presetBox->setColour (juce::ComboBox::backgroundColourId, Colours::panel);
    presetBox->setColour (juce::ComboBox::textColourId, Colours::text);
    presetBox->setColour (juce::ComboBox::outlineColourId, Colours::accent);
    presetBox->addItemList (processor.getPresetNames(), 1);
    presetBox->setSelectedItemIndex (processor.getCurrentPreset(), juce::dontSendNotification);
    auto* presetPtr = presetBox.get();
    presetBox->onChange = [this, presetPtr]
    {
        processor.applyPreset (presetPtr->getSelectedItemIndex());
    };
    addCell ("Preset", std::move (presetBox));
}

void BugWobbleEditor::addCell (const juce::String& name, std::unique_ptr<juce::Component> control)
{
    auto label = std::make_unique<juce::Label> (juce::String(), name);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (*label);
    addAndMakeVisible (*control);

    labels.push_back (std::move (label));
    controls.push_back (std::move (control));
}

void BugWobbleEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    g.setColour (Colours::accent);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("BUG WOBBLE", getLocalBounds().removeFromTop (50).reduced (16, 0),
                juce::Justification::centredLeft);
}

void BugWobbleEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (50);
    area = area.reduced (12);

    constexpr int cols = 4;
    const int rows  = (int) ((controls.size() + cols - 1) / cols);
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / rows;

    for (size_t i = 0; i < controls.size(); ++i)
    {
        const int col = (int) (i % cols);
        const int row = (int) (i / cols);

        auto cell = juce::Rectangle<int> (area.getX() + col * cellW, area.getY() + row * cellH,
                                          cellW, cellH).reduced (6);

        labels[i]->setBounds (cell.removeFromTop (18));

        if (dynamic_cast<juce::ComboBox*> (controls[i].get()) != nullptr
            || dynamic_cast<juce::ToggleButton*> (controls[i].get()) != nullptr)
            controls[i]->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 8, 28));
        else
            controls[i]->setBounds (cell.reduced (4));
    }
}
