#include "PluginEditor.h"

using namespace bb;

namespace
{
const char* modeNames[4]  { "STRINGS", "CELLOS", "ATMOS PADS", "DRUMS" };
const char* articNames[4] { "SUSTAIN", "SPICCATO", "PIZZICATO", "TREMOLO" };
const juce::Colour violet { 0xff8b3dff };
}

OrchestraEditor::OrchestraEditor (OrchestraProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    brain = juce::ImageCache::getFromMemory (BBAssets::head_png, BBAssets::head_pngSize);

    for (int i = 0; i < 4; ++i)
    {
        modeBtn[i].setButtonText (juce::String ("          ") + modeNames[i]); // Platz fuer das Icon links
        modeBtn[i].setClickingTogglesState (false);
        modeBtn[i].getProperties().set ("accent", "brain");
        modeBtn[i].onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter ("mode")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        };
        addAndMakeVisible (modeBtn[i]);

        articBtn[i].setButtonText (articNames[i]);
        articBtn[i].getProperties().set ("mono", true);
        articBtn[i].onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter ("artic")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        };
        addAndMakeVisible (articBtn[i]);
    }

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setColour (juce::ComboBox::backgroundColourId, col::panel2);
    presetBox.setColour (juce::ComboBox::outlineColourId, col::line);
    presetBox.setColour (juce::ComboBox::textColourId, col::ink);
    presetBox.setColour (juce::ComboBox::arrowColourId, col::tox);
    presetBox.setTextWhenNothingSelected ("PRESET");
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0 && id - 1 != proc.getCurrentProgram()) selectPreset (id - 1); };
    addAndMakeVisible (presetBox);
    prevBtn.onClick = [this] { const int n = proc.getNumPrograms(); selectPreset ((proc.getCurrentProgram() + n - 1) % n); };
    nextBtn.onClick = [this] { selectPreset ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    for (auto* b : { &prevBtn, &nextBtn }) { b->getProperties().set ("mono", true); addAndMakeVisible (*b); }

    setupKnob (kAttack, "attack", "ATTACK");
    setupKnob (kRelease, "release", "RELEASE");
    setupKnob (kTone, "tone", "TONE");
    setupKnob (kTune, "tune", "TUNE");
    setupKnob (kEnsemble, "ensemble", "ENSEMBLE", "brain");
    setupKnob (kHumanize, "humanize", "HUMANIZE", "brain");
    setupKnob (kAir, "air", "AIR", "brain");
    setupKnob (kShimmer, "shimmer", "SHIMMER", "brain");
    setupKnob (kMotion, "motion", "MOTION", "brain");
    setupKnob (kPunch, "punch", "PUNCH", "brain");
    setupKnob (kDecay, "decay", "DECAY", "brain");
    setupKnob (kReverb, "reverb", "REVERB");
    setupKnob (kSpace, "space", "SPACE");
    setupKnob (kWidth, "width", "WIDTH");
    setupKnob (kVolume, "volume", "VOLUME");

    setSize (940, 600);
    updateModeUi();
    startTimerHz (30);
}

OrchestraEditor::~OrchestraEditor() { setLookAndFeel (nullptr); }

void OrchestraEditor::setupKnob (Knob& k, const char* id, const char* text, const char* accent)
{
    k.slider.getProperties().set ("accent", accent);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, col::inkDim);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (fonts::display (13.0f, true));
    k.label.setColour (juce::Label::textColourId, juce::String (accent) == "brain" ? col::brain : col::tox);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void OrchestraEditor::selectPreset (int idx)
{
    proc.setCurrentProgram (idx);
    presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
}

void OrchestraEditor::updateModeUi()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const int artic = (int) proc.apvts.getRawParameterValue ("artic")->load();
    shownMode = mode; shownArtic = artic;

    for (int i = 0; i < 4; ++i)
    {
        modeBtn[i].setToggleState (i == mode, juce::dontSendNotification);
        articBtn[i].setToggleState (i == artic, juce::dontSendNotification);
        articBtn[i].setVisible (mode == orch::M_STRINGS || mode == orch::M_CELLOS);
    }
    auto show = [] (Knob& k, bool v) { k.slider.setVisible (v); k.label.setVisible (v); };
    show (kEnsemble, mode <= orch::M_CELLOS); show (kHumanize, mode <= orch::M_CELLOS);
    show (kAir, mode == orch::M_ATMOS); show (kShimmer, mode == orch::M_ATMOS); show (kMotion, mode == orch::M_ATMOS);
    show (kPunch, mode == orch::M_DRUMS); show (kDecay, mode == orch::M_DRUMS);
    // Drums sind One-Shots: Attack/Release greifen dort nicht
    for (auto* k : { &kAttack, &kRelease })
    {
        k->slider.setEnabled (mode != orch::M_DRUMS);
        k->slider.setAlpha (mode == orch::M_DRUMS ? 0.3f : 1.0f);
        k->label.setAlpha (mode == orch::M_DRUMS ? 0.3f : 1.0f);
    }
    resized();
    repaint();
}

void OrchestraEditor::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const int artic = (int) proc.apvts.getRawParameterValue ("artic")->load();
    if (mode != shownMode || artic != shownArtic) updateModeUi();
    if (proc.getCurrentProgram() != shownPreset || proc.presetChanged.load() != 0)
    {
        proc.presetChanged = 0;
        shownPreset = proc.getCurrentProgram();
        presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
    }

    phase += 1.0f / 30.0f;
    levelSm = std::max (proc.engine.level, levelSm * 0.9f);
    pulse = 0.5f + 0.5f * std::sin (phase * 2.2f);

    // Drum-Pads aufleuchten lassen
    const auto t = proc.engine.lastHitTime.load();
    if (t != lastHitSeen)
    {
        lastHitSeen = t;
        const int n = proc.engine.lastHitNote.load() - 36;
        if (juce::isPositiveAndBelow (n, (int) padGlow.size())) padGlow[(size_t) n] = 1.0f;
    }
    for (auto& gl : padGlow) gl *= 0.88f;

    repaint (displayArea.expanded (4));
    repaint (0, 0, 560, 84);
}

void OrchestraEditor::resized()
{
    auto r = getLocalBounds().reduced (22, 0);
    auto header = r.removeFromTop (84);
    auto presetArea = header.removeFromRight (300).withSizeKeepingCentre (300, 34);
    prevBtn.setBounds (presetArea.removeFromLeft (34));
    nextBtn.setBounds (presetArea.removeFromRight (34));
    presetBox.setBounds (presetArea.reduced (6, 0));

    auto tiles = r.removeFromTop (70);
    const int tw = (tiles.getWidth() - 3 * 12) / 4;
    for (int i = 0; i < 4; ++i) modeBtn[i].setBounds (tiles.getX() + i * (tw + 12), tiles.getY() + 6, tw, 58);

    r.removeFromTop (12);
    displayArea = r.removeFromTop (178);
    auto artRow = r.removeFromTop (46).withTrimmedTop (8);
    const int aw = 150, total = aw * 4 + 10 * 3;
    for (int i = 0; i < 4; ++i)
        articBtn[i].setBounds (artRow.getCentreX() - total / 2 + i * (aw + 10), artRow.getY(), aw, 32);

    // Reglerreihe: 4 Grundregler | Modus-Regler | 4 FX-Regler
    r.removeFromTop (10);
    auto knobs = r.removeFromTop (150);
    std::vector<Knob*> row { &kAttack, &kRelease, &kTone, &kTune };
    const int mode = shownMode;
    if (mode <= orch::M_CELLOS) { row.push_back (&kEnsemble); row.push_back (&kHumanize); }
    else if (mode == orch::M_ATMOS) { row.push_back (&kAir); row.push_back (&kShimmer); row.push_back (&kMotion); }
    else { row.push_back (&kPunch); row.push_back (&kDecay); }
    for (auto* k : { &kReverb, &kSpace, &kWidth, &kVolume }) row.push_back (k);
    const int kw = knobs.getWidth() / (int) row.size();
    for (size_t i = 0; i < row.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (knobs.getX() + (int) i * kw, knobs.getY(), kw, knobs.getHeight()).reduced (4, 6);
        row[i]->label.setBounds (cell.removeFromTop (20));
        row[i]->slider.setBounds (cell);
    }
    modeKnobArea = juce::Rectangle<int> (knobs.getX() + 4 * kw, knobs.getY(), kw * ((int) row.size() - 8), knobs.getHeight());
}

// ---------------------------------------------------------------------------------------------
void OrchestraEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (col::voidBg);

    // Hintergrund: violetter Nebel + feines Raster
    juce::ColourGradient neb (violet.withAlpha (0.18f), b.getWidth() * 0.15f, b.getHeight() * 0.35f,
                              juce::Colours::transparentBlack, b.getWidth() * 0.6f, b.getHeight() * 0.9f, true);
    g.setGradientFill (neb); g.fillRect (b);
    juce::ColourGradient neb2 (col::brain.withAlpha (0.10f), b.getWidth() * 0.9f, b.getHeight() * 0.1f,
                               juce::Colours::transparentBlack, b.getWidth() * 0.4f, b.getHeight() * 0.6f, true);
    g.setGradientFill (neb2); g.fillRect (b);
    g.setColour (col::line.withAlpha (0.25f));
    for (float x = 0; x < b.getWidth(); x += 28) g.drawVerticalLine ((int) x, 0, b.getHeight());
    for (float y = 0; y < b.getHeight(); y += 28) g.drawHorizontalLine ((int) y, 0, b.getWidth());

    // Rahmen mit Neon-Ecken
    auto frame = b.reduced (6.0f);
    g.setColour (col::line); g.drawRoundedRectangle (frame, 10.0f, 1.5f);
    g.setColour (col::brain.withAlpha (0.8f));
    const float cl = 34.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float x = (i & 1) ? frame.getRight() : frame.getX(), y = (i & 2) ? frame.getBottom() : frame.getY();
        const float sx = (i & 1) ? -1.0f : 1.0f, sy = (i & 2) ? -1.0f : 1.0f;
        juce::Path p; p.startNewSubPath (x + sx * cl, y); p.lineTo (x, y); p.lineTo (x, y + sy * cl);
        g.strokePath (p, juce::PathStrokeType (2.5f));
    }

    // Logo: Brain-Bugz-Schriftzug, leuchtet mit dem Pegel
    const auto logoR = juce::Rectangle<float> (20.0f, 8.0f, 168.0f, 75.0f);
    const float glow = juce::jlimit (0.0f, 1.0f, 0.25f * pulse + 1.5f * levelSm);
    g.setGradientFill (juce::ColourGradient (col::tox.withAlpha (0.22f * glow), logoR.getCentreX(), logoR.getCentreY(),
                                             juce::Colours::transparentBlack, logoR.getRight() + 10.0f, logoR.getCentreY(), true));
    g.fillEllipse (logoR.expanded (14.0f, 4.0f));
    if (logo.isValid()) g.drawImage (logo, logoR, juce::RectanglePlacement::centred);
    // Gehirn-Kaefer-Kopf, pulsiert mit dem Pegel
    if (brain.isValid())
    {
        const float bs = 1.0f + 0.06f * glow;
        auto br = juce::Rectangle<float> (194.0f, 22.0f, 54.0f, 27.0f).withSizeKeepingCentre (54.0f * bs, 27.0f * bs);
        g.setColour (col::brain.withAlpha (0.25f * glow));
        g.fillEllipse (br.expanded (6.0f));
        g.setOpacity (1.0f);
        g.drawImage (brain, br, juce::RectanglePlacement::centred);
    }
    g.setColour (col::brain);
    g.setFont (fonts::display (26.0f, true));
    g.drawText ("ORCHESTRA", 258, 16, 240, 30, juce::Justification::centredLeft);
    g.setFont (fonts::mono (12.0f));
    g.setColour (col::inkDim);
    g.drawText ("CINEMATIC STRINGS / CELLOS / PADS / DRUMS", 260, 46, 300, 18, juce::Justification::centredLeft);

    // Modus-Kacheln: Icons auf die Buttons (Buttons zeichnen Hintergrund + Text)
    drawDisplay (g, displayArea.toFloat());

    // Trenner um die Modus-Regler
    if (! modeKnobArea.isEmpty())
    {
        auto m = modeKnobArea.toFloat().reduced (2.0f, 0.0f);
        g.setColour (col::brain.withAlpha (0.08f));
        g.fillRoundedRectangle (m, 8.0f);
        g.setColour (col::brain.withAlpha (0.35f));
        g.drawRoundedRectangle (m, 8.0f, 1.0f);
    }

    g.setFont (fonts::mono (12.0f));
    g.setColour (col::inkDim.withAlpha (0.7f));
    g.drawText ("MOD WHEEL / CC11 = DYNAMICS   |   SAMPLES: VSCO 2 CE (CC0)   |   BRAIN BUGZ",
                getLocalBounds().removeFromBottom (30), juce::Justification::centred);
}

void OrchestraEditor::paintOverChildren (juce::Graphics& g)
{
    for (int i = 0; i < 4; ++i)
    {
        auto r = modeBtn[i].getBounds().toFloat();
        const auto c = modeBtn[i].getToggleState() ? col::brain : col::inkDim.withAlpha (0.6f);
        drawModeIcon (g, i, r.removeFromLeft (52).reduced (12.0f), c);
    }
}

void OrchestraEditor::drawModeIcon (juce::Graphics& g, int mode, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour (c);
    juce::Path p;
    const float cx = r.getCentreX(), cy = r.getCentreY(), w = r.getWidth(), h = r.getHeight();
    if (mode == 0 || mode == 1)
    {
        // Geigen-/Cello-Korpus + Bogen
        const float s = mode == 0 ? 0.8f : 1.0f;
        p.addEllipse (cx - w * 0.22f * s, cy - h * 0.05f, w * 0.44f * s, h * 0.42f * s);
        p.addEllipse (cx - w * 0.17f * s, cy - h * 0.38f * s, w * 0.34f * s, h * 0.36f * s);
        p.startNewSubPath (cx, cy - h * 0.5f); p.lineTo (cx, cy + h * 0.3f);
        p.startNewSubPath (cx - w * 0.5f, cy + h * 0.1f); p.lineTo (cx + w * 0.5f, cy - h * 0.35f);
    }
    else if (mode == 2)
    {
        for (int k = 0; k < 3; ++k)
        {
            const float yy = cy - h * 0.25f + (float) k * h * 0.25f;
            p.startNewSubPath (r.getX(), yy);
            for (float x = 0; x <= 1.0f; x += 0.1f)
                p.lineTo (r.getX() + x * w, yy + std::sin (x * 6.28f + (float) k + phase) * h * 0.08f);
        }
    }
    else
    {
        p.addEllipse (cx - w * 0.4f, cy - h * 0.2f, w * 0.8f, h * 0.25f);
        p.startNewSubPath (cx - w * 0.4f, cy - h * 0.07f); p.lineTo (cx - w * 0.4f, cy + h * 0.3f);
        p.startNewSubPath (cx + w * 0.4f, cy - h * 0.07f); p.lineTo (cx + w * 0.4f, cy + h * 0.3f);
        p.startNewSubPath (cx - w * 0.4f, cy + h * 0.3f); p.quadraticTo (cx, cy + h * 0.45f, cx + w * 0.4f, cy + h * 0.3f);
        p.startNewSubPath (cx - w * 0.1f, cy - h * 0.2f); p.lineTo (cx - w * 0.35f, cy - h * 0.5f);
        p.startNewSubPath (cx + w * 0.1f, cy - h * 0.2f); p.lineTo (cx + w * 0.35f, cy - h * 0.5f);
    }
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Klick auf ein Drum-Pad spielt das Instrument an
void OrchestraEditor::mouseDown (const juce::MouseEvent& e)
{
    if (shownMode != orch::M_DRUMS || ! displayArea.contains (e.getPosition())) return;
    auto inner = displayArea.toFloat().reduced (14.0f, 12.0f);
    const int cx = (int) ((e.position.x - inner.getX()) / (inner.getWidth() / 7.0f));
    const int cy = (int) ((e.position.y - inner.getY()) / (inner.getHeight() / 4.0f));
    if (juce::isPositiveAndBelow (cx, 7) && juce::isPositiveAndBelow (cy, 4))
        proc.padNote = orch::kitPieces[cy * 7 + cx].note;
}

void OrchestraEditor::drawDisplay (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::panel.withAlpha (0.92f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (col::tox.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 10.0f, 1.3f);

    auto inner = r.reduced (14.0f, 12.0f);
    const int mode = shownMode;

    if (proc.loading.load())
    {
        g.setColour (col::tox.withAlpha (0.6f + 0.4f * pulse));
        g.setFont (fonts::display (22.0f, true));
        g.drawText ("LOADING SAMPLES ...", inner, juce::Justification::centred);
        auto bar = inner.withSizeKeepingCentre (260.0f, 4.0f).translated (0, 26);
        g.setColour (col::line); g.fillRoundedRectangle (bar, 2.0f);
        const float x = std::fmod (phase * 0.8f, 1.0f);
        g.setColour (col::brain);
        g.fillRoundedRectangle (bar.withWidth (60.0f).withX (bar.getX() + x * (bar.getWidth() - 60.0f)), 2.0f);
        return;
    }

    if (mode == orch::M_DRUMS)
    {
        // Kit-Karte: 28 Pads (C1 .. D#3), leuchten beim Anschlag
        const int cols = 7, rows = 4;
        const float pw = inner.getWidth() / (float) cols, ph = inner.getHeight() / (float) rows;
        for (int i = 0; i < 28; ++i)
        {
            auto pad = juce::Rectangle<float> (inner.getX() + (float) (i % cols) * pw, inner.getY() + (float) (i / cols) * ph, pw, ph).reduced (3.0f);
            const float gl = padGlow[(size_t) i];
            g.setColour (col::panel2.interpolatedWith (col::brain, gl * 0.6f));
            g.fillRoundedRectangle (pad, 5.0f);
            g.setColour (gl > 0.05f ? col::brain : col::line);
            g.drawRoundedRectangle (pad, 5.0f, 1.0f);
            const int note = orch::kitPieces[i].note;
            const juce::String nn = juce::MidiMessage::getMidiNoteName (note, true, true, 3);
            g.setFont (fonts::mono (11.0f));
            g.setColour (col::tox.withAlpha (0.8f));
            g.drawText (nn, pad.reduced (6.0f, 3.0f), juce::Justification::topLeft);
            g.setFont (fonts::display (12.5f, true));
            g.setColour (col::ink);
            g.drawFittedText (juce::String (orch::kitPieces[i].name).toUpperCase(), pad.reduced (6.0f, 3.0f).toNearestInt(),
                              juce::Justification::bottomLeft, 1);
        }
        return;
    }

    // Strings/Cellos/Atmos: leuchtende Wellen, die mit dem Pegel atmen
    const float lv = juce::jlimit (0.0f, 1.0f, levelSm * 1.6f);
    const int lines = mode == orch::M_ATMOS ? 9 : 6;
    for (int k = 0; k < lines; ++k)
    {
        juce::Path p;
        const float f = (float) k / (float) (lines - 1);
        const float amp = inner.getHeight() * (0.06f + 0.30f * lv) * (0.4f + 0.6f * std::sin (f * 3.14f));
        const float speed = mode == orch::M_ATMOS ? 0.4f : 1.2f;
        for (int xi = 0; xi <= 100; ++xi)
        {
            const float x = (float) xi * 0.01f;
            float y = std::sin (x * (5.0f + (float) k) + phase * speed * (1.0f + f)) * std::sin (x * 3.14159f);
            if (mode != orch::M_ATMOS && shownArtic == orch::A_TREM) y *= 0.7f + 0.3f * std::sin (x * 120.0f + phase * 30.0f);
            if (mode != orch::M_ATMOS && (shownArtic == orch::A_SPIC || shownArtic == orch::A_PIZZ))
                y *= std::pow (std::abs (std::sin (x * 9.0f - phase * 2.0f)), 6.0f) * 1.6f;
            const float px = inner.getX() + x * inner.getWidth(), py = inner.getCentreY() + y * amp;
            if (xi == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        const auto c = col::tox.interpolatedWith (col::brain, f).withAlpha (0.35f + 0.55f * (1.0f - std::abs (f - 0.5f)));
        g.setColour (c.withAlpha (0.15f));
        g.strokePath (p, juce::PathStrokeType (5.0f));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }

    // Beschriftung
    g.setFont (fonts::display (15.0f, true));
    g.setColour (col::ink.withAlpha (0.85f));
    juce::String info = modeNames[mode];
    if (mode <= orch::M_CELLOS) info << "  /  " << articNames[shownArtic];
    else info << "  /  STRING LAYERS + AIR";
    g.drawText (info, inner.removeFromTop (20.0f), juce::Justification::topLeft);
    g.setFont (fonts::mono (12.0f));
    g.setColour (col::inkDim);
    const char* range = mode == orch::M_STRINGS ? "RANGE C1 - C7  |  CELLOS < C3 < VIOLAS < G3 < VIOLINS"
                      : mode == orch::M_CELLOS ? "RANGE C1 - C5"
                      : "PLAY CHORDS  |  HOLD LONG NOTES";
    g.drawText (range, inner.removeFromBottom (16.0f), juce::Justification::bottomRight);
}
