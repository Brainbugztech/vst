#include "PluginEditor.h"

namespace
{
    const juce::Colour tox     { 0xffb6ff1f };
    const juce::Colour toxHot  { 0xffe4ff3a };
    const juce::Colour hotPink { 0xffff1fa8 };
    const juce::Colour pinkHot { 0xffff5cc4 };
    const juce::Colour cyan    { 0xff23e6f0 };

    constexpr float sweep = juce::MathConstants<float>::pi * 145.0f / 180.0f;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // Flächen im Originalbild
    const juce::Rectangle<float> transientBar { 150.0f, 790.0f, 174.0f, 18.0f };
    const juce::Rectangle<float> brainArt { 560.0f, 0.0f, 620.0f, 360.0f };
    const juce::Point<float> brainCentre { 860.0f, 170.0f };
    const juce::Rectangle<float> ledLeft  { 465.0f, 774.0f, 90.0f, 8.0f };
    const juce::Rectangle<float> ledRight { 1115.0f, 774.0f, 90.0f, 8.0f };

    juce::Path outputBarPath()
    {
        juce::Path p;
        p.startNewSubPath (622.0f, 731.0f);
        p.lineTo (1040.0f, 731.0f);
        p.lineTo (1063.0f, 751.0f);
        p.lineTo (601.0f, 751.0f);
        p.closeSubPath();
        return p;
    }

}

//==============================================================================
NeonKnob::NeonKnob (juce::Colour arcFrom, juce::Colour arcTo, juce::Colour pointer, float ringRadius)
    : ring (ringRadius), from (arcFrom), to (arcTo), ptr (pointer)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (twoPi - sweep, twoPi + sweep, true);
    setMouseDragSensitivity (260);
    setRepaintsOnMouseActivity (true);
    setVelocityBasedMode (false);
}

void NeonKnob::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float s = ring / 103.0f;
    const float pos = (float) valueToProportionOfLength (getValue());
    const float a0 = -sweep;
    const float a = a0 + pos * 2.0f * sweep;

    // Leuchtbogen: Stärke wie im Bild, Farbe läuft über den Bogen
    if (pos > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, ring, ring, 0.0f, a0, a, true);

        const auto p0 = c.getPointOnCircumference (ring, a0);
        const auto p1 = c.getPointOnCircumference (ring, a);
        juce::ColourGradient grad (from, p0.x, p0.y, to, p1.x, p1.y, false);

        auto stroke = [&] (float width, float alpha)
        {
            g.setGradientFill (grad);
            g.setOpacity (alpha);
            g.strokePath (arc, juce::PathStrokeType (width * s, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        };
        stroke (26.0f, 0.05f);
        stroke (19.0f, 0.09f);
        stroke (14.0f, 0.22f);
        stroke (9.5f, 1.0f);

        // Heller Kern
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.strokePath (arc, juce::PathStrokeType (2.6f * s, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // Zeiger
    const auto inner = c.getPointOnCircumference (ring * 0.31f, a);
    const auto outer = c.getPointOnCircumference (ring * 0.68f, a);
    juce::Line<float> line (inner, outer);
    auto drawPtr = [&] (float width, juce::Colour col)
    {
        juce::Path p;
        p.addLineSegment (line, 0.0f);
        juce::Path st;
        juce::PathStrokeType (width * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (st, p);
        g.setColour (col);
        g.fillPath (st);
    };
    drawPtr (13.0f, ptr.withAlpha (0.10f));
    drawPtr (8.0f, ptr.withAlpha (0.28f));
    drawPtr (4.6f, ptr);
    drawPtr (1.6f, juce::Colours::white.withAlpha (0.75f));

    // Wert beim Überfahren und Drehen auf der Kappe
    if (isMouseOverOrDragging())
    {
        const float r = ring * 0.30f;
        const auto txt = getTextFromValue (getValue());
        g.setColour (juce::Colours::black.withAlpha (0.74f));
        g.fillEllipse (c.x - r * 1.45f, c.y - r * 0.72f, r * 2.9f, r * 1.44f);
        g.setColour (ptr.brighter (0.2f));
        g.setFont (bb::fonts::mono (ring < 80.0f ? 16.0f : 25.0f));
        g.drawFittedText (txt, juce::Rectangle<float> (c.x - r * 1.4f, c.y - r * 0.6f, r * 2.8f, r * 1.2f).toNearestInt(),
                          juce::Justification::centred, 1);
    }
}

//==============================================================================
void AutoGainButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    const float c = 8.0f;

    juce::Path plate;
    plate.startNewSubPath (b.getX() + c, b.getY());
    plate.lineTo (b.getRight(), b.getY());
    plate.lineTo (b.getRight(), b.getBottom() - c);
    plate.lineTo (b.getRight() - c, b.getBottom());
    plate.lineTo (b.getX(), b.getBottom());
    plate.lineTo (b.getX(), b.getY() + c);
    plate.closeSubPath();

    g.setColour (juce::Colour (down ? 0xff060805 : 0xff0c0e0b).withAlpha (0.94f));
    g.fillPath (plate);

    if (on)
    {
        g.setColour (tox.withAlpha (0.10f));
        g.strokePath (plate, juce::PathStrokeType (8.0f));
        g.setColour (tox.withAlpha (0.24f));
        g.strokePath (plate, juce::PathStrokeType (4.0f));
    }
    g.setColour (on ? tox : juce::Colour (over ? 0xff8a8f86 : 0xff5a5f58));
    g.strokePath (plate, juce::PathStrokeType (1.8f));

    // Lampe
    const auto led = juce::Rectangle<float> (b.getX() + 12.0f, b.getCentreY() - 5.0f, 10.0f, 10.0f);
    if (on)
    {
        g.setColour (tox.withAlpha (0.28f));
        g.fillEllipse (led.expanded (5.0f));
    }
    g.setColour (on ? toxHot : juce::Colour (0xff2b2e29));
    g.fillEllipse (led);

    g.setColour (on ? tox : juce::Colour (over ? 0xffc8cdc4 : 0xff9aa096));
    g.setFont (bb::fonts::display (19.0f, true));
    g.drawText ("AUTO GAIN", b.withTrimmedLeft (28.0f).withTrimmedRight (4.0f), juce::Justification::centred, false);
}

//==============================================================================
KickTransmuterEditor::KickTransmuterEditor (KickTransmuterProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      attack  (tox, toxHot, tox, 103.0f),
      sustain (hotPink, pinkHot, pinkHot, 103.0f),
      punch   (tox, toxHot, tox, 103.0f),
      sat     (hotPink, pinkHot, pinkHot, 103.0f),
      low     (tox, toxHot, tox, 103.0f),
      mix     (cyan, tox, cyan, 103.0f),
      output  (tox, toxHot, tox, 61.0f)
{
    bg = juce::ImageCache::getFromMemory (KTAssets::kick_bg_jpg, KTAssets::kick_bg_jpgSize);
    brain = juce::ImageCache::getFromMemory (KTAssets::kick_brain_png, KTAssets::kick_brain_pngSize);

    content.painter = [this] (juce::Graphics& g) { paintContent (g); };
    addAndMakeVisible (content);

    auto place = [this] (NeonKnob& k, float cx, float cy, const char* title)
    {
        const float r = k.ring + 12.0f;
        k.setBounds (juce::Rectangle<float> (cx - r, cy - r, 2.0f * r, 2.0f * r).toNearestInt());
        k.setTitle (title);
        content.addAndMakeVisible (k);
    };
    place (attack,  178.0f, 529.0f, "Attack");
    place (sustain, 445.5f, 530.0f, "Sustain");
    place (punch,   708.0f, 530.0f, "Punch");
    place (sat,     971.0f, 530.0f, "Saturation");
    place (low,     1233.5f, 529.5f, "Low End");
    place (mix,     1496.5f, 529.5f, "Mix");
    place (output,  1529.0f, 793.5f, "Output");

    auto& s = p.apvts;
    attAttack  = std::make_unique<SA> (s, ParamIDs::attack, attack);
    attSustain = std::make_unique<SA> (s, ParamIDs::sustain, sustain);
    attPunch   = std::make_unique<SA> (s, ParamIDs::punch, punch);
    attSat     = std::make_unique<SA> (s, ParamIDs::sat, sat);
    attLow     = std::make_unique<SA> (s, ParamIDs::lowEnd, low);
    attMix     = std::make_unique<SA> (s, ParamIDs::mix, mix);
    attOutput  = std::make_unique<SA> (s, ParamIDs::output, output);

    autoGainBtn.setBounds (1320, 828, 134, 32);
    autoGainBtn.setTitle ("Auto Gain");
    content.addAndMakeVisible (autoGainBtn);
    attAuto = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, ParamIDs::autoGain, autoGainBtn);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, artW, artH);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) artW / (double) artH);
    setSize (baseW, baseH);

    lastHits = proc.engine.vizHits.load (std::memory_order_relaxed);
    lastMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (40);
}

KickTransmuterEditor::~KickTransmuterEditor()
{
    stopTimer();
}

void KickTransmuterEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void KickTransmuterEditor::resized()
{
    const float s = (float) getWidth() / (float) artW;
    content.setBounds (0, 0, artW, artH);
    content.setTransform (juce::AffineTransform::scale (s));
}

void KickTransmuterEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.2, (now - lastMs) * 0.001);
    lastMs = now;

    auto& e = proc.engine;

    const int hits = e.vizHits.load (std::memory_order_relaxed);
    const float oldPulse = pulse;
    if (hits != lastHits)
    {
        lastHits = hits;
        pulse = 1.0f;
    }
    else
    {
        pulse *= std::exp (-dt / 0.16f);
        if (pulse < 0.01f) pulse = 0.0f;
    }

    // Anzeigewerte: schnell hoch, langsam runter
    const float newT = e.vizTransientDb.exchange (0.0f, std::memory_order_relaxed);
    const float fall = std::exp (-dt / 0.18f);
    const float oldT = transientDb;
    transientDb = std::abs (newT) > std::abs (transientDb * fall) ? newT : transientDb * fall;
    if (std::abs (transientDb) < 0.02f) transientDb = 0.0f;

    const float pk = e.vizOutPeak.exchange (0.0f, std::memory_order_relaxed);
    const float oldOut = outLevel;
    outLevel = std::max (pk, outLevel * std::exp (-dt / 0.22f));
    if (outLevel < 1.0e-4f) outLevel = 0.0f;

    const float ag = e.vizAutoGainDb.load (std::memory_order_relaxed);
    const bool agOn = autoGainBtn.getToggleState();

    if (pulse != oldPulse)
    {
        content.repaint (brainArt.expanded (12.0f).toNearestInt());
        content.repaint (ledLeft.expanded (12.0f).toNearestInt());
        content.repaint (ledRight.expanded (12.0f).toNearestInt());
    }
    if (transientDb != oldT)
        content.repaint (transientBar.expanded (10.0f).toNearestInt());
    if (outLevel != oldOut)
        content.repaint (outputBarPath().getBounds().expanded (6.0f).toNearestInt());
    if (ag != shownAg || agOn != shownAgOn)
    {
        shownAg = ag;
        shownAgOn = agOn;
        content.repaint (1322, 738, 130, 34);
    }
}

void KickTransmuterEditor::paintContent (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (bg, content.getLocalBounds().toFloat());

    // Gehirn pulsiert bei jeder Kick, die Kabel leuchten mit
    if (pulse > 0.0f)
    {
        const float scale = 1.0f + 0.016f * pulse;
        const auto tr = juce::AffineTransform::translation (brainArt.getX(), brainArt.getY())
                            .scaled (scale, scale, brainCentre.x, brainCentre.y);
        g.setOpacity (juce::jmin (1.0f, pulse * 1.05f));
        g.drawImageTransformed (brain, tr, false);

        for (const auto& led : { ledLeft, ledRight })
        {
            g.setColour (tox.withAlpha (0.16f * pulse));
            g.fillRoundedRectangle (led.expanded (8.0f), 6.0f);
            g.setColour (toxHot.withAlpha (0.9f * pulse));
            g.fillRoundedRectangle (led, 2.0f);
        }
    }

    // Linker Balken: was Attack und Sustain gerade tun (Mitte = 0, rechts lauter, links leiser)
    {
        const auto b = transientBar;
        g.setColour (juce::Colour (0xff070708));
        g.fillRect (b);
        const float cx = b.getCentreX();
        const float v = juce::jlimit (-1.0f, 1.0f, transientDb / 18.0f);
        if (v != 0.0f)
        {
            const float w = std::abs (v) * b.getWidth() * 0.5f;
            const auto r = v > 0.0f ? juce::Rectangle<float> (cx, b.getY(), w, b.getHeight())
                                    : juce::Rectangle<float> (cx - w, b.getY(), w, b.getHeight());
            const auto col = v > 0.0f ? hotPink : tox;
            g.setColour (col.withAlpha (0.25f));
            g.fillRect (r.expanded (0.0f, 4.0f));
            g.setColour (col);
            g.fillRect (r);
        }
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillRect (cx - 1.0f, b.getY() - 2.0f, 2.0f, b.getHeight() + 4.0f);
    }

    // Mittlerer Balken: Ausgangspegel (-48 .. +6 dB), ab 0 dB pink
    {
        const auto path = outputBarPath();
        const auto b = path.getBounds();
        g.setColour (juce::Colour (0xff070807));
        g.fillPath (path);

        if (outLevel > 0.0f)
        {
            const float db = juce::Decibels::gainToDecibels (outLevel, -100.0f);
            const float pos = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 54.0f);
            const float zeroPos = 48.0f / 54.0f;
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (path);
            juce::ColourGradient grad (tox, b.getX(), 0.0f, hotPink, b.getRight(), 0.0f, false);
            grad.addColour (zeroPos - 0.12, toxHot);
            grad.addColour (zeroPos, juce::Colour (0xffffb020));
            g.setGradientFill (grad);
            g.fillRect (b.withWidth (b.getWidth() * pos));
        }
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        const float zx = b.getX() + b.getWidth() * (48.0f / 54.0f);
        g.fillRect (zx - 1.0f, b.getY(), 2.0f, b.getHeight());
    }

    // Auto-Gain-Wert über dem OUTPUT-Schriftzug
    if (shownAgOn)
    {
        g.setColour (tox.withAlpha (0.92f));
        g.setFont (bb::fonts::mono (22.0f));
        const juce::String txt = (shownAg > 0.04f ? "+" : "") + juce::String (shownAg, 1) + " dB";
        g.drawText (txt, juce::Rectangle<int> (1322, 740, 130, 30), juce::Justification::centred, false);
    }
}
