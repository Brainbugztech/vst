#include "PluginEditor.h"

using namespace bb;
using namespace orch;

namespace
{
const juce::Colour violet { 0xff8b3dff };
const juce::Colour cyan   { 0xff3de8ff };
const juce::Colour modeColour[4] { juce::Colour (0xffb6ff1f), juce::Colour (0xff7dff6a), juce::Colour (0xff3de8ff), juce::Colour (0xffff2f8e) };

void glowRect (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float radius, float strength)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.07f * strength * (float) (4 - i)));
        g.drawRoundedRectangle (r.expanded ((float) i * 1.5f), radius + (float) i, 2.0f);
    }
    g.setColour (c.withAlpha (0.85f * strength));
    g.drawRoundedRectangle (r, radius, 1.4f);
}

void drawModeIcon (juce::Graphics& g, int mode, juce::Rectangle<float> r, juce::Colour c, float phase)
{
    g.setColour (c);
    juce::Path p;
    const float cx = r.getCentreX(), cy = r.getCentreY(), w = r.getWidth(), h = r.getHeight();
    if (mode == 0 || mode == 1)
    {
        const float s = mode == 0 ? 0.8f : 1.0f;
        p.addEllipse (cx - w * 0.22f * s, cy - h * 0.05f, w * 0.44f * s, h * 0.42f * s);
        p.addEllipse (cx - w * 0.17f * s, cy - h * 0.38f * s, w * 0.34f * s, h * 0.36f * s);
        p.startNewSubPath (cx, cy - h * 0.5f); p.lineTo (cx, cy + h * 0.32f);
        p.startNewSubPath (cx - w * 0.5f, cy + h * 0.12f); p.lineTo (cx + w * 0.5f, cy - h * 0.35f);
        if (mode == 1) { p.startNewSubPath (cx, cy + h * 0.32f); p.lineTo (cx, cy + h * 0.5f); }
    }
    else if (mode == 2)
    {
        for (int k = 0; k < 3; ++k)
        {
            const float yy = cy - h * 0.25f + (float) k * h * 0.25f;
            p.startNewSubPath (r.getX(), yy);
            for (int xi = 1; xi <= 20; ++xi)
            {
                const float x = (float) xi / 20.0f;
                p.lineTo (r.getX() + x * w, yy + std::sin (x * 6.28f + (float) k + phase) * h * 0.09f);
            }
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
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}

// =============================================================================================
void EngineTile::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();
    const auto c = modeColour[mode];
    juce::ColourGradient bg (juce::Colour (0xff1c1426).withMultipliedBrightness (on ? 1.4f : 1.0f), r.getX(), r.getY(),
                             juce::Colour (0xff0c0910), r.getRight(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 10.0f);
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.25f), r.getX(), r.getCentreY(), c.withAlpha (0.0f), r.getRight(), r.getCentreY(), false));
        g.fillRoundedRectangle (r, 10.0f);
        glowRect (g, r, c, 10.0f, 0.7f + 0.3f * glow);
        g.setColour (c);
        g.fillRoundedRectangle (r.getX() + 1.0f, r.getY() + 14.0f, 4.0f, r.getHeight() - 28.0f, 2.0f);
    }
    else
    {
        g.setColour (over ? c.withAlpha (0.5f) : col::line);
        g.drawRoundedRectangle (r, 10.0f, 1.2f);
    }
    drawModeIcon (g, mode, r.withWidth (54.0f).withTrimmedLeft (8.0f).reduced (6.0f, r.getHeight() * 0.28f),
                  on ? c : col::inkDim, glow * 6.0f);
    auto t = r.withTrimmedLeft (60.0f).reduced (4.0f, 0.0f);
    g.setFont (fonts::display (17.0f, true));
    g.setColour (on ? juce::Colours::white : col::ink);
    g.drawText (getName(), t.withTrimmedBottom (t.getHeight() * 0.42f), juce::Justification::bottomLeft);
    g.setFont (fonts::mono (10.5f));
    g.setColour (on ? c : col::inkDim);
    g.drawText (subtitle, t.withTrimmedTop (t.getHeight() * 0.6f), juce::Justification::topLeft);
}

// =============================================================================================
juce::Colour ZoneKeyboard::zoneColour (int note) const
{
    const int mode = (int) proc.getParam ("mode");
    if (mode == M_STRINGS || mode == M_CELLOS)
    {
        if (note >= keyswitchLow && note < keyswitchLow + A_NUM)
            return note - keyswitchLow == (int) proc.getParam ("artic") ? col::brain : col::brain.withAlpha (0.45f);
        const int lo = mode == M_STRINGS ? 36 : 36, hi = mode == M_STRINGS ? 98 : 84;
        if (note >= lo && note <= hi) return modeColour[mode].withAlpha (0.28f);
        return {};
    }
    if (mode == M_ATMOS) return note >= 28 && note <= 103 ? modeColour[2].withAlpha (0.22f) : juce::Colour();
    const int kit = (int) proc.getParam ("kit");
    if (kit == K_TIMPANI)
    {
        if (note >= 36 && note <= 59) return juce::Colour (0xffffb02e).withAlpha (0.4f);
        if (note >= 60 && note <= 83) return juce::Colour (0xffff7a1a).withAlpha (0.3f);
        return {};
    }
    if (kit == K_TRAILER) return note >= 36 && note < 36 + numTrailer ? modeColour[3].withAlpha (0.35f) : juce::Colour();
    for (auto& k : kitPieces) if (k.kit == kit && k.note == note) return modeColour[3].withAlpha (0.35f);
    return {};
}

void ZoneKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> a, bool down, bool over, juce::Colour, juce::Colour)
{
    auto r = a.reduced (0.6f, 0.0f);
    juce::ColourGradient grad (juce::Colour (0xffe9e4ef), r.getX(), r.getY(), juce::Colour (0xffb9b0c4), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r.withTrimmedTop (-4.0f), 3.0f);
    const auto z = zoneColour (note);
    if (! z.isTransparent())
    {
        g.setColour (z);
        g.fillRect (r.withTop (r.getBottom() - 9.0f));
        g.setColour (z.withAlpha (z.getFloatAlpha() * 0.35f));
        g.fillRect (r);
    }
    if (down) { g.setColour (col::tox.withAlpha (0.85f)); g.fillRoundedRectangle (r.withTrimmedTop (-4.0f), 3.0f); }
    else if (over) { g.setColour (col::tox.withAlpha (0.2f)); g.fillRect (r); }
    if (note % 12 == 0)
    {
        g.setColour (juce::Colour (0xff3a3046));
        g.setFont (fonts::mono (10.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3), r.withTrimmedBottom (10.0f), juce::Justification::centredBottom);
    }
}

void ZoneKeyboard::drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> a, bool down, bool over, juce::Colour)
{
    auto r = a.reduced (1.0f, 0.0f);
    juce::ColourGradient grad (juce::Colour (0xff2a2232), r.getX(), r.getY(), juce::Colour (0xff0b080e), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r.withTrimmedTop (-4.0f), 2.5f);
    const auto z = zoneColour (note);
    if (! z.isTransparent()) { g.setColour (z.withAlpha (std::min (1.0f, z.getFloatAlpha() * 2.0f))); g.fillRect (r.withTop (r.getBottom() - 5.0f).reduced (2.0f, 0.0f)); }
    if (down) { g.setColour (col::tox); g.fillRoundedRectangle (r.withTrimmedTop (-4.0f), 2.5f); }
    else if (over) { g.setColour (col::tox.withAlpha (0.25f)); g.fillRect (r); }
}

// =============================================================================================
OrchestraEditor::OrchestraEditor (OrchestraProcessor& p)
    : AudioProcessorEditor (p), proc (p), keyboard (p.keyboardState, p)
{
    setLookAndFeel (&look);
    look.setColour (juce::ComboBox::backgroundColourId, col::panel2);
    look.setColour (juce::ComboBox::outlineColourId, col::line);
    look.setColour (juce::ComboBox::textColourId, col::ink);
    look.setColour (juce::ComboBox::arrowColourId, col::tox);
    look.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff120d18));
    look.setColour (juce::PopupMenu::textColourId, col::ink);
    look.setColour (juce::PopupMenu::highlightedBackgroundColourId, col::brain.withAlpha (0.35f));
    look.setColour (juce::PopupMenu::headerTextColourId, col::tox);
    look.setColour (juce::Slider::trackColourId, col::brain);
    look.setColour (juce::Slider::backgroundColourId, col::line);
    look.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    look.setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff120d18));
    look.setColour (juce::AlertWindow::textColourId, col::ink);
    look.setColour (juce::TextEditor::backgroundColourId, col::panel2);
    look.setColour (juce::TextEditor::textColourId, col::ink);

    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    brain = juce::ImageCache::getFromMemory (BBAssets::head_png, BBAssets::head_pngSize);

    for (auto& t : tiles)
    {
        t.setClickingTogglesState (false);
        t.onClick = [this, &t] { proc.setParam ("mode", (float) t.mode); };
        addAndMakeVisible (t);
    }
    for (int i = 0; i < A_NUM; ++i)
    {
        articBtn[i].setButtonText (articNames[i]);
        articBtn[i].getProperties().set ("mono", true);
        articBtn[i].onClick = [this, i] { proc.setParam ("artic", (float) i); };
        addChildComponent (articBtn[i]);
    }
    for (int i = 0; i < K_NUM; ++i)
    {
        kitBtn[i].setButtonText (kitNames[i]);
        kitBtn[i].getProperties().set ("accent", "brain");
        kitBtn[i].onClick = [this, i] { proc.setParam ("kit", (float) i); };
        addChildComponent (kitBtn[i]);
    }
    const char* rhyNames[3] { "OFF", "GATE", "STRIKE" };
    for (int i = 0; i < 3; ++i)
    {
        rhyBtn[i].setButtonText (rhyNames[i]);
        rhyBtn[i].getProperties().set ("mono", true);
        rhyBtn[i].getProperties().set ("accent", "brain");
        rhyBtn[i].onClick = [this, i] { proc.setParam ("rhyMode", (float) i); };
        addChildComponent (rhyBtn[i]);
    }
    legatoBtn.setClickingTogglesState (true);
    legatoBtn.getProperties().set ("mono", true);
    legatoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "legato", legatoBtn);
    addChildComponent (legatoBtn);

    for (auto& s : padSources) { padABox.addItem (s.name, padABox.getNumItems() + 1); padBBox.addItem (s.name, padBBox.getNumItems() + 1); }
    for (auto& pt : patterns) patternBox.addItem (pt.name, patternBox.getNumItems() + 1);
    for (auto* r : rateNames) rateBox.addItem (r, rateBox.getNumItems() + 1);
    padAAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "padA", padABox);
    padBAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "padB", padBBox);
    patternAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "rhyPattern", patternBox);
    rateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "rhyRate", rateBox);
    for (auto* c : { &padABox, &padBBox, &patternBox, &rateBox }) addChildComponent (*c);
    morphAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "morph", morphSlider);
    addChildComponent (morphSlider);

    presetBtn.getProperties().set ("mono", true);
    presetBtn.onClick = [this] { showPresetMenu(); };
    prevBtn.onClick = [this] { const int n = proc.getNumPrograms(); selectFactory ((proc.getCurrentProgram() + n - 1) % n); };
    nextBtn.onClick = [this] { selectFactory ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    saveBtn.onClick = [this] { savePresetDialog(); };
    saveBtn.getProperties().set ("accent", "brain");
    for (auto* b : { &presetBtn, &prevBtn, &nextBtn, &saveBtn }) { b->getProperties().set ("mono", true); addAndMakeVisible (*b); }

    setupKnob (kAttack, "attack", "ATTACK");   setupKnob (kRelease, "release", "RELEASE");
    setupKnob (kTone, "tone", "TONE");         setupKnob (kTune, "tune", "TUNE");
    setupKnob (kDrive, "drive", "DRIVE");      setupKnob (kReverb, "reverb", "REVERB");
    setupKnob (kSpace, "space", "SPACE");      setupKnob (kWidth, "width", "WIDTH");
    setupKnob (kVolume, "volume", "VOLUME");
    setupKnob (kEnsemble, "ensemble", "ENSEMBLE", "brain"); setupKnob (kHumanize, "humanize", "HUMANIZE", "brain");
    setupKnob (kBasses, "basses", "BASSES", "brain");       setupKnob (kGlide, "glide", "GLIDE", "brain");
    setupKnob (kEvolve, "evolve", "EVOLVE", "brain");       setupKnob (kAir, "air", "AIR", "brain");
    setupKnob (kShimmer, "shimmer", "SHIMMER", "brain");    setupKnob (kMotion, "motion", "MOTION", "brain");
    setupKnob (kSub, "sub", "SUB", "brain");                 setupKnob (kStack, "stack", "STACK", "brain");
    setupKnob (kPunch, "punch", "PUNCH", "brain");           setupKnob (kDecay, "decay", "DECAY", "brain");
    setupKnob (kSwing, "swing", "SWING", "brain");           setupKnob (kDepth, "rhyDepth", "DEPTH", "brain");

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (24);
    keyboard.setKeyWidth (22.0f);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff1a1420));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, col::panel);
    keyboard.setMidiChannel (1);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    juce::Random rng (7);
    for (int i = 0; i < 140; ++i)
        particles.push_back ({ rng.nextFloat(), rng.nextFloat(), (rng.nextFloat() - 0.5f) * 0.0015f, (rng.nextFloat() - 0.5f) * 0.001f,
                               1.0f + rng.nextFloat() * 3.0f, rng.nextFloat() });

    setSize (1200, 790);
    updateModeUi();
    startTimerHz (30);
}

OrchestraEditor::~OrchestraEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void OrchestraEditor::setupKnob (Knob& k, const char* id, const char* text, const char* accent)
{
    k.slider.getProperties().set ("accent", accent);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, col::inkDim);
    auto* prm = proc.apvts.getParameter (id);
    k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (fonts::display (12.5f, true));
    k.label.setColour (juce::Label::textColourId, juce::String (accent) == "brain" ? col::brain : col::tox);
    k.label.setInterceptsMouseClicks (false, false);
    addChildComponent (k.slider);
    addChildComponent (k.label);
}

void OrchestraEditor::placeKnob (Knob& k, juce::Rectangle<int> cell, bool small)
{
    k.slider.setVisible (true); k.label.setVisible (true);
    k.label.setFont (fonts::display (small ? 11.5f : 12.5f, true));
    k.label.setBounds (cell.removeFromTop (16));
    k.slider.setBounds (cell);
}

void OrchestraEditor::selectFactory (int idx) { proc.setCurrentProgram (idx); }

void OrchestraEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& fp = factoryPresets();
    juce::StringArray cats;
    for (auto& pr : fp) cats.addIfNotAlreadyThere (pr.category);
    for (auto& c : cats)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) fp.size(); ++i)
            if (c == fp[(size_t) i].category)
                sub.addItem (i + 1, fp[(size_t) i].name, true, proc.currentPresetName == fp[(size_t) i].name);
        menu.addSubMenu (c, sub);
    }
    const auto user = proc.getUserPresets();
    menu.addSeparator();
    juce::PopupMenu um;
    for (int i = 0; i < user.size(); ++i) um.addItem (10000 + i, user[i].getFileNameWithoutExtension());
    if (user.isEmpty()) um.addItem (-1, "(noch keine eigenen Presets)", false);
    menu.addSubMenu ("MY PRESETS", um);
    menu.addItem (20000, "Save current as ...");
    menu.addItem (20001, "Open preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn).withMinimumWidth (presetBtn.getWidth()),
        [this, user] (int r)
        {
            if (r <= 0) return;
            if (r == 20000) { savePresetDialog(); return; }
            if (r == 20001) { OrchestraProcessor::userPresetFolder().createDirectory(); OrchestraProcessor::userPresetFolder().startAsProcess(); return; }
            if (r >= 10000) { if (juce::isPositiveAndBelow (r - 10000, user.size())) proc.loadUserPreset (user[r - 10000]); return; }
            selectFactory (r - 1);
        });
}

void OrchestraEditor::savePresetDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("SAVE PRESET", "Name fuer dein Preset:", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->setLookAndFeel (&look);
    saveWindow->addTextEditor ("name", proc.currentPresetName);
    saveWindow->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && saveWindow != nullptr) proc.saveUserPreset (saveWindow->getTextEditorContents ("name"));
        saveWindow.reset();
    }), false);
}

// ---------------------------------------------------------------------------------------------
void OrchestraEditor::updateModeUi()
{
    shownMode = (int) proc.getParam ("mode");
    shownArtic = (int) proc.getParam ("artic");
    shownKit = (int) proc.getParam ("kit");
    shownRhy = (int) proc.getParam ("rhyMode");
    for (auto& t : tiles) t.setToggleState (t.mode == shownMode, juce::dontSendNotification);
    for (int i = 0; i < A_NUM; ++i) articBtn[i].setToggleState (i == shownArtic, juce::dontSendNotification);
    for (int i = 0; i < K_NUM; ++i) kitBtn[i].setToggleState (i == shownKit, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) rhyBtn[i].setToggleState (i == shownRhy, juce::dontSendNotification);
    const bool drums = shownMode == M_DRUMS;
    for (auto* k : { &kAttack, &kRelease })
    {
        k->slider.setEnabled (! drums);
        k->slider.setAlpha (drums ? 0.3f : 1.0f); k->label.setAlpha (drums ? 0.3f : 1.0f);
    }
    resized();
    repaint();
}

void OrchestraEditor::resized()
{
    // alles Modusabhaengige erst ausblenden
    for (auto& b : articBtn) b.setVisible (false);
    for (auto& b : kitBtn) b.setVisible (false);
    for (auto& b : rhyBtn) b.setVisible (false);
    legatoBtn.setVisible (false);
    for (auto* c : { &padABox, &padBBox, &patternBox, &rateBox }) c->setVisible (false);
    morphSlider.setVisible (false);
    for (auto* k : { &kEnsemble, &kHumanize, &kBasses, &kGlide, &kEvolve, &kAir, &kShimmer, &kMotion,
                     &kSub, &kStack, &kPunch, &kDecay, &kSwing, &kDepth })
    { k->slider.setVisible (false); k->label.setVisible (false); }
    panels.clear();

    // Kopfzeile
    auto presetRow = juce::Rectangle<int> (560, 22, 470, 36);
    prevBtn.setBounds (presetRow.removeFromLeft (36));
    saveBtn.setBounds (presetRow.removeFromRight (70));
    presetRow.removeFromRight (6);
    nextBtn.setBounds (presetRow.removeFromRight (36));
    presetBtn.setBounds (presetRow.reduced (6, 0));

    // Engine-Leiste
    for (int i = 0; i < 4; ++i) tiles[i].setBounds (16, 90 + i * 126, 182, 116);

    const int mx = 212, mw = getWidth() - mx - 16;
    stageArea = { mx, 90, mw, 220 };
    auto ctl = juce::Rectangle<int> (mx, 320, mw, 154);
    auto knobRow = juce::Rectangle<int> (mx, 484, mw, 140);
    keyboard.setBounds (16, 656, getWidth() - 32, 118);

    auto panelIn = [] (juce::Rectangle<int> r) { return r.reduced (10, 0).withTrimmedTop (28).withTrimmedBottom (8); };
    auto knobCells = [this] (std::initializer_list<Knob*> ks, juce::Rectangle<int> area, bool small)
    {
        const int n = (int) ks.size(), w = area.getWidth() / std::max (1, n);
        int i = 0;
        for (auto* k : ks) placeKnob (*k, juce::Rectangle<int> (area.getX() + i++ * w, area.getY(), w, area.getHeight()), small);
    };

    const int mode = shownMode;
    if (mode == M_STRINGS || mode == M_CELLOS || mode == M_ATMOS)
    {
        const int rhyW = 330;
        auto rhy = ctl.removeFromRight (rhyW);
        ctl.removeFromRight (10);
        if (mode == M_ATMOS)
        {
            auto src = ctl.removeFromLeft (ctl.getWidth() * 45 / 100);
            ctl.removeFromLeft (10);
            panels.push_back ({ src, "SOURCES  A / B", modeColour[2] });
            panels.push_back ({ ctl, "TEXTURE", modeColour[2] });
            auto in = panelIn (src);
            auto row = in.removeFromTop (28);
            padABox.setBounds (row.removeFromLeft (row.getWidth() / 2 - 6)); row.removeFromLeft (12); padBBox.setBounds (row);
            in.removeFromTop (18);
            morphLabelArea = in.removeFromTop (18);
            auto mrow = in.removeFromTop (34);
            morphSlider.setBounds (mrow.reduced (4, 2));
            morphSlider.setVisible (true);
            padABox.setVisible (true); padBBox.setVisible (true);
            knobCells ({ &kEvolve, &kAir, &kShimmer, &kMotion }, panelIn (ctl), true);
        }
        else
        {
            auto art = ctl.removeFromLeft (318);
            ctl.removeFromLeft (10);
            panels.push_back ({ art, "ARTICULATION   (KEYSWITCH C0 - F0)", modeColour[mode] });
            panels.push_back ({ ctl, "PLAYING", modeColour[mode] });
            auto in = panelIn (art);
            const int bw = (in.getWidth() - 12) / 3, bh = (in.getHeight() - 8) / 2;
            for (int i = 0; i < A_NUM; ++i)
            {
                articBtn[i].setBounds (in.getX() + (i % 3) * (bw + 6), in.getY() + (i / 3) * (bh + 8), bw, bh);
                articBtn[i].setVisible (true);
            }
            auto pin = panelIn (ctl);
            auto top = pin.removeFromTop (26);
            legatoBtn.setBounds (top.withSizeKeepingCentre (110, 24).withX (top.getX()));
            legatoBtn.setVisible (true);
            knobCells ({ &kGlide, &kBasses, &kEnsemble, &kHumanize }, pin.withTrimmedTop (2), true);
        }
        // Rhythm
        panels.push_back ({ rhy, "RHYTHM ENGINE   (SYNC TO HOST)", col::brain });
        auto in = panelIn (rhy);
        auto row1 = in.removeFromTop (24);
        const int rb = (row1.getWidth() - 12) / 3;
        for (int i = 0; i < 3; ++i) { rhyBtn[i].setBounds (row1.getX() + i * (rb + 6), row1.getY(), rb, 24); rhyBtn[i].setVisible (true); }
        in.removeFromTop (6);
        auto row2 = in.removeFromTop (24);
        rateBox.setBounds (row2.removeFromRight (84)); row2.removeFromRight (6); patternBox.setBounds (row2);
        patternBox.setVisible (true); rateBox.setVisible (true);
        in.removeFromTop (4);
        auto knobsR = in.removeFromRight (150);
        stepArea = in.reduced (2, 6);
        knobCells ({ &kSwing, &kDepth }, knobsR, true);
    }
    else
    {
        auto kitP = ctl.removeFromLeft (430);
        ctl.removeFromLeft (10);
        panels.push_back ({ kitP, "DRUM KIT", modeColour[3] });
        panels.push_back ({ ctl, "IMPACT", modeColour[3] });
        auto in = panelIn (kitP);
        const int bh = (in.getHeight() - 8) / 2;
        for (int i = 0; i < K_NUM; ++i)
        {
            const int cols = i < 3 ? 3 : 2, row = i < 3 ? 0 : 1, idx = i < 3 ? i : i - 3;
            const int bw = (in.getWidth() - (cols - 1) * 8) / cols;
            kitBtn[i].setBounds (in.getX() + idx * (bw + 8), in.getY() + row * (bh + 8), bw, bh);
            kitBtn[i].setVisible (true);
        }
        knobCells ({ &kSub, &kStack, &kPunch, &kDecay }, panelIn (ctl), false);
        stepArea = {};
    }

    // globale Regler
    auto shape = knobRow.removeFromLeft (knobRow.getWidth() * 4 / 9);
    knobRow.removeFromLeft (10);
    panels.push_back ({ shape, "SHAPE", col::tox });
    panels.push_back ({ knobRow, "FX & OUTPUT", col::tox });
    knobCells ({ &kAttack, &kRelease, &kTone, &kTune }, panelIn (shape), false);
    knobCells ({ &kDrive, &kReverb, &kSpace, &kWidth, &kVolume }, panelIn (knobRow), false);
    // Tastenbreite so, dass C0 .. C8 genau passt (50 weisse Tasten)
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);
}

// ---------------------------------------------------------------------------------------------
void OrchestraEditor::timerCallback()
{
    if ((int) proc.getParam ("mode") != shownMode || (int) proc.getParam ("artic") != shownArtic
        || (int) proc.getParam ("kit") != shownKit || (int) proc.getParam ("rhyMode") != shownRhy)
        updateModeUi();

    if (proc.presetChanged.exchange (0) != 0) repaint();
    presetBtn.setButtonText (proc.currentPresetName);

    phase += 1.0f / 30.0f;
    pulse = 0.5f + 0.5f * std::sin (phase * 2.2f);
    levelSm = std::max (proc.engine.level.load(), levelSm * 0.9f);
    for (auto& t : tiles) { t.glow = pulse; if (t.getToggleState()) t.repaint(); }

    // Drum-Treffer -> Pad-Glow + Ripple
    const auto hits = proc.engine.lastHitCount.load();
    if (hits != lastHitSeen)
    {
        lastHitSeen = hits;
        const int note = proc.engine.lastHitNote.load();
        const auto pads = currentKitPads();
        for (int i = 0; i < (int) pads.size(); ++i)
            if ((pads[(size_t) i].first == note || (shownKit == K_TIMPANI && (note >= 60) == (i == 1))) && i < (int) padGlow.size())
            {
                padGlow[(size_t) i] = 1.0f;
                ripples.push_back ({ padRect (i, (int) pads.size(), stageArea.toFloat().reduced (14.0f, 30.0f)).getCentre(), 0.0f });
            }
    }
    for (auto& gl : padGlow) gl *= 0.9f;
    for (auto& r : ripples) r.age += 1.0f / 30.0f;
    ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age > 0.9f; }), ripples.end());

    for (int n = 0; n < 128; ++n)
    {
        const bool on = proc.keyboardState.isNoteOn (1, n);
        stringEnergy[(size_t) n] = on ? std::min (1.0f, stringEnergy[(size_t) n] + 0.25f) : stringEnergy[(size_t) n] * 0.9f;
    }
    for (auto& p : particles)
    {
        const float sp = 1.0f + 6.0f * levelSm;
        p.x += p.vx * sp; p.y += p.vy * sp + std::sin (phase * 0.7f + p.hue * 6.0f) * 0.0006f;
        if (p.x < 0) p.x += 1;
        if (p.x > 1) p.x -= 1;
        if (p.y < 0) p.y += 1;
        if (p.y > 1) p.y -= 1;
    }

    repaint (stageArea.expanded (6));
    repaint (stepArea);
    repaint (morphLabelArea);
    repaint (0, 0, getWidth(), 86);
    if (shownMode != M_DRUMS) keyboard.repaint();
}

void OrchestraEditor::mouseDown (const juce::MouseEvent& e)
{
    if (shownMode != M_DRUMS || ! stageArea.contains (e.getPosition())) return;
    const auto pads = currentKitPads();
    const auto area = stageArea.toFloat().reduced (14.0f, 30.0f);
    for (int i = 0; i < (int) pads.size(); ++i)
        if (padRect (i, (int) pads.size(), area).contains (e.position)) { proc.padNote = pads[(size_t) i].first; return; }
}

// ---------------------------------------------------------------------------------------------
std::vector<std::pair<int, juce::String>> OrchestraEditor::currentKitPads() const
{
    std::vector<std::pair<int, juce::String>> v;
    if (shownKit == K_TRAILER) for (auto& p : trailerPieces) v.push_back ({ p.note, p.name });
    else if (shownKit == K_TIMPANI) { v.push_back ({ 36, "HITS  C1 - B2" }); v.push_back ({ 60, "ROLLS  C3 - B4" }); }
    else for (auto& k : kitPieces) if (k.kit == shownKit) v.push_back ({ k.note, k.name });
    return v;
}

juce::Rectangle<float> OrchestraEditor::padRect (int i, int count, juce::Rectangle<float> a) const
{
    if (count <= 2) return a.withWidth (a.getWidth() / 2.0f).translated ((float) i * a.getWidth() / 2.0f, 0).reduced (6.0f);
    const int cols = count > 24 ? 9 : 8, rows = (count + cols - 1) / cols;
    const float w = a.getWidth() / (float) cols, h = a.getHeight() / (float) rows;
    return { a.getX() + (float) (i % cols) * w + 2.5f, a.getY() + (float) (i / cols) * h + 2.5f, w - 5.0f, h - 5.0f };
}

void OrchestraEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (col::voidBg);

    // Hintergrund: Nebel, Raster, Scanlines
    g.setGradientFill (juce::ColourGradient (violet.withAlpha (0.20f), b.getWidth() * 0.1f, b.getHeight() * 0.4f,
                                             juce::Colours::transparentBlack, b.getWidth() * 0.55f, b.getHeight(), true));
    g.fillRect (b);
    g.setGradientFill (juce::ColourGradient (col::brain.withAlpha (0.12f), b.getWidth() * 0.95f, 0.0f,
                                             juce::Colours::transparentBlack, b.getWidth() * 0.5f, b.getHeight() * 0.5f, true));
    g.fillRect (b);
    g.setColour (col::line.withAlpha (0.2f));
    for (float x = 0; x < b.getWidth(); x += 24) g.drawVerticalLine ((int) x, 0, b.getHeight());
    for (float y = 0; y < b.getHeight(); y += 24) g.drawHorizontalLine ((int) y, 0, b.getWidth());

    auto frame = b.reduced (5.0f);
    g.setColour (col::line); g.drawRoundedRectangle (frame, 12.0f, 1.5f);
    const float cl = 40.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float x = (i & 1) ? frame.getRight() : frame.getX(), y = (i & 2) ? frame.getBottom() : frame.getY();
        const float sx = (i & 1) ? -1.0f : 1.0f, sy = (i & 2) ? -1.0f : 1.0f;
        juce::Path p; p.startNewSubPath (x + sx * cl, y); p.lineTo (x, y); p.lineTo (x, y + sy * cl);
        g.setColour ((i & 1) ? col::brain : col::tox);
        g.strokePath (p, juce::PathStrokeType (2.5f));
    }

    // Panels
    for (auto& p : panels)
    {
        auto r = p.r.toFloat();
        g.setColour (juce::Colour (0xe6100c15));
        g.fillRoundedRectangle (r, 9.0f);
        g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (0.10f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 40.0f, false));
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (p.accent.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 9.0f, 1.0f);
        g.setColour (p.accent);
        g.fillRoundedRectangle (r.getX() + 12.0f, r.getY() + 9.0f, 3.0f, 12.0f, 1.5f);
        g.setFont (fonts::display (12.5f, true));
        g.drawText (p.title, (int) r.getX() + 21, (int) r.getY() + 5, (int) r.getWidth() - 30, 20, juce::Justification::centredLeft);
    }

    // Tastatur-Rahmen
    auto kb = keyboard.getBounds().toFloat().expanded (4.0f, 22.0f).withTrimmedBottom (18.0f);
    g.setColour (juce::Colour (0xe6100c15)); g.fillRoundedRectangle (kb, 9.0f);
    g.setColour (col::line); g.drawRoundedRectangle (kb, 9.0f, 1.0f);
    g.setFont (fonts::mono (11.0f));
    g.setColour (col::inkDim);
    juce::String hint = shownMode == M_DRUMS ? "KEYS: DRUM MAP  |  CLICK THE PADS ABOVE TO AUDITION"
                      : shownMode == M_ATMOS ? "KEYS: PLAY CHORDS  |  MOD WHEEL = DYNAMICS"
                      : "PINK = ARTICULATION KEYSWITCHES  |  MOD WHEEL / CC11 = DYNAMICS  |  SUSTAIN PEDAL";
    g.drawText (hint, kb.withHeight (20.0f).reduced (12.0f, 0.0f), juce::Justification::centredLeft);
    g.drawText ("VSCO 2 CE (CC0)  |  BRAIN BUGZ", kb.withHeight (20.0f).reduced (12.0f, 0.0f), juce::Justification::centredRight);

    // Kopfzeile
    const auto logoR = juce::Rectangle<float> (18.0f, 6.0f, 170.0f, 76.0f);
    const float glow = juce::jlimit (0.0f, 1.0f, 0.25f * pulse + 1.5f * levelSm);
    g.setGradientFill (juce::ColourGradient (col::tox.withAlpha (0.22f * glow), logoR.getCentreX(), logoR.getCentreY(),
                                             juce::Colours::transparentBlack, logoR.getRight() + 10.0f, logoR.getCentreY(), true));
    g.fillEllipse (logoR.expanded (14.0f, 4.0f));
    g.setOpacity (1.0f);
    if (logo.isValid()) g.drawImage (logo, logoR, juce::RectanglePlacement::centred);
    if (brain.isValid())
    {
        const float bs = 1.0f + 0.07f * glow;
        auto br = juce::Rectangle<float> (198.0f, 20.0f, 60.0f, 30.0f).withSizeKeepingCentre (60.0f * bs, 30.0f * bs);
        g.setColour (col::brain.withAlpha (0.3f * glow)); g.fillEllipse (br.expanded (8.0f));
        g.setOpacity (1.0f);
        g.drawImage (brain, br, juce::RectanglePlacement::centred);
    }
    g.setFont (fonts::display (30.0f, true));
    g.setColour (col::brain.withAlpha (0.35f)); g.drawText ("ORCHESTRA", 268, 13, 300, 34, juce::Justification::centredLeft);
    g.setColour (col::brain); g.drawText ("ORCHESTRA", 266, 11, 300, 34, juce::Justification::centredLeft);
    g.setFont (fonts::mono (11.5f));
    g.setColour (col::inkDim);
    g.drawText ("CINEMATIC SAMPLE ENGINE  v2", 268, 46, 300, 16, juce::Justification::centredLeft);
    g.setFont (fonts::mono (10.5f));
    g.setColour (col::tox.withAlpha (0.7f));
    g.drawText ("PRESET", 562, 4, 100, 16, juce::Justification::centredLeft);

    // Pegelanzeige
    auto meter = juce::Rectangle<float> ((float) getWidth() - 150.0f, 28.0f, 120.0f, 10.0f);
    g.setColour (col::line); g.fillRoundedRectangle (meter, 3.0f);
    const float lv = juce::jlimit (0.0f, 1.0f, levelSm);
    g.setGradientFill (juce::ColourGradient (col::tox, meter.getX(), 0, col::brain, meter.getRight(), 0, false));
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * lv), 3.0f);
    g.setFont (fonts::mono (10.5f));
    g.setColour (col::inkDim);
    g.drawText ("OUTPUT", meter.translated (0, 12).withHeight (14), juce::Justification::centredLeft);
    g.drawText (proc.loading.load() ? "LOADING" : "READY", meter.translated (0, 12).withHeight (14), juce::Justification::centredRight);

    drawStage (g, stageArea.toFloat());
    if (! stepArea.isEmpty()) drawSteps (g, stepArea.toFloat());

    if (shownMode == M_ATMOS && ! morphLabelArea.isEmpty())
    {
        const auto ca = juce::Colour (padSources[(int) proc.getParam ("padA")].colour);
        const auto cb = juce::Colour (padSources[(int) proc.getParam ("padB")].colour);
        g.setFont (fonts::display (11.5f, true));
        g.setColour (ca); g.drawText ("A", morphLabelArea, juce::Justification::centredLeft);
        g.setColour (cb); g.drawText ("B", morphLabelArea, juce::Justification::centredRight);
        g.setColour (col::inkDim); g.drawText ("MORPH", morphLabelArea, juce::Justification::centred);
    }
}

void OrchestraEditor::paintOverChildren (juce::Graphics& g)
{
    if (proc.loading.load())
    {
        auto r = stageArea.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (col::tox.withAlpha (0.6f + 0.4f * pulse));
        g.setFont (fonts::display (24.0f, true));
        g.drawText ("LOADING SAMPLES ...", r, juce::Justification::centred);
        auto bar = r.withSizeKeepingCentre (280.0f, 4.0f).translated (0, 28);
        g.setColour (col::line); g.fillRoundedRectangle (bar, 2.0f);
        const float x = std::fmod (phase * 0.8f, 1.0f);
        g.setColour (col::brain);
        g.fillRoundedRectangle (bar.withWidth (70.0f).withX (bar.getX() + x * (bar.getWidth() - 70.0f)), 2.0f);
    }
}

// ---------------------------------------------------------------------------------------------
void OrchestraEditor::drawStage (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto mc = modeColour[juce::jlimit (0, 3, shownMode)];
    g.setColour (juce::Colour (0xff09070c));
    g.fillRoundedRectangle (r, 12.0f);

    // Synthwave-Boden
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (r, 12.0f); g.reduceClipRegion (clip);
        const float horizon = r.getY() + r.getHeight() * 0.58f;
        g.setGradientFill (juce::ColourGradient (mc.withAlpha (0.16f), r.getCentreX(), horizon, juce::Colours::transparentBlack, r.getCentreX(), r.getY(), false));
        g.fillRect (r.withBottom (horizon));
        g.setColour (mc.withAlpha (0.12f));
        for (int i = -12; i <= 12; ++i)
            g.drawLine (r.getCentreX() + (float) i * 18.0f, horizon, r.getCentreX() + (float) i * 140.0f, r.getBottom(), 1.0f);
        const float sp = std::fmod (phase * 0.5f, 1.0f);
        for (int i = 0; i < 7; ++i)
        {
            const float t = ((float) i + sp) / 7.0f;
            const float y = horizon + (r.getBottom() - horizon) * t * t;
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }
        g.setColour (mc.withAlpha (0.4f));
        g.drawHorizontalLine ((int) horizon, r.getX(), r.getRight());

        if (shownMode == M_DRUMS) drawDrums (g, r);
        else if (shownMode == M_ATMOS) drawPads (g, r);
        else drawStrings (g, r);

        // Scanlines
        g.setColour (juce::Colours::black.withAlpha (0.13f));
        for (float y = r.getY(); y < r.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }
    glowRect (g, r, mc, 12.0f, 0.55f + 0.25f * levelSm);
}

void OrchestraEditor::drawStrings (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float lv = juce::jlimit (0.0f, 1.0f, levelSm * 1.8f);
    const int lo = 28, hi = 100;
    // Saiten: eine Linie pro Taste, gespielte Noten schwingen und leuchten
    for (int n = lo; n <= hi; ++n)
    {
        const float x = r.getX() + 20.0f + (r.getWidth() - 40.0f) * (float) (n - lo) / (float) (hi - lo);
        const float e = stringEnergy[(size_t) n];
        const auto c = col::tox.interpolatedWith (col::brain, (float) (n - lo) / (float) (hi - lo));
        if (e < 0.02f)
        {
            g.setColour (c.withAlpha (0.07f));
            g.drawVerticalLine ((int) x, r.getY() + 34.0f, r.getBottom() - 14.0f);
            continue;
        }
        juce::Path p;
        const float amp = (3.0f + 12.0f * lv) * e;
        const float trem = shownArtic == A_TREM ? 3.0f : 1.0f;
        for (int k = 0; k <= 40; ++k)
        {
            const float t = (float) k / 40.0f;
            const float y = r.getY() + 34.0f + t * (r.getHeight() - 48.0f);
            const float dx = std::sin (t * 3.14159f) * std::sin (phase * 18.0f * trem + (float) n) * amp;
            if (k == 0) p.startNewSubPath (x + dx, y); else p.lineTo (x + dx, y);
        }
        g.setColour (c.withAlpha (0.18f * e)); g.strokePath (p, juce::PathStrokeType (9.0f));
        g.setColour (c.withAlpha (0.9f * e)); g.strokePath (p, juce::PathStrokeType (2.0f));
    }
    // Info
    g.setFont (fonts::display (16.0f, true));
    g.setColour (juce::Colours::white);
    const juce::String name = shownMode == M_STRINGS ? "STRINGS" : "CELLOS";
    g.drawText (name + "  /  " + articNames[juce::jlimit (0, A_NUM - 1, shownArtic)], r.reduced (18.0f, 10.0f).withHeight (22.0f), juce::Justification::centredLeft);
    g.setFont (fonts::mono (11.0f));
    g.setColour (col::inkDim);
    juce::String info = shownMode == M_STRINGS ? "CELLOS < C3 < VIOLAS < G3 < VIOLINS" : "CELLO SECTION";
    if (proc.getParam ("basses") > 0.01f) info << "  +  CONTRABASS";
    if (proc.getParam ("legato") > 0.5f) info << "  +  LEGATO";
    g.drawText (info, r.reduced (18.0f, 10.0f).withHeight (22.0f).translated (0, 18.0f), juce::Justification::centredLeft);
    if (shownRhy > 0)
    {
        g.setColour (col::brain);
        g.setFont (fonts::display (13.0f, true));
        g.drawText ((shownRhy == R_STRIKE ? "STRIKE: " : "GATE: ") + juce::String (patterns[(int) proc.getParam ("rhyPattern")].name).toUpperCase(),
                    r.reduced (18.0f, 10.0f).withHeight (22.0f), juce::Justification::centredRight);
    }
}

void OrchestraEditor::drawPads (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int a = (int) proc.getParam ("padA"), b = (int) proc.getParam ("padB");
    const auto ca = juce::Colour (padSources[a].colour), cb = juce::Colour (padSources[b].colour);
    const float m = proc.engine.morphPos.load();
    const float lv = juce::jlimit (0.0f, 1.0f, levelSm * 2.0f);

    // Nebel aus zwei Farben, Gewicht nach Morph
    g.setGradientFill (juce::ColourGradient (ca.withAlpha (0.28f * (1.0f - m) * (0.4f + lv)), r.getX() + r.getWidth() * 0.3f, r.getCentreY(),
                                             juce::Colours::transparentBlack, r.getX() + r.getWidth() * 0.65f, r.getY(), true));
    g.fillRect (r);
    g.setGradientFill (juce::ColourGradient (cb.withAlpha (0.28f * m * (0.4f + lv)), r.getX() + r.getWidth() * 0.7f, r.getCentreY(),
                                             juce::Colours::transparentBlack, r.getX() + r.getWidth() * 0.35f, r.getBottom(), true));
    g.fillRect (r);
    for (auto& p : particles)
    {
        const auto c = (p.hue < m ? cb : ca).withAlpha (0.25f + 0.6f * lv * p.hue);
        const float s = p.size * (1.0f + lv);
        g.setColour (c.withAlpha (c.getFloatAlpha() * 0.3f));
        g.fillEllipse (r.getX() + p.x * r.getWidth() - s * 2.0f, r.getY() + p.y * r.getHeight() - s * 2.0f, s * 4.0f, s * 4.0f);
        g.setColour (c);
        g.fillEllipse (r.getX() + p.x * r.getWidth() - s * 0.5f, r.getY() + p.y * r.getHeight() - s * 0.5f, s, s);
    }
    // gespielte Noten als Lichtsaeulen
    for (int n = 24; n < 109; ++n)
    {
        const float e = stringEnergy[(size_t) n];
        if (e < 0.02f) continue;
        const float x = r.getX() + 20.0f + (r.getWidth() - 40.0f) * (float) (n - 24) / 84.0f;
        g.setGradientFill (juce::ColourGradient (ca.interpolatedWith (cb, m).withAlpha (0.5f * e), x, r.getBottom(),
                                                 juce::Colours::transparentBlack, x, r.getY() + 20.0f, false));
        g.fillRect (x - 6.0f, r.getY() + 20.0f, 12.0f, r.getHeight() - 20.0f);
    }
    g.setFont (fonts::display (16.0f, true));
    g.setColour (ca); g.drawText (padSources[a].name, r.reduced (18.0f, 10.0f).withHeight (22.0f), juce::Justification::centredLeft);
    g.setColour (cb); g.drawText (padSources[b].name, r.reduced (18.0f, 10.0f).withHeight (22.0f), juce::Justification::centredRight);
    // Morph-Leiste
    auto bar = r.reduced (18.0f, 14.0f).withTop (r.getBottom() - 26.0f).withHeight (6.0f);
    g.setGradientFill (juce::ColourGradient (ca, bar.getX(), 0, cb, bar.getRight(), 0, false));
    g.fillRoundedRectangle (bar, 3.0f);
    const float mx = bar.getX() + m * bar.getWidth();
    g.setColour (juce::Colours::white);
    g.fillEllipse (mx - 7.0f, bar.getCentreY() - 7.0f, 14.0f, 14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.fillEllipse (mx - 12.0f, bar.getCentreY() - 12.0f, 24.0f, 24.0f);
}

void OrchestraEditor::drawDrums (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto pads = currentKitPads();
    const auto area = r.reduced (14.0f, 30.0f);
    g.setFont (fonts::display (15.0f, true));
    g.setColour (juce::Colours::white);
    g.drawText (juce::String ("KIT: ") + kitNames[juce::jlimit (0, K_NUM - 1, shownKit)], r.reduced (18.0f, 7.0f).withHeight (20.0f), juce::Justification::centredLeft);
    g.setFont (fonts::mono (11.0f));
    g.setColour (col::inkDim);
    g.drawText (juce::String ((int) pads.size() <= 2 ? 48 : (int) pads.size()) + " SOUNDS  |  CLICK TO PLAY", r.reduced (18.0f, 7.0f).withHeight (20.0f), juce::Justification::centredRight);

    for (int i = 0; i < (int) pads.size(); ++i)
    {
        auto pr = padRect (i, (int) pads.size(), area);
        const float gl = i < (int) padGlow.size() ? padGlow[(size_t) i] : 0.0f;
        g.setColour (juce::Colour (0xff160f1c).interpolatedWith (col::brain, gl * 0.55f));
        g.fillRoundedRectangle (pr, 6.0f);
        g.setColour (gl > 0.05f ? col::brain : col::line.brighter (0.2f));
        g.drawRoundedRectangle (pr, 6.0f, gl > 0.05f ? 1.6f : 1.0f);
        g.setFont (fonts::mono (10.0f));
        g.setColour (col::tox.withAlpha (0.85f));
        g.drawText (juce::MidiMessage::getMidiNoteName (pads[(size_t) i].first, true, true, 3), pr.reduced (6.0f, 3.0f), juce::Justification::topLeft);
        g.setFont (fonts::display ((int) pads.size() <= 2 ? 22.0f : 11.5f, true));
        g.setColour (col::ink);
        g.drawFittedText (pads[(size_t) i].second.toUpperCase(), pr.reduced (6.0f, 3.0f).toNearestInt(),
                          (int) pads.size() <= 2 ? juce::Justification::centred : juce::Justification::bottomLeft, 2, 0.8f);
    }
    for (auto& rp : ripples)
    {
        const float rad = 20.0f + rp.age * 160.0f;
        g.setColour (col::brain.withAlpha (0.6f * (1.0f - rp.age / 0.9f)));
        g.drawEllipse (rp.p.x - rad, rp.p.y - rad * 0.5f, rad * 2.0f, rad, 2.0f);
    }
}

void OrchestraEditor::drawSteps (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& pt = patterns[juce::jlimit (0, numPatterns - 1, (int) proc.getParam ("rhyPattern"))];
    const int cur = proc.engine.currentStep.load();
    const bool on = shownRhy != R_OFF;
    const float w = r.getWidth() / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        auto cell = juce::Rectangle<float> (r.getX() + (float) i * w + 1.5f, r.getY(), w - 3.0f, r.getHeight());
        const float hgt = pt.s[i] == 0 ? 0.08f : (float) pt.s[i] / 3.0f;
        auto bar = cell.withTop (cell.getBottom() - cell.getHeight() * hgt);
        g.setColour (col::line.withAlpha (0.6f));
        g.fillRoundedRectangle (cell, 2.0f);
        juce::Colour c = (i % 4 == 0) ? col::tox : col::tox.withAlpha (0.7f);
        if (! on) c = c.withAlpha (0.25f);
        if (on && i == cur) c = col::brain;
        g.setColour (c);
        g.fillRoundedRectangle (bar, 2.0f);
        if (on && i == cur) { g.setColour (col::brain.withAlpha (0.3f)); g.fillRoundedRectangle (cell.expanded (2.0f), 3.0f); }
    }
}
