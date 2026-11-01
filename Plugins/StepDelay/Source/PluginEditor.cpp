#include "PluginEditor.h"
#include "Presets.h"

using namespace sd;

namespace
{
    constexpr float kScanSeconds = 3.6f, kScanPause = 1.4f; // Durchlauf und Pause des Scanners
    constexpr float kButtonY = 485, kButtonH = 48;
    const juce::Rectangle<float> kField { 46, 178, 1692, 260 };
    const juce::Rectangle<float> kPresetName { 122, 492, 220, 36 };
}

StepDelayEditor::StepDelayEditor (StepDelayProcessor& p)
    : AudioProcessorEditor (&p), proc (p), bars (p)
{
    setLookAndFeel (&lnf);
    bgSource = juce::ImageCache::getFromMemory (SDAssets::stepdelay_bg_jpg, SDAssets::stepdelay_bg_jpgSize);
    addAndMakeVisible (bars);

    // --- Pattern Control ---
    prevBtn.getProperties().set ("arrow", true);
    nextBtn.getProperties().set ("arrow", true);
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

    // --- Sound Engine (Ringfarben wie in der Vorlage) ---
    const juce::Colour rings[kNumKnobs] = { col::magenta, col::lime, col::magenta, col::cyan, col::lime, col::magenta };
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& s = knobs[(size_t) i];
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (bb::LookAndFeel::startAngle, bb::LookAndFeel::endAngle, true);
        s.setMouseDragSensitivity (200);
        s.getProperties().set ("ring", (juce::int64) rings[i].getARGB());
        addAndMakeVisible (s);
        knobAtts[(size_t) i] = std::make_unique<SA> (proc.apvts, knobIds[(size_t) i], s);
    }
    knobs[0].setTitle ("Steps");  knobs[1].setTitle ("Time");   knobs[2].setTitle ("Mix");
    knobs[3].setTitle ("Lo Cut"); knobs[4].setTitle ("Hi Cut"); knobs[5].setTitle ("Output");
    knobs[0].setMouseDragSensitivity (140);
    knobs[0].setDoubleClickReturnValue (true, 8.0);
    knobs[1].setDoubleClickReturnValue (true, 250.0);
    knobs[2].setDoubleClickReturnValue (true, 0.4);
    knobs[3].setDoubleClickReturnValue (true, 20.0);
    knobs[4].setDoubleClickReturnValue (true, 20000.0);
    knobs[5].setDoubleClickReturnValue (true, 0.0);

    getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff140a1d));
    getLookAndFeel().setColour (juce::PopupMenu::textColourId, col::ink);
    getLookAndFeel().setColour (juce::PopupMenu::highlightedBackgroundColourId, col::magentaDk);

    setResizable (true, true);
    setResizeLimits (designW / 2, designH / 2, designW, designH);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / (double) designH);
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

void StepDelayEditor::stepPreset (int dir)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + dir + n) % n);
    repaint (R (kPresetName.getX(), kPresetName.getY(), kPresetName.getWidth(), kPresetName.getHeight()).getSmallestIntegerContainer());
}

void StepDelayEditor::resized()
{
    u = (float) getWidth() / (float) designW;

    // Vorlage einmal auf Fenstergröße skalieren (spart Rechenzeit beim Zeichnen)
    background = juce::Image (juce::Image::RGB, std::max (1, getWidth()), std::max (1, getHeight()), false);
    {
        juce::Graphics g (background);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (bgSource, background.getBounds().toFloat());
    }

    bars.setBounds (R (kField.getX(), kField.getY(), kField.getWidth(), kField.getHeight()).toNearestInt());

    auto btn = [this] (juce::Button& b, float x0, float x1) { b.setBounds (R (x0, kButtonY, x1 - x0, kButtonH).toNearestInt()); };
    btn (prevBtn, 45, 103);
    btn (nextBtn, 359, 419);
    btn (syncBtn, 458, 554);
    for (int i = 0; i < bb::stepgrid::kNumRates; ++i)
        btn (rateBtns[(size_t) i], 563.0f + (float) i * 89.7f, 644.0f + (float) i * 89.7f);
    btn (modeBtns[0], 1207, 1309);
    btn (modeBtns[1], 1316, 1412);
    btn (modeBtns[2], 1418, 1514);
    btn (invBtn, 1519, 1616);
    btn (resetBtn, 1633, 1732);

    for (int i = 0; i < kNumKnobs; ++i)
        knobs[(size_t) i].setBounds (R (knobX[(size_t) i] - 88, knobY - 88, 176, 176).toNearestInt());
}

void StepDelayEditor::mouseDown (const juce::MouseEvent& e)
{
    // Klick auf den Preset-Namen: Liste aller Presets
    const auto nameBox = R (kPresetName.getX(), kPresetName.getY(), kPresetName.getWidth(), kPresetName.getHeight());
    if (! nameBox.contains (e.position)) return;

    juce::PopupMenu m;
    const auto& list = bb::stepdelay::presets();
    for (int i = 0; i < (int) list.size(); ++i)
        m.addItem (i + 1, juce::String::fromUTF8 (list[(size_t) i].name), true, i == proc.getCurrentProgram());
    m.addSeparator();
    m.addItem (100, "Zufallsmuster");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (nameBox.withTrimmedTop (nameBox.getHeight()).toNearestInt())),
                     [this] (int id)
                     {
                         if (id == 100) proc.randomizePattern();
                         else if (id > 0) proc.setCurrentProgram (id - 1);
                         repaint();
                     });
}

juce::Rectangle<int> StepDelayEditor::scannerArea (float pos) const
{
    const float x = pos * (float) getWidth();
    return juce::Rectangle<float> (x - 200 * u, 0, 230 * u, (float) getHeight()).getSmallestIntegerContainer();
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
    knobs[1].setAlpha (sync ? 0.6f : 1.0f);

    // Scanner weiterlaufen lassen
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.1, (now - lastMs) / 1000.0);
    lastMs = now;
    const auto oldArea = scannerArea ((float) (scanPos / kScanSeconds) * 1.15f - 0.05f);
    scanPos = std::fmod (scanPos + dt, (double) (kScanSeconds + kScanPause));
    const float sp = (float) (scanPos / kScanSeconds) * 1.15f - 0.05f;
    const bool running = scanPos < kScanSeconds;
    bars.scanX = running ? sp * (float) getWidth() - (float) bars.getX() : -10000.0f;
    if (running || oldArea.getX() < getWidth())
        repaint (oldArea.getUnion (scannerArea (sp)));

    bars.tick();
    // Texte mit Werten
    repaint (R (110, 136, 260, 44).getSmallestIntegerContainer());
    repaint (R (1440, 136, 220, 40).getSmallestIntegerContainer());
    repaint (R (110, 488, 245, 44).getSmallestIntegerContainer());
    for (float cx : knobX)
        repaint (R (cx - 48, 795, 96, 36).getSmallestIntegerContainer());
}

//==============================================================================
void StepDelayEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);

    // Scanner: weicher Lichtbalken mit feinen Linien, läuft unter den Bedienelementen durch
    if (scanPos < kScanSeconds)
    {
        const float x = ((float) (scanPos / kScanSeconds) * 1.15f - 0.05f) * (float) getWidth();
        const float bw = 200 * u;
        juce::ColourGradient band (col::magenta.withAlpha (0.0f), x - bw, 0, col::magenta.withAlpha (0.0f), x + 30 * u, 0, false);
        band.addColour (0.75, col::magenta.withAlpha (0.16f));
        band.addColour (0.86, juce::Colour (0xffffb0f6).withAlpha (0.24f));
        g.setGradientFill (band);
        g.fillRect (juce::Rectangle<float> (x - bw, 0, bw + 30 * u, (float) getHeight()));
        g.setColour (juce::Colour (0xffffc0f8).withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (x - 1.0f * u, 0, 2.0f * u, (float) getHeight()));
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (float y = 0; y < (float) getHeight(); y += 4.0f * u)
            g.fillRect (juce::Rectangle<float> (x - bw * 0.6f, y, bw * 0.6f + 30 * u, 1.0f * u));
    }

    // Kopfzeile des Balkenfelds: "16 STEP PATTERN" und "1/16 GRID"
    const int n = (int) proc.getParam (ParamIDs::steps);
    g.setFont (font (34 * u));
    g.setColour (col::lilac);
    g.drawText (juce::String (n) + " STEP PATTERN", R (126, 138, 300, 40), juce::Justification::centredLeft, false);
    const bool sync = proc.getParam (ParamIDs::sync) > 0.5f;
    const juce::String grid = sync ? proc.gridName().toUpperCase() + " GRID"
                                   : juce::String (juce::roundToInt (proc.getParam (ParamIDs::timeMs))) + " MS";
    g.drawText (grid, R (1440, 136, 200, 40), juce::Justification::centredRight, false);

    // Preset-Name
    g.setFont (font (32 * u));
    g.setColour (col::green);
    g.drawFittedText (proc.getProgramName (proc.getCurrentProgram()).toUpperCase(),
                      R (kPresetName.getX() + 6, kPresetName.getY(), kPresetName.getWidth() - 12, kPresetName.getHeight()).toNearestInt(),
                      juce::Justification::centred, 1, 0.75f);

    // Werte unter den Reglern
    for (int i = 0; i < kNumKnobs; ++i)
    {
        juce::String val;
        if (i == 1 && sync) val = proc.gridName();
        else if (auto* prm = proc.apvts.getParameter (knobIds[(size_t) i]))
            val = prm->getCurrentValueAsText();
        g.setFont (font (33 * u));
        g.setColour (i == 1 && sync ? col::inkDim : col::ink);
        g.drawFittedText (val.toUpperCase().replace ("DB", "dB"), R (knobX[(size_t) i] - 46, 795, 92, 36).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    }
}
