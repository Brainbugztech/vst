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

    const float cardY = 226.0f, cardH = 358.0f;
    const juce::Rectangle<float> slicerCard { 20.0f, cardY, 320.0f, cardH };
    const juce::Rectangle<float> stretchCard { 350.0f, cardY, 150.0f, cardH };
    const juce::Rectangle<float> repeatCard { 510.0f, cardY, 215.0f, cardH };
    const juce::Rectangle<float> glitchCard { 735.0f, cardY, 205.0f, cardH };
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
    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    fx = juce::Image (juce::Image::ARGB, 240, 150, true);
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
    const int cy = (int) cardY;
    sliceView.setBounds (20, 80, 920, 134);

    // Slicer: Modus, großer Regler, Bereichsbalken, kleine Regler
    cutBtn.setBounds (178, cy + 8, 74, 24);
    warpBtn.setBounds (256, cy + 8, 74, 24);
    repeatK.setBounds (100, cy + 40, 160, 152);
    phMinB.setBounds (32, cy + 202, 140, 28);
    phMaxB.setBounds (188, cy + 202, 140, 28);
    repMinB.setBounds (32, cy + 236, 140, 28);
    repMaxB.setBounds (188, cy + 236, 140, 28);
    loopK.setBounds (40, cy + 274, 84, 78);
    subdivK.setBounds (132, cy + 274, 84, 78);
    seedK.setBounds (224, cy + 274, 84, 78);

    // Stretch
    strOnBtn.setBounds (350 + 150 - 48, cy + 8, 40, 24);
    strAmtK.setBounds (350 + 15, cy + 40, 120, 152);
    strMixK.setBounds (350 + 20, cy + 206, 110, 140);

    // Repeat
    repOnBtn.setBounds (510 + 215 - 48, cy + 8, 40, 24);
    repChanceK.setBounds (510 + 47, cy + 40, 120, 152);
    repRateK.setBounds (510 + 12, cy + 274, 62, 78);
    repGateK.setBounds (510 + 76, cy + 274, 62, 78);
    repDecayK.setBounds (510 + 140, cy + 274, 62, 78);

    // Glitch
    gliOnBtn.setBounds (735 + 205 - 48, cy + 8, 40, 24);
    gliAmtK.setBounds (735 + 42, cy + 40, 120, 152);
    bitBtn.setBounds (735 + 14, cy + 204, 56, 26);
    revBtn.setBounds (735 + 74, cy + 204, 56, 26);
    stopBtn.setBounds (735 + 134, cy + 204, 56, 26);
    frzBtn.setBounds (735 + 14, cy + 236, 86, 26);
    pitBtn.setBounds (735 + 104, cy + 236, 86, 26);
    gliMixK.setBounds (735 + 60, cy + 274, 84, 78);

    // Kopfzeile
    prevBtn.setBounds (344, 20, 28, 32);
    presetBox.setBounds (376, 20, 206, 32);
    nextBtn.setBounds (586, 20, 28, 32);
    rollBtn.setBounds (630, 20, 62, 32);
    lockBtn.setBounds (698, 20, 62, 32);
    mixK.setBounds (800, 6, 64, 68);
    outK.setBounds (874, 6, 64, 68);

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
    g.fillAll (col::bg);
    const float W = (float) fx.getWidth(), H = (float) fx.getHeight();
    const float tt = (float) t;

    auto blob = [&] (float cx, float cy, float rad, juce::Colour c, float a)
    {
        juce::ColourGradient gr (c.withAlpha (a), cx * W, cy * H, c.withAlpha (0.0f), cx * W + rad * W, cy * H, true);
        g.setGradientFill (gr);
        g.fillEllipse (cx * W - rad * W, cy * H - rad * W, rad * W * 2.0f, rad * W * 2.0f);
    };
    blob (0.16f + 0.10f * std::sin (tt * 0.13f), 0.30f + 0.18f * std::cos (tt * 0.10f), 0.42f, col::green, 0.26f);
    blob (0.82f + 0.10f * std::cos (tt * 0.11f), 0.22f + 0.15f * std::sin (tt * 0.14f), 0.40f, col::pink, 0.24f);
    blob (0.52f + 0.22f * std::sin (tt * 0.07f), 0.88f + 0.10f * std::cos (tt * 0.12f), 0.46f, col::cyan, 0.20f);
    blob (0.34f + 0.25f * std::cos (tt * 0.09f), 0.62f + 0.15f * std::sin (tt * 0.08f), 0.34f, col::violet, 0.24f);
    blob (0.90f + 0.05f * std::sin (tt * 0.17f), 0.80f, 0.22f, col::orange, 0.16f);

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
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.fillPath (beam);
    }
}

void BraincutEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (fx, getLocalBounds().toFloat());

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
    // Glas-Karte: dunkle Fläche, weicher Farbschein von oben, feiner Rand
    juce::ColourGradient body (juce::Colour (0xff160e24).withAlpha (on ? 0.78f : 0.62f), 0, r.getY(),
                               juce::Colour (0xff0b0714).withAlpha (on ? 0.80f : 0.64f), 0, r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 14.0f);

    juce::ColourGradient glow (accent.withAlpha (on ? 0.22f : 0.05f), r.getCentreX(), r.getY(), accent.withAlpha (0.0f), r.getCentreX(), r.getY() + 150.0f, false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (r, 14.0f);

    juce::ColourGradient edge (accent.withAlpha (on ? 0.55f : 0.16f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.07f), r.getX(), r.getBottom(), false);
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

    // Kopf: Farbpunkt + Titel
    const float cx = r.getX() + 20.0f, cy = r.getY() + 20.0f;
    if (on)
    {
        g.setColour (accent.withAlpha (0.30f));
        g.fillEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    }
    g.setColour (on ? accent : accent.withAlpha (0.35f));
    g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    auto f = bb::fonts::display (13.5f, true);
    f.setExtraKerningFactor (0.05f);
    g.setFont (f);
    g.setColour (on ? bb::col::ink : bb::col::inkDim);
    g.drawText (title, (int) r.getX() + 34, (int) r.getY() + 8, (int) r.getWidth() - 84, 24, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (r.getX() + 16.0f, r.getY() + 38.0f, r.getWidth() - 32.0f, 1.0f);
}

void BraincutEditor::paintContent (juce::Graphics& g)
{
    // Kopfzeile: kleines Logo und Titel
    if (logo.isValid())
    {
        const float lh = 34.0f;
        const float lw = lh * (float) logo.getWidth() / (float) std::max (1, logo.getHeight());
        g.drawImage (logo, juce::Rectangle<float> (20.0f, 18.0f, lw, lh), juce::RectanglePlacement::centred);
    }
    juce::ColourGradient tg (col::green, 124.0f, 0.0f, col::pink, 320.0f, 0.0f, false);
    tg.addColour (0.5, col::cyan);
    g.setGradientFill (tg);
    auto tf = bb::fonts::display (28.0f, true);
    tf.setExtraKerningFactor (0.10f);
    g.setFont (tf);
    g.drawText ("BRAINCUT", 124, 14, 210, 40, juce::Justification::centredLeft);

    paintCard (g, slicerCard, "SLICER", col::green, true);
    paintCard (g, stretchCard, "STRETCH", col::cyan, flag (ParamIDs::strOn));
    paintCard (g, repeatCard, "REPEAT", col::pink, flag (ParamIDs::repOn));
    paintCard (g, glitchCard, "GLITCH", col::violet, flag (ParamIDs::gliOn));
}
