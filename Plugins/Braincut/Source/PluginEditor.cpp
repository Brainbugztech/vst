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

    // Module liegen frei auf dem Bild (Kette: 1 Slicer, 2 Stretch, 3 Repeat, 4 Glitch)
    const juce::Rectangle<float> slicerCard { 62.0f, 128.0f, 296.0f, 262.0f };
    const juce::Rectangle<float> stretchCard { 602.0f, 128.0f, 296.0f, 262.0f };
    const juce::Rectangle<float> repeatCard { 84.0f, 402.0f, 352.0f, 214.0f };
    const juce::Rectangle<float> glitchCard { 446.0f, 402.0f, 290.0f, 214.0f };
    const juce::Rectangle<float> masterCard { 746.0f, 402.0f, 152.0f, 214.0f };
}

BraincutEditor::BraincutEditor (BraincutProcessor& p)
    : AudioProcessorEditor (&p), proc (p), sliceView (p),
      repeatK (p.apvts, ParamIDs::repeat, "AMOUNT", col::green),
      loopK (p.apvts, ParamIDs::loop, "LOOP", col::green),
      subdivK (p.apvts, ParamIDs::subdiv, "SUB", col::green),
      seedK (p.apvts, ParamIDs::seed, "SEED", col::green),
      phMinB (p.apvts, ParamIDs::phMin, "MIN PHRASE", col::green, "BAR"),
      phMaxB (p.apvts, ParamIDs::phMax, "MAX PHRASE", col::green, "BAR"),
      repMinB (p.apvts, ParamIDs::repMin, "MIN REP", col::green, "REP"),
      repMaxB (p.apvts, ParamIDs::repMax, "MAX REP", col::green, "REP"),
      strAmtK (p.apvts, ParamIDs::strAmt, "AMOUNT", col::cyan),
      strMixK (p.apvts, ParamIDs::strMix, "MIX", col::cyan),
      repChanceK (p.apvts, ParamIDs::repChance, "CHANCE", col::pink),
      repRateK (p.apvts, ParamIDs::repRate, "RATE", col::pink),
      repGateK (p.apvts, ParamIDs::repGate, "GATE", col::pink),
      repDecayK (p.apvts, ParamIDs::repDecay, "DECAY", col::pink),
      gliAmtK (p.apvts, ParamIDs::gliAmt, "AMOUNT", col::violet),
      gliMixK (p.apvts, ParamIDs::gliMix, "MIX", col::violet),
      mixK (p.apvts, ParamIDs::mix, "MIX", col::green),
      outK (p.apvts, ParamIDs::out, "OUT", col::green)
{
    setLookAndFeel (&lnf);
    bg = juce::ImageCache::getFromMemory (BCAssets::braincut_bg_jpg, BCAssets::braincut_bg_jpgSize);
    fx = juce::Image (juce::Image::ARGB, 240, 160, true);
    renderFx (0.0);

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
    toggle (lockBtn, col::green, ParamIDs::lock, lockAtt);
    toggle (strOnBtn, col::cyan, ParamIDs::strOn, strOnAtt);
    toggle (repOnBtn, col::pink, ParamIDs::repOn, repOnAtt);
    toggle (gliOnBtn, col::violet, ParamIDs::gliOn, gliOnAtt);
    toggle (bitBtn, col::violet, ParamIDs::gBit, bitAtt);
    toggle (revBtn, col::violet, ParamIDs::gRev, revAtt);
    toggle (stopBtn, col::violet, ParamIDs::gStop, stopAtt);
    toggle (frzBtn, col::violet, ParamIDs::gFrz, frzAtt);
    toggle (pitBtn, col::violet, ParamIDs::gPit, pitAtt);

    // CUT / WARP als Umschalter
    for (auto* b : { &cutBtn, &warpBtn })
    {
        b->setClickingTogglesState (false);
        setAccent (*b, col::green);
        content.addAndMakeVisible (*b);
    }
    cutBtn.onClick  = [this] { proc.setParam (ParamIDs::mode, 0.0f); };
    warpBtn.onClick = [this] { proc.setParam (ParamIDs::mode, 1.0f); };

    // Würfel: neuer Seed
    setAccent (rollBtn, col::orange);
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
        setAccent (*b, col::green);
        content.addAndMakeVisible (*b);
    }
    prevBtn.onClick = [this] { proc.applyPreset ((proc.getPresetIndex() + bcpresets::kNumPresets - 1) % bcpresets::kNumPresets); };
    nextBtn.onClick = [this] { proc.applyPreset ((proc.getPresetIndex() + 1) % bcpresets::kNumPresets); };

    // Positionen (in Basis-Pixeln, Fenster skaliert alles)
    sliceView.setBounds (355, 125, 250, 275);

    // 1 Slicer (x 62, y 128)
    cutBtn.setBounds (62 + 160, 128 + 6, 64, 24);
    warpBtn.setBounds (62 + 228, 128 + 6, 64, 24);
    repeatK.setBounds (62 + 8, 128 + 34, 120, 150);
    phMaxB.setBounds (62 + 144, 128 + 44, 148, 26);
    phMinB.setBounds (62 + 144, 128 + 76, 148, 26);
    repMaxB.setBounds (62 + 144, 128 + 108, 148, 26);
    repMinB.setBounds (62 + 144, 128 + 140, 148, 26);
    loopK.setBounds (62 + 28, 128 + 184, 70, 76);
    subdivK.setBounds (62 + 113, 128 + 184, 70, 76);
    seedK.setBounds (62 + 198, 128 + 184, 70, 76);

    // 2 Stretch (x 602, y 128)
    strOnBtn.setBounds (602 + 296 - 48, 128 + 6, 40, 24);
    strAmtK.setBounds (602 + 24, 128 + 40, 120, 150);
    strMixK.setBounds (602 + 168, 128 + 56, 100, 130);

    // 3 Repeat (x 84, y 402)
    repOnBtn.setBounds (84 + 352 - 48, 402 + 6, 40, 24);
    repChanceK.setBounds (84 + 8, 402 + 34, 120, 150);
    repRateK.setBounds (84 + 138, 402 + 58, 66, 80);
    repGateK.setBounds (84 + 209, 402 + 58, 66, 80);
    repDecayK.setBounds (84 + 280, 402 + 58, 66, 80);

    // 4 Glitch (x 446, y 402)
    gliOnBtn.setBounds (446 + 290 - 48, 402 + 6, 40, 24);
    gliAmtK.setBounds (446 + 8, 402 + 34, 120, 150);
    bitBtn.setBounds (446 + 138, 402 + 44, 46, 26);
    revBtn.setBounds (446 + 188, 402 + 44, 46, 26);
    stopBtn.setBounds (446 + 238, 402 + 44, 46, 26);
    frzBtn.setBounds (446 + 138, 402 + 76, 72, 26);
    pitBtn.setBounds (446 + 214, 402 + 76, 70, 26);
    gliMixK.setBounds (446 + 168, 402 + 110, 80, 82);

    // Master + Preset-Zeile oben rechts (links oben steckt das Logo im Bild)
    mixK.setBounds (746 + 4, 402 + 44, 70, 100);
    outK.setBounds (746 + 78, 402 + 44, 70, 100);
    prevBtn.setBounds (430, 52, 28, 32);
    presetBox.setBounds (462, 52, 190, 32);
    nextBtn.setBounds (656, 52, 28, 32);
    rollBtn.setBounds (708, 52, 58, 32);
    lockBtn.setBounds (772, 52, 58, 32);

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
    renderFx (timeSec);
    syncFromParams();
    repaint();
}

//==============================================================================
// Hintergrund: weiche, langsam wandernde Farbwolken und Prisma-Strahlen (klein gezeichnet und weich hochskaliert)
void BraincutEditor::renderFx (double t)
{
    juce::Graphics g (fx);
    fx.clear (fx.getBounds());
    const float W = (float) fx.getWidth(), H = (float) fx.getHeight();
    const float tt = (float) t;

    auto blob = [&] (float cx, float cy, float rad, juce::Colour c, float a)
    {
        juce::ColourGradient gr (c.withAlpha (a), cx * W, cy * H, c.withAlpha (0.0f), cx * W + rad * W, cy * H, true);
        g.setGradientFill (gr);
        g.fillEllipse (cx * W - rad * W, cy * H - rad * W, rad * W * 2.0f, rad * W * 2.0f);
    };
    blob (0.16f + 0.10f * std::sin (tt * 0.13f), 0.30f + 0.18f * std::cos (tt * 0.10f), 0.42f, col::green, 0.10f);
    blob (0.82f + 0.10f * std::cos (tt * 0.11f), 0.22f + 0.15f * std::sin (tt * 0.14f), 0.40f, col::pink, 0.12f);
    blob (0.52f + 0.22f * std::sin (tt * 0.07f), 0.88f + 0.10f * std::cos (tt * 0.12f), 0.46f, col::cyan, 0.10f);
    blob (0.34f + 0.25f * std::cos (tt * 0.09f), 0.62f + 0.15f * std::sin (tt * 0.08f), 0.34f, col::violet, 0.16f);
    blob (0.90f + 0.05f * std::sin (tt * 0.17f), 0.80f, 0.22f, col::orange, 0.06f);

    for (int k = 0; k < 3; ++k)
    {
        const float x0 = (0.12f + 0.32f * (float) k + 0.08f * std::sin (tt * 0.06f + (float) k)) * W;
        const float lean = (0.50f + 0.12f * std::sin (tt * 0.05f + (float) k * 2.1f)) * H;
        juce::Path beam;
        beam.startNewSubPath (x0, 0.0f);
        beam.lineTo (x0 + 0.09f * W, 0.0f);
        beam.lineTo (x0 + 0.09f * W + lean, H);
        beam.lineTo (x0 + lean, H);
        beam.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.02f));
        g.fillPath (beam);
    }
}

void BraincutEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (bg.isValid())
        g.drawImage (bg, getLocalBounds().toFloat());
    g.drawImage (fx, getLocalBounds().toFloat()); // sanfte Farbwolken über dem Bild

    // schwebende Funken
    const float W = (float) getWidth(), H = (float) getHeight();
    const float t = (float) timeSec;
    const juce::Colour palette[4] { col::green, col::pink, col::cyan, col::violet };
    for (int i = 0; i < 18; ++i)
    {
        const float sp = 0.5f + rnd (i, 3);
        float y = rnd (i, 2) - 0.012f * sp * t;
        y -= std::floor (y);
        const float x = rnd (i, 1) + 0.02f * std::sin (t * 0.4f * sp + rnd (i, 4) * 6.28f);
        const float a = 0.10f + 0.22f * (0.5f + 0.5f * std::sin (t * sp * 1.3f + (float) i));
        const float rad = (1.0f + 1.8f * rnd (i, 5)) * (W / (float) baseW);
        const auto c = palette[i % 4];
        g.setColour (c.withAlpha (a * 0.35f));
        g.fillEllipse (x * W - rad * 2.2f, y * H - rad * 2.2f, rad * 4.4f, rad * 4.4f);
        g.setColour (c.withAlpha (a));
        g.fillEllipse (x * W - rad, y * H - rad, rad * 2.0f, rad * 2.0f);
    }
}

void BraincutEditor::paintCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent, bool on)
{
    // Flache Glasplatte ohne Rahmen; die Ecken sind wie im Bildrahmen als Winkel gezeichnet
    juce::ColourGradient body (juce::Colour (0xff0b0714).withAlpha (on ? 0.58f : 0.44f), r.getX(), r.getY(),
                               juce::Colour (0xff0b0714).withAlpha (on ? 0.36f : 0.28f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 4.0f);
    juce::ColourGradient glow (accent.withAlpha (on ? 0.20f : 0.04f), r.getX(), r.getY(), accent.withAlpha (0.0f), r.getX() + 120.0f, r.getY() + 90.0f, false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (r, 4.0f);

    const auto ac = accent.withAlpha (on ? 0.95f : 0.35f);
    const float L = 16.0f;
    auto corner = [&] (float x, float y, float dx, float dy)
    {
        juce::Path p;
        p.startNewSubPath (x + dx * L, y);
        p.lineTo (x, y);
        p.lineTo (x, y + dy * L);
        g.setColour (ac);
        g.strokePath (p, juce::PathStrokeType (1.6f));
    };
    corner (r.getX(), r.getY(), 1, 1);
    corner (r.getRight(), r.getY(), -1, 1);
    corner (r.getX(), r.getBottom(), 1, -1);
    corner (r.getRight(), r.getBottom(), -1, -1);

    // Titel mit Kettennummer
    auto f = bb::fonts::display (13.0f, true);
    f.setExtraKerningFactor (0.10f);
    g.setFont (f);
    g.setColour (on ? bb::col::ink : bb::col::inkDim);
    g.drawText (title, (int) r.getX() + 14, (int) r.getY() + 7, (int) r.getWidth() - 60, 22, juce::Justification::centredLeft);
    g.setColour (ac.withAlpha (on ? 0.7f : 0.25f));
    g.fillRect (r.getX() + 14.0f, r.getY() + 31.0f, 22.0f, 2.0f);
}

void BraincutEditor::paintContent (juce::Graphics& g)
{
    paintCard (g, slicerCard, "1  SLICER", col::green, true);
    paintCard (g, stretchCard, "2  STRETCH", col::cyan, flag (ParamIDs::strOn));
    paintCard (g, repeatCard, "3  REPEAT", col::pink, flag (ParamIDs::repOn));
    paintCard (g, glitchCard, "4  GLITCH", col::violet, flag (ParamIDs::gliOn));
    paintCard (g, masterCard, "OUTPUT", col::orange, true);
}
