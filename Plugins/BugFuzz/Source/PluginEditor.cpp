#include "PluginEditor.h"

namespace
{
    // Reglerpositionen (Mittelpunkt x) im 900er-Raster
    constexpr float knobY = 384.0f, knobS = 128.0f;
    constexpr float driveX = 150.0f, mixX = 370.0f, outX = 590.0f;
}

BugFuzzEditor::BugFuzzEditor (BugFuzzProcessor& p)
    : AudioProcessorEditor (&p), proc (p), bands (p)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    addAndMakeVisible (bands);

    for (auto* s : { &drive, &mix, &output })
    {
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (bb::LookAndFeel::startAngle, bb::LookAndFeel::endAngle, true);
        s->setMouseDragSensitivity (220);
        s->setVelocityBasedMode (false);
        addAndMakeVisible (*s);
    }
    drive.getProperties().set ("krass", true);
    mix.getProperties().set ("accent", "brain");
    drive.setTitle ("Drive");
    mix.setTitle ("Mix");
    output.setTitle ("Output");

    driveAtt = std::make_unique<SA> (proc.apvts, ParamIDs::drive, drive);
    mixAtt   = std::make_unique<SA> (proc.apvts, ParamIDs::mix, mix);
    outAtt   = std::make_unique<SA> (proc.apvts, ParamIDs::out, output);
    drive.setDoubleClickReturnValue (true, 0.45);
    mix.setDoubleClickReturnValue (true, 1.0);
    output.setDoubleClickReturnValue (true, 0.0);

    for (auto* b : { &octaBtn, &agBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    agBtn.getProperties().set ("accent", "brain");
    octaAtt = std::make_unique<BA> (proc.apvts, ParamIDs::octa, octaBtn);
    agAtt   = std::make_unique<BA> (proc.apvts, ParamIDs::autoGain, agBtn);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);
    startTimerHz (15);
}

BugFuzzEditor::~BugFuzzEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BugFuzzEditor::resized()
{
    u = (float) getWidth() / (float) baseW;
    bands.setBounds (R (20, 76, 860, 230).toNearestInt());
    drive.setBounds  (R (driveX - knobS / 2, knobY - knobS / 2, knobS, knobS).toNearestInt());
    mix.setBounds    (R (mixX - knobS / 2, knobY - knobS / 2, knobS, knobS).toNearestInt());
    output.setBounds (R (outX - knobS / 2, knobY - knobS / 2, knobS, knobS).toNearestInt());
    octaBtn.setBounds (R (720, 350, 150, 40).toNearestInt());
    agBtn.setBounds   (R (720, 404, 150, 40).toNearestInt());
}

void BugFuzzEditor::timerCallback()
{
    // nur die Werte-Zeile unter den Reglern neu zeichnen
    repaint (R (0, 452, 900, 60).getSmallestIntegerContainer());
}

void BugFuzzEditor::paint (juce::Graphics& g)
{
    g.fillAll (bb::col::voidBg);

    // leichter Pink/Grün-Schimmer im Hintergrund
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (bb::col::tox.withAlpha (0.05f), 0, full.getBottom(),
                                             bb::col::tox.withAlpha (0.0f), full.getWidth() * 0.45f, full.getBottom(), true));
    g.fillRect (full);
    g.setGradientFill (juce::ColourGradient (bb::col::brain.withAlpha (0.06f), full.getRight(), 0,
                                             bb::col::brain.withAlpha (0.0f), full.getRight() - full.getWidth() * 0.45f, 0, true));
    g.fillRect (full);

    // Kopfzeile: Logo, Name mit pinkem F, Untertitel
    const float lh = 48 * u;
    const float lw = lh * (float) logo.getWidth() / (float) std::max (1, logo.getHeight());
    g.drawImage (logo, juce::Rectangle<float> (18 * u, 12 * u, lw, lh), juce::RectanglePlacement::centred);

    const float x0 = 18 * u + lw + 16 * u;
    g.setFont (bb::fonts::display (30 * u, true));
    float x = x0;
    const char* letters = "BUGFUZZ";
    for (int i = 0; letters[i] != 0; ++i)
    {
        const juce::String ch = juce::String::charToString ((juce::juce_wchar) letters[i]);
        g.setColour (i == 3 ? bb::col::brain : bb::col::ink);
        g.drawText (ch, juce::Rectangle<float> (x, 10 * u, 40 * u, 32 * u), juce::Justification::left);
        x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), ch) + 4 * u;
    }
    g.setFont (bb::fonts::mono (12 * u));
    g.setColour (bb::col::inkDim);
    g.drawText (juce::String::fromUTF8 ("Multiband-Distortion \xc2\xb7 4 B\xc3\xa4nder \xc2\xb7 v0.2"),
                juce::Rectangle<float> (x0, 42 * u, 400 * u, 16 * u), juce::Justification::left);
    g.drawText ("BRAIN BUGZ", R (680, 0, 200, 70), juce::Justification::centredRight);

    // Hinweis über der Band-Anzeige
    g.setFont (bb::fonts::mono (10.5f * u));
    g.setColour (bb::col::inkDim.withAlpha (0.6f));
    g.drawText (juce::String::fromUTF8 ("Weichen seitlich ziehen \xc2\xb7 B\xc3\xa4nder hoch/runter \xc2\xb7 Doppelklick = Standard"),
                R (20, 308, 860, 14), juce::Justification::centredRight);

    // Trennlinie zum Regler-Bereich
    g.setColour (bb::col::line);
    g.fillRect (R (20, 324, 860, 1));

    // Regler-Beschriftungen und Werte
    struct K { float cx; const char* label; const char* id; juce::Colour c; };
    const auto prm = proc.readParams();
    const auto driveCol = prm.drive > 0.5f ? bb::col::brain : bb::col::tox;
    for (const auto& k : { K { driveX, "DRIVE", ParamIDs::drive, driveCol }, K { mixX, "MIX", ParamIDs::mix, bb::col::brain },
                           K { outX, "OUTPUT", ParamIDs::out, bb::col::tox } })
    {
        juce::String spaced;
        for (auto ch : juce::String (k.label)) spaced << juce::String::charToString (ch) << " ";
        g.setFont (bb::fonts::display (13 * u));
        g.setColour (k.c);
        g.drawText (spaced.trim(), R (k.cx - 90, 454, 180, 20), juce::Justification::centred);
        g.setFont (bb::fonts::mono (16 * u));
        g.setColour (bb::col::ink);
        g.drawText (proc.apvts.getParameter (k.id)->getCurrentValueAsText(), R (k.cx - 90, 476, 180, 22),
                    juce::Justification::centred);
    }

    // kleine Info rechts unten
    float maxDb = 0.0f;
    for (auto b : prm.band) maxDb = std::max (maxDb, bb::BugFuzzEngine::bandGainDb (prm.drive, b));
    g.setFont (bb::fonts::mono (11 * u));
    g.setColour (bb::col::inkDim);
    g.drawText ("Fuzz bis +" + juce::String (juce::roundToInt (maxDb)) + " dB", R (700, 458, 180, 16), juce::Justification::centred);
    g.drawText ("8x Oversampling", R (700, 478, 180, 16), juce::Justification::centred);
}
