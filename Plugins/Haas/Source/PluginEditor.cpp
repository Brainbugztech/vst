#include "PluginEditor.h"

namespace
{
    constexpr float deckH = (float) (HaasEditor::baseH - HaasEditor::headerH - HaasEditor::sceneH); // 250

    void screw (juce::Graphics& g, float cx, float cy, float r)
    {
        juce::ColourGradient grad (juce::Colour (0xff6b6274), cx - r * 0.3f, cy - r * 0.3f, juce::Colour (0xff1c1820), cx + r, cy + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (cx - r * 0.6f, cy, cx + r * 0.6f, cy, 1.2f);
    }
}

HaasEditor::HaasEditor (HaasProcessor& p)
    : AudioProcessorEditor (&p), proc (p), scene (p)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    addAndMakeVisible (scene);

    for (auto* s : { &width, &monoBass })
    {
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (bb::LookAndFeel::startAngle, bb::LookAndFeel::endAngle, true);
        s->setMouseDragSensitivity (220);
        s->setVelocityBasedMode (false);
        addAndMakeVisible (*s);
    }
    width.getProperties().set ("krass", true);
    monoBass.getProperties().set ("accent", "brain");
    width.setTitle ("Width");
    monoBass.setTitle ("Mono Bass");

    widthAtt = std::make_unique<SA> (proc.apvts, ParamIDs::width, width);
    bassAtt  = std::make_unique<SA> (proc.apvts, ParamIDs::monoBass, monoBass);
    width.setDoubleClickReturnValue (true, 0.8);
    monoBass.setDoubleClickReturnValue (true, 120.0);

    for (auto* b : { &flipBtn, &monoBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    monoBtn.getProperties().set ("accent", "brain");
    flipAtt = std::make_unique<BA> (proc.apvts, ParamIDs::flip, flipBtn);
    monoAtt = std::make_unique<BA> (proc.apvts, ParamIDs::mono, monoBtn);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    lastMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

HaasEditor::~HaasEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Rectangle<float> HaasEditor::deckRect (float x, float y, float w, float h) const
{
    const float top = (float) (headerH + sceneH) * u;
    return { x * u, top + y * u, w * u, h * u };
}

void HaasEditor::resized()
{
    u = (float) getWidth() / (float) baseW;
    scene.setBounds (0, juce::roundToInt (headerH * u), getWidth(), juce::roundToInt (sceneH * u));
    width.setBounds (deckRect (190, 22, 150, 150).toNearestInt());
    monoBass.setBounds (deckRect (650, 22, 150, 150).toNearestInt());
    flipBtn.setBounds (deckRect (392, 176, 100, 34).toNearestInt());
    monoBtn.setBounds (deckRect (500, 176, 100, 34).toNearestInt());
}

void HaasEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.1, (now - lastMs) / 1000.0);
    lastMs = now;
    const auto prm = proc.readParams();
    const double period = 3.2 - std::min (1.0f, prm.width) * 2.6 - std::max (0.0f, prm.width - 1.0f) * 0.3;
    tubePhase += dt / std::max (0.25, period);
    repaint (deckRect (0, 0, (float) baseW, deckH).getSmallestIntegerContainer());
}

//==============================================================================
void HaasEditor::paint (juce::Graphics& g)
{
    g.fillAll (bb::col::voidBg);
    paintHeader (g, { 0, 0, (float) getWidth(), headerH * u });
    paintDeck (g, deckRect (0, 0, (float) baseW, deckH));
}

void HaasEditor::paintHeader (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff0c0810));
    g.fillRect (r);
    g.setColour (bb::col::line);
    g.drawHorizontalLine ((int) r.getBottom() - 1, 0, r.getRight());

    // Logo
    const float lh = 52 * u;
    const float lw = lh * (float) logo.getWidth() / (float) std::max (1, logo.getHeight());
    g.drawImage (logo, juce::Rectangle<float> (18 * u, (r.getHeight() - lh) / 2, lw, lh), juce::RectanglePlacement::centred);

    // Produktname mit pinkem A
    const float x0 = 18 * u + lw + 18 * u;
    g.setFont (bb::fonts::display (30 * u, true));
    const char* letters = "HAAS";
    float x = x0;
    for (int i = 0; i < 4; ++i)
    {
        const juce::String ch = juce::String::charToString ((juce::juce_wchar) letters[i]);
        g.setColour (i == 2 ? bb::col::brain : bb::col::ink);
        g.drawText (ch, juce::Rectangle<float> (x, 12 * u, 40 * u, 32 * u), juce::Justification::left);
        x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), ch) + 9 * u;
    }
    g.setFont (bb::fonts::mono (12 * u));
    g.setColour (bb::col::inkDim);
    g.drawText (juce::String::fromUTF8 ("Stereo-Spreizer \xc2\xb7 2 Regler \xc2\xb7 v0.1"),
                juce::Rectangle<float> (x0, 44 * u, 400 * u, 16 * u), juce::Justification::left);

    g.setFont (bb::fonts::mono (12 * u));
    g.drawText ("BRAIN BUGZ LAB", juce::Rectangle<float> (r.getRight() - 220 * u, 0, 200 * u, r.getHeight()),
                juce::Justification::centredRight);
}

void HaasEditor::paintDeck (juce::Graphics& g, juce::Rectangle<float> r)
{
    // gebürstetes Metall
    juce::ColourGradient metal (juce::Colour (0xff2a2430), 0, r.getY(), juce::Colour (0xff0f0c13), 0, r.getBottom(), false);
    metal.addColour (0.38, juce::Colour (0xff1a1520));
    g.setGradientFill (metal);
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.018f));
    for (float y = r.getY(); y < r.getBottom(); y += 3 * u) g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
    juce::ColourGradient top (bb::col::tox.withAlpha (0.07f), r.getCentreX(), r.getY(), bb::col::tox.withAlpha (0.0f),
                              r.getCentreX() + r.getWidth() * 0.5f, r.getY(), true);
    g.setGradientFill (top);
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) r.getY(), r.getX(), r.getRight());

    // Schrauben und Lüftungsschlitze
    for (auto p : { juce::Point<float> (22, 20), juce::Point<float> (baseW - 22.0f, 20), juce::Point<float> (22, deckH - 20),
                    juce::Point<float> (baseW - 22.0f, deckH - 20) })
        screw (g, r.getX() + p.x * u, r.getY() + p.y * u, 6 * u);
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < 6; ++i)
        {
            const auto v = deckRect (side ? baseW - 44 - 96.0f : 44.0f, 82 + i * 13.0f, 96, 6);
            g.setColour (juce::Colour (0xff07060a));
            g.fillRoundedRectangle (v, 3 * u);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawHorizontalLine ((int) v.getBottom(), v.getX() + 3 * u, v.getRight() - 3 * u);
        }

    // Mulden mit Schimmer
    const auto prm = proc.readParams();
    for (int i = 0; i < 2; ++i)
    {
        const auto well = deckRect (i ? 632.0f : 172.0f, 12, 186, 222);
        juce::ColourGradient wg (juce::Colour (0xff0d0b10), 0, well.getY(), juce::Colour (0xff16121b), 0, well.getBottom(), false);
        g.setGradientFill (wg);
        g.fillRoundedRectangle (well, 22 * u);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (well.reduced (1), 22 * u, 2 * u);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine ((int) well.getBottom(), well.getX() + 20 * u, well.getRight() - 20 * u);
        const auto glow = (i ? bb::col::brain : bb::col::tox);
        const auto c = well.getCentre().withY (well.getY() + 87 * u);
        g.setGradientFill (juce::ColourGradient (glow.withAlpha (0.16f), c.x, c.y, glow.withAlpha (0.0f), c.x + 85 * u, c.y, true));
        g.fillEllipse (c.x - 85 * u, c.y - 85 * u, 170 * u, 170 * u);

        // Beschriftung und Wert
        g.setFont (bb::fonts::display (13 * u));
        g.setColour (i ? bb::col::brain : juce::Colour (0xffcfc6d8));
        const juce::String label = i ? "MONO BASS" : "WIDTH";
        juce::String spaced;
        for (auto ch : label) spaced << juce::String::charToString (ch) << " ";
        g.drawText (spaced.trim(), juce::Rectangle<float> (well.getX(), well.getY() + 166 * u, well.getWidth(), 20 * u),
                    juce::Justification::centred);
        g.setFont (bb::fonts::mono (16 * u));
        g.setColour (bb::col::ink);
        const auto* prmObj = proc.apvts.getParameter (i ? ParamIDs::monoBass : ParamIDs::width);
        g.drawText (prmObj->getCurrentValueAsText(), juce::Rectangle<float> (well.getX(), well.getY() + 188 * u, well.getWidth(), 22 * u),
                    juce::Justification::centred);
    }

    // Anzeige-Zeile
    float dl, dr;
    bb::HaasEngine::delaysMs (prm, dl, dr);
    const auto disp = deckRect (380, 52, 230, 32);
    g.setColour (juce::Colour (0xff050806));
    g.fillRoundedRectangle (disp, 8 * u);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) disp.getBottom(), disp.getX() + 8 * u, disp.getRight() - 8 * u);
    g.setFont (bb::fonts::mono (13 * u));
    juce::AttributedString as;
    as.setJustification (juce::Justification::centred);
    as.append ("L ", bb::fonts::mono (13 * u), bb::col::inkDim);
    as.append (juce::String (dl, 1) + " ms", bb::fonts::mono (13 * u), bb::col::tox);
    as.append (juce::String::fromUTF8 ("  \xc2\xb7  R "), bb::fonts::mono (13 * u), bb::col::inkDim);
    as.append (juce::String (dr, 1) + " ms", bb::fonts::mono (13 * u), bb::col::brain);
    as.draw (g, disp.reduced (6 * u, 0));

    g.setFont (bb::fonts::mono (11 * u));
    g.setColour (bb::col::inkDim);
    g.drawText (prm.monoBass < 30 ? juce::String ("Bass-Mono aus") : "Bass mono unter " + juce::String (juce::roundToInt (prm.monoBass)) + " Hz",
                deckRect (380, 88, 230, 18), juce::Justification::centred);

    // Glasröhrchen mit fließender Flüssigkeit
    const auto tube = deckRect (380, 122, 230, 14);
    g.setColour (juce::Colour (0xff06050a));
    g.fillRoundedRectangle (tube, 7 * u);
    {
        juce::Graphics::ScopedSaveState ss (g);
        auto inner = tube.reduced (3 * u);
        juce::Path clip;
        clip.addRoundedRectangle (inner, 4 * u);
        g.reduceClipRegion (clip);
        const float seg = 60 * u, off = (float) std::fmod (tubePhase, 1.0) * seg * 2;
        for (float x = inner.getX() - seg * 2 + off; x < inner.getRight(); x += seg * 2)
        {
            g.setColour (bb::col::tox.withAlpha (0.9f));   g.fillRect (x, inner.getY(), 18 * u, inner.getHeight());
            g.setColour (bb::col::tox.withAlpha (0.35f));  g.fillRect (x + 18 * u, inner.getY(), 12 * u, inner.getHeight());
            g.setColour (bb::col::brain.withAlpha (0.85f)); g.fillRect (x + 30 * u, inner.getY(), 18 * u, inner.getHeight());
            g.setColour (bb::col::brain.withAlpha (0.3f));  g.fillRect (x + 48 * u, inner.getY(), 12 * u, inner.getHeight());
            g.setColour (bb::col::tox.withAlpha (0.9f));   g.fillRect (x + 60 * u, inner.getY(), 18 * u, inner.getHeight());
            g.setColour (bb::col::tox.withAlpha (0.35f));  g.fillRect (x + 78 * u, inner.getY(), 12 * u, inner.getHeight());
            g.setColour (bb::col::brain.withAlpha (0.85f)); g.fillRect (x + 90 * u, inner.getY(), 18 * u, inner.getHeight());
            g.setColour (bb::col::brain.withAlpha (0.3f));  g.fillRect (x + 108 * u, inner.getY(), 12 * u, inner.getHeight());
        }
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRoundedRectangle (inner.getX() + 3 * u, inner.getY(), inner.getWidth() - 6 * u, 2 * u, 1 * u);
    }
    g.setColour (bb::col::tox.withAlpha (0.15f));
    g.drawRoundedRectangle (tube.expanded (2 * u), 8 * u, 2 * u);
}
