#include "PluginEditor.h"
#include "PrizmAssets.h"

#include <cmath>

using namespace prizm;

namespace
{
    constexpr int kWidth = 800;
    constexpr int kHeight = 830;
    constexpr float kTitleSize = 48.0f;

    juce::Font display (float size, bool bold = false) { return bb::fonts::display (size, bold); }
    juce::Font mono (float size) { return bb::fonts::mono (size); }
    juce::Font titleFont() { return display (kTitleSize, true).withExtraKerningFactor (0.18f); }

    bool isSharp (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

    // Tooltip-Texte stehen als UTF-8 im Quelltext (Umlaute)
    juce::String de (const char* text) { return juce::String::fromUTF8 (text); }

    void softGlow (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c, float amount, int rings = 4)
    {
        for (int i = rings; i >= 1; --i)
        {
            const float grow = (float) i * 2.6f;
            g.setColour (c.withAlpha (amount * 0.12f / (float) i));
            g.fillRoundedRectangle (r.expanded (grow), corner + grow);
        }
    }
} // namespace

//==============================================================================
void PrismGlyph::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    const float h = b.getHeight() * 0.84f;
    const float side = h / 0.866f;
    const float cx = b.getX() + b.getWidth() * 0.36f;
    const float top = b.getCentreY() - h * 0.5f;

    const Point<float> apex (cx, top), left (cx - side * 0.5f, top + h), right (cx + side * 0.5f, top + h);
    Path tri;
    tri.addTriangle (apex, left, right);

    const Point<float> beamStart (b.getX(), top + h * 0.70f);
    const Point<float> entry = apex + (left - apex) * 0.58f;
    const Point<float> exit = apex + (right - apex) * 0.46f;

    // white beam in
    g.setColour (Col::holoBright.withAlpha (0.15f));
    g.drawLine ({ beamStart, entry }, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine ({ beamStart, entry }, 1.4f);

    // spectrum out, gently breathing
    const std::array<Colour, 5> rays { Col::pink, Col::amber, Col::tox, Col::cyan, Col::violet };
    for (size_t i = 0; i < rays.size(); ++i)
    {
        const float fi = (float) i;
        const float angle = 0.08f + (fi - 2.0f) * 0.11f;
        const float len = (b.getRight() - exit.x) * (0.84f + 0.14f * std::sin (phase * 1.3f + fi * 1.1f));
        const float alpha = 0.5f + 0.4f * (0.5f + 0.5f * std::sin (phase * 2.1f + fi * 0.9f));
        const Point<float> end (exit.x + len * std::cos (angle), exit.y + len * std::sin (angle));
        g.setColour (rays[i].withAlpha (alpha * 0.22f));
        g.drawLine ({ exit, end }, 4.5f);
        g.setColour (rays[i].withAlpha (alpha));
        g.drawLine ({ exit, end }, 1.5f);
    }

    // hologram glass prism
    ColourGradient glass (Col::holo.withAlpha (0.28f), apex.x, apex.y, Col::holo.withAlpha (0.04f), cx, top + h, false);
    g.setGradientFill (glass);
    g.fillPath (tri);
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawLine ({ entry, exit }, 1.0f);
    g.setColour (Col::holo.withAlpha (0.25f));
    g.strokePath (tri, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Col::holoBright);
    g.strokePath (tri, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
void ModeSwitch::setMode (int newMode)
{
    if (newMode != mode)
    {
        mode = newMode;
        repaint();
    }
}

bool ModeSwitch::advance()
{
    const float target = (float) mode;
    const float diff = target - thumb;
    if (std::abs (diff) < 0.002f)
    {
        if (std::abs (diff) > 0.0f)
        {
            thumb = target;
            repaint();
        }
        return false;
    }
    thumb += diff * 0.28f;
    repaint();
    return true;
}

void ModeSwitch::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto r = getLocalBounds().toFloat().reduced (5.0f);
    holo::box (g, r, 1.0f, isMouseOver());

    const float halfW = r.getWidth() * 0.5f;
    const auto thumbRect = Rectangle<float> (r.getX() + 4.0f + thumb * halfW, r.getY() + 4.0f, halfW - 8.0f, r.getHeight() - 8.0f);

    softGlow (g, thumbRect, 2.0f, Col::holo, 1.2f, 3);
    ColourGradient fill (Col::holoBright, 0.0f, thumbRect.getY(), Col::holo.darker (0.25f), 0.0f, thumbRect.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (thumbRect, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawHorizontalLine ((int) thumbRect.getY() + 1, thumbRect.getX() + 2.0f, thumbRect.getRight() - 2.0f);

    const auto mouse = getMouseXYRelative().toFloat();
    const std::array<const char*, 2> labels { "ZERO LATENCY", "HIGH QUALITY" };
    g.setFont (display (14.0f, true).withExtraKerningFactor (0.08f));
    for (int i = 0; i < 2; ++i)
    {
        const auto half = Rectangle<float> (r.getX() + (float) i * halfW, r.getY(), halfW, r.getHeight());
        const float onAmount = 1.0f - jlimit (0.0f, 1.0f, std::abs (thumb - (float) i));
        const bool hover = isMouseOver() && half.contains (mouse) && i != mode;
        g.setColour ((hover ? Col::ink : Col::muted).interpolatedWith (Col::glassDeep, onAmount));
        g.drawText (labels[(size_t) i], half, Justification::centred);
    }
}

void ModeSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int clicked = e.x < getWidth() / 2 ? 0 : 1;
    if (clicked != mode && onChange)
        onChange (clicked);
}

//==============================================================================
void KeyStrip::setState (PitchMask newMask, int newRoot, int newSource)
{
    if (newMask != mask || newRoot != root || newSource != source)
    {
        mask = newMask;
        root = newRoot;
        source = newSource;
        repaint();
    }
}

juce::Rectangle<float> KeyStrip::cellBounds (int pc) const
{
    const auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    const float gap = 7.0f;
    const float w = (area.getWidth() - gap * 11.0f) / 12.0f;
    return { area.getX() + (float) pc * (w + gap), area.getY(), w, area.getHeight() };
}

int KeyStrip::cellAt (int x) const
{
    for (int pc = 0; pc < 12; ++pc)
    {
        const auto c = cellBounds (pc);
        if ((float) x >= c.getX() - 3.5f && (float) x < c.getRight() + 3.5f)
            return pc;
    }
    return -1;
}

void KeyStrip::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto onColour = source == 1 ? Col::pink : (source == 2 ? Col::cyan : Col::tox);
    const auto onInk = source == 1 ? juce::Colours::white : Col::toxInk;

    for (int pc = 0; pc < 12; ++pc)
    {
        const auto cell = cellBounds (pc);
        const bool on = (mask >> pc) & 1u;
        const float e = jlimit (0.0f, 1.0f, energy[(size_t) pc]);
        const float corner = 3.0f;

        if (e > 0.02f)
            softGlow (g, cell, corner, on ? onColour : Col::pink, e, 4);

        if (on)
        {
            // Gewichtung: Füllhöhe der Taste (darüber dunkles Glas)
            const float wgt = getWeight ? jlimit (0.0f, 1.0f, getWeight (pc)) : 1.0f;
            if (wgt < 0.999f)
                holo::box (g, cell, 1.0f, false);
            const auto filled = cell.withTop (cell.getBottom() - cell.getHeight() * jmax (0.08f, wgt));
            ColourGradient fill (onColour.brighter (0.15f + 0.3f * e), 0.0f, cell.getY(), onColour.darker (0.45f), 0.0f, cell.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (filled, corner);
            if (wgt < 0.999f)
            {
                g.setFont (mono (10.0f));
                g.setColour (Col::ink);
                g.drawText (String (roundToInt (wgt * 100.0f)) + "%", cell.withHeight (14.0f).translated (0.0f, 3.0f), Justification::centred);
            }
            g.setColour (juce::Colours::white.withAlpha (0.45f)); // wet highlight
            g.fillRoundedRectangle (cell.reduced (3.0f, 0.0f).withHeight (1.5f).translated (0.0f, 2.0f), 0.75f);
            g.setColour (onColour.darker (0.2f));
            g.drawRoundedRectangle (cell.reduced (0.5f), corner, 1.0f);
        }
        else
        {
            holo::box (g, cell, isSharp (pc) ? 0.7f : 1.0f, pc == hover);
        }

        if (pc == root)
        {
            g.setColour (Col::pink.withAlpha (0.3f));
            g.drawRoundedRectangle (cell.expanded (1.5f), corner + 1.5f, 3.0f);
            g.setColour (Col::pink);
            g.drawRoundedRectangle (cell.reduced (0.5f), corner, 1.6f);
        }

        // Notenname: auf teilweise gefüllten Tasten hell, damit er auf dem dunklen Teil lesbar bleibt
        const float nameWeight = (on && getWeight) ? getWeight (pc) : 1.0f;
        g.setColour (on ? (nameWeight > 0.6f ? onInk : Col::ink) : (isSharp (pc) ? Col::dim : Col::muted));
        g.setFont (display (18.0f, true));
        g.drawText (kNoteNames[(size_t) pc], cell.withTrimmedBottom (8.0f), Justification::centred);

        if (pc == root)
        {
            g.setFont (mono (10.0f));
            g.setColour (on ? onInk.withAlpha (0.75f) : Col::pink);
            g.drawText ("ROOT", cell.withTrimmedTop (cell.getHeight() * 0.60f).withTrimmedBottom (8.0f), Justification::centred);
        }

        const float barW = (cell.getWidth() - 14.0f) * e;
        if (barW > 0.5f)
        {
            const auto bar = Rectangle<float> (cell.getCentreX() - barW * 0.5f, cell.getBottom() - 7.0f, barW, 3.0f);
            g.setColour (on ? onInk.withAlpha (0.55f) : Col::pink);
            g.fillRoundedRectangle (bar, 1.5f);
        }
    }
}

void KeyStrip::mouseDown (const juce::MouseEvent& e)
{
    // Klick = Note an/aus (beim Loslassen), Ziehen hoch/runter = Gewichtung
    dragNote = cellAt (e.x);
    dragging = false;
    dragStartWeight = (dragNote >= 0 && getWeight) ? getWeight (dragNote) : 1.0f;
}

void KeyStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNote < 0 || ! ((mask >> dragNote) & 1u) || ! onWeight)
        return;
    const int dy = e.getDistanceFromDragStartY();
    if (! dragging && std::abs (dy) < 4)
        return;
    const bool start = ! dragging;
    dragging = true;
    const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
    onWeight (dragNote, juce::jlimit (0.0f, 1.0f, dragStartWeight - (float) dy / 80.0f * fine), start, false);
}

void KeyStrip::mouseUp (const juce::MouseEvent& e)
{
    if (dragging)
    {
        if (onWeight && getWeight)
            onWeight (dragNote, getWeight (dragNote), false, true);
    }
    else if (dragNote >= 0 && dragNote == cellAt (e.x) && onToggle && e.getNumberOfClicks() < 2)
    {
        onToggle (dragNote);
    }
    dragNote = -1;
    dragging = false;
}

void KeyStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Doppelklick: Gewichtung zurück auf 100 % (und Note wieder an, falls der erste Klick sie ausgeschaltet hat)
    const int pc = cellAt (e.x);
    if (pc < 0 || ! onWeight)
        return;
    if (! ((mask >> pc) & 1u) && onToggle)
        onToggle (pc);
    onWeight (pc, 1.0f, true, true);
}

juce::String KeyStrip::getTooltip()
{
    const int pc = cellAt (getMouseXYRelative().x);
    if (pc < 0)
        return {};
    const juce::String n = kNoteNames[(size_t) pc];
    const bool on = (mask >> pc) & 1u;
    juce::String src;
    if (source == 1) src = de ("\nGerade gibt MIDI KEY die Tonart vor.");
    if (source == 2) src = de ("\nGerade gibt die SIDECHAIN die Tonart vor.");
    if (on)
    {
        const int w = getWeight ? juce::roundToInt (getWeight (pc) * 100.0f) : 100;
        return n + de ("  IN DER TONART") + (w < 100 ? de ("  (") + juce::String (w) + " %)" : juce::String())
               + de ("\n") + n + de (" darf durch. Hoch/runter ziehen: Gewichtung, wie stark die Note durchkommt (Shift = fein). Klick: Note ausschalten. Doppelklick: 100 %.")
               + de ("\nDas Leuchten zeigt, wie stark ") + n + de (" gerade reinkommt.") + src;
    }
    return n + de ("  NICHT IN DER TONART\n") + n
           + de (" wird rausgefiltert oder mit SHIFT auf die nächste erlaubte Note gezogen. In der Pitch Matrix darunter siehst du, wohin. Klick: Note erlauben.") + src;
}

void KeyStrip::mouseMove (const juce::MouseEvent& e)
{
    const int pc = cellAt (e.x);
    if (pc != hover)
    {
        hover = pc;
        repaint();
    }
}

//==============================================================================
PrizmAudioProcessorEditor::PrizmAudioProcessorEditor (PrizmAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    auto& apvts = processor.apvts;

    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    texture = juce::ImageCache::getFromMemory (PrizmAssets::prizm_backdrop_jpg, PrizmAssets::prizm_backdrop_jpgSize);

    addAndMakeVisible (prism);
    prism.setInterceptsMouseClicks (false, false);

    modeSwitch.onChange = [this] (int m) { setMode (m); };
    modeSwitch.setTooltip (de ("MODUS\nZero Latency: Filterbank ohne Verzögerung, gut für Live und Recording.\nHigh Quality: spektral, sauberer bei Anschlägen und kann Noten verschieben (SHIFT, MAP). 85 ms Latenz, die DAW gleicht sie automatisch aus."));
    addAndMakeVisible (modeSwitch);

    for (auto* n : kNoteNames)
        keyBox.addItem (n, keyBox.getNumItems() + 1);
    for (auto& s : kScales)
        scaleBox.addItem (s.name, scaleBox.getNumItems() + 1);
    keyBox.setTooltip (de ("KEY\nGrundton der Tonart. Zusammen mit SCALE legt er fest, welche Noten erlaubt sind (grüne Tasten)."));
    scaleBox.setTooltip (de ("SCALE\nTonleiter zum Grundton. Sobald du einzelne Tasten an- oder ausschaltest, wird daraus automatisch Custom."));
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::key, keyBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::scale, scaleBox);

    midiButton.setClickingTogglesState (true);
    midiButton.setTooltip (de ("MIDI KEY\nSpiel Noten auf den MIDI-Eingang von Prizm: der zuletzt gespielte Akkord wird zur Tonart. So folgt Prizm deinen Akkordwechseln."));
    addAndMakeVisible (midiButton);
    midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamIDs::midiKey, midiButton);

    learnButton.setTooltip (de ("LEARN\nKlicken und Material abspielen: Prizm hört 8 Sekunden zu und stellt KEY, SCALE und TUNE selbst ein. Nochmal klicken bricht ab."));
    learnButton.onClick = [this]
    {
        if (processor.isLearning())
            processor.cancelLearn();
        else
            processor.startLearn();
    };
    addAndMakeVisible (learnButton);

    // Pitch Matrix: zeigt, wo jede Note landet, und ist gleichzeitig die Bedienung der Noten-Map
    pitchMatrix.onSetMap = [this] (int pc, int index) { processor.setMap (pc, index); updateMatrix ((int) processor.apvts.getRawParameterValue (ParamIDs::mode)->load()); };
    pitchMatrix.onResetAll = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::reset); };
    addAndMakeVisible (pitchMatrix);

    majToMinButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::majorToMinor); };
    minToMajButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::minorToMajor); };
    resetMapButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::reset); };
    majToMinButton.setTooltip (de ("MAJ > MIN\nMacht aus Dur Moll, bezogen auf KEY: Terz, Sexte und Septime wandern einen Halbton tiefer. Braucht High Quality."));
    minToMajButton.setTooltip (de ("MIN > MAJ\nMacht aus Moll Dur, bezogen auf KEY: Terz, Sexte und Septime wandern einen Halbton höher. Braucht High Quality."));
    resetMapButton.setTooltip (de ("RESET MAP\nAlle Umleitungen und OFF-Noten zurück auf normal."));
    for (auto* b : { &majToMinButton, &minToMajButton, &resetMapButton })
        addAndMakeVisible (*b);

    // Sidechain-Tonart
    scButton.setClickingTogglesState (true);
    scButton.setTooltip (de ("SIDECHAIN\nDie Tonart folgt den Noten einer anderen Spur, z. B. Bass oder Keys. Route die Spur in der DAW auf den Sidechain-Eingang von Prizm."));
    addAndMakeVisible (scButton);
    scAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamIDs::scKey, scButton);

    // Presets
    presetBox.setTooltip (de ("PRESET\nStartpunkte für typische Aufgaben und deine eigenen Presets. KEY, SCALE und TUNE bleiben beim Durchblättern stehen."));
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx != processor.getCurrentPreset())
            processor.loadPreset (idx);
        else if (idx >= 0)
            processor.loadPreset (idx); // gleiches Preset nochmal = zurücksetzen
        shownPreset = processor.getCurrentPreset();
    };
    addAndMakeVisible (presetBox);
    savePresetButton.setTooltip (de ("SAVE\nAktuelle Einstellungen als eigenes Preset speichern (Dokumente/Brain Bugz/Prizm/Presets)."));
    savePresetButton.onClick = [this] { askPresetName(); };
    addAndMakeVisible (savePresetButton);
    refreshPresetBox();

    // Mitte/Seite
    stereoBox.addItem ("STEREO", 1);
    stereoBox.addItem ("MID ONLY", 2);
    stereoBox.addItem ("SIDE ONLY", 3);
    stereoBox.setTooltip (de ("PROCESS\nStereo: beide Kanäle. Mid only: nur die Mitte. Side only: nur die Stereo-Seiten, z. B. um nur die Breite eines Pads zu stimmen. Der Rest bleibt unverändert."));
    addAndMakeVisible (stereoBox);
    stereoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::stereo, stereoBox);

    keyStrip.onToggle = [this] (int pc) { processor.togglePitchClass (pc); };
    keyStrip.onWeight = [this] (int pc, float w, bool start, bool end) { processor.setWeight (pc, w, start, end); keyStrip.repaint(); };
    keyStrip.getWeight = [this] (int pc) { return processor.getWeight (pc); };
    addAndMakeVisible (keyStrip);

    struct Def { const char* id; const char* title; const char* hint; };
    const std::array<Def, 11> defs {{
        { ParamIDs::strength, "STRENGTH",   "Wie viel vom gestimmten Signal das Original ersetzt. 100 % = voll in der Tonart, 50 % = halb Original." },
        { ParamIDs::color,    "COLOR",      "Wie tonal es klingt. Wenig = weich und natürlich, 100 % = deutlich gestimmt. Über 100 % (pink) klingen die Töne lange nach." },
        { ParamIDs::clear,    "CLEAR",      "Lässt Anschläge trocken durch und dämpft altes Nachklingen bei neuen Schlägen. Hoch für Drums, niedrig für Flächen." },
        { ParamIDs::mix,      "MIX",        "Trocken/nass. Im High-Quality-Modus wird das Original passend verzögert, damit nichts phast." },
        { ParamIDs::output,   "OUTPUT",     "Ausgangspegel. Ein sanfter Limiter verhindert hartes Übersteuern." },
        { ParamIDs::rangeLow, "RANGE LOW",  "Unterhalb dieser Frequenz bleibt alles unverändert, z. B. Kick und Sub. OFF = ganzer Bereich." },
        { ParamIDs::rangeHigh,"RANGE HIGH", "Oberhalb dieser Frequenz bleibt alles unverändert, z. B. Becken und Luft. OFF = ganzer Bereich." },
        { ParamIDs::gate,     "GATE",       "Drückt leise Noten gegenüber der lautesten herunter. Räumt Rauschen und Matsch zwischen den Tönen auf." },
        { ParamIDs::shift,    "SHIFT",      "Nur High Quality: zieht schiefe Noten samt Obertönen auf die nächste erlaubte Note, statt sie nur leiser zu machen. In der Pitch Matrix: cyan Kurven." },
        { ParamIDs::formant,  "FORMANT",    "Nur High Quality: erhält die Klangfarbe, wenn Noten verschoben werden. Verhindert den Mickey-Mouse-Effekt bei Stimmen." },
        { ParamIDs::tune,     "TUNE A4",    "Stimmton, auf den sich alle Noten beziehen. Alte Samples und Vinyl liegen oft nicht bei 440 Hz. LEARN stellt ihn automatisch ein." },
    }};

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.title = defs[i].title;
        // Tooltip vor dem Textfeld setzen, damit auch das Wertefeld ihn übernimmt
        k.slider.setTooltip (juce::String (defs[i].title) + "\n" + de (defs[i].hint));
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 24);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, defs[i].id, k.slider);
        if (juce::String (defs[i].id) == ParamIDs::color)
            k.slider.getProperties().set ("overdrive", true); // über 100 % pink
        if (auto* param = apvts.getParameter (defs[i].id))
            k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    processor.setEditorOpen (true);
    setSize (kWidth, kHeight);
    timerCallback();
    startTimerHz (30);
}

PrizmAudioProcessorEditor::~PrizmAudioProcessorEditor()
{
    processor.setEditorOpen (false);
    stopTimer();
    setLookAndFeel (nullptr);
}

void PrizmAudioProcessorEditor::refreshPresetBox()
{
    processor.refreshUserPresets();
    presetBox.clear (juce::dontSendNotification);
    const auto names = processor.getPresetNames();
    const int numFactory = processor.getNumFactoryPresets();
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == numFactory)
            presetBox.addSeparator();
        presetBox.addItem (names[i], i + 1);
    }
    shownPreset = processor.getCurrentPreset();
    presetBox.setSelectedItemIndex (shownPreset, juce::dontSendNotification);
}

void PrizmAudioProcessorEditor::askPresetName()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", {}, {});
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PrizmAudioProcessorEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            const auto name = w->getTextEditorContents ("name");
            if (safe->processor.saveUserPreset (name))
                safe->refreshPresetBox();
        }
    }), true);
}

void PrizmAudioProcessorEditor::setMode (int mode)
{
    if (auto* param = processor.apvts.getParameter (ParamIDs::mode))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) mode));
        param->endChangeGesture();
    }
}

void PrizmAudioProcessorEditor::updateEnergy()
{
    const uint32_t counter = processor.getBlockCounter();
    idleTicks = (counter == lastCounter) ? idleTicks + 1 : 0;
    lastCounter = counter;

    std::array<float, 12> raw {};
    float total = 0.0f, peak = 0.0f;
    for (int pc = 0; pc < 12; ++pc)
    {
        raw[(size_t) pc] = std::max (0.0f, processor.getPitchEnergy (pc));
        total += raw[(size_t) pc];
        peak = std::max (peak, raw[(size_t) pc]);
    }

    energyRef = std::max (total, energyRef * 0.985f);
    const bool silent = idleTicks > 6 || peak <= 0.0f || total < energyRef * 1.0e-4f;

    for (size_t pc = 0; pc < 12; ++pc)
    {
        const float target = silent ? 0.0f : std::sqrt (raw[pc] / peak);
        shownEnergy[pc] = std::max (target, shownEnergy[pc] * 0.84f);
        if (shownEnergy[pc] < 0.01f)
            shownEnergy[pc] = 0.0f;
    }
    // Eingang: Noten, die gerade reinkommen (Grundtöne), relativ zur stärksten
    std::array<float, 12> rawIn {};
    float peakIn = 0.0f;
    for (int pc = 0; pc < 12; ++pc)
    {
        rawIn[(size_t) pc] = std::max (0.0f, processor.getInputEnergy (pc));
        peakIn = std::max (peakIn, rawIn[(size_t) pc]);
    }
    inputRef = std::max (peakIn, inputRef * 0.99f);
    const bool silentIn = idleTicks > 6 || peakIn <= inputRef * 0.01f;
    for (size_t pc = 0; pc < 12; ++pc)
    {
        const float target = (silentIn || peakIn <= 0.0f) ? 0.0f : std::sqrt (rawIn[pc] / peakIn);
        shownInput[pc] = std::max (target, shownInput[pc] * 0.82f);
        if (shownInput[pc] < 0.01f)
            shownInput[pc] = 0.0f;
    }
    keyStrip.setEnergy (shownInput);
}

void PrizmAudioProcessorEditor::updateMatrix (int mode)
{
    using Route = PitchMatrix::Route;
    auto& apvts = processor.apvts;
    const bool hq = mode == highQuality;
    const PitchMask mask = processor.getDisplayMask();
    const float shift = hq ? apvts.getRawParameterValue (ParamIDs::shift)->load() * 0.01f : 0.0f;
    const float strength = apvts.getRawParameterValue (ParamIDs::strength)->load() * 0.01f;
    const float mix = apvts.getRawParameterValue (ParamIDs::mix)->load() * 0.01f;
    const float dry = juce::jlimit (0.0f, 1.0f, 1.0f - strength * mix);

    std::array<Route, 12> routes {};
    for (int pc = 0; pc < 12; ++pc)
    {
        Route r;
        r.target = pc;
        r.targetPos = (float) pc;
        const int mi = processor.getMapIndex (pc);
        if (mi == 13)
        {
            r.kind = Route::mute;
        }
        else if (mi >= 1 && hq)
        {
            r.kind = Route::map;
            r.target = mi - 1;
            r.targetPos = (float) r.target;
            r.weight = processor.getWeight (r.target);
        }
        else
        {
            if (mi >= 1)
                r.ghost = mi - 1;
            if (isAllowed (mask, pc))
            {
                r.kind = Route::pass;
                r.weight = processor.getWeight (pc);
            }
            else if (shift > 0.001f)
            {
                const int n = nearestAllowedNote (mask, 60.0f + (float) pc);
                r.kind = Route::shift;
                r.target = ((n % 12) + 12) % 12;
                r.targetPos = (float) pc + shift * (float) (r.target - pc);
                r.weight = processor.getWeight (r.target);
            }
            else
            {
                r.kind = Route::filtered;
            }
        }
        if ((r.kind == Route::pass || r.kind == Route::map || r.kind == Route::shift) && r.weight < 0.02f)
            r.kind = Route::filtered;
        r.dry = r.kind == Route::pass ? 0.0f : dry;
        routes[(size_t) pc] = r;
    }
    pitchMatrix.setRoutes (routes, hq, mask, processor.getScaleMask());
}

juce::Rectangle<int> PrizmAudioProcessorEditor::scanBand (juce::Rectangle<int> panel, float phase) const
{
    // a soft scan line that sweeps slowly down a hologram panel
    const float period = 6.0f;
    const float t = std::fmod (phase, period) / period;
    const int bandH = 46;
    const int y = panel.getY() - bandH + juce::roundToInt (t * (float) (panel.getHeight() + bandH));
    return { panel.getX(), y, panel.getWidth(), bandH };
}

void PrizmAudioProcessorEditor::timerCallback()
{
    auto& apvts = processor.apvts;
    const int mode = (int) apvts.getRawParameterValue (ParamIDs::mode)->load();
    modeSwitch.setMode (mode);
    modeSwitch.advance();

    // Learn: Fortschritt anzeigen, Ergebnis übernehmen
    if (processor.isLearning())
    {
        learnButton.setButtonText ("LISTENING " + juce::String (juce::roundToInt (processor.getLearnProgress() * 100.0f)) + " %");
        learnButton.setToggleState (true, juce::dontSendNotification);
    }
    else if (learnButton.getToggleState())
    {
        learnButton.setButtonText ("LEARN");
        learnButton.setToggleState (false, juce::dontSendNotification);
    }
    prizm::KeyAnalyzer::Result learned;
    if (processor.applyLearnResult (learned))
    {
        learnMessage = "LEARNED: " + juce::String (kNoteNames[(size_t) learned.key]) + (learned.minor ? " MINOR" : " MAJOR")
                       + "  //  A4 = " + juce::String (learned.a4, 1) + " HZ"
                       + (learned.confidence < 0.25f ? "  //  UNSURE, CHECK BY EAR" : "");
        learnMessageTicks = 30 * 8;
        repaint();
    }
    else if (learnMessageTicks > 0 && --learnMessageTicks == 0)
    {
        learnMessage = {};
        repaint();
    }

    for (size_t ki : { kShiftKnob, kFormantKnob })
    {
        auto& sl = knobs[ki].slider;
        if (sl.isEnabled() != (mode == highQuality))
        {
            sl.setEnabled (mode == highQuality);
            sl.setAlpha (mode == highQuality ? 1.0f : 0.35f);
        }
    }
    for (auto* b : { &majToMinButton, &minToMajButton })
        b->setAlpha (mode == highQuality ? 1.0f : 0.45f);

    animPhase += 1.0f / 30.0f;
    prism.setPhase (animPhase);

    // scan lines: repaint only the old and new band areas
    const auto a = scanBand (keyPanel, animPhase);
    const auto b = scanBand (knobPanel, animPhase + 3.0f);
    repaint (lastScanA.getUnion (a).getIntersection (keyPanel));
    repaint (lastScanB.getUnion (b).getIntersection (knobPanel));
    lastScanA = a;
    lastScanB = b;

    const bool midi = processor.isMidiControllingKey();
    const int source = midi ? 1 : (processor.isSidechainControllingKey() ? 2 : 0);
    const int scaleIndex = (int) apvts.getRawParameterValue (ParamIDs::scale)->load();
    const int key = (int) apvts.getRawParameterValue (ParamIDs::key)->load();
    const bool chromatic = scaleIndex >= 0 && scaleIndex < (int) kScales.size() && kScales[(size_t) scaleIndex].mask == kAllPitches;
    const int root = (source != 0 || scaleIndex == kCustomScaleIndex || chromatic) ? -1 : key;
    keyStrip.setState (processor.getScaleMask(), root, source); // Tasten = gewählte Tonleiter, Umleitungen zeigt die Matrix
    updateEnergy();
    updateMatrix (mode);
    pitchMatrix.setLevels (shownInput, shownEnergy, animPhase);

    juce::String text = "LATENCY 0 SMP";
    if (mode == highQuality)
    {
        const int latency = processor.getLatencySamples();
        const double sr = processor.getSampleRate();
        text = "LATENCY " + juce::String (latency) + " SMP";
        if (sr > 0.0)
            text << " / " << juce::String (juce::roundToInt (1000.0 * latency / sr)) << " MS";
    }

    if (text != latencyText || source != sourceShown)
    {
        latencyText = text;
        sourceShown = source;
        repaint();
    }

    // Sidechain-Knopf nur aktiv, wenn der Host den Sidechain-Eingang verbunden hat
    {
        const bool connected = processor.isSidechainConnected();
        scButton.setAlpha (connected ? 1.0f : 0.45f);
        static const auto tipOn = de ("SIDECHAIN\nDie Tonart folgt den Noten einer anderen Spur, z. B. Bass oder Keys. Erkannt werden nur Grundtöne, bei Stille bleibt der letzte Akkord stehen.");
        static const auto tipOff = de ("SIDECHAIN\nDie Tonart folgt den Noten einer anderen Spur. Gerade ist kein Sidechain verbunden: route die Spur in der DAW auf den Sidechain-Eingang von Prizm.");
        const auto& tip = connected ? tipOn : tipOff;
        if (scButton.getTooltip() != tip)
            scButton.setTooltip (tip);
    }

    if (processor.getCurrentPreset() != shownPreset)
    {
        shownPreset = processor.getCurrentPreset();
        presetBox.setSelectedItemIndex (shownPreset, juce::dontSendNotification);
    }
}

void PrizmAudioProcessorEditor::drawHoloPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& tag, const juce::String& code)
{
    using namespace juce;

    // projector light pooling under the panel (on the wet surface)
    ColourGradient pool (Col::holo.withAlpha (0.20f), r.getCentreX(), r.getBottom() + 4.0f,
                         Col::holo.withAlpha (0.0f), r.getCentreX() + r.getWidth() * 0.45f, r.getBottom() + 4.0f, true);
    g.setGradientFill (pool);
    g.fillEllipse (r.getX(), r.getBottom() - 10.0f, r.getWidth(), 28.0f);

    // glass
    ColourGradient fill (Colour (0xff0b1015).withAlpha (0.80f), 0.0f, r.getY(), Colour (0xff05080b).withAlpha (0.72f), 0.0f, r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRect (r);

    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());

        // grid
        g.setColour (Col::holo.withAlpha (0.022f));
        for (float x = r.getX() + 20.0f; x < r.getRight(); x += 20.0f)
            g.fillRect (x, r.getY(), 1.0f, r.getHeight());
        for (float y = r.getY() + 20.0f; y < r.getBottom(); y += 20.0f)
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);

        // scanlines
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        for (float y = r.getY(); y < r.getBottom(); y += 3.0f)
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);

        // inner top glow
        ColourGradient topGlow (Col::holo.withAlpha (0.07f), 0.0f, r.getY(), Col::holo.withAlpha (0.0f), 0.0f, r.getY() + 46.0f, false);
        g.setGradientFill (topGlow);
        g.fillRect (r.withHeight (46.0f));
    }

    // frame
    g.setColour (Col::holoDim.withAlpha (0.55f));
    g.drawRect (r, 1.0f);
    ColourGradient edge (Col::holo.withAlpha (0.0f), r.getX(), r.getY(), Col::holo.withAlpha (0.0f), r.getRight(), r.getY(), false);
    edge.addColour (0.5, Col::holoBright);
    g.setGradientFill (edge);
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.5f);
    holo::cornerBrackets (g, r, 16.0f, Col::holoBright, 2.0f);

    // bright projector line along the bottom edge
    ColourGradient base (Col::holo.withAlpha (0.0f), r.getX(), r.getBottom(), Col::holo.withAlpha (0.0f), r.getRight(), r.getBottom(), false);
    base.addColour (0.3, Col::holo);
    base.addColour (0.5, juce::Colours::white.withAlpha (0.9f));
    base.addColour (0.7, Col::holo);
    g.setGradientFill (base);
    g.fillRect (r.getX(), r.getBottom() - 1.5f, r.getWidth(), 2.0f);

    // header tag + status ticks
    g.setFont (mono (12.0f));
    g.setColour (Col::holoBright);
    g.drawText (tag, (int) r.getX() + 20, (int) r.getY() + 9, 300, 14, Justification::centredLeft);
    if (code.isEmpty())
        return;
    g.setColour (Col::dim);
    g.drawText (code, (int) r.getRight() - 20 - 300 - 48, (int) r.getY() + 9, 300, 14, Justification::centredRight);
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (i < 3 ? Col::holo.withAlpha (0.85f) : Col::holoDim);
        g.fillRect (r.getRight() - 20.0f - 40.0f + (float) i * 10.0f, r.getY() + 11.0f, 7.0f, 10.0f);
    }
}

void PrizmAudioProcessorEditor::renderStaticLayer (float scale)
{
    using namespace juce;
    staticLayer = Image (Image::RGB, roundToInt ((float) getWidth() * scale), roundToInt ((float) getHeight() * scale), true);
    Graphics g (staticLayer);
    g.addTransform (AffineTransform::scale (scale));

    // wet tech surface
    if (texture.isValid())
        g.drawImage (texture, getLocalBounds().toFloat(), RectanglePlacement::fillDestination);
    else
        g.fillAll (Col::bg);

    // logo
    if (logo.isValid())
        g.drawImageWithin (logo, logoArea.getX(), logoArea.getY(), logoArea.getWidth(), logoArea.getHeight(),
                           RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize);

    ColourGradient vline (Col::holo.withAlpha (0.0f), 0.0f, 20.0f, Col::holo.withAlpha (0.0f), 0.0f, 84.0f, false);
    vline.addColour (0.5, Col::holo.withAlpha (0.7f));
    g.setGradientFill (vline);
    g.fillRect ((float) logoArea.getRight() + 12.0f, 20.0f, 1.0f, 64.0f);

    // PRIZM as hologram lettering: glow, gradient fill, scan lines, thin rim
    {
        GlyphArrangement ga;
        ga.addLineOfText (titleFont(), "PRIZM", (float) titleArea.getX(), (float) titleArea.getY() + kTitleSize * 0.80f);
        Path text;
        ga.createPath (text);
        g.setColour (Col::holo.withAlpha (0.10f));
        g.strokePath (text, PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Col::holo.withAlpha (0.20f));
        g.strokePath (text, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        const auto tb = text.getBounds();
        ColourGradient fill (juce::Colours::white, 0.0f, tb.getY(), Col::holoBright.withMultipliedSaturation (1.2f), 0.0f, tb.getBottom(), false);
        g.setGradientFill (fill);
        g.fillPath (text);
        {
            Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (text);
            g.setColour (Col::holo.withAlpha (0.28f));
            for (float y = tb.getY(); y < tb.getBottom(); y += 3.0f)
                g.fillRect (tb.getX(), y, tb.getWidth(), 1.0f);
        }
        g.setColour (Col::holo.withAlpha (0.8f));
        g.strokePath (text, PathStrokeType (0.8f));

        const float ulY = tb.getBottom() + 6.0f;
        g.setGradientFill (Col::spectrum (tb.getX(), ulY, tb.getRight(), 0.95f));
        g.fillRoundedRectangle (tb.getX(), ulY, tb.getWidth(), 2.0f, 1.0f);

        g.setFont (mono (13.0f));
        g.setColour (Col::muted);
        g.drawText ("HARMONIC SNAP  //  SCALE SWEETENER", (int) tb.getX(), (int) ulY + 6, 340, 16, Justification::centredLeft);
    }

    // panels
    drawHoloPanel (g, keyPanel.toFloat(), "// KEY + PITCH MATRIX", {});
    drawHoloPanel (g, knobPanel.toFloat(), "// SIGNAL", "SPECTRAL CORE  //  PRZ-02");
    drawHoloPanel (g, extraPanel.toFloat(), "// RANGE + SPECTRAL", {});
    g.setFont (mono (12.0f));
    g.setColour (Col::dim);
    g.drawText ("PRESET", presetBox.getX() - 62, presetBox.getY(), 56, presetBox.getHeight(), Justification::centredRight);
    g.drawText ("PROCESS", stereoBox.getX() - 70, stereoBox.getY(), 64, stereoBox.getHeight(), Justification::centredRight);

    // section labels
    g.setFont (display (12.5f, true).withExtraKerningFactor (0.1f));
    g.setColour (Col::muted);
    g.drawText ("KEY", keyLabelArea, Justification::centredLeft);
    g.drawText ("SCALE", scaleLabelArea, Justification::centredLeft);

    // knob titles + dividers
    g.setFont (display (13.5f, true).withExtraKerningFactor (0.12f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = knobs[i];
        g.setColour (Col::ink.withAlpha (0.85f));
        const auto& panel = i < 5 ? knobPanel : extraPanel;
        g.drawText (k.title, k.slider.getX(), panel.getY() + 30, k.slider.getWidth(), 18, Justification::centred);
        if (i != 0 && i != 5)
        {
            const float x = (float) k.slider.getX() - 6.0f;
            ColourGradient div (Col::holo.withAlpha (0.0f), x, (float) panel.getY() + 30.0f, Col::holo.withAlpha (0.0f), x, (float) panel.getBottom() - 14.0f, false);
            div.addColour (0.5, Col::holo.withAlpha (0.35f));
            g.setGradientFill (div);
            g.fillRect (x, (float) panel.getY() + 30.0f, 1.0f, (float) panel.getHeight() - 44.0f);
        }
    }

    // footer
    g.setFont (mono (11.5f));
    g.setColour (Col::dim);
    g.drawText ("DRAG TO ADJUST  //  DOUBLE-CLICK = DEFAULT", footerArea, Justification::centredLeft);
    g.drawText ("BRAIN BUGZ  //  PRIZM  v0.1", footerArea, Justification::centredRight);
}

void PrizmAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const float scale = jmax (1.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
    if (staticLayer.isNull() || staticLayer.getWidth() != roundToInt ((float) getWidth() * scale))
        renderStaticLayer (scale);
    g.drawImage (staticLayer, getLocalBounds().toFloat());

    // scan sweeps over the hologram panels
    auto drawScan = [&g] (Rectangle<int> panel, Rectangle<int> band)
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (panel.reduced (1));
        const auto b = band.toFloat();
        ColourGradient sweep (Col::holo.withAlpha (0.0f), 0.0f, b.getY(), Col::holo.withAlpha (0.0f), 0.0f, b.getBottom(), false);
        sweep.addColour (0.85, Col::holo.withAlpha (0.07f));
        sweep.addColour (0.97, Col::holoBright.withAlpha (0.16f));
        g.setGradientFill (sweep);
        g.fillRect (b);
    };
    drawScan (keyPanel, scanBand (keyPanel, animPhase));
    drawScan (knobPanel, scanBand (knobPanel, animPhase + 3.0f));

    // latency under the mode switch
    g.setFont (mono (12.0f));
    g.setColour (Col::muted);
    g.drawText (latencyText, modeSwitch.getRight() - 300, modeSwitch.getBottom() + 2, 295, 16, Justification::centredRight);

    // notes-in-key readout
    {
        auto r = readoutArea;
        const int count = BigInteger ((int) processor.getScaleMask()).countNumberOfSetBits();
        g.setFont (display (12.5f, true).withExtraKerningFactor (0.1f));
        g.setColour (Col::muted);
        g.drawText ("IN KEY", r.removeFromTop (16), Justification::centredRight);
        const auto box = r.withTrimmedTop (2).removeFromRight (62).withHeight (34).toFloat();
        holo::box (g, box);
        g.setFont (mono (18.0f));
        g.setColour (sourceShown == 1 ? Col::pink : (sourceShown == 2 ? Col::cyan : Col::ink));
        g.drawText (String (count) + "/12", box, Justification::centred);
    }

    // Legende der Pitch Matrix
    {
        const bool hqMode = (int) processor.apvts.getRawParameterValue (ParamIDs::mode)->load() == highQuality;
        auto r = legendArea.toFloat();
        g.setFont (mono (11.0f));
        auto item = [&] (Colour c, const String& text, int style)
        {
            const float y = r.getCentreY();
            if (style == 0)
            {
                g.setColour (c);
                g.fillRoundedRectangle (r.getX(), y - 1.0f, 18.0f, 2.0f, 1.0f);
            }
            else if (style == 1)
            {
                Path p;
                p.startNewSubPath (r.getX(), y + 4.0f);
                p.cubicTo (r.getX() + 8.0f, y + 4.0f, r.getX() + 10.0f, y - 4.0f, r.getX() + 18.0f, y - 4.0f);
                g.setColour (c);
                g.strokePath (p, PathStrokeType (2.0f));
            }
            else
            {
                g.setColour (c);
                g.drawLine (r.getX() + 5.0f, y - 4.0f, r.getX() + 13.0f, y + 4.0f, 1.5f);
                g.drawLine (r.getX() + 5.0f, y + 4.0f, r.getX() + 13.0f, y - 4.0f, 1.5f);
            }
            g.setColour (Col::muted);
            const float w = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text);
            g.drawText (text, r.withLeft (r.getX() + 24.0f).withWidth (w + 4.0f), Justification::centredLeft);
            r.removeFromLeft (24.0f + w + 16.0f);
        };
        item (Col::tox, "STAYS", 0);
        item (Col::cyan, "SHIFT", 1);
        item (Col::pink, hqMode ? "MAP" : "MAP (HQ)", 1);
        item (Col::muted, "FILTERED / OFF", 2);
    }

    // IN / OUT an den Rand der Matrix
    {
        Graphics::ScopedSaveState s (g);
        g.setFont (mono (9.5f));
        g.setColour (Col::dim);
        auto vertical = [&g] (const String& text, float cx, float cy)
        {
            Graphics::ScopedSaveState inner (g);
            g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, cx, cy));
            g.drawText (text, Rectangle<float> (cx - 30.0f, cy - 6.0f, 60.0f, 12.0f), Justification::centred);
        };
        vertical ("IN", (float) keyStrip.getX() + 3.0f, (float) keyStrip.getBounds().getCentreY());
        vertical ("OUT", (float) pitchMatrix.getX() + 3.0f, (float) pitchMatrix.getBottom() - 15.0f);
    }

    g.setFont (display (12.5f, true).withExtraKerningFactor (0.1f));
    if (learnMessage.isNotEmpty())
    {
        g.setColour (Col::tox);
        g.drawText (learnMessage, stripLabelArea, Justification::centredLeft);
    }
    else
    {
        g.setColour (sourceShown == 1 ? Col::pink : (sourceShown == 2 ? Col::cyan : Col::muted));
        g.drawText (sourceShown == 1 ? "KEY SET BY MIDI  -  LAST CHORD"
                    : sourceShown == 2 ? "KEY SET BY SIDECHAIN  -  NOTES OF THE OTHER TRACK"
                                       : "NOTES IN  -  CLICK = ON/OFF, DRAG UP/DOWN = WEIGHT  //  PITCH MATRIX: DRAG A BEAM TO RE-ROUTE",
                    stripLabelArea, Justification::centredLeft);
    }
}

void PrizmAudioProcessorEditor::resized()
{
    staticLayer = {};

    logoArea = { 14, 8, 182, 84 };
    titleArea = { logoArea.getRight() + 26, 12, 240, 60 };

    const int titleW = juce::GlyphArrangement::getStringWidthInt (titleFont(), "PRIZM");
    prism.setBounds (titleArea.getX() + titleW + 4, 14, 76, 52);

    modeSwitch.setBounds (getWidth() - 24 - 300, 18, 300, 48);

    keyPanel = { 20, 104, getWidth() - 40, 268 };
    const int rowY = keyPanel.getY() + 34;
    keyLabelArea = { keyPanel.getX() + 20, rowY, 100, 16 };
    keyBox.setBounds (keyPanel.getX() + 20, rowY + 18, 100, 34);
    scaleLabelArea = { keyPanel.getX() + 132, rowY, 200, 16 };
    scaleBox.setBounds (keyPanel.getX() + 132, rowY + 18, 200, 34);
    midiButton.setBounds (keyPanel.getX() + 344, rowY + 18, 104, 34);
    scButton.setBounds (keyPanel.getX() + 456, rowY + 18, 104, 34);
    learnButton.setBounds (keyPanel.getX() + 568, rowY + 18, 104, 34);
    presetBox.setBounds (keyPanel.getRight() - 20 - 64 - 6 - 220, keyPanel.getY() + 6, 220, 24);
    savePresetButton.setBounds (keyPanel.getRight() - 20 - 64, keyPanel.getY() + 6, 64, 24);
    readoutArea = { keyPanel.getX() + 680, rowY, keyPanel.getRight() - 20 - (keyPanel.getX() + 680), 52 };

    stripLabelArea = { keyPanel.getX() + 20, keyPanel.getY() + 94, keyPanel.getWidth() - 40, 16 };
    keyStrip.setBounds (keyPanel.getX() + 10, keyPanel.getY() + 110, keyPanel.getWidth() - 20, 70);

    // Pitch Matrix direkt darunter, gleiche Spalten wie die Tasten
    pitchMatrix.setBounds (keyStrip.getX(), keyStrip.getBottom(), keyStrip.getWidth(), 112);

    {
        const int legendY = pitchMatrix.getBottom() + 6;
        legendArea = { keyPanel.getX() + 20, legendY, 440, 22 };
        auto btnRow = juce::Rectangle<int> (keyPanel.getRight() - 20 - 3 * 100 - 2 * 8, legendY, 3 * 100 + 2 * 8, 22);
        majToMinButton.setBounds (btnRow.removeFromLeft (100));
        btnRow.removeFromLeft (8);
        minToMajButton.setBounds (btnRow.removeFromLeft (100));
        btnRow.removeFromLeft (8);
        resetMapButton.setBounds (btnRow);
        keyPanel.setHeight (legendY + 22 + 12 - keyPanel.getY());
    }

    knobPanel = { 20, keyPanel.getBottom() + 12, getWidth() - 40, 186 };
    auto knobArea = knobPanel.reduced (14, 0);
    knobArea.removeFromTop (50);
    knobArea.removeFromBottom (10);
    const int w = knobArea.getWidth() / 5;
    for (size_t i = 0; i < 5; ++i)
        knobs[i].slider.setBounds (knobArea.removeFromLeft (w).reduced (12, 0));

    extraPanel = { 20, knobPanel.getBottom() + 12, getWidth() - 40, 150 };
    stereoBox.setBounds (extraPanel.getRight() - 20 - 150, extraPanel.getY() + 6, 150, 24);
    auto extraArea = extraPanel.reduced (14, 0);
    extraArea.removeFromTop (48);
    extraArea.removeFromBottom (8);
    const int w2 = extraArea.getWidth() / 6;
    for (size_t i = 5; i < 11; ++i)
        knobs[i].slider.setBounds (extraArea.removeFromLeft (w2).reduced (8, 0));

    footerArea = { 24, getHeight() - 30, getWidth() - 48, 20 };
}
