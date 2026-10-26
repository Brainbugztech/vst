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

    const juce::Rectangle<float> slicerCard { 16.0f, 170.0f, 240.0f, 334.0f };
    const juce::Rectangle<float> stretchCard { 268.0f, 170.0f, 170.0f, 334.0f };
    const juce::Rectangle<float> repeatCard { 450.0f, 170.0f, 212.0f, 334.0f };
    const juce::Rectangle<float> glitchCard { 674.0f, 170.0f, 210.0f, 334.0f };
}

BraincutEditor::BraincutEditor (BraincutProcessor& p)
    : AudioProcessorEditor (&p), proc (p), sliceView (p),
      repeatK (p.apvts, ParamIDs::repeat, "REPEAT", col::green),
      phraseK (p.apvts, ParamIDs::phrase, "PHRASE", col::green),
      subdivK (p.apvts, ParamIDs::subdiv, "SUB", col::green),
      seedK (p.apvts, ParamIDs::seed, "SEED", col::green),
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
    fx = juce::Image (juce::Image::ARGB, 240, 140, true);
    renderFx (0.0);

    content.painter = [this] (juce::Graphics& g) { paintContent (g); };
    addAndMakeVisible (content);

    content.addAndMakeVisible (sliceView);
    for (auto* k : { &repeatK, &phraseK, &subdivK, &seedK, &strAmtK, &strMixK, &repChanceK, &repRateK, &repGateK, &repDecayK,
                     &gliAmtK, &gliMixK, &mixK, &outK })
        content.addAndMakeVisible (*k);

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
    sliceView.setBounds (16, 74, 868, 86);

    cutBtn.setBounds (30, 212, 98, 26);
    warpBtn.setBounds (134, 212, 98, 26);
    repeatK.setBounds (76, 246, 120, 150);
    phraseK.setBounds (20, 404, 72, 84);
    subdivK.setBounds (100, 404, 72, 84);
    seedK.setBounds (180, 404, 72, 84);

    strOnBtn.setBounds (376, 176, 48, 22);
    strAmtK.setBounds (293, 246, 120, 150);
    strMixK.setBounds (317, 404, 72, 84);

    repOnBtn.setBounds (600, 176, 48, 22);
    repChanceK.setBounds (496, 246, 120, 150);
    repRateK.setBounds (454, 404, 66, 84);
    repGateK.setBounds (524, 404, 66, 84);
    repDecayK.setBounds (594, 404, 66, 84);

    gliOnBtn.setBounds (822, 176, 48, 22);
    bitBtn.setBounds (684, 208, 58, 24);
    revBtn.setBounds (750, 208, 58, 24);
    stopBtn.setBounds (816, 208, 58, 24);
    frzBtn.setBounds (684, 238, 92, 24);
    pitBtn.setBounds (782, 238, 92, 24);
    gliAmtK.setBounds (727, 272, 104, 134);
    gliMixK.setBounds (743, 410, 72, 84);

    prevBtn.setBounds (330, 18, 28, 30);
    presetBox.setBounds (362, 18, 196, 30);
    nextBtn.setBounds (562, 18, 28, 30);
    rollBtn.setBounds (602, 18, 60, 30);
    lockBtn.setBounds (668, 18, 60, 30);
    mixK.setBounds (748, 0, 64, 66);
    outK.setBounds (820, 0, 64, 66);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    syncFromParams();
    lastMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (20);
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
    blob (0.16f + 0.10f * std::sin (tt * 0.13f), 0.30f + 0.18f * std::cos (tt * 0.10f), 0.42f, col::green, 0.34f);
    blob (0.82f + 0.10f * std::cos (tt * 0.11f), 0.22f + 0.15f * std::sin (tt * 0.14f), 0.40f, col::pink, 0.32f);
    blob (0.52f + 0.22f * std::sin (tt * 0.07f), 0.88f + 0.10f * std::cos (tt * 0.12f), 0.46f, col::cyan, 0.26f);
    blob (0.34f + 0.25f * std::cos (tt * 0.09f), 0.62f + 0.15f * std::sin (tt * 0.08f), 0.34f, col::violet, 0.28f);
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

void BraincutEditor::paintCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent,
                                bool on, const juce::String& caption)
{
    g.setColour (col::card.withAlpha (on ? 0.64f : 0.52f));
    g.fillRoundedRectangle (r, 12.0f);

    juce::ColourGradient glow (accent.withAlpha (on ? 0.20f : 0.06f), r.getX(), r.getY(), accent.withAlpha (0.0f), r.getX(), r.getY() + 110.0f, false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (r, 12.0f);

    g.setColour (accent.withAlpha (on ? 0.38f : 0.12f));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    g.setFont (bb::fonts::display (15.0f, true));
    g.setColour (accent.withAlpha (on ? 1.0f : 0.5f));
    g.drawText (title, r.getX() + 16.0f, r.getY() + 6.0f, r.getWidth() - 80.0f, 24.0f, juce::Justification::centredLeft);

    if (caption.isNotEmpty())
    {
        g.setFont (bb::fonts::mono (11.5f));
        g.setColour (bb::col::inkDim.withAlpha (on ? 0.9f : 0.5f));
        g.drawText (caption, r.getX() + 16.0f, r.getY() + 40.0f, r.getWidth() - 24.0f, 18.0f, juce::Justification::centredLeft);
    }
}

void BraincutEditor::paintContent (juce::Graphics& g)
{
    // Kopfzeile: kleines Logo und Titel
    if (logo.isValid())
    {
        const float lh = 38.0f;
        const float lw = lh * (float) logo.getWidth() / (float) std::max (1, logo.getHeight());
        g.drawImage (logo, juce::Rectangle<float> (16.0f, 14.0f, lw, lh), juce::RectanglePlacement::centred);
    }
    juce::ColourGradient tg (col::green, 112.0f, 0.0f, col::pink, 300.0f, 0.0f, false);
    tg.addColour (0.5, col::cyan);
    g.setGradientFill (tg);
    g.setFont (bb::fonts::display (30.0f, true));
    g.drawText ("BRAINCUT", 112, 10, 200, 36, juce::Justification::centredLeft);
    g.setFont (bb::fonts::mono (11.0f));
    g.setColour (bb::col::inkDim);
    g.drawText ("slice  stretch  repeat  glitch", 114, 44, 210, 14, juce::Justification::centredLeft);

    paintCard (g, slicerCard, "SLICER", col::green, true, {});
    paintCard (g, stretchCard, "STRETCH", col::cyan, flag (ParamIDs::strOn), juce::String::fromUTF8 ("Zufalls-Streckung \xc2\xb7 Pitch bleibt"));
    paintCard (g, repeatCard, "REPEAT", col::pink, flag (ParamIDs::repOn), juce::String::fromUTF8 ("Stotterer & Rolls"));
    paintCard (g, glitchCard, "GLITCH", col::violet, flag (ParamIDs::gliOn), {});
}
