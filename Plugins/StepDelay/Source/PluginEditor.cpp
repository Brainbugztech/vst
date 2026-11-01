#include "PluginEditor.h"
#include "Presets.h"

using namespace sd;

StepDelayEditor::StepDelayEditor (StepDelayProcessor& p)
    : AudioProcessorEditor (&p), proc (p), bars (p)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    addAndMakeVisible (bars);

    // --- Pattern Control ---
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    prevBtn.setTooltip ("Vorheriges Preset");
    nextBtn.setTooltip (juce::String::fromUTF8 ("N\xc3\xa4" "chstes Preset"));

    syncBtn.onClick = [this] { proc.setParam (ParamIDs::sync, proc.getParam (ParamIDs::sync) > 0.5f ? 0.0f : 1.0f); };

    for (int i = 0; i < bb::stepgrid::kNumRates; ++i)
    {
        auto& b = rateBtns[(size_t) i];
        b.setButtonText (bb::stepgrid::rateNames[i]);
        b.onClick = [this, i]
        {
            proc.setParam (ParamIDs::rate, (float) i);
            proc.setParam (ParamIDs::sync, 1.0f);
        };
    }
    const char* modeLabels[] = { "GERADE", "PUNKT.", "TRIOLE" };
    for (int i = 0; i < bb::stepgrid::kNumModes; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (modeLabels[i]);
        b.onClick = [this, i]
        {
            proc.setParam (ParamIDs::mode, (float) i);
            proc.setParam (ParamIDs::sync, 1.0f);
        };
    }
    invBtn.onClick   = [this] { proc.invertPattern(); };
    resetBtn.onClick = [this] { proc.resetPattern(); };
    resetBtn.getProperties().set ("accent", true);

    for (auto* b : { &prevBtn, &nextBtn, &syncBtn, &invBtn, &resetBtn })
        addAndMakeVisible (*b);
    for (auto& b : rateBtns) addAndMakeVisible (b);
    for (auto& b : modeBtns) addAndMakeVisible (b);

    // --- Sound Engine ---
    const juce::Colour rings[kNumKnobs] = { col::magenta, col::yellow, col::magenta, col::cyan, col::yellow, col::magenta };
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& s = knobs[(size_t) i];
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (bb::LookAndFeel::startAngle, bb::LookAndFeel::endAngle, true);
        s.setMouseDragSensitivity (200);
        s.setTitle (knobNames[(size_t) i]);
        s.getProperties().set ("ring", (juce::int64) rings[i].getARGB());
        addAndMakeVisible (s);
        knobAtts[(size_t) i] = std::make_unique<SA> (proc.apvts, knobIds[(size_t) i], s);
    }
    knobs[0].setMouseDragSensitivity (140);
    knobs[0].setDoubleClickReturnValue (true, 8.0);
    knobs[1].setDoubleClickReturnValue (true, 250.0);
    knobs[2].setDoubleClickReturnValue (true, 0.4);
    knobs[3].setDoubleClickReturnValue (true, 20.0);
    knobs[4].setDoubleClickReturnValue (true, 20000.0);
    knobs[5].setDoubleClickReturnValue (true, 0.0);

    getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId, col::panel);
    getLookAndFeel().setColour (juce::PopupMenu::textColourId, col::ink);
    getLookAndFeel().setColour (juce::PopupMenu::highlightedBackgroundColourId, col::magentaDk);

    setResizable (true, true);
    setResizeLimits (baseW * 2 / 3, baseH * 2 / 3, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (baseW, baseH);

    timerCallback();
    startTimerHz (30);
}

StepDelayEditor::~StepDelayEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void StepDelayEditor::stepPreset (int dir)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + dir + n) % n);
    repaint();
}

void StepDelayEditor::resized()
{
    u = (float) getWidth() / (float) designW;
    renderBackground();

    bars.setBounds (R (8, 74, 984, 172).toNearestInt());

    const float by = 272, bh = 32;
    prevBtn.setBounds (R (22, by, 34, bh).toNearestInt());
    nextBtn.setBounds (R (202, by, 34, bh).toNearestInt());
    syncBtn.setBounds (R (258, by, 52, bh).toNearestInt());
    for (int i = 0; i < bb::stepgrid::kNumRates; ++i)
        rateBtns[(size_t) i].setBounds (R (318.0f + (float) i * 50.3f, by, 44, bh).toNearestInt());
    modeBtns[0].setBounds (R (680, by, 58, bh).toNearestInt());
    modeBtns[1].setBounds (R (742, by, 54, bh).toNearestInt());
    modeBtns[2].setBounds (R (800, by, 54, bh).toNearestInt());
    invBtn.setBounds   (R (858, by, 56, bh).toNearestInt());
    resetBtn.setBounds (R (920, by, 58, bh).toNearestInt());

    for (int i = 0; i < kNumKnobs; ++i)
        knobs[(size_t) i].setBounds (R (knobX[(size_t) i] - 52, 352, 104, 104).toNearestInt());
}

void StepDelayEditor::mouseDown (const juce::MouseEvent& e)
{
    // Klick auf den Preset-Namen: Liste aller Presets
    if (! R (62, 272, 134, 32).contains (e.position)) return;

    juce::PopupMenu m;
    const auto& list = bb::stepdelay::presets();
    for (int i = 0; i < (int) list.size(); ++i)
        m.addItem (i + 1, juce::String::fromUTF8 (list[(size_t) i].name), true, i == proc.getCurrentProgram());
    m.addSeparator();
    m.addItem (100, "Zufallsmuster");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (R (62, 304, 134, 1).toNearestInt())),
                     [this] (int id)
                     {
                         if (id == 100) proc.randomizePattern();
                         else if (id > 0) proc.setCurrentProgram (id - 1);
                         repaint();
                     });
}

void StepDelayEditor::timerCallback()
{
    const bool sync = proc.getParam (ParamIDs::sync) > 0.5f;
    const int rate = (int) proc.getParam (ParamIDs::rate);
    const int mode = (int) proc.getParam (ParamIDs::mode);
    syncBtn.setToggleState (sync, juce::dontSendNotification);
    for (int i = 0; i < bb::stepgrid::kNumRates; ++i)
        rateBtns[(size_t) i].setToggleState (sync && i == rate, juce::dontSendNotification);
    for (int i = 0; i < bb::stepgrid::kNumModes; ++i)
        modeBtns[(size_t) i].setToggleState (sync && i == mode, juce::dontSendNotification);

    // Zeit-Regler gilt nur ohne Sync
    knobs[1].setEnabled (! sync);
    knobs[1].setAlpha (sync ? 0.55f : 1.0f);

    bars.tick();
    repaint (R (62, 272, 134, 32).getSmallestIntegerContainer());
    repaint (R (8, 452, 984, 36).getSmallestIntegerContainer());
}

//==============================================================================
void StepDelayEditor::renderBackground()
{
    const int w = std::max (1, getWidth()), h = std::max (1, getHeight());
    background = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (background);

    g.fillAll (col::bg);
    // lila Nebel
    for (auto [x, y, r, a] : { std::tuple { 0.82f, 0.08f, 0.45f, 0.55f }, { 0.15f, 0.85f, 0.5f, 0.45f },
                               { 0.55f, 0.55f, 0.6f, 0.30f }, { 0.98f, 0.95f, 0.35f, 0.40f } })
    {
        juce::ColourGradient neb (col::bgPurple.withAlpha (a), (float) w * x, (float) h * y,
                                  col::bgPurple.withAlpha (0.0f), (float) w * (x + r), (float) h * y, true);
        g.setGradientFill (neb);
        g.fillRect (0, 0, w, h);
    }

    // Risse / Adern (fester Zufall, damit es immer gleich aussieht)
    juce::Random rnd (4711);
    for (int i = 0; i < 26; ++i)
    {
        juce::Path crack;
        float x = rnd.nextFloat() * (float) w, y = rnd.nextFloat() * (float) h;
        crack.startNewSubPath (x, y);
        float ang = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
        const int segs = 5 + rnd.nextInt (8);
        for (int s = 0; s < segs; ++s)
        {
            ang += (rnd.nextFloat() - 0.5f) * 1.4f;
            const float len = (6.0f + rnd.nextFloat() * 22.0f) * u;
            x += std::cos (ang) * len;
            y += std::sin (ang) * len;
            crack.lineTo (x, y);
        }
        const bool green = rnd.nextFloat() < 0.25f;
        const auto c = green ? col::green : col::magenta.interpolatedWith (col::bgPurple, 0.4f);
        g.setColour (c.withAlpha (green ? 0.10f : 0.16f));
        g.strokePath (crack, juce::PathStrokeType (3.0f * u));
        g.setColour (c.withAlpha (green ? 0.35f : 0.40f));
        g.strokePath (crack, juce::PathStrokeType (0.8f * u));
    }

    // grüne Neon-Spritzer an den Rändern
    for (int i = 0; i < 40; ++i)
    {
        const bool left = rnd.nextBool();
        const float x = left ? rnd.nextFloat() * 0.06f * (float) w : (float) w * (0.94f + rnd.nextFloat() * 0.06f);
        const float y = rnd.nextFloat() * (float) h;
        const float r = (0.6f + rnd.nextFloat() * 2.2f) * u;
        g.setColour (col::green.withAlpha (0.25f + rnd.nextFloat() * 0.5f));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
    }

    // Panels für Pattern Control und Sound Engine
    neonPanel (g, R (8, 264, 984, 48), u, col::edge, 0.0f, 10.0f);
    neonPanel (g, R (8, 330, 984, 162), u, col::edge, 0.0f, 18.0f);
    // grüne Akzente an den Ecken des Engine-Panels
    g.setColour (col::green);
    g.fillRect (R (8, 474, 3, 18));
    g.fillRect (R (989, 330, 3, 18));
    // Trennlinien in der Pattern-Leiste
    g.setColour (col::edge);
    g.fillRect (R (246, 270, 1.2f, 36));
    g.fillRect (R (670, 270, 1.2f, 36));

    // Bezel hinter jedem Regler
    for (float cx : knobX)
    {
        juce::Path hex;
        hex.addPolygon ({ cx * u, 404 * u }, 8, 62 * u, juce::MathConstants<float>::pi / 8.0f);
        g.setColour (juce::Colour (0xff120a1a));
        g.fillPath (hex);
        g.setColour (col::edge.withAlpha (0.7f));
        g.strokePath (hex, juce::PathStrokeType (1.2f * u));
        // Werte-Plakette
        g.setColour (juce::Colour (0xff0a0510));
        g.fillPath (chamfer (R (cx - 48, 456, 96, 26), 5 * u, true));
        g.setColour (col::edge);
        g.strokePath (chamfer (R (cx - 48, 456, 96, 26), 5 * u, true), juce::PathStrokeType (1.0f * u));
    }

    juce::Graphics& gg = g;
    sectionLabel (gg, 18, 250, "PATTERN CONTROL");
    sectionLabel (gg, 18, 316, "SOUND ENGINE");
    paintHeader (gg);
}

void StepDelayEditor::sectionLabel (juce::Graphics& g, float x, float y, const juce::String& text)
{
    slashes (g, x * u, (y + 3) * u, 9 * u, 1, col::magenta, u);
    g.setFont (bb::fonts::display (15 * u, true));
    g.setColour (col::ink);
    g.drawText (text, R (x + 12, y, 220, 15), juce::Justification::centredLeft, false);
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
    slashes (g, (x + 18) * u + tw, (y + 3) * u, 9 * u, 5, col::inkDim.withAlpha (0.6f), u);
}

void StepDelayEditor::paintHeader (juce::Graphics& g)
{
    // Logo
    if (logo.isValid())
    {
        const float lh = 58 * u;
        const float lw = std::min (175 * u, lh * (float) logo.getWidth() / (float) std::max (1, logo.getHeight()));
        g.drawImage (logo, juce::Rectangle<float> (14 * u, 7 * u, lw, lh), juce::RectanglePlacement::centred);
    }

    // Titel-Plakette mit schrägen Kanten
    juce::Path plate;
    plate.startNewSubPath (226 * u, 8 * u);
    plate.lineTo (720 * u, 8 * u);
    plate.lineTo (690 * u, 64 * u);
    plate.lineTo (200 * u, 64 * u);
    plate.closeSubPath();
    juce::ColourGradient pg (juce::Colour (0xff1a0c26), 0, 8 * u, juce::Colour (0xff09040d), 0, 64 * u, false);
    g.setGradientFill (pg);
    g.fillPath (plate);
    g.setColour (col::edge.brighter (0.2f));
    g.strokePath (plate, juce::PathStrokeType (1.4f * u));
    g.setColour (col::green.withAlpha (0.8f));
    g.fillRect (R (232, 8, 60, 2));

    // "STEP" magenta, "DELAY" grün, kursiv
    g.saveState();
    g.addTransform (juce::AffineTransform::shear (-0.22f, 0.0f).translated (36 * 0.22f * u, 0));
    g.setFont (bb::fonts::display (48 * u, true).withExtraKerningFactor (-0.02f));
    const juce::String a ("STEP "), b ("DELAY");
    float x = 268 * u;
    g.setColour (col::magenta.withAlpha (0.3f));
    g.drawText (a, juce::Rectangle<float> (x, 9 * u, 300 * u, 54 * u).translated (0, 1.5f * u), juce::Justification::centredLeft, false);
    g.setColour (col::magenta);
    g.drawText (a, juce::Rectangle<float> (x, 9 * u, 300 * u, 54 * u), juce::Justification::centredLeft, false);
    x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), a);
    g.setColour (col::green);
    g.drawText (b, juce::Rectangle<float> (x, 9 * u, 300 * u, 54 * u), juce::Justification::centredLeft, false);
    x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), b);
    g.restoreState();
    slashes (g, x + 22 * u, 26 * u, 20 * u, 7, col::green, u);

    // Version
    g.setFont (bb::fonts::display (17 * u, true));
    g.setColour (col::ink);
    g.drawText ("v1.0", R (880, 14, 90, 24), juce::Justification::centredRight, false);
    g.setColour (col::edge);
    g.fillRect (R (740, 64, 252, 1.2f));
}

void StepDelayEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);

    // Preset-Name
    auto nameBox = R (62, 272, 134, 32);
    g.setColour (juce::Colour (0xff060309));
    g.fillPath (chamfer (nameBox, 4 * u, true));
    g.setColour (col::edge);
    g.strokePath (chamfer (nameBox, 4 * u, true), juce::PathStrokeType (1.0f * u));
    g.setFont (bb::fonts::display (14.5f * u, true));
    g.setColour (col::green);
    g.drawFittedText (proc.getProgramName (proc.getCurrentProgram()).toUpperCase(), nameBox.reduced (6 * u, 0).toNearestInt(),
                      juce::Justification::centred, 1, 0.7f);

    // Regler-Namen und Werte
    const bool sync = proc.getParam (ParamIDs::sync) > 0.5f;
    for (int i = 0; i < kNumKnobs; ++i)
    {
        const float cx = knobX[(size_t) i];
        g.setFont (bb::fonts::display (15 * u, true));
        g.setColour (col::ink);
        g.drawText (knobNames[(size_t) i], R (cx - 60, 337, 120, 16), juce::Justification::centred, false);

        juce::String val;
        if (i == 1 && sync) val = proc.gridName();
        else if (auto* prm = proc.apvts.getParameter (knobIds[(size_t) i]))
            val = prm->getCurrentValueAsText();
        g.setFont (bb::fonts::display (15 * u, true));
        g.setColour (i == 1 && sync ? col::inkDim : col::ink);
        g.drawText (val.toUpperCase(), R (cx - 48, 456, 96, 26), juce::Justification::centred, false);
    }
}
