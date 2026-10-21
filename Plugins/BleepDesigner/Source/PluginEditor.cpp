#include "PluginEditor.h"

// ---------------------------------------------------------------- BleepLook

BleepLook::BleepLook()
{
    setColour (juce::Slider::textBoxTextColourId, bleepcol::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, bleepcol::red.withAlpha (0.4f));
    setColour (juce::Label::textColourId, bleepcol::ink);
    setColour (juce::TextEditor::textColourId, bleepcol::ink);
    setColour (juce::TextEditor::backgroundColourId, bleepcol::bg);
    setColour (juce::TextEditor::highlightColourId, bleepcol::red.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, bleepcol::red);
}

void BleepLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider&)
{
    constexpr float startA = juce::MathConstants<float>::pi * 1.25f;
    constexpr float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float size = (float) std::min (w, h);
    const juce::Point<float> c ((float) x + w * 0.5f, (float) y + h * 0.5f);
    const float rArc = size * 0.46f, rBody = size * 0.36f;
    const float a = startA + pos * (endA - startA);

    // Spur und roter Wertebogen
    juce::Path track;
    track.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, startA, endA, true);
    g.setColour (bleepcol::line);
    g.strokePath (track, juce::PathStrokeType (size * 0.04f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.003f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, startA, a, true);
        g.setColour (bleepcol::red.withAlpha (0.22f));
        g.strokePath (val, juce::PathStrokeType (size * 0.11f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (bleepcol::red);
        g.strokePath (val, juce::PathStrokeType (size * 0.04f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Kappe
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - rBody, c.y - rBody + size * 0.03f, rBody * 2, rBody * 2);
    juce::ColourGradient cap (juce::Colour (0xff2a2628), c.x - rBody * 0.4f, c.y - rBody * 0.6f,
                              juce::Colour (0xff121012), c.x + rBody * 0.4f, c.y + rBody, true);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
    g.setColour (juce::Colour (0xff3e3234));
    g.drawEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2, std::max (1.0f, size * 0.025f));

    // Zeiger mit Glow
    juce::Path pointer;
    pointer.startNewSubPath (0.0f, -rBody * 0.25f);
    pointer.lineTo (0.0f, -rBody * 0.85f);
    const auto tf = juce::AffineTransform::rotation (a).translated (c);
    g.setColour (bleepcol::red.withAlpha (0.35f));
    g.strokePath (pointer, juce::PathStrokeType (size * 0.12f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), tf);
    g.setColour (bleepcol::red);
    g.strokePath (pointer, juce::PathStrokeType (size * 0.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), tf);
}

void BleepLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = b.getToggleState();
    g.setColour (on ? bleepcol::gold.withAlpha (0.12f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? bleepcol::gold : (over ? bleepcol::inkDim : bleepcol::line));
    g.drawRoundedRectangle (r, rad, 1.0f);
}

void BleepLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (bb::fonts::mono ((float) b.getHeight() * 0.55f));
    g.setColour (b.getToggleState() ? bleepcol::gold : bleepcol::inkDim);
    g.drawFittedText (b.getButtonText() + (b.getToggleState() ? " AN" : " AUS"), b.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Font BleepLook::getLabelFont (juce::Label& l)
{
    return bb::fonts::mono (std::max (9.0f, (float) l.getHeight() * 0.78f));
}

juce::Label* BleepLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

// -------------------------------------------------------------- BleepEditor

BleepEditor::Knob* BleepEditor::makeKnob (const char* id, const juce::String& label)
{
    auto k = std::make_unique<Knob>();
    k->label = label;
    auto& s = k->slider;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    s.setMouseDragSensitivity (220);
    addAndMakeVisible (s);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
    if (auto* prm = proc.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

BleepEditor::BleepEditor (BleepProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    using namespace ParamIDs;

    groups.push_back ({ "SQUARE", "", { makeKnob (sqOct, "Okt"), makeKnob (sqLevel, "Pegel"), makeKnob (sqPw, "PW") }, false, {} });
    groups.push_back ({ "TRI-MOD", "stumm", { makeKnob (modOct, "Okt"), makeKnob (modPd, "PD") }, true, {} });
    groups.push_back ({ "PITCH ENV", "", { makeKnob (penvAmt, "Amt"), makeKnob (penvDecay, "Decay") }, false, {} });
    groups.push_back ({ "LFO", "", { makeKnob (lfoRate, "Rate"), makeKnob (lfoPitch, "Pitch"), makeKnob (lfoPd, "PD") }, false, {} });
    groups.push_back ({ "AMP ENV", "", { makeKnob (ampA, "A"), makeKnob (ampD, "D"), makeKnob (ampS, "S"), makeKnob (ampR, "R") }, false, {} });
    groups.push_back ({ "OUT", "", { makeKnob (output, "Vol") }, false, {} });

    pitchLinkBtn.setClickingTogglesState (true);
    addAndMakeVisible (pitchLinkBtn);
    pitchLinkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pitchLink, pitchLinkBtn);

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
    repaint (stageArea.toNearestInt().expanded (4));
}

void BleepEditor::resized()
{
    const float u = getWidth() / (float) baseW;
    constexpr float cell = 50.0f, pad = 12.0f, gap = 10.0f, top = 64.0f, groupH = 158.0f;
    float x = 16.0f;

    for (auto& gr : groups)
    {
        const float w = pad * 2 + cell * (float) gr.knobs.size();
        gr.area = juce::Rectangle<float> (x, top, w, groupH) * u;
        for (size_t i = 0; i < gr.knobs.size(); ++i)
        {
            const float kx = x + pad + cell * (float) i;
            gr.knobs[i]->labelArea = juce::Rectangle<float> (kx, top + 34.0f, cell, 14.0f) * u;
            gr.knobs[i]->slider.setBounds ((juce::Rectangle<float> (kx + 2.0f, top + 52.0f, cell - 4.0f, 66.0f) * u).toNearestInt());
            gr.knobs[i]->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::roundToInt ((cell - 2.0f) * u), juce::roundToInt (16.0f * u));
        }
        if (gr.hasToggle)
            pitchLinkBtn.setBounds ((juce::Rectangle<float> (x + pad, top + groupH - 32.0f, cell * 2.0f, 20.0f) * u).toNearestInt());
        x += w + gap;
    }

    stageArea = juce::Rectangle<float> (16.0f, top + groupH + 12.0f, baseW - 32.0f, baseH - (top + groupH + 12.0f) - 16.0f) * u;
}

void BleepEditor::paint (juce::Graphics& g)
{
    const float u = getWidth() / (float) baseW;
    g.fillAll (bleepcol::bg);

    // Kopfzeile
    g.setFont (bb::fonts::mono (13.0f * u));
    g.setColour (bleepcol::red);
    g.drawText ("BRAIN BUGZ", juce::Rectangle<float> (18, 14, 120, 30) * u, juce::Justification::centredLeft);
    g.setFont (bb::fonts::display (26.0f * u, true));
    g.setColour (bleepcol::ink);
    g.drawText ("BLEEP DESIGNER", juce::Rectangle<float> (128, 12, 400, 34) * u, juce::Justification::centredLeft);

    // Generator-Gruppen
    for (auto& gr : groups)
    {
        g.setColour (bleepcol::panel);
        g.fillRoundedRectangle (gr.area, 10.0f * u);
        g.setColour (bleepcol::line);
        g.drawRoundedRectangle (gr.area, 10.0f * u, 1.0f);

        g.setFont (bb::fonts::mono (13.0f * u));
        g.setColour (bleepcol::ink);
        const auto titleArea = juce::Rectangle<float> (gr.area.getX() + 12.0f * u, gr.area.getY() + 8.0f * u, gr.area.getWidth(), 18.0f * u);
        g.drawText (gr.title, titleArea, juce::Justification::centredLeft);

        if (gr.tag.isNotEmpty())
        {
            const float tw = (float) gr.title.length() * 7.6f * u + 8.0f * u;
            auto tag = juce::Rectangle<float> (titleArea.getX() + tw, titleArea.getY() + 2.0f * u, 42.0f * u, 14.0f * u);
            g.setColour (bleepcol::inkDim);
            g.fillRoundedRectangle (tag, 3.0f * u);
            g.setFont (bb::fonts::mono (10.0f * u));
            g.setColour (bleepcol::bg);
            g.drawText (gr.tag, tag, juce::Justification::centred);
        }

        g.setFont (bb::fonts::mono (11.0f * u));
        g.setColour (bleepcol::inkDim);
        for (auto* k : gr.knobs)
            g.drawText (k->label.toUpperCase(), k->labelArea, juce::Justification::centred);
    }

    // Buehne: Anschlag-Anzeige, bis Slots, Arp und Mech dazukommen
    g.setColour (bleepcol::panel);
    g.fillRoundedRectangle (stageArea, 10.0f * u);
    g.setColour (bleepcol::line);
    g.drawRoundedRectangle (stageArea, 10.0f * u, 1.0f);

    const auto c = stageArea.getCentre().translated (0, -40.0f * u);
    const float r = 46.0f * u;
    g.setColour (bleepcol::red.withAlpha (0.10f + 0.45f * flash));
    g.fillEllipse (c.x - r * 1.6f, c.y - r * 1.6f, r * 3.2f, r * 3.2f);
    g.setColour (bleepcol::red.withAlpha (0.35f + 0.65f * flash));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

    const int note = proc.lastNote.load (std::memory_order_relaxed);
    g.setFont (bb::fonts::display (30.0f * u, true));
    g.setColour (bleepcol::bg);
    g.drawText (note < 0 ? juce::String ("--") : juce::MidiMessage::getMidiNoteName (note, true, true, 3),
                juce::Rectangle<float> (c.x - r, c.y - r, r * 2, r * 2), juce::Justification::centred);

    g.setFont (bb::fonts::mono (13.0f * u));
    g.setColour (bleepcol::inkDim);
    g.drawText ("Etappe 2 - Generator live spielbar. Slots, Arp, Effekte und Mech folgen.",
                juce::Rectangle<float> (stageArea.getX(), c.y + r * 1.9f, stageArea.getWidth(), 20.0f * u), juce::Justification::centred);

    const float lvl = juce::jlimit (0.0f, 1.0f, proc.vizLevel.load (std::memory_order_relaxed) * 2.0f);
    auto bar = juce::Rectangle<float> (stageArea.getCentreX() - 150.0f * u, c.y + r * 1.9f + 34.0f * u, 300.0f * u, 6.0f * u);
    g.setColour (bleepcol::line);
    g.fillRoundedRectangle (bar, 3.0f * u);
    g.setColour (bleepcol::gold);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * lvl), 3.0f * u);
}
