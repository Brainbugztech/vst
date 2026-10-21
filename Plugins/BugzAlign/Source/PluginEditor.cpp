#include "PluginEditor.h"

namespace
{
    constexpr float knobY = 506.0f, knobS = 92.0f;
    struct KnobDef { const char* id; const char* label; float cx; bool brain; double def; };
    const KnobDef kKnobs[6] {
        { ParamIDs::amount, "AMOUNT", 70.0f, false, 1.0 },
        { ParamIDs::split,  "SPLIT", 185.0f, true, 180.0 },
        { ParamIDs::window, "WINDOW", 300.0f, false, 50.0 },
        { ParamIDs::glide,  "GLIDE", 415.0f, false, 15.0 },
        { ParamIDs::sens,   "KICK SENS", 530.0f, false, 0.5 },
        { ParamIDs::offset, "OFFSET", 645.0f, true, 0.0 },
    };
}

BugzAlignEditor::BugzAlignEditor (BugzAlignProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    addAndMakeVisible (view);

    juce::Slider* sliders[6] { &amount, &split, &window, &glide, &sens, &offset };
    for (int i = 0; i < 6; ++i)
    {
        auto* s = sliders[i];
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (bb::LookAndFeel::startAngle, bb::LookAndFeel::endAngle, true);
        s->setMouseDragSensitivity (220);
        s->setVelocityBasedMode (false);
        if (kKnobs[i].brain) s->getProperties().set ("accent", "brain");
        s->setTitle (kKnobs[i].label);
        addAndMakeVisible (*s);
        sAtt.push_back (std::make_unique<SA> (proc.apvts, kKnobs[i].id, *s));
        s->setDoubleClickReturnValue (true, kKnobs[i].def);
    }

    for (auto* b : { &holdBtn, &abBtn, &subBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    abBtn.getProperties().set ("accent", "brain");
    subBtn.getProperties().set ("accent", "brain");
    holdBtn.setTooltip ("Winkel einfrieren");
    abBtn.setTooltip ("Korrektur aus zum Vergleichen");
    subBtn.setTooltip ("Nur Sub von Bass + Kick abhoeren (mono)");
    bAtt.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::hold, holdBtn));
    bAtt.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::bypass, abBtn));
    bAtt.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::subCheck, subBtn));

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);
    startTimerHz (30);
}

BugzAlignEditor::~BugzAlignEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BugzAlignEditor::resized()
{
    u = (float) getWidth() / (float) baseW;
    view.setBounds (R (20, 76, 860, 348).toNearestInt());
    juce::Slider* sliders[6] { &amount, &split, &window, &glide, &sens, &offset };
    for (int i = 0; i < 6; ++i)
        sliders[i]->setBounds (R (kKnobs[i].cx - knobS / 2, knobY - knobS / 2, knobS, knobS).toNearestInt());
    holdBtn.setBounds (R (730, 462, 150, 36).toNearestInt());
    abBtn.setBounds   (R (730, 508, 150, 36).toNearestInt());
    subBtn.setBounds  (R (730, 554, 150, 36).toNearestInt());
}

void BugzAlignEditor::timerCallback()
{
    view.update();
    repaint (R (0, 548, 720, 50).getSmallestIntegerContainer());
}

void BugzAlignEditor::paint (juce::Graphics& g)
{
    g.fillAll (bb::col::voidBg);

    const auto full = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (bb::col::tox.withAlpha (0.05f), 0, full.getBottom(),
                                             bb::col::tox.withAlpha (0.0f), full.getWidth() * 0.45f, full.getBottom(), true));
    g.fillRect (full);
    g.setGradientFill (juce::ColourGradient (bb::col::brain.withAlpha (0.06f), full.getRight(), 0,
                                             bb::col::brain.withAlpha (0.0f), full.getRight() - full.getWidth() * 0.45f, 0, true));
    g.fillRect (full);

    // Kopfzeile: Logo, Name mit pinkem A, Untertitel
    const float lh = 48 * u;
    const float lw = lh * (float) logo.getWidth() / (float) std::max (1, logo.getHeight());
    g.drawImage (logo, juce::Rectangle<float> (18 * u, 12 * u, lw, lh), juce::RectanglePlacement::centred);

    const float x0 = 18 * u + lw + 16 * u;
    g.setFont (bb::fonts::display (30 * u, true));
    float x = x0;
    const char* letters = "BUGZ ALIGN";
    for (int i = 0; letters[i] != 0; ++i)
    {
        const juce::String ch = juce::String::charToString ((juce::juce_wchar) letters[i]);
        g.setColour (i == 5 ? bb::col::brain : bb::col::ink);
        g.drawText (ch, juce::Rectangle<float> (x, 10 * u, 40 * u, 32 * u), juce::Justification::left);
        x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), ch) + (letters[i] == ' ' ? 2 : 4) * u;
    }
    g.setFont (bb::fonts::mono (12 * u));
    g.setColour (bb::col::inkDim);
    g.drawText (juce::String::fromUTF8 ("Kick/Bass Phase-Align \xc2\xb7 Sidechain \xc2\xb7 linearphasig \xc2\xb7 v0.2"),
                juce::Rectangle<float> (x0, 42 * u, 400 * u, 16 * u), juce::Justification::left);
    g.drawText ("BRAIN BUGZ", R (680, 0, 200, 70), juce::Justification::centredRight);

    g.setFont (bb::fonts::mono (10.5f * u));
    g.setColour (bb::col::inkDim.withAlpha (0.6f));
    g.drawText (juce::String::fromUTF8 ("Auf den Bass \xc2\xb7 Kick per Sidechain \xc2\xb7 Split \xc3\xbc" "ber den Bass-Grundton \xc2\xb7 Doppelklick = Standard"),
                R (20, 428, 860, 14), juce::Justification::centredRight);

    g.setColour (bb::col::line);
    g.fillRect (R (20, 446, 860, 1));

    for (const auto& kd : kKnobs)
    {
        juce::String spaced;
        for (auto ch : juce::String (kd.label)) spaced << juce::String::charToString (ch) << " ";
        g.setFont (bb::fonts::display (12 * u));
        g.setColour (kd.brain ? bb::col::brain : bb::col::tox);
        g.drawText (spaced.trim(), R (kd.cx - 60, 552, 120, 18), juce::Justification::centred);
        g.setFont (bb::fonts::mono (15 * u));
        g.setColour (bb::col::ink);
        g.drawText (proc.apvts.getParameter (kd.id)->getCurrentValueAsText(), R (kd.cx - 60, 570, 120, 22),
                    juce::Justification::centred);
    }
}
