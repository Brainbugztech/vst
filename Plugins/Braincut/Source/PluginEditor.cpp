#include "PluginEditor.h"

using namespace bcui;

namespace
{
    // Zahl in [0,1) aus einem Index (für die schwebenden Funken)
    float rnd (int i, int salt)
    {
        const uint32_t h = (uint32_t) (i * 7919 + salt * 104729);
        return (float) (((h * 2654435761u) >> 8) & 0xffffu) / 65536.0f;
    }

    // Module liegen frei auf dem Bild (Kette: 1 Slicer, 2 Stretch, 3 Repeat, 4 Glitch).
    // Die Slice-Anzeige sitzt auf dem Vortex in der Bildmitte, unten rechts und links bleiben die Bugs frei.
    const juce::Rectangle<float> slicerCard  { 60.0f, 128.0f, 292.0f, 300.0f };
    const juce::Rectangle<float> stretchCard { 610.0f, 128.0f, 290.0f, 300.0f };
    const juce::Rectangle<float> repeatCard  { 104.0f, 438.0f, 336.0f, 180.0f };
    const juce::Rectangle<float> glitchCard  { 448.0f, 438.0f, 290.0f, 180.0f };
    const juce::Rectangle<float> masterCard  { 832.0f, 40.0f, 106.0f, 84.0f };
}

BraincutEditor::BraincutEditor (BraincutProcessor& p)
    : AudioProcessorEditor (&p), proc (p), sliceView (p),
      repeatK (p.apvts, ParamIDs::repeat, "AMOUNT", col::slicer),
      loopK (p.apvts, ParamIDs::loop, "LOOP", col::slicer),
      subdivK (p.apvts, ParamIDs::subdiv, "SUB", col::slicer),
      seedK (p.apvts, ParamIDs::seed, "SEED", col::slicer),
      phMinB (p.apvts, ParamIDs::phMin, "MIN PHRASE", col::slicer, "BAR"),
      phMaxB (p.apvts, ParamIDs::phMax, "MAX PHRASE", col::slicer, "BAR"),
      repMinB (p.apvts, ParamIDs::repMin, "MIN REP", col::slicer, "REP"),
      repMaxB (p.apvts, ParamIDs::repMax, "MAX REP", col::slicer, "REP"),
      strAmtK (p.apvts, ParamIDs::strAmt, "AMOUNT", col::stretch),
      strMixK (p.apvts, ParamIDs::strMix, "MIX", col::stretch),
      repChanceK (p.apvts, ParamIDs::repChance, "CHANCE", col::repeat),
      repRateK (p.apvts, ParamIDs::repRate, "RATE", col::repeat),
      repGateK (p.apvts, ParamIDs::repGate, "GATE", col::repeat),
      repDecayK (p.apvts, ParamIDs::repDecay, "DECAY", col::repeat),
      gliAmtK (p.apvts, ParamIDs::gliAmt, "AMOUNT", col::glitch),
      gliMixK (p.apvts, ParamIDs::gliMix, "MIX", col::glitch),
      mixK (p.apvts, ParamIDs::mix, "MIX", col::master),
      outK (p.apvts, ParamIDs::out, "OUT", col::master)
{
    setLookAndFeel (&lnf);
    bg = juce::ImageCache::getFromMemory (BCAssets::braincut_bg_jpg, BCAssets::braincut_bg_jpgSize);

    content.painter = [this] (juce::Graphics& g) { paintContent (g); };
    addAndMakeVisible (content);

    content.addAndMakeVisible (sliceView);
    for (auto* k : { &repeatK, &loopK, &subdivK, &seedK, &strAmtK, &strMixK, &repChanceK, &repRateK, &repGateK, &repDecayK,
                     &gliAmtK, &gliMixK, &mixK, &outK })
        content.addAndMakeVisible (*k);
    for (auto* v : { &phMinB, &phMaxB, &repMinB, &repMaxB })
        content.addAndMakeVisible (*v);

    // Feste Standardwerte per Doppelklick kommen aus den Parametern (SliderAttachment)

    // Taster mit Parameter-Bindung
    auto toggle = [this] (juce::TextButton& b, juce::Colour accent, const char* id, std::unique_ptr<BA>& att)
    {
        b.setClickingTogglesState (true);
        setAccent (b, accent);
        content.addAndMakeVisible (b);
        att = std::make_unique<BA> (proc.apvts, id, b);
    };
    toggle (lockBtn, col::slicer, ParamIDs::lock, lockAtt);
    toggle (strOnBtn, col::stretch, ParamIDs::strOn, strOnAtt);
    toggle (repOnBtn, col::repeat, ParamIDs::repOn, repOnAtt);
    toggle (gliOnBtn, col::glitch, ParamIDs::gliOn, gliOnAtt);
    toggle (bitBtn, col::glitch, ParamIDs::gBit, bitAtt);
    toggle (revBtn, col::glitch, ParamIDs::gRev, revAtt);
    toggle (stopBtn, col::glitch, ParamIDs::gStop, stopAtt);
    toggle (frzBtn, col::glitch, ParamIDs::gFrz, frzAtt);
    toggle (pitBtn, col::glitch, ParamIDs::gPit, pitAtt);

    // CUT / WARP als Umschalter
    for (auto* b : { &cutBtn, &warpBtn })
    {
        b->setClickingTogglesState (false);
        setAccent (*b, col::slicer);
        content.addAndMakeVisible (*b);
    }
    cutBtn.onClick  = [this] { proc.setParam (ParamIDs::mode, 0.0f); };
    warpBtn.onClick = [this] { proc.setParam (ParamIDs::mode, 1.0f); };

    // Würfel: neuer Seed
    setAccent (rollBtn, col::master);
    rollBtn.onClick = [this] { proc.rollSeed(); };
    content.addAndMakeVisible (rollBtn);

    // Presets
    for (int i = 0; i < bcpresets::kNumPresets; ++i)
        presetBox.addItem (bcpresets::kPresets[i].name, i + 1);
    presetBox.setSelectedItemIndex (proc.getPresetIndex(), juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0) proc.applyPreset (idx);
    };
    content.addAndMakeVisible (presetBox);
    for (auto* b : { &prevBtn, &nextBtn })
    {
        setAccent (*b, col::slicer);
        content.addAndMakeVisible (*b);
    }
    prevBtn.onClick = [this] { proc.applyPreset ((proc.getPresetIndex() + bcpresets::kNumPresets - 1) % bcpresets::kNumPresets); };
    nextBtn.onClick = [this] { proc.applyPreset ((proc.getPresetIndex() + 1) % bcpresets::kNumPresets); };

    // Positionen (in Basis-Pixeln, Fenster skaliert alles)
    sliceView.setBounds (356, 176, 250, 254);

    // 1 Slicer (x 60, y 128, 292 x 300)
    cutBtn.setBounds (60 + 154, 128 + 7, 64, 24);
    warpBtn.setBounds (60 + 222, 128 + 7, 64, 24);
    repeatK.setBounds (60 + 8, 128 + 44, 120, 150);
    phMaxB.setBounds (60 + 140, 128 + 52, 144, 26);
    phMinB.setBounds (60 + 140, 128 + 86, 144, 26);
    repMaxB.setBounds (60 + 140, 128 + 120, 144, 26);
    repMinB.setBounds (60 + 140, 128 + 154, 144, 26);
    loopK.setBounds (60 + 26, 128 + 204, 70, 86);
    subdivK.setBounds (60 + 111, 128 + 204, 70, 86);
    seedK.setBounds (60 + 196, 128 + 204, 70, 86);

    // 2 Stretch (x 610, y 128, 290 x 300)
    strOnBtn.setBounds (610 + 290 - 50, 128 + 7, 42, 24);
    strAmtK.setBounds (610 + 18, 128 + 70, 130, 162);
    strMixK.setBounds (610 + 164, 128 + 86, 108, 140);

    // 3 Repeat (x 104, y 438, 336 x 180)
    repOnBtn.setBounds (104 + 336 - 50, 438 + 7, 42, 24);
    repChanceK.setBounds (104 + 6, 438 + 30, 120, 148);
    repRateK.setBounds (104 + 136, 438 + 56, 64, 86);
    repGateK.setBounds (104 + 201, 438 + 56, 64, 86);
    repDecayK.setBounds (104 + 266, 438 + 56, 64, 86);

    // 4 Glitch (x 448, y 438, 290 x 180)
    gliOnBtn.setBounds (448 + 290 - 50, 438 + 7, 42, 24);
    gliAmtK.setBounds (448 + 6, 438 + 30, 120, 148);
    bitBtn.setBounds (448 + 136, 438 + 40, 46, 26);
    revBtn.setBounds (448 + 186, 438 + 40, 46, 26);
    stopBtn.setBounds (448 + 236, 438 + 40, 46, 26);
    frzBtn.setBounds (448 + 136, 438 + 70, 72, 26);
    pitBtn.setBounds (448 + 212, 438 + 70, 70, 26);
    gliMixK.setBounds (448 + 170, 438 + 100, 72, 76);

    // Preset-Zeile oben (links oben steckt das Logo im Bild), Ausgang oben rechts
    prevBtn.setBounds (422, 52, 30, 32);
    presetBox.setBounds (456, 52, 200, 32);
    nextBtn.setBounds (660, 52, 30, 32);
    rollBtn.setBounds (704, 52, 58, 32);
    lockBtn.setBounds (766, 52, 58, 32);
    mixK.setBounds (834, 44, 51, 78);
    outK.setBounds (885, 44, 51, 78);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    syncFromParams();
    lastMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

BraincutEditor::~BraincutEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BraincutEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (juce::AffineTransform::scale (s));
}

//==============================================================================
void BraincutEditor::syncFromParams()
{
    const int mode = (int) std::lround (proc.apvts.getRawParameterValue (ParamIDs::mode)->load());
    cutBtn.setToggleState (mode == 0, juce::dontSendNotification);
    warpBtn.setToggleState (mode == 1, juce::dontSendNotification);

    const int pi = proc.getPresetIndex();
    if (presetBox.getSelectedItemIndex() != pi)
        presetBox.setSelectedItemIndex (pi, juce::dontSendNotification);

    // ausgeschaltete Sektionen abdunkeln
    const float aStr = flag (ParamIDs::strOn) ? 1.0f : 0.4f;
    const float aRep = flag (ParamIDs::repOn) ? 1.0f : 0.4f;
    const float aGli = flag (ParamIDs::gliOn) ? 1.0f : 0.4f;
    for (auto* c : { (juce::Component*) &strAmtK, (juce::Component*) &strMixK }) c->setAlpha (aStr);
    for (auto* c : { (juce::Component*) &repChanceK, (juce::Component*) &repRateK, (juce::Component*) &repGateK, (juce::Component*) &repDecayK }) c->setAlpha (aRep);
    for (auto* c : { (juce::Component*) &gliAmtK, (juce::Component*) &gliMixK, (juce::Component*) &bitBtn, (juce::Component*) &revBtn,
                     (juce::Component*) &stopBtn, (juce::Component*) &frzBtn, (juce::Component*) &pitBtn }) c->setAlpha (aGli);
}

void BraincutEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    timeSec += juce::jlimit (0.0, 0.2, (now - lastMs) / 1000.0);
    lastMs = now;
    syncFromParams();
    repaint();
}

//==============================================================================
// Hintergrund: das Bild, darüber ein leises Pulsieren im Vortex, springende Glitch-Streifen und Scanlines
void BraincutEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (bg.isValid())
        g.drawImage (bg, getLocalBounds().toFloat());
    paintGlitch (g);
}

void BraincutEditor::paintGlitch (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    const float s = W / (float) baseW;
    const float t = (float) timeSec;

    // Vortex atmet langsam (Mitte des Bildes bei etwa 481 / 300)
    {
        const float cx = 481.0f * s, cy = 300.0f * s;
        const float a = 0.06f + 0.04f * std::sin (t * 0.8f);
        juce::ColourGradient gl (col::frame.withAlpha (a), cx, cy, col::frame.withAlpha (0.0f), cx + 330.0f * s, cy, true);
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::scale (1.0f, 0.42f, cx, cy));
        g.setGradientFill (gl);
        g.fillEllipse (cx - 330.0f * s, cy - 330.0f * s, 660.0f * s, 660.0f * s);
    }

    // Glitch-Streifen: springen etwa achtmal pro Sekunde an neue Stellen, meist im Band des Vortex
    const int frame = (int) std::floor (t * 8.0f);
    for (int i = 0; i < 7; ++i)
    {
        const float y = (0.22f + 0.56f * rnd (frame * 7 + i, 11)) * H;
        const float x = (0.10f + 0.70f * rnd (frame * 7 + i, 12)) * W;
        const float w = (0.03f + 0.22f * rnd (frame * 7 + i, 13)) * W;
        const float h = (1.0f + 3.0f * rnd (frame * 7 + i, 14)) * s;
        const float a = 0.05f + 0.16f * rnd (frame * 7 + i, 15);
        g.setColour (spectrum (0.15f + 0.6f * rnd (i, 16)).withAlpha (a));
        g.fillRect (x, y, w, h);
    }

    // Funken in Lila und Weiß
    for (int i = 0; i < 16; ++i)
    {
        const float sp = 0.5f + rnd (i, 3);
        float y = rnd (i, 2) - 0.012f * sp * t;
        y -= std::floor (y);
        const float x = rnd (i, 1) + 0.02f * std::sin (t * 0.4f * sp + rnd (i, 4) * 6.28f);
        const float a = 0.10f + 0.25f * (0.5f + 0.5f * std::sin (t * sp * 1.3f + (float) i));
        const float rad = (0.8f + 1.4f * rnd (i, 5)) * s;
        const auto c = spectrum (0.3f + 0.7f * rnd (i, 6));
        g.setColour (c.withAlpha (a * 0.35f));
        g.fillEllipse (x * W - rad * 2.4f, y * H - rad * 2.4f, rad * 4.8f, rad * 4.8f);
        g.setColour (c.withAlpha (a));
        g.fillEllipse (x * W - rad, y * H - rad, rad * 2.0f, rad * 2.0f);
    }

    // Scanlines wie auf dem Bild
    g.setColour (juce::Colours::black.withAlpha (0.10f));
    for (float y = 0.0f; y < H; y += 3.0f * s)
        g.fillRect (0.0f, y, W, std::max (1.0f, s));
}

void BraincutEditor::paintCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent, bool on)
{
    // Glasplatte mit abgeschrägten Ecken wie der Bildrahmen
    const auto shape = chamfer (r, 14.0f);
    juce::ColourGradient body (col::glass.withAlpha (on ? 0.62f : 0.48f), r.getX(), r.getY(),
                               col::glass.withAlpha (on ? 0.40f : 0.32f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillPath (shape);
    juce::ColourGradient glow (accent.withAlpha (on ? 0.16f : 0.03f), r.getX(), r.getY(), accent.withAlpha (0.0f), r.getX() + 140.0f, r.getY() + 100.0f, false);
    g.setGradientFill (glow);
    g.fillPath (shape);

    const auto ac = accent.withAlpha (on ? 0.95f : 0.35f);
    g.setColour (col::frame.withAlpha (on ? 0.75f : 0.35f));
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    // helle Akzente an den abgeschrägten Ecken
    auto seg = [&] (juce::Point<float> a, juce::Point<float> b, juce::Point<float> c)
    {
        juce::Path p;
        p.startNewSubPath (a); p.lineTo (b); p.lineTo (c);
        g.setColour (ac.withAlpha (on ? 0.25f : 0.08f));
        g.strokePath (p, juce::PathStrokeType (4.0f));
        g.setColour (ac);
        g.strokePath (p, juce::PathStrokeType (1.6f));
    };
    seg ({ r.getX(), r.getY() + 40.0f }, { r.getX(), r.getY() + 14.0f }, { r.getX() + 14.0f, r.getY() });
    g.setColour (ac);
    g.fillRect (r.getX() + 14.0f, r.getY() - 0.8f, 46.0f, 1.6f);
    seg ({ r.getRight(), r.getBottom() - 40.0f }, { r.getRight(), r.getBottom() - 14.0f }, { r.getRight() - 14.0f, r.getBottom() });
    g.fillRect (r.getRight() - 60.0f, r.getBottom() - 0.8f, 46.0f, 1.6f);

    // Streifen auf der Unterkante, wie unten im Bildrahmen
    hatch (g, r.getX() + 22.0f, r.getBottom() - 3.0f, 5, 6.0f, ac.withAlpha (on ? 0.8f : 0.3f));

    if (title.isEmpty())
        return;

    // Titel mit Kettennummer
    auto f = bb::fonts::display (13.0f, true);
    f.setExtraKerningFactor (0.14f);
    g.setFont (f);
    g.setColour (on ? col::ink : col::inkDim);
    g.drawText (title, (int) r.getX() + 18, (int) r.getY() + 8, (int) r.getWidth() - 60, 22, juce::Justification::centredLeft);
    g.setColour (ac.withAlpha (on ? 0.7f : 0.25f));
    g.fillRect (r.getX() + 18.0f, r.getY() + 31.0f, 24.0f, 2.0f);
}

void BraincutEditor::paintContent (juce::Graphics& g)
{
    paintCard (g, slicerCard, "1  SLICER", col::slicer, true);
    paintCard (g, stretchCard, "2  STRETCH", col::stretch, flag (ParamIDs::strOn));
    paintCard (g, repeatCard, "3  REPEAT", col::repeat, flag (ParamIDs::repOn));
    paintCard (g, glitchCard, "4  GLITCH", col::glitch, flag (ParamIDs::gliOn));
    paintCard (g, masterCard, {}, col::master, true);
}
