#include "PluginEditor.h"

namespace
{
    constexpr float deckH = (float) (BugFuzzEditor::baseH - BugFuzzEditor::headerH - BugFuzzEditor::sceneH); // 250

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

//==============================================================================
// Output-Pegel: ziehen, Mausrad, Pfeiltasten, Doppelklick = 0 dB
class BugFuzzEditor::OutValue : public juce::Component
{
public:
    explicit OutValue (BugFuzzProcessor& p) : proc (p)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
        setTitle ("Output");
    }

    float current() const { return proc.apvts.getRawParameterValue (ParamIDs::out)->load(); }
    void set (float db) { proc.setParam (ParamIDs::out, juce::jlimit (-24.0f, 12.0f, db)); repaint(); }

    void paint (juce::Graphics& g) override
    {
        const float h = (float) getHeight();
        juce::AttributedString as;
        as.setJustification (juce::Justification::centred);
        as.append ("OUT  ", bb::fonts::mono (h * 0.42f), bb::col::inkDim);
        const float v = current();
        as.append ((v > 0.04f ? "+" : "") + juce::String (v, 1) + " dB", bb::fonts::mono (h * 0.42f),
                   v > 0.04f ? bb::col::brain : bb::col::tox);
        as.draw (g, getLocalBounds().toFloat().reduced (6, 0));
        if (hasKeyboardFocus (false) || isMouseOver())
        {
            g.setColour (bb::col::tox.withAlpha (0.35f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1), h * 0.25f, 1.0f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDown (const juce::MouseEvent&) override  { startValue = current(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float sens = e.mods.isShiftDown() ? 0.02f : 0.12f;
        set (startValue - (float) e.getDistanceFromDragStartY() * sens);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        set (current() + (w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.1f : 0.5f));
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { set (0.0f); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        const float st = k.getModifiers().isShiftDown() ? 0.1f : 0.5f;
        if (k.isKeyCode (juce::KeyPress::upKey))   { set (current() + st); return true; }
        if (k.isKeyCode (juce::KeyPress::downKey)) { set (current() - st); return true; }
        return false;
    }

private:
    BugFuzzProcessor& proc;
    float startValue = 0.0f;
};

//==============================================================================
BugFuzzEditor::BugFuzzEditor (BugFuzzProcessor& p)
    : AudioProcessorEditor (&p), proc (p), scene (p)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    addAndMakeVisible (scene);

    for (auto* s : { &drive, &mix })
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

    driveAtt = std::make_unique<SA> (proc.apvts, ParamIDs::drive, drive);
    mixAtt   = std::make_unique<SA> (proc.apvts, ParamIDs::mix, mix);
    drive.setDoubleClickReturnValue (true, 0.45);
    mix.setDoubleClickReturnValue (true, 1.0);

    for (auto* b : { &octaBtn, &agBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    agBtn.getProperties().set ("accent", "brain");
    octaAtt = std::make_unique<BA> (proc.apvts, ParamIDs::octa, octaBtn);
    agAtt   = std::make_unique<BA> (proc.apvts, ParamIDs::autoGain, agBtn);

    outVal = std::make_unique<OutValue> (proc);
    addAndMakeVisible (*outVal);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    lastMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

BugFuzzEditor::~BugFuzzEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Rectangle<float> BugFuzzEditor::deckRect (float x, float y, float w, float h) const
{
    const float top = (float) (headerH + sceneH) * u;
    return { x * u, top + y * u, w * u, h * u };
}

void BugFuzzEditor::resized()
{
    u = (float) getWidth() / (float) baseW;
    scene.setBounds (0, juce::roundToInt (headerH * u), getWidth(), juce::roundToInt (sceneH * u));
    drive.setBounds (deckRect (190, 22, 150, 150).toNearestInt());
    mix.setBounds (deckRect (650, 22, 150, 150).toNearestInt());
    outVal->setBounds (deckRect (380, 52, 230, 32).toNearestInt());
    octaBtn.setBounds (deckRect (392, 176, 100, 34).toNearestInt());
    agBtn.setBounds (deckRect (500, 176, 100, 34).toNearestInt());
}

void BugFuzzEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.1, (now - lastMs) / 1000.0);
    lastMs = now;
    const auto prm = proc.readParams();
    const double period = 3.0 - prm.drive * 2.4;
    tubePhase += dt / std::max (0.3, period);
    outVal->repaint();
    repaint (deckRect (0, 0, (float) baseW, deckH).getSmallestIntegerContainer());
}

//==============================================================================
void BugFuzzEditor::paint (juce::Graphics& g)
{
    g.fillAll (bb::col::voidBg);
    paintHeader (g, { 0, 0, (float) getWidth(), headerH * u });
    paintDeck (g, deckRect (0, 0, (float) baseW, deckH));
}

void BugFuzzEditor::paintHeader (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff0c0810));
    g.fillRect (r);
    g.setColour (bb::col::line);
    g.drawHorizontalLine ((int) r.getBottom() - 1, 0, r.getRight());

    // Logo
    const float lh = 52 * u;
    const float lw = lh * (float) logo.getWidth() / (float) std::max (1, logo.getHeight());
    g.drawImage (logo, juce::Rectangle<float> (18 * u, (r.getHeight() - lh) / 2, lw, lh), juce::RectanglePlacement::centred);

    // Produktname mit pinkem F
    const float x0 = 18 * u + lw + 18 * u;
    g.setFont (bb::fonts::display (30 * u, true));
    const char* letters = "BUGFUZZ";
    float x = x0;
    for (int i = 0; letters[i] != 0; ++i)
    {
        const juce::String ch = juce::String::charToString ((juce::juce_wchar) letters[i]);
        g.setColour (i == 3 ? bb::col::brain : bb::col::ink);
        g.drawText (ch, juce::Rectangle<float> (x, 12 * u, 40 * u, 32 * u), juce::Justification::left);
        x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), ch) + (i == 2 ? 14 : 7) * u;
    }
    g.setFont (bb::fonts::mono (12 * u));
    g.setColour (bb::col::inkDim);
    g.drawText (juce::String::fromUTF8 ("4-Band-Fuzz \xc2\xb7 Quadra-Prinzip \xc2\xb7 v0.1"),
                juce::Rectangle<float> (x0, 44 * u, 400 * u, 16 * u), juce::Justification::left);

    g.setFont (bb::fonts::mono (12 * u));
    g.drawText ("BRAIN BUGZ LAB", juce::Rectangle<float> (r.getRight() - 220 * u, 0, 200 * u, r.getHeight()),
                juce::Justification::centredRight);
}

void BugFuzzEditor::paintDeck (juce::Graphics& g, juce::Rectangle<float> r)
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
        const auto glow = i ? bb::col::brain : (prm.drive > 0.5f ? bb::col::tox.interpolatedWith (bb::col::brain, (prm.drive - 0.5f) * 2.0f) : bb::col::tox);
        const auto c = well.getCentre().withY (well.getY() + 87 * u);
        g.setGradientFill (juce::ColourGradient (glow.withAlpha (0.16f), c.x, c.y, glow.withAlpha (0.0f), c.x + 85 * u, c.y, true));
        g.fillEllipse (c.x - 85 * u, c.y - 85 * u, 170 * u, 170 * u);

        // Beschriftung und Wert
        g.setFont (bb::fonts::display (13 * u));
        g.setColour (i ? bb::col::brain : juce::Colour (0xffcfc6d8));
        const juce::String label = i ? "MIX" : "DRIVE";
        juce::String spaced;
        for (auto ch : label) spaced << juce::String::charToString (ch) << " ";
        g.drawText (spaced.trim(), juce::Rectangle<float> (well.getX(), well.getY() + 166 * u, well.getWidth(), 20 * u),
                    juce::Justification::centred);
        g.setFont (bb::fonts::mono (16 * u));
        g.setColour (bb::col::ink);
        const auto* prmObj = proc.apvts.getParameter (i ? ParamIDs::mix : ParamIDs::drive);
        g.drawText (prmObj->getCurrentValueAsText(), juce::Rectangle<float> (well.getX(), well.getY() + 188 * u, well.getWidth(), 22 * u),
                    juce::Justification::centred);
    }

    // Anzeige-Feld (der Wert selbst ist die ziehbare OutValue-Komponente)
    const auto disp = deckRect (380, 52, 230, 32);
    g.setColour (juce::Colour (0xff050806));
    g.fillRoundedRectangle (disp, 8 * u);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) disp.getBottom(), disp.getX() + 8 * u, disp.getRight() - 8 * u);

    float maxDb = 0.0f;
    for (auto b : prm.band) maxDb = std::max (maxDb, bb::BugFuzzEngine::bandGainDb (prm.drive, b));
    g.setFont (bb::fonts::mono (11 * u));
    g.setColour (bb::col::inkDim);
    juce::String info = "Fuzz bis +" + juce::String (juce::roundToInt (maxDb)) + " dB";
    if (prm.octa) info << juce::String::fromUTF8 (" \xc2\xb7 Oktave");
    if (prm.autoGain) info << juce::String::fromUTF8 (" \xc2\xb7 Auto-Gain");
    g.drawText (info, deckRect (360, 88, 270, 18), juce::Justification::centred);

    // Glasröhrchen: Flüssigkeit fließt schneller und knistert im Krass-Bereich
    const float krass = std::max (0.0f, prm.drive - 0.5f) * 2.0f;
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
        juce::Random rnd ((juce::int64) (tubePhase * 7.0));
        for (float x = inner.getX() - seg * 2 + off; x < inner.getRight(); x += seg * 2)
            for (int k = 0; k < 2; ++k)
            {
                const float xx = x + k * 60 * u;
                const float jit = 1.0f - krass * 0.5f * rnd.nextFloat();
                g.setColour (bb::col::tox.withAlpha (0.9f * jit));    g.fillRect (xx, inner.getY(), 18 * u, inner.getHeight());
                g.setColour (bb::col::tox.withAlpha (0.35f * jit));   g.fillRect (xx + 18 * u, inner.getY(), 12 * u, inner.getHeight());
                g.setColour (bb::col::brain.withAlpha (0.85f * jit)); g.fillRect (xx + 30 * u, inner.getY(), 18 * u, inner.getHeight());
                g.setColour (bb::col::brain.withAlpha (0.3f * jit));  g.fillRect (xx + 48 * u, inner.getY(), 12 * u, inner.getHeight());
            }
        // Funken im Röhrchen
        const int sparks = juce::roundToInt (krass * 6.0f);
        for (int k = 0; k < sparks; ++k)
        {
            g.setColour (juce::Colours::white.withAlpha (0.5f + 0.5f * rnd.nextFloat()));
            g.fillRect (inner.getX() + rnd.nextFloat() * inner.getWidth(), inner.getY(), 2 * u, inner.getHeight());
        }
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRoundedRectangle (inner.getX() + 3 * u, inner.getY(), inner.getWidth() - 6 * u, 2 * u, 1 * u);
    }
    g.setColour (bb::col::tox.withAlpha (0.15f));
    g.drawRoundedRectangle (tube.expanded (2 * u), 8 * u, 2 * u);
}
