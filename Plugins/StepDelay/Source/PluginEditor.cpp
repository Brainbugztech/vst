#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    void panel (juce::Graphics& g, juce::Rectangle<float> r, float u)
    {
        g.setColour (bb::col::panel);
        g.fillRoundedRectangle (r, 8.0f * u);
        g.setColour (bb::col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f * u, 1.0f);
    }
}

StepDelayEditor::StepDelayEditor (StepDelayProcessor& p)
    : AudioProcessorEditor (&p), proc (p), bars (p)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    addAndMakeVisible (bars);

    // --- Raster-Taster ---
    syncBtn.onClick = [this] { proc.setParam (ParamIDs::sync, proc.getParam (ParamIDs::sync) > 0.5f ? 0.0f : 1.0f); };
    syncBtn.getProperties().set ("accent", "brain");
    addAndMakeVisible (syncBtn);

    for (int i = 0; i < bb::stepgrid::kNumRates; ++i)
    {
        auto& b = rateBtns[(size_t) i];
        b.setButtonText (bb::stepgrid::rateNames[i]);
        b.getProperties().set ("mono", true);
        b.onClick = [this, i]
        {
            proc.setParam (ParamIDs::rate, (float) i);
            proc.setParam (ParamIDs::sync, 1.0f);
        };
        addAndMakeVisible (b);
    }
    for (int i = 0; i < bb::stepgrid::kNumModes; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (juce::String (bb::stepgrid::modeNames[i]).toUpperCase());
        b.getProperties().set ("accent", "brain");
        b.onClick = [this, i]
        {
            proc.setParam (ParamIDs::mode, (float) i);
            proc.setParam (ParamIDs::sync, 1.0f);
        };
        addAndMakeVisible (b);
    }

    // --- Regler ---
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& s = knobs[(size_t) i];
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (bb::LookAndFeel::startAngle, bb::LookAndFeel::endAngle, true);
        s.setMouseDragSensitivity (200);
        s.setTitle (knobNames[(size_t) i]);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        knobAtts[(size_t) i] = std::make_unique<SA> (proc.apvts, knobIds[(size_t) i], s);
    }
    knobs[0].setMouseDragSensitivity (140);
    knobs[0].getProperties().set ("accent", "brain");
    knobs[2].getProperties().set ("accent", "brain");
    knobs[0].setDoubleClickReturnValue (true, 8.0);
    knobs[1].setDoubleClickReturnValue (true, 250.0);
    knobs[2].setDoubleClickReturnValue (true, 0.4);
    knobs[3].setDoubleClickReturnValue (true, 20.0);
    knobs[4].setDoubleClickReturnValue (true, 20000.0);
    knobs[5].setDoubleClickReturnValue (true, 0.0);

    // --- Muster-Werkzeuge ---
    const auto& list = bb::stepdelay::presets();
    for (int i = 0; i < (int) list.size(); ++i)
        presetBox.addItem (juce::String::fromUTF8 (list[(size_t) i].name), i + 1);
    presetBox.setTextWhenNothingSelected ("Muster-Presets");
    presetBox.setColour (juce::ComboBox::backgroundColourId, bb::col::panel2);
    presetBox.setColour (juce::ComboBox::textColourId, bb::col::ink);
    presetBox.setColour (juce::ComboBox::outlineColourId, bb::col::line);
    presetBox.setColour (juce::ComboBox::arrowColourId, bb::col::tox);
    getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId, bb::col::panel);
    getLookAndFeel().setColour (juce::PopupMenu::textColourId, bb::col::ink);
    getLookAndFeel().setColour (juce::PopupMenu::highlightedBackgroundColourId, bb::col::toxDim);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) proc.setCurrentProgram (id - 1);
    };
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    addAndMakeVisible (presetBox);

    randBtn.onClick  = [this] { proc.randomizePattern(); };
    resetBtn.onClick = [this] { proc.resetPattern(); };
    invBtn.onClick   = [this] { proc.invertPattern(); };
    leftBtn.onClick  = [this] { proc.shiftPattern (-1); };
    rightBtn.onClick = [this] { proc.shiftPattern (1); };
    randBtn.getProperties().set ("accent", "brain");
    leftBtn.setTooltip ("Muster nach links schieben");
    rightBtn.setTooltip ("Muster nach rechts schieben");
    for (auto* b : { &randBtn, &resetBtn, &invBtn, &leftBtn, &rightBtn })
        addAndMakeVisible (*b);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    lastMs = juce::Time::getMillisecondCounterHiRes();
    timerCallback();
    startTimerHz (30);
}

StepDelayEditor::~StepDelayEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void StepDelayEditor::resized()
{
    u = (float) getWidth() / (float) baseW;
    bars.setBounds (R (20, 76, 940, 290).toNearestInt());

    presetBox.setBounds (R (20, 378, 300, 32).toNearestInt());
    randBtn.setBounds  (R (332, 378, 92, 32).toNearestInt());
    resetBtn.setBounds (R (432, 378, 92, 32).toNearestInt());
    invBtn.setBounds   (R (532, 378, 92, 32).toNearestInt());
    leftBtn.setBounds  (R (636, 378, 40, 32).toNearestInt());
    rightBtn.setBounds (R (682, 378, 40, 32).toNearestInt());

    syncBtn.setBounds (R (36, 458, 80, 32).toNearestInt());
    for (int i = 0; i < bb::stepgrid::kNumRates; ++i)
        rateBtns[(size_t) i].setBounds (R (36.0f + (float) i * 56.0f, 502, 50, 32).toNearestInt());
    for (int i = 0; i < bb::stepgrid::kNumModes; ++i)
        modeBtns[(size_t) i].setBounds (R (36.0f + (float) i * 130.0f, 546, 124, 32).toNearestInt());

    for (int i = 0; i < kNumKnobs; ++i)
        knobs[(size_t) i].setBounds (R (462.0f + (float) i * 82.0f, 462, 76, 76).toNearestInt());
}

void StepDelayEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.1, (now - lastMs) / 1000.0);
    lastMs = now;
    bugPhase += dt * 4.0;

    const bool sync = proc.getParam (ParamIDs::sync) > 0.5f;
    const int rate = (int) proc.getParam (ParamIDs::rate);
    const int mode = (int) proc.getParam (ParamIDs::mode);
    syncBtn.setToggleState (sync, juce::dontSendNotification);
    for (int i = 0; i < bb::stepgrid::kNumRates; ++i)
        rateBtns[(size_t) i].setToggleState (sync && i == rate, juce::dontSendNotification);
    for (int i = 0; i < bb::stepgrid::kNumModes; ++i)
        modeBtns[(size_t) i].setToggleState (sync && i == mode, juce::dontSendNotification);

    // Zeit-Regler nur im freien Modus
    knobs[1].setEnabled (! sync);
    knobs[1].setAlpha (sync ? 0.35f : 1.0f);

    bars.tick();
    repaint (R (0, 0, (float) baseW, 64).getSmallestIntegerContainer());
    repaint (R (20, 424, 940, 200).getSmallestIntegerContainer());
}

//==============================================================================
void StepDelayEditor::paint (juce::Graphics& g)
{
    using namespace bb;
    g.fillAll (col::voidBg);
    paintHeader (g);

    // Raster-Panel
    auto left = R (20, 424, 420, 200);
    panel (g, left, u);
    g.setFont (fonts::display (14 * u, true));
    g.setColour (col::inkDim);
    g.drawText ("RASTER", R (36, 432, 200, 20), juce::Justification::centredLeft, false);

    const double ms = proc.stepLengthMs();
    const bool sync = proc.getParam (ParamIDs::sync) > 0.5f;
    g.setFont (fonts::mono (17 * u));
    g.setColour (col::tox);
    g.drawText ("1 Step = " + proc.gridName() + "  " + juce::String (ms, ms < 100 ? 1 : 0) + " ms",
                R (128, 458, 300, 32), juce::Justification::centredLeft, false);
    g.setFont (fonts::mono (13 * u));
    g.setColour (col::inkDim);
    g.drawText (sync ? "Tempo " + juce::String (proc.vizBpm.load(), 1) + " BPM, alle Steps im selben Raster"
                     : juce::String ("frei: Zeit-Regler bestimmt den Abstand"),
                R (36, 586, 400, 24), juce::Justification::centredLeft, false);

    // Regler-Panel
    auto right = R (452, 424, 508, 200);
    panel (g, right, u);
    for (int i = 0; i < kNumKnobs; ++i)
    {
        const float x = 462.0f + (float) i * 82.0f;
        g.setFont (fonts::display (13 * u, true));
        g.setColour (i == 1 && sync ? col::inkDim.withAlpha (0.4f) : col::inkDim);
        g.drawText (knobNames[(size_t) i], R (x - 6, 436, 88, 20), juce::Justification::centred, false);

        juce::String val;
        if (auto* prm = proc.apvts.getParameter (knobIds[(size_t) i]))
            val = prm->getCurrentValueAsText();
        if (i == 1 && sync) val = "sync";
        g.setFont (fonts::mono (15 * u));
        g.setColour (col::ink);
        g.drawText (val, R (x - 6, 544, 88, 22), juce::Justification::centred, false);
    }

    // Hinweis zur Bedienung
    g.setFont (fonts::mono (12 * u));
    g.setColour (col::inkDim.withAlpha (0.8f));
    g.drawText (juce::String::fromUTF8 ("Ziehen = Lautst\xc3\xa4rke \xc2\xb7 Rechtsklick = stumm \xc2\xb7 Shift = Raster"),
                R (452, 586, 508, 24), juce::Justification::centred, false);
}

void StepDelayEditor::paintHeader (juce::Graphics& g)
{
    using namespace bb;
    auto r = R (0, 0, (float) baseW, 64);
    g.setColour (juce::Colour (0xff0c0810));
    g.fillRect (r);
    g.setColour (col::line);
    g.drawHorizontalLine ((int) r.getBottom() - 1, 0, r.getRight());

    // Logo
    const float lh = 48 * u;
    const float lw = lh * (float) logo.getWidth() / (float) std::max (1, logo.getHeight());
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (18 * u, (r.getHeight() - lh) / 2, lw, lh), juce::RectanglePlacement::centred);

    // Produktname: STEP grün, DELAY pink
    float x = 18 * u + lw + 18 * u;
    g.setFont (fonts::display (30 * u, true));
    const juce::String a ("STEP "), b ("DELAY");
    g.setColour (col::tox);
    g.drawText (a, juce::Rectangle<float> (x, 0, 200 * u, r.getHeight()), juce::Justification::centredLeft, false);
    x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), a);
    g.setColour (col::brain);
    g.drawText (b, juce::Rectangle<float> (x, 0, 200 * u, r.getHeight()), juce::Justification::centredLeft, false);
    x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), b);

    paintBug (g, { x + 34 * u, r.getCentreY() }, 34 * u);

    // Anzeige rechts
    const int n = (int) proc.getParam (ParamIDs::steps);
    const double ms = proc.stepLengthMs();
    const double total = ms * n / 1000.0;
    g.setFont (fonts::mono (16 * u));
    g.setColour (col::ink);
    g.drawText (proc.gridName() + juce::String::fromUTF8 ("  \xc2\xb7  ") + juce::String (n) + (n == 1 ? " Step" : " Steps"),
                R (600, 10, 362, 22), juce::Justification::centredRight, false);
    g.setColour (total > bb::StepDelayEngine::kMaxSeconds ? col::brain : col::inkDim);
    g.drawText (juce::String::fromUTF8 ("letztes Echo nach ") + juce::String (total, 2) + " s"
                    + (total > bb::StepDelayEngine::kMaxSeconds ? " (max 24 s)" : ""),
                R (600, 32, 362, 22), juce::Justification::centredRight, false);
}

// Kleiner Echo-Käfer: krabbelt im Takt mit, Fühler leuchten mit den Echos
void StepDelayEditor::paintBug (juce::Graphics& g, juce::Point<float> c, float size)
{
    using namespace bb;
    float act = 0.0f;
    for (int k = 0; k < StepDelayProcessor::kMaxSteps; ++k)
        act = std::max (act, proc.vizTap[(size_t) k].load (std::memory_order_relaxed));
    act = juce::jlimit (0.0f, 1.0f, act * 1.5f);

    const float s = size / 34.0f;
    const float wig = (float) std::sin (bugPhase) * 3.0f * s;

    // Beine
    g.setColour (col::toxDim);
    for (int side : { -1, 1 })
        for (int i = 0; i < 3; ++i)
        {
            const float y = c.y - 6 * s + (float) i * 6 * s;
            const float sway = (i % 2 == 0 ? wig : -wig) * (float) side;
            juce::Path leg;
            leg.startNewSubPath (c.x + 6.0f * s * (float) side, y);
            leg.lineTo (c.x + 13.0f * s * (float) side, y - 3 * s + sway);
            leg.lineTo (c.x + 17.0f * s * (float) side, y + 2 * s + sway);
            g.strokePath (leg, juce::PathStrokeType (1.6f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

    // Körper (zwei Flügeldecken) und Kopf
    auto body = juce::Rectangle<float> (c.x - 9 * s, c.y - 9 * s, 18 * s, 22 * s);
    juce::ColourGradient grad (col::tox, body.getCentreX(), body.getY(), col::toxDim.darker (0.6f), body.getCentreX(), body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (c.x, body.getY() + 2 * s, c.x, body.getBottom() - 1 * s, 1.2f * s);
    // Echo-Ringe auf dem Rücken
    g.setColour (col::brain.withAlpha (0.35f + 0.65f * act));
    for (int i = 0; i < 3; ++i)
    {
        const float rr = (2.0f + (float) i * 2.5f) * s;
        g.drawEllipse (c.x - rr, c.y + 3 * s - rr, rr * 2, rr * 2, 0.9f * s);
    }
    g.setColour (col::panel2);
    g.fillEllipse (c.x - 6 * s, c.y - 15 * s, 12 * s, 8 * s);
    g.setColour (col::brain);
    g.fillEllipse (c.x - 4 * s, c.y - 13 * s, 2.6f * s, 2.6f * s);
    g.fillEllipse (c.x + 1.4f * s, c.y - 13 * s, 2.6f * s, 2.6f * s);

    // Fühler
    for (int side : { -1, 1 })
    {
        juce::Path ant;
        ant.startNewSubPath (c.x + 2 * s * (float) side, c.y - 14 * s);
        ant.quadraticTo (c.x + 6 * s * (float) side, c.y - 22 * s, c.x + (10 * s + wig * 0.4f) * (float) side, c.y - 21 * s);
        g.setColour (col::toxDim);
        g.strokePath (ant, juce::PathStrokeType (1.3f * s));
        const auto tip = juce::Point<float> (c.x + (10 * s + wig * 0.4f) * (float) side, c.y - 21 * s);
        g.setColour (col::brain.withAlpha (0.25f + 0.75f * act));
        g.fillEllipse (tip.x - 2.2f * s, tip.y - 2.2f * s, 4.4f * s, 4.4f * s);
    }
}
