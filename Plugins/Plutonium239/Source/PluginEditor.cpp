#include "PluginEditor.h"

using namespace pluto;
using juce::Rectangle;

// ---------------- Wellenform-Vorschau ----------------
void WavePreview::paint (juce::Graphics& g)
{
    using namespace juce;
    auto b = getLocalBounds().toFloat();
    g.setColour (col::cyanBox);
    g.fillRect (b);
    const int w = (int) wave.value;
    const float pw = pwm.value;
    const int pm = (int) pdmode.value;
    const double pd = pdamt.value;
    Path p;
    const int n = 96;
    for (int i = 0; i <= n; ++i)
    {
        const float x0 = (float) i / n;
        float v;
        if (pluto::pdIsReso (pm)) v = pluto::pdReso (pm, pd, x0);
        else
        {
            const float x = (float) pluto::pdWarp (pm, pm == pluto::pdOff ? 0.0 : pd, x0);
            switch (w)
            {
                case 0: v = std::sin (MathConstants<float>::twoPi * x); break;
                case 1: { const float sk = 0.5f - pw * 0.49f; const float t = x < sk ? x / sk : 1.0f - (x - sk) / (1.0f - sk); v = 2 * t - 1; break; }
                case 2: v = 2 * x - 1; break;
                case 3: v = x < 0.5f - pw * 0.45f ? 1.0f : -1.0f; break;
                default: v = reader ? reader ((int) wt.value, wtpos.value, x) : 0.0f; break;
            }
        }
        const float px = 2 + x0 * (b.getWidth() - 4), py = b.getCentreY() - v * (b.getHeight() * 0.5f - 3);
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (col::teal.withAlpha (0.3f));
    g.drawHorizontalLine ((int) b.getCentreY(), 0, b.getWidth());
    g.setColour (col::textDark);
    g.strokePath (p, PathStrokeType (1.2f));
    g.setColour (col::border);
    g.drawRect (b, 1.0f);
}

// ---------------- Content ----------------
void PlutoEditor::Content::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (col::outer);

    // Kopfzeile
    auto top = Rectangle<int> (0, 0, getWidth(), 34);
    g.setGradientFill (ColourGradient (Colour (0xff3d3d3d), 0, 0, Colour (0xff262626), 0, 34, false));
    g.fillRect (top);
    g.setColour (col::border);
    g.drawHorizontalLine (34, 0, (float) getWidth());

    for (auto& b : boxes)
        if (b.section) drawSection (g, b.r, b.t); else drawPanel (g, b.r, b.t);

    g.setFont (PlutoLook::font (11.0f));
    g.setColour (col::text);
    for (auto& t : texts) g.drawText (t.t, t.r, t.j);

    for (int i = 0; i < (int) nameBoxes.size(); ++i)
    {
        g.setColour (col::valueBox);
        g.fillRect (nameBoxes[(size_t) i]);
        g.setColour (col::border);
        g.drawRect (nameBoxes[(size_t) i]);
        g.setColour (col::textDark);
        g.drawText (nameTexts[i], nameBoxes[(size_t) i], Justification::centred);
    }

    if (! dropHighlight.isEmpty())
    {
        g.setColour (col::cyan.withAlpha (0.18f));
        g.fillRect (dropHighlight);
        g.setColour (col::cyan);
        g.drawRect (dropHighlight, 2);
    }

    // Titel
    g.setColour (col::cyan);
    g.setFont (PlutoLook::font (19.0f, true));
    g.drawText ("PLUTONIUM239", 12, 5, 190, 24, Justification::centredLeft);
    g.setColour (col::text.withAlpha (0.45f));
    g.setFont (PlutoLook::font (10.0f));
    g.drawText ("Brain Bugz", getWidth() - 110, 5, 98, 24, Justification::centredRight);
}

// ---------------- Bausteine ----------------
void PlutoEditor::text (Rectangle<int> r, const juce::String& t, juce::Justification j)
{
    content.texts.push_back ({ r, t, j });
}

juce::Slider* PlutoEditor::knob (const juce::String& id, Rectangle<int> r, const juce::String& label, bool textBox)
{
    if (label.isNotEmpty()) { text (r.removeFromTop (11), label); }
    auto* s = own (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, textBox ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox));
    if (textBox) s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 12);
    else s->setPopupDisplayEnabled (true, true, nullptr);
    s->setBounds (r);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
    if (auto* p = proc.apvts.getParameter (id)) s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    s->setTooltip (proc.apvts.getParameter (id)->getName (64));
    return s;
}

juce::Slider* PlutoEditor::vSlider (const juce::String& id, Rectangle<int> r, bool textBox)
{
    auto* s = own (new juce::Slider (juce::Slider::LinearVertical, textBox ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox));
    if (textBox) s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 12);
    else s->setPopupDisplayEnabled (true, true, nullptr);
    s->setBounds (r);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
    if (auto* p = proc.apvts.getParameter (id)) s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    s->setTooltip (proc.apvts.getParameter (id)->getName (64));
    return s;
}

juce::Slider* PlutoEditor::hSlider (const juce::String& id, Rectangle<int> r, int textBoxW)
{
    auto* s = own (new juce::Slider (juce::Slider::LinearHorizontal, textBoxW > 0 ? juce::Slider::TextBoxRight : juce::Slider::NoTextBox));
    if (textBoxW > 0) s->setTextBoxStyle (juce::Slider::TextBoxRight, false, textBoxW, r.getHeight());
    else s->setPopupDisplayEnabled (true, true, nullptr);
    s->setBounds (r);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
    if (auto* p = proc.apvts.getParameter (id)) s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    s->setTooltip (proc.apvts.getParameter (id)->getName (64));
    return s;
}

juce::ComboBox* PlutoEditor::combo (const juce::String& id, Rectangle<int> r)
{
    auto* c = own (new juce::ComboBox());
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->addItemList (p->choices, 1);
    c->setBounds (r);
    cAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c));
    return c;
}

juce::ToggleButton* PlutoEditor::toggle (const juce::String& id, Rectangle<int> r, const juce::String& t)
{
    auto* b = own (new juce::ToggleButton (t));
    b->setBounds (r);
    bAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *b));
    return b;
}

// ---------------- Aufbau ----------------
PlutoEditor::PlutoEditor (PlutoProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    content.setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, baseW, baseH);

    buildTopBar();

    // Linke Spalte: Modulatorsektion I, II, Carriersektion, LFO-Sektion
    content.boxes.push_back ({ { 8, 40, 840, 286 }, "Modulatorsektion I", true });
    content.boxes.push_back ({ { 8, 332, 840, 284 }, "Modulatorsektion 2", true });
    content.boxes.push_back ({ { 8, 622, 840, 264 }, "Carriersektion", true });
    content.boxes.push_back ({ { 8, 892, 840, 134 }, "LFO-Sektion", true });
    for (int i = 0; i < 3; ++i)
    {
        const int x = 14 + i * 278;
        buildOsc (i, x, 60);
        buildOsc (3 + i, x, 352);
        buildOsc (6 + i, x, 642);
    }
    for (int l = 0; l < numLfo; ++l) buildLfo (l, 14 + l * 208, 912);

    // Rechte Spalte
    content.boxes.push_back ({ { 856, 40, 316, 484 }, "Filtersektion", true });
    for (int f = 0; f < numFilt; ++f) buildFilter (f, 862, 60 + f * 154);
    buildMixer (856, 530);
    buildSync (856, 640);
    buildMaster (856, 855);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.6), (int) (baseH * 1.6));
    setSize ((int) (baseW * 0.85), (int) (baseH * 0.85));
    startTimerHz (30);
}

PlutoEditor::~PlutoEditor()
{
    stopTimer();
    sAtt.clear(); cAtt.clear(); bAtt.clear();
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void PlutoEditor::buildTopBar()
{
    presetBox = own (new juce::ComboBox());
    presetBox->setBounds (210, 7, 190, 20);
    for (int i = 0; i < (int) factoryPresets().size(); ++i) presetBox->addItem (factoryPresets()[(size_t) i].name, i + 1);
    presetBox->setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox->onChange = [this]
    {
        const int idx = presetBox->getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
        else if (idx >= 0) proc.setCurrentProgram (idx);
    };
    auto* prev = own (new juce::TextButton ("<"));
    auto* next = own (new juce::TextButton (">"));
    prev->setBounds (404, 7, 20, 20);
    next->setBounds (426, 7, 20, 20);
    auto stepPreset = [this] (int d)
    {
        const int n = (int) factoryPresets().size();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
    };
    prev->onClick = [stepPreset] { stepPreset (-1); };
    next->onClick = [stepPreset] { stepPreset (1); };

    text ({ 462, 7, 44, 20 }, "Voices", juce::Justification::centredRight);
    auto* v = own (new juce::Slider (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft));
    v->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 28, 20);
    v->setBounds (510, 7, 64, 20);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "m_voices", *v));

    text ({ 584, 7, 40, 20 }, "Glide", juce::Justification::centredRight);
    hSlider ("m_glide", { 628, 7, 150, 20 }, 48);

    text ({ 786, 7, 36, 20 }, "Bend", juce::Justification::centredRight);
    auto* bnd = own (new juce::Slider (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft));
    bnd->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 28, 20);
    bnd->setBounds (826, 7, 64, 20);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "m_bend", *bnd));

    text ({ 900, 7, 30, 20 }, "Vel", juce::Justification::centredRight);
    hSlider ("m_vel", { 934, 7, 120, 20 }, 40);
}

void PlutoEditor::buildOsc (int o, int x, int y)
{
    const bool carrier = isCarrier (o);
    const auto name = oscNames()[o];
    const juce::String title = carrier ? "Carrier " + juce::String (o - 5) : "Modulator " + name.substring (1);
    content.boxes.push_back ({ { x, y, 272, 186 }, title, false });

    auto* ws = own (new WaveSelector (proc.apvts, oid (o, "wave")));
    ws->setBounds (x + 4, y + 13, 48, 92);

    knob (oid (o, "oct"),  { x + 56, y + 14, 38, 46 }, "Oct", true);
    knob (oid (o, "semi"), { x + 96, y + 14, 38, 46 }, "Semi", true);
    auto* pwLabel = own (new juce::Label ({}, "PWM"));
    pwLabel->setJustificationType (juce::Justification::centred);
    pwLabel->setFont (PlutoLook::font (11.0f));
    pwLabel->setBounds (x + 56, y + 60, 38, 11);
    pwLabel->setBorderSize ({});
    knob (oid (o, "pwm"),  { x + 56, y + 71, 38, 35 }, {}, true);
    knob (oid (o, "cent"), { x + 96, y + 60, 38, 46 }, "Cent", true);

    text ({ x + 134, y + 14, 30, 11 }, "Fine");
    vSlider (oid (o, "fine"), { x + 136, y + 26, 28, 80 }, true);
    text ({ x + 162, y + 14, 32, 11 }, "Coarse");
    vSlider (oid (o, "coarse"), { x + 164, y + 26, 28, 80 }, true);

    // Wavetable: Tabelle, Vorschau, Position
    auto* wt = combo (oid (o, "wt"), { x + 196, y + 14, 72, 16 });
    tableBoxes.push_back (wt);
    auto* prev = own (new WavePreview (proc.apvts, o));
    prev->setBounds (x + 196, y + 33, 36, 38);
    prev->reader = [this, o] (int table, float pos, float ph)
    {
        if (table == userTableIndex)
        {
            if (auto* t = proc.getUserTable (o)) return t->read (pos, ph, 0.0);
            return 0.0f;
        }
        return proc.bank->readDisplay (table, pos, ph);
    };
    prev->onLoadRequest = [this, o] { openWavetableChooser (o); };
    previews.push_back (prev);
    auto* pos = knob (oid (o, "wtpos"), { x + 234, y + 31, 34, 40 }, "Pos", false);

    // Phase Distortion
    text ({ x + 196, y + 74, 44, 11 }, "PD");
    combo (oid (o, "pdmode"), { x + 196, y + 87, 46, 17 })->setTooltip ("Phase Distortion (CZ-Stil)");
    knob (oid (o, "pdamt"), { x + 244, y + 73, 24, 33 }, "Amt", false);

    auto update = [wt, pos, pwLabel] (int w)
    {
        wt->setEnabled (w == 4);
        pos->setEnabled (w == 4);
        pwLabel->setText (w == 1 ? "Skew" : "PWM", juce::dontSendNotification);
    };
    ws->onWaveChanged = update;
    update (ws->current());

    oscRects[(size_t) o] = { x, y, 272, 186 };

    auto* env = own (new EnvDisplay (proc.apvts, o));
    env->setBounds (x + 4, y + 109, 264, 73);

    const int ry = y + 190;
    if (! carrier)
    {
        content.boxes.push_back ({ { x, ry, 272, 66 }, "Routing+FMAMT", false });
        text ({ x + 4, ry + 13, 66, 11 }, "Destination");
        combo (oid (o, "dest"), { x + 4, ry + 25, 66, 17 });
        text ({ x + 74, ry + 13, 72, 11 }, "FM Type");
        combo (oid (o, "fmtype"), { x + 74, ry + 25, 72, 17 });
        knob (oid (o, "fmfine"), { x + 152, ry + 12, 34, 33 }, "Fine", false);
        knob (oid (o, "fb"), { x + 190, ry + 12, 34, 33 }, "Fb", false);
        hSlider (oid (o, "fmamt"), { x + 4, ry + 47, 264, 15 }, 38);
        text ({ x + 226, ry + 13, 42, 11 }, "FM AMT");
    }
    else
    {
        content.boxes.push_back ({ { x, ry, 272, 46 }, "Choose Filter", false });
        text ({ x, ry + 12, 272, 11 }, "Filter Destination");
        combo (oid (o, "fdest"), { x + 101, ry + 25, 70, 17 });
    }
}

void PlutoEditor::buildFilter (int f, int x, int y)
{
    content.boxes.push_back ({ { x, y, 304, 150 }, {}, false });
    auto* title = own (new juce::Label());
    title->setJustificationType (juce::Justification::centred);
    title->setFont (PlutoLook::font (11.5f));
    title->setBounds (x + 6, y + 1, 140, 14);
    title->setBorderSize ({});
    toggle (fid (f, "on"), { x + 146, y + 2, 28, 12 }, {})->setTooltip ("Filter " + juce::String (f + 1) + " an/aus (aus = Bypass)");

    auto* disp = own (new FilterDisplay (proc.apvts, f));
    disp->setBounds (x + 6, y + 16, 168, 84);

    text ({ x + 4, y + 103, 28, 14 }, "D/W", juce::Justification::centredLeft);
    hSlider (fid (f, "mix"), { x + 32, y + 103, 142, 14 }, 0);
    text ({ x + 4, y + 124, 28, 16 }, "List", juce::Justification::centredLeft);
    auto* type = combo (fid (f, "type"), { x + 32, y + 123, 80, 18 });
    auto setTitle = [title, type, f]
    {
        const int i = juce::jmax (0, type->getSelectedId() - 1);
        title->setText ("Filter " + juce::String (f + 1) + "  " + filterLongNames()[i], juce::dontSendNotification);
    };
    type->onChange = setTitle;
    setTitle();

    knob (fid (f, "drive"), { x + 180, y + 14, 40, 40 }, "Drive", false);
    knob (fid (f, "cut"), { x + 222, y + 14, 58, 66 }, "Cutoff", true);
    knob (fid (f, "res"), { x + 180, y + 82, 44, 56 }, "Res", true);
    knob (fid (f, "pan"), { x + 226, y + 86, 28, 40 }, "/ Pan", false);
    knob (fid (f, "xpan"), { x + 254, y + 86, 28, 40 }, "X Pan", false);
    vSlider (fid (f, "lvl"), { x + 284, y + 14, 16, 128 }, false);
}

void PlutoEditor::buildMixer (int x, int y)
{
    content.boxes.push_back ({ { x, y, 316, 104 }, "Mixer", true });
    for (int o = 0; o < numOsc; ++o)
    {
        const int fx = x + 14 + o * 30 + (o / 3) * 12;
        text ({ fx - 4, y + 18, 32, 11 }, oscNames()[o]);
        vSlider (oid (o, "lvl"), { fx, y + 30, 24, 68 }, false);
    }
}

void PlutoEditor::buildSync (int x, int y)
{
    content.boxes.push_back ({ { x, y, 316, 209 }, "SncMatrix", true });
    for (int o = 0; o < numOsc; ++o)
    {
        const int ry = y + 20 + o * 19 + (o / 3) * 6;
        auto* km = own (new KeyModeSelector (proc.apvts, oid (o, "key")));
        km->setBounds (x + 8, ry, 66, 17);
        content.nameBoxes.push_back ({ x + 82, ry + 1, 64, 15 });
        content.nameTexts.add (oscNames()[o]);
        combo (oid (o, "sync"), { x + 156, ry, 150, 17 })->setTooltip ("Hard-Sync: Phase wird von diesem Oszillator zurueckgesetzt");
    }
}

void PlutoEditor::buildMaster (int x, int y)
{
    content.boxes.push_back ({ { x, y, 316, 171 }, "MIDI/Master", true });
    text ({ x + 6, y + 19, 34, 16 }, "Tune", juce::Justification::centredLeft);
    hSlider ("m_tune", { x + 40, y + 19, 200, 16 }, 44);

    content.boxes.push_back ({ { x + 6, y + 40, 240, 122 }, "MstrPitchCtrl", false });
    knob ("m_oct",  { x + 30, y + 54, 44, 52 }, "Oct", true);
    knob ("m_semi", { x + 104, y + 54, 44, 52 }, "Semi", true);
    knob ("m_cent", { x + 178, y + 54, 44, 52 }, "Cent", true);
    text ({ x + 10, y + 114, 40, 16 }, "Fine", juce::Justification::centredLeft);
    hSlider ("m_fine", { x + 50, y + 114, 190, 16 }, 46);
    text ({ x + 10, y + 136, 40, 16 }, "Coarse", juce::Justification::centredLeft);
    hSlider ("m_coarse", { x + 50, y + 136, 190, 16 }, 46);

    text ({ x + 254, y + 19, 56, 12 }, "Vol");
    vSlider ("m_vol", { x + 262, y + 32, 40, 134 }, false);
}

void PlutoEditor::buildLfo (int l, int x, int y)
{
    content.boxes.push_back ({ { x, y, 202, 108 }, "LFO " + juce::String (l + 1), false });
    auto* d = own (new LfoDisplay (proc.apvts, l));
    d->setBounds (x + 6, y + 15, 86, 38);
    lfoDisplays.push_back (d);
    combo (lid (l, "shape"), { x + 6, y + 57, 86, 17 })->setTooltip ("Form");
    combo (lid (l, "div"), { x + 6, y + 80, 86, 17 })->setTooltip ("Notenwert bei Sync");

    knob (lid (l, "rate"), { x + 96, y + 12, 48, 44 }, "Rate", true);
    toggle (lid (l, "sync"), { x + 144, y + 17, 58, 16 }, "Sync")->setTooltip ("Tempo-Sync zum Host (Notenwert links)");
    toggle (lid (l, "retrig"), { x + 144, y + 37, 58, 16 }, "Retrig")->setTooltip ("LFO startet bei jeder neuen Note von vorne");
    combo (lid (l, "tA"), { x + 96, y + 60, 72, 17 })->setTooltip ("Ziel A");
    knob (lid (l, "aA"), { x + 170, y + 57, 28, 23 }, {}, false);
    combo (lid (l, "tB"), { x + 96, y + 83, 72, 17 })->setTooltip ("Ziel B");
    knob (lid (l, "aB"), { x + 170, y + 80, 28, 23 }, {}, false);
}

void PlutoEditor::paint (juce::Graphics& g) { g.fillAll (col::outer); }

void PlutoEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (s));
}

int PlutoEditor::oscAt (int x, int y) const
{
    const float sc = (float) getWidth() / (float) baseW;
    const juce::Point<int> p ((int) ((float) x / sc), (int) ((float) y / sc));
    for (int o = 0; o < numOsc; ++o)
        if (oscRects[(size_t) o].contains (p)) return o;
    return -1;
}

void PlutoEditor::setDropTarget (int osc)
{
    if (osc == dropOsc) return;
    dropOsc = osc;
    content.dropHighlight = osc >= 0 ? oscRects[(size_t) osc] : juce::Rectangle<int>();
    content.repaint();
}

bool PlutoEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac")) return true;
    return false;
}

void PlutoEditor::fileDragMove (const juce::StringArray&, int x, int y) { setDropTarget (oscAt (x, y)); }

void PlutoEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    const int o = oscAt (x, y);
    setDropTarget (-1);
    if (o < 0 || files.isEmpty()) return;
    loadWavetable (o, juce::File (files[0]));
}

void PlutoEditor::loadWavetable (int osc, const juce::File& f)
{
    juce::String err;
    if (! proc.loadUserWavetable (osc, f, err))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Wavetable", f.getFileName() + ": " + err);
}

void PlutoEditor::openWavetableChooser (int osc)
{
    chooser = std::make_unique<juce::FileChooser> ("Wavetable laden (" + oscNames()[osc] + ")", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, osc] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) loadWavetable (osc, f);
                          });
}

void PlutoEditor::updateTableTooltips()
{
    for (int o = 0; o < (int) tableBoxes.size(); ++o)
    {
        const auto n = proc.getUserTableName (o);
        tableBoxes[(size_t) o]->setTooltip (n.isNotEmpty() ? "User: " + n : "Wavetable (eigene: .wav aufs Feld ziehen)");
        previews[(size_t) o]->repaint();
    }
}

void PlutoEditor::timerCallback()
{
    const int uc = proc.userTablesChanged.load();
    if (uc != lastUserCounter) { lastUserCounter = uc; updateTableTooltips(); }
    for (int l = 0; l < (int) lfoDisplays.size(); ++l)
    {
        lfoDisplays[(size_t) l]->phase = proc.engine.lfoPhaseDisplay[l].load();
        lfoDisplays[(size_t) l]->repaint();
    }
    const int pc = proc.presetChanged.load();
    if (pc != lastPresetCounter)
    {
        lastPresetCounter = pc;
        presetBox->setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    }
}
