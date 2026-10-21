#include "PluginEditor.h"

BleepEditor::BleepEditor (BleepProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    output.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    output.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    output.setColour (juce::Slider::rotarySliderFillColourId, bleepcol::red);
    output.setColour (juce::Slider::thumbColourId, bleepcol::red);
    addAndMakeVisible (output);
    outputAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::output, output);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (baseW, baseH);

    startTimerHz (30);
}

BleepEditor::~BleepEditor()
{
    setLookAndFeel (nullptr);
}

void BleepEditor::timerCallback()
{
    const int c = proc.noteCounter.load (std::memory_order_relaxed);
    if (c != seenNotes)
    {
        seenNotes = c;
        flash = 1.0f;
    }
    else
    {
        flash *= 0.82f;
    }
    repaint();
}

void BleepEditor::paint (juce::Graphics& g)
{
    const float u = getWidth() / (float) baseW;
    g.fillAll (bleepcol::bg);

    // Kopfzeile
    g.setFont (bb::fonts::mono (13.0f * u));
    g.setColour (bleepcol::red);
    g.drawText ("BRAIN BUGZ", juce::Rectangle<float> (18 * u, 14 * u, 140 * u, 30 * u), juce::Justification::centredLeft);
    g.setFont (bb::fonts::display (26.0f * u, true));
    g.setColour (bleepcol::ink);
    g.drawText ("BLEEP DESIGNER", juce::Rectangle<float> (130 * u, 12 * u, 400 * u, 34 * u), juce::Justification::centredLeft);

    // Hauptfeld
    auto area = juce::Rectangle<float> (16 * u, 60 * u, (baseW - 32) * u, (baseH - 76) * u);
    g.setColour (bleepcol::panel);
    g.fillRoundedRectangle (area, 10 * u);
    g.setColour (bleepcol::line);
    g.drawRoundedRectangle (area, 10 * u, 1.0f);

    // Trigger-Anzeige: leuchtet bei jedem Note-On auf
    const auto c = area.getCentre().translated (0, -60 * u);
    const float r = 46 * u;
    g.setColour (bleepcol::red.withAlpha (0.12f + 0.5f * flash));
    g.fillEllipse (c.x - r * 1.6f, c.y - r * 1.6f, r * 3.2f, r * 3.2f);
    g.setColour (bleepcol::red.withAlpha (0.35f + 0.65f * flash));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

    const int note = proc.lastNote.load (std::memory_order_relaxed);
    g.setFont (bb::fonts::display (30.0f * u, true));
    g.setColour (bleepcol::bg);
    g.drawText (note < 0 ? juce::String ("--") : juce::MidiMessage::getMidiNoteName (note, true, true, 3),
                juce::Rectangle<float> (c.x - r, c.y - r, r * 2, r * 2), juce::Justification::centred);

    g.setFont (bb::fonts::mono (14.0f * u));
    g.setColour (bleepcol::inkDim);
    g.drawText ("Etappe 1 - Grundgeruest: spiel eine MIDI-Note", area.withTrimmedTop (area.getHeight() * 0.5f + 40 * u).withHeight (24 * u),
                juce::Justification::centred);

    // Pegel-Balken
    const float lvl = juce::jlimit (0.0f, 1.0f, proc.vizLevel.load (std::memory_order_relaxed) * 2.0f);
    auto bar = juce::Rectangle<float> (area.getCentreX() - 150 * u, area.getCentreY() + 80 * u, 300 * u, 6 * u);
    g.setColour (bleepcol::line);
    g.fillRoundedRectangle (bar, 3 * u);
    g.setColour (bleepcol::gold);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * lvl), 3 * u);

    g.setFont (bb::fonts::mono (12.0f * u));
    g.setColour (bleepcol::inkDim);
    g.drawText ("OUTPUT", juce::Rectangle<float> (area.getRight() - 130 * u, area.getBottom() - 150 * u, 110 * u, 18 * u),
                juce::Justification::centred);
}

void BleepEditor::resized()
{
    const float u = getWidth() / (float) baseW;
    output.setBounds (juce::roundToInt ((baseW - 16 - 120) * u), juce::roundToInt ((baseH - 16 - 130) * u),
                      juce::roundToInt (110 * u), juce::roundToInt (110 * u));
}
