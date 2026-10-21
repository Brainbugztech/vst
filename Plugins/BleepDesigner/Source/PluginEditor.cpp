#include "PluginEditor.h"
#include "BleepAssets.h"

namespace
{
    juce::Image loadImage (const char* name)
    {
        int size = 0;
        if (auto* data = BleepAssets::getNamedResource (name, size))
            return juce::ImageFileFormat::loadFrom (data, (size_t) size);
        return {};
    }

    juce::Typeface::Ptr loadTypeface (const char* name)
    {
        int size = 0;
        if (auto* data = BleepAssets::getNamedResource (name, size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        return nullptr;
    }

    double now() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    // Position im Mech-Bild (Anteile von Breite und Hoehe)
    constexpr float kMuzzleX = 0.918f, kMuzzleY = 0.146f;
    constexpr float kEyesX = 0.469f, kEyesY = 0.113f;
}

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
    setColour (juce::ComboBox::backgroundColourId, bleepcol::bg);
    setColour (juce::ComboBox::textColourId, bleepcol::ink);
    setColour (juce::ComboBox::outlineColourId, bleepcol::line);
    setColour (juce::ComboBox::arrowColourId, bleepcol::inkDim);
    setColour (juce::ComboBox::focusedOutlineColourId, bleepcol::red);
    setColour (juce::PopupMenu::backgroundColourId, bleepcol::panel);
    setColour (juce::PopupMenu::textColourId, bleepcol::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, bleepcol::red.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, bleepcol::ink);
}

void BleepLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider&)
{
    constexpr float startA = juce::MathConstants<float>::pi * 1.25f;
    constexpr float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float size = (float) std::min (w, h);
    const juce::Point<float> c ((float) x + w * 0.5f, (float) y + h * 0.5f);
    const float rArc = size * 0.46f, rBody = size * 0.36f;
    const float a = startA + pos * (endA - startA);

    juce::Path track;
    track.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, startA, endA, true);
    g.setColour (bleepcol::line);
    g.strokePath (track, juce::PathStrokeType (size * 0.045f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.003f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, startA, a, true);
        g.setColour (bleepcol::red.withAlpha (0.22f));
        g.strokePath (val, juce::PathStrokeType (size * 0.11f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (bleepcol::red);
        g.strokePath (val, juce::PathStrokeType (size * 0.045f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - rBody, c.y - rBody + size * 0.03f, rBody * 2, rBody * 2);
    juce::ColourGradient cap (juce::Colour (0xff2a2628), c.x - rBody * 0.4f, c.y - rBody * 0.6f,
                              juce::Colour (0xff121012), c.x + rBody * 0.4f, c.y + rBody, true);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
    g.setColour (juce::Colour (0xff3e3234));
    g.drawEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2, std::max (1.0f, size * 0.025f));

    juce::Path pointer;
    pointer.startNewSubPath (0.0f, -rBody * 0.25f);
    pointer.lineTo (0.0f, -rBody * 0.85f);
    const auto tf = juce::AffineTransform::rotation (a).translated (c);
    g.setColour (bleepcol::red.withAlpha (0.35f));
    g.strokePath (pointer, juce::PathStrokeType (size * 0.12f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), tf);
    g.setColour (bleepcol::red);
    g.strokePath (pointer, juce::PathStrokeType (size * 0.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), tf);
}

void BleepLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto style = b.getProperties()["style"].toString();
    const bool on = b.getToggleState();

    if (style == "primary")
    {
        const float rr = std::min (8.0f, r.getHeight() * 0.25f);
        g.setColour (bleepcol::red.withAlpha (0.22f));
        g.fillRoundedRectangle (r, rr);
        g.setColour (down ? bleepcol::red.darker (0.3f) : (over ? bleepcol::red.brighter (0.15f) : bleepcol::red));
        g.fillRoundedRectangle (r.reduced (1.5f), rr);
        return;
    }

    if (style == "power")
    {
        // Titel eines Effekts mit Power-Punkt
        const float d = r.getHeight() * 0.5f;
        const auto dot = juce::Rectangle<float> (r.getX() + 2.0f, r.getCentreY() - d * 0.5f, d, d);
        if (on)
        {
            g.setColour (bleepcol::red.withAlpha (0.3f));
            g.fillEllipse (dot.expanded (2.5f));
            g.setColour (bleepcol::red);
            g.fillEllipse (dot);
        }
        else
        {
            g.setColour (over ? bleepcol::inkDim : bleepcol::line);
            g.drawEllipse (dot, 1.5f);
        }
        return;
    }

    if (style == "lock" || style == "rerender")
    {
        const float rr = 5.0f;
        g.setColour (on ? bleepcol::gold : bleepcol::knob);
        g.fillRoundedRectangle (r, rr);
        g.setColour (on ? bleepcol::gold : (over ? bleepcol::inkDim : juce::Colour (0xff3e3234)));
        g.drawRoundedRectangle (r, rr, 1.0f);

        const auto ink = on ? bleepcol::bg : (over ? bleepcol::ink : bleepcol::inkDim);
        const float sz = std::min (r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre();
        juce::Path p;
        if (style == "lock")
        {
            const auto body = juce::Rectangle<float> (c.x - sz * 0.5f, c.y - sz * 0.05f, sz, sz * 0.6f);
            g.setColour (ink);
            g.fillRoundedRectangle (body, sz * 0.12f);
            p.startNewSubPath (c.x - sz * 0.3f, body.getY());
            p.lineTo (c.x - sz * 0.3f, c.y - sz * 0.35f);
            p.addCentredArc (c.x, c.y - sz * 0.35f, sz * 0.3f, sz * 0.3f, 0.0f, -juce::MathConstants<float>::halfPi,
                             on ? juce::MathConstants<float>::halfPi : juce::MathConstants<float>::halfPi * 0.4f, false);
            if (on)
                p.lineTo (c.x + sz * 0.3f, body.getY());
        }
        else
        {
            p.addCentredArc (c.x, c.y, sz * 0.55f, sz * 0.55f, 0.0f, 0.5f, juce::MathConstants<float>::twoPi - 0.2f, true);
            const auto tip = juce::Point<float> (c.x + sz * 0.55f * std::sin (0.5f), c.y - sz * 0.55f * std::cos (0.5f));
            p.startNewSubPath (tip.translated (-sz * 0.32f, -sz * 0.08f));
            p.lineTo (tip);
            p.lineTo (tip.translated (-sz * 0.05f, sz * 0.32f));
        }
        g.setColour (ink);
        g.strokePath (p, juce::PathStrokeType (std::max (1.2f, sz * 0.16f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    const float rad = r.getHeight() * 0.5f;
    g.setColour (on ? bleepcol::gold.withAlpha (0.12f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? bleepcol::gold : (over ? bleepcol::inkDim : bleepcol::line));
    g.drawRoundedRectangle (r, rad, 1.0f);
}

void BleepLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto style = b.getProperties()["style"].toString();
    const bool on = b.getToggleState();

    if (style == "primary")
    {
        g.setFont (bb::fonts::display ((float) b.getHeight() * 0.48f, true));
        g.setColour (bleepcol::bg);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
        return;
    }
    if (style == "lock" || style == "rerender")
        return;
    if (style == "power")
    {
        g.setFont (bb::fonts::mono ((float) b.getHeight() * 0.72f));
        g.setColour (on ? bleepcol::ink : bleepcol::inkDim);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (b.getHeight()), juce::Justification::centredLeft, 1);
        return;
    }

    g.setFont (bb::fonts::mono ((float) b.getHeight() * 0.58f));
    g.setColour (on ? bleepcol::gold : bleepcol::inkDim);
    const auto suffix = style == "onoff" ? juce::String (on ? " AN" : " AUS") : juce::String();
    g.drawFittedText (b.getButtonText() + suffix, b.getLocalBounds(), juce::Justification::centred, 1);
}

void BleepLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (bleepcol::bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? bleepcol::red : bleepcol::line);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    const float as = (float) h * 0.22f;
    juce::Path arrow;
    const float ax = (float) w - (float) h * 0.45f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - as, ay - as * 0.5f, ax + as, ay - as * 0.5f, ax, ay + as * 0.6f);
    g.setColour (bleepcol::inkDim);
    g.fillPath (arrow);
}

juce::Font BleepLook::getComboBoxFont (juce::ComboBox& b)
{
    return bb::fonts::mono (std::max (9.0f, (float) b.getHeight() * 0.6f));
}

juce::Font BleepLook::getPopupMenuFont()
{
    return bb::fonts::mono (14.0f);
}

juce::Font BleepLook::getLabelFont (juce::Label& l)
{
    return bb::fonts::mono (std::max (8.5f, (float) l.getHeight() * 0.8f));
}

juce::Label* BleepLook::createSliderTextBox (juce::Slider& sl)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (sl);
    l->setJustificationType (juce::Justification::centred);
    l->setMinimumHorizontalScale (0.6f);
    return l;
}

// -------------------------------------------------------------- BleepEditor

BleepEditor::Knob* BleepEditor::makeKnob (const juce::String& id, const juce::String& label)
{
    auto k = std::make_unique<Knob>();
    k->label = label;
    auto& sl = k->slider;
    sl.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    sl.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 14);
    sl.setMouseDragSensitivity (220);
    addAndMakeVisible (sl);
    k->att = std::make_unique<SliderAtt> (proc.apvts, id, sl);
    if (auto* prm = proc.apvts.getParameter (id))
        sl.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

BleepEditor::Choice* BleepEditor::makeChoice (const juce::String& id, const juce::String& label)
{
    auto c = std::make_unique<Choice>();
    c->label = label;
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->box.addItemList (prm->choices, 1);
    addAndMakeVisible (c->box);
    c->att = std::make_unique<ComboAtt> (proc.apvts, id, c->box);
    choices.push_back (std::move (c));
    return choices.back().get();
}

BleepEditor::Toggle* BleepEditor::makeToggle (const juce::String& id, const juce::String& text, const juce::String& style)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (text);
    t->button.getProperties().set ("style", style);
    t->button.setClickingTogglesState (true);
    addAndMakeVisible (t->button);
    t->att = std::make_unique<ButtonAtt> (proc.apvts, id, t->button);
    toggles.push_back (std::move (t));
    return toggles.back().get();
}

BleepEditor::BleepEditor (BleepProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    using namespace ParamIDs;

    logo = loadImage ("logo_red_png");
    mech = loadImage ("mech_jpg");
    graffiti = loadTypeface ("SedgwickAveDisplayRegular_ttf");

    // Header: Quelle
    liveBtn.onClick = [this] { setSource (0); };
    slotsBtn.onClick = [this] { setSource (1); };
    addAndMakeVisible (liveBtn);
    addAndMakeVisible (slotsBtn);

    // Generator
    genPanels.push_back ({ "SQUARE", "", { makeKnob (sqOct, "Okt"), makeKnob (sqLevel, "Pegel"), makeKnob (sqPw, "PW") }, {}, 46.0f });
    genPanels.push_back ({ "TRI-MOD", "stumm", { makeKnob (modOct, "Okt"), makeKnob (modPd, "PD") }, {}, 46.0f });
    genPanels.push_back ({ "PITCH ENV", "", { makeKnob (penvAmt, "Amt"), makeKnob (penvDecay, "Decay") }, {}, 46.0f });
    genPanels.push_back ({ "LFO", "", { makeKnob (lfoRate, "Rate"), makeKnob (lfoPitch, "Pitch"), makeKnob (lfoPd, "PD") }, {}, 46.0f });
    genPanels.push_back ({ "AMP ENV", "", { makeKnob (ampA, "A"), makeKnob (ampD, "D"), makeKnob (ampS, "S"), makeKnob (ampR, "R") }, {}, 46.0f });
    pitchLink = makeToggle (ParamIDs::pitchLink, "PITCH-LINK", "onoff");

    generatePanel = { "", "", { makeKnob (variation, "Variation"), makeKnob (slotLen, "Laenge"), makeKnob (rootNote, "Root") }, {}, 46.0f };
    generateBtn.getProperties().set ("style", "primary");
    generateBtn.onClick = [this] { proc.generate(); };
    generateBtn.setTooltip ("Neuer Seed, alle nicht gesperrten Slots neu");
    rerenderBtn.getProperties().set ("style", "rerender");
    rerenderBtn.onClick = [this] { proc.renderSlots(); };
    rerenderBtn.setTooltip ("Gleicher Seed, mit den aktuellen Generator-Einstellungen neu rendern");
    addAndMakeVisible (generateBtn);
    addAndMakeVisible (rerenderBtn);

    // Slots
    for (int i = 0; i < BleepProcessor::kNumSlots; ++i)
    {
        slotLocks[(size_t) i] = makeToggle (ParamIDs::slotLock (i), "", "lock");
        slotKnobs[(size_t) i * 2] = makeKnob (ParamIDs::slotWeight (i), "Gewicht");
        slotKnobs[(size_t) i * 2 + 1] = makeKnob (ParamIDs::slotLevel (i), "Pegel");
    }

    // Sequencer
    addAndMakeVisible (grid);
    seqKnobs = { makeKnob (arpSteps, "Steps"), makeKnob (arpSwing, "Swing"), makeKnob (arpGate, "Gate"),
                 makeKnob (arpRange, "Range"), makeKnob (density, "Dichte"), makeKnob (seed, "Seed") };
    seqChoices = { makeChoice (arpRate, "Rate"), makeChoice (arpMode, "Modus"), makeChoice (arpScale, "Scale"),
                   makeChoice (autoRoll, "Auto"), makeChoice (randMode, "Zufall"), makeChoice (slotMode, "Slots") };
    arpOn = makeToggle (ParamIDs::arpOn, "ARP", "onoff");
    restart = makeToggle (ParamIDs::arpRestart, "RESTART", "onoff");
    rollBtn.getProperties().set ("style", "primary");
    rollBtn.onClick = [this] { proc.rollSteps(); grid.repaint(); };
    addAndMakeVisible (rollBtn);

    // Effekte
    fxPanels.push_back ({ "COMB", "", { makeKnob (combFreq, "Freq"), makeKnob (combFb, "FB"), makeKnob (combDamp, "Damp"),
                                        makeKnob (combKey, "Key"), makeKnob (combCount, "Combs"), makeKnob (combMix, "Mix") }, {}, 44.0f });
    fxPanels.push_back ({ "CRUSH", "", { makeKnob (crushBits, "Bits"), makeKnob (crushRate, "Rate"), makeKnob (crushMix, "Mix") }, {}, 44.0f });
    fxPanels.push_back ({ "FILTER", "", { makeKnob (filterCut, "Cut"), makeKnob (filterRes, "Res"), makeKnob (filterEnv, "Env"),
                                          makeKnob (filterDec, "Dec"), makeKnob (filterKey, "Key") }, {}, 44.0f });
    fxPanels.push_back ({ "DELAY", "", { makeKnob (delayFb, "FB"), makeKnob (delayMix, "Mix"), makeKnob (delayLow, "Lo"),
                                         makeKnob (delayHigh, "Hi") }, {}, 44.0f });
    fxPanels.push_back ({ "OUT", "", { makeKnob (output, "Vol") }, {}, 44.0f });
    fxPower = { makeToggle (combOn, "COMB", "power"), makeToggle (crushOn, "CRUSH", "power"),
                makeToggle (filterOn, "FILTER", "power"), makeToggle (delayOn, "DELAY", "power") };
    filterType = makeChoice (ParamIDs::filterType, "");
    delayTime = makeChoice (ParamIDs::delayTime, "");
    pingPong = makeToggle (delayPP, "PING", "seg");

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (baseW, baseH);

    for (int i = 0; i < BleepProcessor::kNumSlots; ++i)
        seenHits[(size_t) i] = proc.slotHits[(size_t) i].load();
    seenAccents = proc.accentHits.load();
    startTimerHz (60);
}

BleepEditor::~BleepEditor()
{
    setLookAndFeel (nullptr);
}

void BleepEditor::setSource (int index)
{
    if (auto* prm = proc.apvts.getParameter (ParamIDs::source))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
        prm->endChangeGesture();
    }
}

// ------------------------------------------------------------------ Layout

void BleepEditor::place (juce::Component& c, juce::Rectangle<float> base)
{
    c.setBounds (s (base).toNearestInt());
}

void BleepEditor::placeKnobs (Panel& panel, float x, float labelY, float sliderY, float sliderH)
{
    for (size_t i = 0; i < panel.knobs.size(); ++i)
    {
        auto* k = panel.knobs[i];
        const float kx = x + panel.cell * (float) i;
        k->labelArea = { kx, labelY, panel.cell, 12.0f };
        place (k->slider, { kx + 1.0f, sliderY, panel.cell - 2.0f, sliderH });
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::roundToInt ((panel.cell - 2.0f) * u), juce::roundToInt (14.0f * u));
    }
}

void BleepEditor::resized()
{
    u = getWidth() / (float) baseW;

    // Header
    place (liveBtn, { 840.0f, 18.0f, 68.0f, 24.0f });
    place (slotsBtn, { 912.0f, 18.0f, 72.0f, 24.0f });

    // Generator-Reihe
    const float gy = 62.0f, gh = 136.0f;
    float x = 16.0f;
    for (auto& pnl : genPanels)
    {
        const float w = 24.0f + pnl.cell * (float) pnl.knobs.size();
        pnl.area = { x, gy, w, gh };
        placeKnobs (pnl, x + 12.0f, gy + 30.0f, gy + 44.0f, 66.0f);
        if (pnl.title == "TRI-MOD")
            place (pitchLink->button, { x + 12.0f, gy + 113.0f, 92.0f, 17.0f });
        x += w + 8.0f;
    }
    generatePanel.area = { x, gy, 984.0f - x, gh };
    place (generateBtn, { x + 10.0f, gy + 8.0f, generatePanel.area.getWidth() - 56.0f, 34.0f });
    place (rerenderBtn, { generatePanel.area.getRight() - 42.0f, gy + 8.0f, 32.0f, 34.0f });
    placeKnobs (generatePanel, x + (generatePanel.area.getWidth() - 138.0f) * 0.5f, gy + 50.0f, gy + 64.0f, 66.0f);

    // Mech und Slots
    mechArea = { 16.0f, 206.0f, 250.0f, 370.0f };
    for (int i = 0; i < BleepProcessor::kNumSlots; ++i)
    {
        const float tx = 276.0f + (float) i * 179.0f, tw = 171.0f, ty = 206.0f;
        slotTiles[(size_t) i] = { tx, ty, tw, 192.0f };
        waveAreas[(size_t) i] = { tx + 8.0f, ty + 28.0f, tw - 16.0f, 66.0f };
        place (slotLocks[(size_t) i]->button, { tx + tw - 32.0f, ty + 5.0f, 24.0f, 20.0f });
        for (int k = 0; k < 2; ++k)
        {
            auto* kn = slotKnobs[(size_t) (i * 2 + k)];
            const float kx = tx + (tw - 92.0f) * 0.5f + 46.0f * (float) k;
            kn->labelArea = { kx, ty + 102.0f, 46.0f, 12.0f };
            place (kn->slider, { kx + 1.0f, ty + 114.0f, 44.0f, 66.0f });
            kn->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::roundToInt (44.0f * u), juce::roundToInt (14.0f * u));
        }
    }

    // Sequencer
    seqArea = { 276.0f, 406.0f, 708.0f, 170.0f };
    place (grid, { 286.0f, 414.0f, 688.0f, 84.0f });
    for (size_t i = 0; i < seqKnobs.size(); ++i)
    {
        auto* k = seqKnobs[i];
        const float kx = 286.0f + 46.0f * (float) i;
        k->labelArea = { kx, 503.0f, 46.0f, 12.0f };
        place (k->slider, { kx + 1.0f, 515.0f, 44.0f, 58.0f });
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::roundToInt (44.0f * u), juce::roundToInt (13.0f * u));
    }
    for (size_t i = 0; i < seqChoices.size(); ++i)
    {
        const float cx = 572.0f + (float) (i % 2) * 144.0f;
        const float cy = 506.0f + (float) (i / 2) * 23.0f;
        seqChoices[i]->labelArea = { cx, cy, 42.0f, 20.0f };
        place (seqChoices[i]->box, { cx + 42.0f, cy, 96.0f, 20.0f });
    }
    place (arpOn->button, { 864.0f, 506.0f, 110.0f, 20.0f });
    place (restart->button, { 864.0f, 529.0f, 110.0f, 20.0f });
    place (rollBtn, { 864.0f, 552.0f, 110.0f, 22.0f });

    // Effekte
    const float fy = 584.0f, fh = 120.0f;
    x = 16.0f;
    for (size_t i = 0; i < fxPanels.size(); ++i)
    {
        auto& pnl = fxPanels[i];
        const float w = 20.0f + pnl.cell * (float) pnl.knobs.size();
        pnl.area = { x, fy, w, fh };
        placeKnobs (pnl, x + 10.0f, fy + 28.0f, fy + 40.0f, 66.0f);
        if (i < fxPower.size())
            place (fxPower[i]->button, { x + 8.0f, fy + 5.0f, 64.0f, 18.0f });
        if (pnl.title == "FILTER")
            place (filterType->box, { x + w - 62.0f, fy + 5.0f, 54.0f, 18.0f });
        if (pnl.title == "DELAY")
        {
            place (delayTime->box, { x + w - 120.0f, fy + 5.0f, 64.0f, 18.0f });
            place (pingPong->button, { x + w - 52.0f, fy + 5.0f, 44.0f, 18.0f });
        }
        x += w + 8.0f;
    }

    rebuildWaves();
}

// ------------------------------------------------------------------- Logik

juce::Point<float> BleepEditor::muzzle() const
{
    const float ih = mechArea.getHeight(), iw = ih * 640.0f / 960.0f;
    const float ix = mechArea.getCentreX() - iw * 0.5f;
    return { (ix + iw * kMuzzleX) * u, (mechArea.getY() + ih * kMuzzleY) * u };
}

juce::Point<float> BleepEditor::slotTarget (int slot) const
{
    const auto& t = slotTiles[(size_t) juce::jlimit (0, BleepProcessor::kNumSlots - 1, slot)];
    return { t.getCentreX() * u, muzzle().y };
}

void BleepEditor::rebuildWaves()
{
    for (int i = 0; i < BleepProcessor::kNumSlots; ++i)
    {
        auto& path = wavePaths[(size_t) i];
        path.clear();
        const auto& sl = shownSlots[(size_t) i];
        const auto area = s (waveAreas[(size_t) i]).reduced (2.0f * u);
        if (sl == nullptr || area.isEmpty())
            continue;

        const auto* d = sl->audio.getReadPointer (0);
        const int n = sl->audio.getNumSamples();
        const int cols = juce::jmax (2, (int) area.getWidth());
        const float mid = area.getCentreY(), half = area.getHeight() * 0.48f;
        std::vector<float> lo ((size_t) cols), hi ((size_t) cols);
        for (int c = 0; c < cols; ++c)
        {
            const int a = (int) ((int64_t) c * n / cols), b = juce::jmax (a + 1, (int) ((int64_t) (c + 1) * n / cols));
            float mn = 0.0f, mx = 0.0f;
            for (int j = a; j < b && j < n; ++j)
            {
                mn = std::min (mn, d[j]);
                mx = std::max (mx, d[j]);
            }
            lo[(size_t) c] = mn;
            hi[(size_t) c] = mx;
        }
        path.startNewSubPath (area.getX(), mid - hi[0] * half);
        for (int c = 1; c < cols; ++c)
            path.lineTo (area.getX() + (float) c, mid - hi[(size_t) c] * half);
        for (int c = cols; --c >= 0;)
            path.lineTo (area.getX() + (float) c, mid - lo[(size_t) c] * half);
        path.closeSubPath();
    }
}

void BleepEditor::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < BleepProcessor::kNumSlots; ++i)
    {
        if (s (waveAreas[(size_t) i]).contains (e.position))
        {
            proc.auditionSlot (i);
            return;
        }
    }
}

void BleepEditor::timerCallback()
{
    const bool slotMode = proc.apvts.getRawParameterValue (ParamIDs::source)->load() > 0.5f;
    liveBtn.setToggleState (! slotMode, juce::dontSendNotification);
    slotsBtn.setToggleState (slotMode, juce::dontSendNotification);

    bool wavesChanged = false;
    for (int i = 0; i < BleepProcessor::kNumSlots; ++i)
    {
        auto sl = proc.getSlot (i);
        if (sl != shownSlots[(size_t) i])
        {
            shownSlots[(size_t) i] = sl;
            wavesChanged = true;
        }
        const int h = proc.slotHits[(size_t) i].load (std::memory_order_relaxed);
        if (h != seenHits[(size_t) i])
        {
            seenHits[(size_t) i] = h;
            hitGlow[(size_t) i] = 1.0f;
        }
        hitGlow[(size_t) i] *= 0.86f;
        laserGlow[(size_t) i] *= 0.9f;
    }
    if (wavesChanged)
        rebuildWaves();

    // Akzente: der Mech schiesst (hoechstens 4 Strahlen gleichzeitig)
    const int acc = proc.accentHits.load (std::memory_order_relaxed);
    if (acc != seenAccents)
    {
        seenAccents = acc;
        if (bolts.size() < 4)
        {
            Bolt b;
            b.start = now();
            b.target = proc.accentSlot.load (std::memory_order_relaxed);
            b.yOffset = (float) ((acc * 7) % 5 - 2) * 2.0f;
            bolts.push_back (b);
            muzzleFlash = 1.0f;
        }
    }
    muzzleFlash *= 0.8f;

    const double t = now();
    for (auto it = bolts.begin(); it != bolts.end();)
    {
        const float dist = std::abs (slotTarget (it->target).x - muzzle().x) / u;
        const double dur = 0.12 + 0.12 * dist / 700.0;
        if (t - it->start >= dur)
        {
            laserGlow[(size_t) it->target] = 1.0f;
            it = bolts.erase (it);
        }
        else
            ++it;
    }

    repaint (s (juce::Rectangle<float> (16.0f, 200.0f, 968.0f, 204.0f)).toNearestInt());
    grid.repaint();
}

// ---------------------------------------------------------------- Zeichnen

void BleepEditor::paintPanel (juce::Graphics& g, juce::Rectangle<float> base, const juce::String& title, bool highlight)
{
    const auto r = s (base);
    g.setColour (bleepcol::panel);
    g.fillRoundedRectangle (r, 10.0f * u);
    g.setColour (highlight ? bleepcol::gold : bleepcol::line);
    g.drawRoundedRectangle (r, 10.0f * u, 1.0f);
    if (title.isNotEmpty())
    {
        g.setFont (bb::fonts::mono (12.5f * u));
        g.setColour (bleepcol::ink);
        g.drawText (title, s ({ base.getX() + 12.0f, base.getY() + 7.0f, base.getWidth() - 24.0f, 16.0f }), juce::Justification::centredLeft);
    }
}

void BleepEditor::paintHeader (juce::Graphics& g)
{
    // Logo oben links
    if (logo.isValid())
    {
        const float lh = 46.0f, lw = lh * (float) logo.getWidth() / (float) logo.getHeight();
        g.drawImage (logo, s ({ 12.0f, 6.0f, lw, lh }), juce::RectanglePlacement::centred);
    }
    else
    {
        g.setFont (bb::fonts::mono (13.0f * u));
        g.setColour (bleepcol::red);
        g.drawText ("BRAIN BUGZ", s ({ 18.0f, 14.0f, 120.0f, 30.0f }), juce::Justification::centredLeft);
    }

    // Titel in Graffiti-Schrift, mit rotem Glow
    juce::Font font = graffiti != nullptr ? juce::Font (juce::FontOptions (graffiti).withHeight (42.0f * u))
                                          : bb::fonts::display (34.0f * u, true);
    juce::GlyphArrangement ga;
    const auto tr = s ({ 250.0f, 4.0f, 500.0f, 52.0f });
    ga.addFittedText (font, "BLEEP DESIGNER", tr.getX(), tr.getY(), tr.getWidth(), tr.getHeight(), juce::Justification::centred, 1);
    juce::Path title;
    ga.createPath (title);
    g.setColour (bleepcol::red.withAlpha (0.18f));
    g.strokePath (title, juce::PathStrokeType (9.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (bleepcol::red.withAlpha (0.45f));
    g.strokePath (title, juce::PathStrokeType (4.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (bleepcol::red);
    g.strokePath (title, juce::PathStrokeType (1.6f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (bleepcol::ink);
    g.fillPath (title);

    g.setFont (bb::fonts::mono (11.0f * u));
    g.setColour (bleepcol::inkDim);
    g.drawText ("QUELLE", s ({ 780.0f, 18.0f, 56.0f, 24.0f }), juce::Justification::centredRight);
}

void BleepEditor::paintMech (juce::Graphics& g)
{
    const auto r = s (mechArea);
    juce::ColourGradient bgGrad (juce::Colour (0xff24090d), r.getCentreX(), r.getY() + r.getHeight() * 0.4f,
                                 juce::Colour (0xff0b0b0d), r.getX(), r.getBottom(), true);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, 10.0f * u);

    if (mech.isValid())
    {
        const float ih = r.getHeight(), iw = ih * 640.0f / 960.0f;
        const auto ir = juce::Rectangle<float> (r.getCentreX() - iw * 0.5f, r.getY(), iw, ih);
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (r, 10.0f * u);
        g.reduceClipRegion (clip);
        g.drawImage (mech, ir, juce::RectanglePlacement::stretchToFit);

        // Unterkante ins Panel ausblenden
        juce::ColourGradient fade (juce::Colour (0x000b0b0d), r.getX(), r.getY() + r.getHeight() * 0.7f,
                                   juce::Colour (0xff0b0b0d), r.getX(), r.getBottom(), false);
        g.setGradientFill (fade);
        g.fillRect (r);

        // Augen-Glow pulsiert
        const float pulse = 0.5f + 0.5f * std::sin ((float) now() * 3.5f);
        const juce::Point<float> eyes (ir.getX() + iw * kEyesX, ir.getY() + ih * kEyesY);
        const float er = 14.0f * u;
        juce::ColourGradient eg (bleepcol::red.withAlpha (0.25f + 0.35f * pulse), eyes.x, eyes.y,
                                 bleepcol::red.withAlpha (0.0f), eyes.x + er, eyes.y, true);
        g.setGradientFill (eg);
        g.fillEllipse (eyes.x - er, eyes.y - er, er * 2.0f, er * 2.0f);
        g.restoreState();
    }

    g.setColour (bleepcol::line);
    g.drawRoundedRectangle (r, 10.0f * u, 1.0f);
    g.setFont (bb::fonts::mono (10.5f * u));
    g.setColour (bleepcol::inkDim);
    g.drawText ("PEW = AKZENT", s ({ mechArea.getX() + 12.0f, mechArea.getBottom() - 22.0f, 140.0f, 14.0f }), juce::Justification::centredLeft);
}

void BleepEditor::paintSlots (juce::Graphics& g)
{
    for (int i = 0; i < BleepProcessor::kNumSlots; ++i)
    {
        const auto tile = slotTiles[(size_t) i];
        const bool locked = proc.apvts.getRawParameterValue (ParamIDs::slotLock (i))->load() > 0.5f;
        const float glow = std::max (laserGlow[(size_t) i], hitGlow[(size_t) i] * 0.5f);

        if (glow > 0.02f)
        {
            g.setColour (bleepcol::red.withAlpha (0.35f * glow));
            g.fillRoundedRectangle (s (tile).expanded (4.0f * u), 13.0f * u);
        }
        paintPanel (g, tile, "SLOT " + juce::String (i + 1), locked);
        if (glow > 0.02f)
        {
            g.setColour (bleepcol::red.withAlpha (glow));
            g.drawRoundedRectangle (s (tile), 10.0f * u, 1.5f);
        }

        const auto wa = s (waveAreas[(size_t) i]);
        g.setColour (bleepcol::bg);
        g.fillRoundedRectangle (wa, 6.0f * u);
        g.setColour (bleepcol::line);
        g.drawRoundedRectangle (wa, 6.0f * u, 1.0f);
        g.drawHorizontalLine (juce::roundToInt (wa.getCentreY()), wa.getX() + 2.0f, wa.getRight() - 2.0f);
        if (shownSlots[(size_t) i] != nullptr)
        {
            g.setColour ((locked ? bleepcol::gold : bleepcol::ink).withAlpha (0.85f));
            g.fillPath (wavePaths[(size_t) i]);
        }
        else
        {
            g.setFont (bb::fonts::mono (10.0f * u));
            g.setColour (bleepcol::inkDim);
            g.drawText ("leer", wa, juce::Justification::centred);
        }
    }
}

void BleepEditor::paint (juce::Graphics& g)
{
    g.fillAll (bleepcol::bg);
    paintHeader (g);

    for (auto& pnl : genPanels)
    {
        paintPanel (g, pnl.area, pnl.title);
        if (pnl.tag.isNotEmpty())
        {
            const float tw = (float) pnl.title.length() * 7.6f + 18.0f;
            const auto tag = s ({ pnl.area.getX() + tw, pnl.area.getY() + 8.0f, 40.0f, 14.0f });
            g.setColour (bleepcol::inkDim);
            g.fillRoundedRectangle (tag, 3.0f * u);
            g.setFont (bb::fonts::mono (9.5f * u));
            g.setColour (bleepcol::bg);
            g.drawText (pnl.tag, tag, juce::Justification::centred);
        }
    }
    paintPanel (g, generatePanel.area, "");

    paintMech (g);
    paintSlots (g);

    paintPanel (g, seqArea, "");
    for (auto* c : seqChoices)
    {
        g.setFont (bb::fonts::mono (10.0f * u));
        g.setColour (bleepcol::inkDim);
        g.drawText (c->label.toUpperCase(), s (c->labelArea), juce::Justification::centredLeft);
    }

    for (size_t i = 0; i < fxPanels.size(); ++i)
        paintPanel (g, fxPanels[i].area, i < fxPower.size() ? juce::String() : fxPanels[i].title);

    // Regler-Beschriftungen
    g.setFont (bb::fonts::mono (10.0f * u));
    g.setColour (bleepcol::inkDim);
    for (auto& k : knobs)
        if (! k->labelArea.isEmpty())
            g.drawText (k->label.toUpperCase(), s (k->labelArea), juce::Justification::centred);
}

void BleepEditor::paintOverChildren (juce::Graphics& g)
{
    const auto m = muzzle();

    // Muendungsblitz
    if (muzzleFlash > 0.02f)
    {
        const float fr = (14.0f + 10.0f * muzzleFlash) * u;
        juce::ColourGradient fg (juce::Colours::white.withAlpha (0.9f * muzzleFlash), m.x, m.y,
                                 bleepcol::red.withAlpha (0.0f), m.x + fr, m.y, true);
        g.setGradientFill (fg);
        g.fillEllipse (m.x - fr, m.y - fr, fr * 2.0f, fr * 2.0f);
    }

    // Laserschuesse: kurzer Strahl mit hellem Kopf und auslaufendem Schweif
    const double t = now();
    for (const auto& b : bolts)
    {
        const auto target = slotTarget (b.target);
        const float dist = std::abs (target.x - m.x) / u;
        const double dur = 0.12 + 0.12 * dist / 700.0;
        const float p = (float) juce::jlimit (0.0, 1.0, (t - b.start) / dur);
        const float eased = p * (0.7f + 0.3f * p);
        const float headX = m.x + (target.x - m.x) * eased;
        const float y = m.y + b.yOffset * u;
        const float len = juce::jmin (headX - m.x, 70.0f * u);
        if (len <= 1.0f)
            continue;

        const auto beam = juce::Rectangle<float> (headX - len, y - 2.0f * u, len, 4.0f * u);
        g.setColour (bleepcol::red.withAlpha (0.25f));
        g.fillRoundedRectangle (beam.expanded (0.0f, 4.0f * u), 5.0f * u);
        juce::ColourGradient core (bleepcol::red.withAlpha (0.0f), beam.getX(), y,
                                   juce::Colour (0xffffe1e4), beam.getRight(), y, false);
        core.addColour (0.7, bleepcol::red);
        g.setGradientFill (core);
        g.fillRoundedRectangle (beam, 2.0f * u);
    }
}
