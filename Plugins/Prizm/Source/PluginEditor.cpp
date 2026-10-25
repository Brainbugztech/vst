#include "PluginEditor.h"
#include "PrizmAssets.h"

#include <cmath>

using namespace prizm;

namespace
{
    constexpr int kWidth = 800;
    constexpr int kTextureHeight = 1120; // Hintergrund wird immer in dieser Größe gezeichnet (oben verankert)
    constexpr int kTop = 104;            // erster Bereich unter Logo und Titel
    constexpr int kGap = 12;
    constexpr int kCollapsedHeight = 34; // eingeklappt: nur die Kopfzeile
    constexpr int kBodyTop = 30;         // Inhalt beginnt 30 px unter der Oberkante des Panels
    constexpr int kFooterSpace = 44;     // Fußzeile unter dem letzten Bereich
    constexpr int kPoolSpace = 20;       // Lichtschein unter einem Panel
    constexpr int kKeyHeight = 332, kSignalHeight = 186, kRangeHeight = 150, kModHeight = 228;
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
void ModKnob::itemDragEnter (const SourceDetails& d)
{
    const int src = d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
    getProperties().set ("dropHover", src);
    getProperties().set ("dropCol", (juce::int64) modSourceColour (src).getARGB());
    repaint();
}

void ModKnob::itemDragExit (const SourceDetails&)
{
    getProperties().set ("dropHover", 0);
    repaint();
}

void ModKnob::itemDropped (const SourceDetails& d)
{
    getProperties().set ("dropHover", 0);
    repaint();
    if (onDropSource)
        onDropSource (d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue());
}

void ModKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onRemoveMenu)
    {
        onRemoveMenu();
        return;
    }
    juce::Slider::mouseDown (e);
}

//==============================================================================
void HoloCurtain::paint (juce::Graphics& g)
{
    using namespace juce;
    const float w = (float) getWidth(), h = (float) getHeight();
    const float y = edge * h;
    juce::Random rng ((int64) seed * 7919 + 17);

    // Projektion unter der Kante: Raster, das nach unten ausläuft
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (Rectangle<int> (0, (int) y, getWidth(), 48));
        ColourGradient fade (Col::holo.withAlpha (0.14f), 0.0f, y, Col::holo.withAlpha (0.0f), 0.0f, y + 48.0f, false);
        g.setGradientFill (fade);
        g.fillRect (0.0f, y, w, 48.0f);
        g.setColour (Col::holo.withAlpha (0.10f));
        for (float x = 20.0f; x < w; x += 20.0f)
            g.fillRect (x, y, 1.0f, 48.0f);
    }

    // Glitch: versetzte Streifen knapp über der Kante
    for (int i = 0; i < 7; ++i)
    {
        const float by = y - 4.0f - rng.nextFloat() * 46.0f;
        const float bh = 1.0f + rng.nextFloat() * 2.5f;
        const float bx = rng.nextFloat() * w * 0.85f;
        const float bw = 30.0f + rng.nextFloat() * 180.0f;
        const auto c = i % 3 == 0 ? Col::pink : (i % 3 == 1 ? Col::cyan : Col::holoBright);
        g.setColour (c.withAlpha (0.15f + rng.nextFloat() * 0.35f));
        g.fillRect (bx, by, bw, bh);
        if (i % 2 == 0) // Aussetzer: dunkler Streifen über dem Inhalt
        {
            g.setColour (Col::bg.withAlpha (0.55f));
            g.fillRect (rng.nextFloat() * w * 0.8f, by + 3.0f, 40.0f + rng.nextFloat() * 120.0f, 2.0f);
        }
    }

    // Scan-Kante mit Farbversatz
    ColourGradient glow (Col::holo.withAlpha (0.0f), 0.0f, y - 18.0f, Col::holo.withAlpha (0.30f), 0.0f, y, false);
    g.setGradientFill (glow);
    g.fillRect (0.0f, y - 18.0f, w, 18.0f);
    const float jitter = (rng.nextFloat() - 0.5f) * 4.0f;
    g.setColour (Col::pink.withAlpha (0.55f));
    g.fillRect (3.0f + jitter, y - 3.0f, w - 6.0f, 1.0f);
    g.setColour (Col::cyan.withAlpha (0.55f));
    g.fillRect (-3.0f - jitter, y + 2.0f, w, 1.0f);
    ColourGradient line (Col::holo.withAlpha (0.0f), 0.0f, y, Col::holo.withAlpha (0.0f), w, y, false);
    line.addColour (0.15, Col::holoBright);
    line.addColour (0.5, juce::Colours::white);
    line.addColour (0.85, Col::holoBright);
    g.setGradientFill (line);
    g.fillRect (0.0f, y - 0.75f, w, 1.8f);
}

//==============================================================================
PrizmAudioProcessorEditor::PrizmAudioProcessorEditor (PrizmAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), modPanel (p)
{
    setLookAndFeel (&lnf);
    auto& apvts = processor.apvts;

    logo = juce::ImageCache::getFromMemory (BBAssets::logo_text_png, BBAssets::logo_text_pngSize);
    texture = juce::ImageCache::getFromMemory (PrizmAssets::prizm_backdrop_jpg, PrizmAssets::prizm_backdrop_jpgSize);

    // Bereiche: Reihenfolge = Reihenfolge im Fenster
    {
        const std::array<std::tuple<const char*, const char*, const char*, int, bool>, kNumSections> defs {{
            { "key",    "// KEY + PITCH MATRIX", "",                          kKeyHeight,    true },
            { "signal", "// SIGNAL",             "SPECTRAL CORE  //  PRZ-02", kSignalHeight, true },
            { "range",  "// RANGE + SPECTRAL",   "",                          kRangeHeight,  false },
            { "mod",    "// MODULATION",         "",                          kModHeight,    false },
        }};
        for (size_t i = 0; i < sections.size(); ++i)
        {
            auto& s = sections[i];
            s.id = std::get<0> (defs[i]);
            s.tag = std::get<1> (defs[i]);
            s.code = std::get<2> (defs[i]);
            s.fullHeight = std::get<3> (defs[i]);
            s.open = (bool) apvts.state.getProperty ("open_" + s.id, std::get<4> (defs[i]));
            addChildComponent (s.body);
        }
        sections[secKey].body.onPaint = [this] (juce::Graphics& g) { paintKeyBody (g); };
        sections[secSignal].body.onPaint = [this] (juce::Graphics& g) { paintKnobTitles (g, 0, 5, 0); };
        sections[secRange].body.onPaint = [this] (juce::Graphics& g) { paintKnobTitles (g, 5, 11, 0); };
    }
    auto& keyBody = sections[secKey].body;

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
    keyBody.addAndMakeVisible (keyBox);
    keyBody.addAndMakeVisible (scaleBox);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::key, keyBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::scale, scaleBox);

    midiButton.setClickingTogglesState (true);
    midiButton.setTooltip (de ("MIDI KEY\nSpiel Noten auf den MIDI-Eingang von Prizm: der zuletzt gespielte Akkord wird zur Tonart. So folgt Prizm deinen Akkordwechseln."));
    keyBody.addAndMakeVisible (midiButton);
    midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamIDs::midiKey, midiButton);

    learnButton.setTooltip (de ("LEARN\nKlicken und Material abspielen: Prizm hört 8 Sekunden zu und stellt KEY, SCALE und TUNE selbst ein. Nochmal klicken bricht ab."));
    learnButton.onClick = [this]
    {
        if (processor.isLearning())
            processor.cancelLearn();
        else
            processor.startLearn();
    };
    keyBody.addAndMakeVisible (learnButton);

    // Pitch Matrix: zeigt, wo jede Note landet, und ist gleichzeitig die Bedienung der Noten-Map
    pitchMatrix.onSetMap = [this] (int pc, int index) { processor.setMap (pc, index); updateMatrix ((int) processor.apvts.getRawParameterValue (ParamIDs::mode)->load()); };
    pitchMatrix.onResetAll = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::reset); };
    keyBody.addAndMakeVisible (pitchMatrix);

    majToMinButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::majorToMinor); };
    minToMajButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::minorToMajor); };
    resetMapButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::reset); };
    majToMinButton.setTooltip (de ("MAJ > MIN\nMacht aus Dur Moll, bezogen auf KEY: Terz, Sexte und Septime wandern einen Halbton tiefer. Braucht High Quality."));
    minToMajButton.setTooltip (de ("MIN > MAJ\nMacht aus Moll Dur, bezogen auf KEY: Terz, Sexte und Septime wandern einen Halbton höher. Braucht High Quality."));
    resetMapButton.setTooltip (de ("RESET MAP\nAlle Umleitungen und OFF-Noten zurück auf normal."));
    for (auto* b : { &majToMinButton, &minToMajButton, &resetMapButton })
        keyBody.addAndMakeVisible (*b);

    // Sidechain-Tonart
    scButton.setClickingTogglesState (true);
    scButton.setTooltip (de ("SIDECHAIN\nDie Tonart folgt den Noten einer anderen Spur, z. B. Bass oder Keys. Route die Spur in der DAW auf den Sidechain-Eingang von Prizm."));
    keyBody.addAndMakeVisible (scButton);
    scAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamIDs::scKey, scButton);

    // Presets (in der Kopfzeile des Key-Bereichs, bleiben auch eingeklappt erreichbar)
    presetBox.setTooltip (de ("PRESET\nStartpunkte für typische Aufgaben und deine eigenen Presets, nach Gruppen sortiert. KEY, SCALE und TUNE bleiben beim Durchblättern stehen. Unten erscheint kurz, wofür das Preset gedacht ist."));
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx < 0)
            return;
        processor.loadPreset (idx); // gleiches Preset nochmal = zurücksetzen
        shownPreset = processor.getCurrentPreset();
        const auto hint = processor.getPresetHint (idx);
        if (hint.isNotEmpty())
            showFooterMessage (presetBox.getText().toUpperCase() + ":  " + hint, 8);
    };
    addAndMakeVisible (presetBox);
    savePresetButton.setTooltip (de ("SAVE\nAktuelle Einstellungen als eigenes Preset speichern (Dokumente/Brain Bugz/Prizm/Presets). Gezeichnete LFO-Kurven werden mitgespeichert."));
    savePresetButton.onClick = [this] { askPresetName(); };
    addAndMakeVisible (savePresetButton);
    refreshPresetBox();

    // Mitte/Seite (Kopfzeile des Range-Bereichs)
    stereoBox.addItem ("STEREO", 1);
    stereoBox.addItem ("MID ONLY", 2);
    stereoBox.addItem ("SIDE ONLY", 3);
    stereoBox.setTooltip (de ("PROCESS\nStereo: beide Kanäle. Mid only: nur die Mitte. Side only: nur die Stereo-Seiten, z. B. um nur die Breite eines Pads zu stimmen. Der Rest bleibt unverändert."));
    addAndMakeVisible (stereoBox);
    stereoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::stereo, stereoBox);

    keyStrip.onToggle = [this] (int pc) { processor.togglePitchClass (pc); };
    keyStrip.onWeight = [this] (int pc, float w, bool start, bool end) { processor.setWeight (pc, w, start, end); keyStrip.repaint(); };
    keyStrip.getWeight = [this] (int pc) { return processor.getWeight (pc); };
    keyBody.addAndMakeVisible (keyStrip);

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
        k.slider.setBufferedToImage (true); // Regler sind teuer zu zeichnen, beim Auf-/Zuklappen werden sie nur verschoben
        sections[i < 5 ? secSignal : secRange].body.addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, defs[i].id, k.slider);
        if (juce::String (defs[i].id) == ParamIDs::color)
            k.slider.getProperties().set ("overdrive", true); // über 100 % pink
        if (auto* param = apvts.getParameter (defs[i].id))
            k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        // Modulation: Quelle draufziehen, Rechtsklick entfernt
        const int target = (int) i;
        k.slider.target = target;
        k.slider.onDropSource = [this, target] (int src)
        {
            const int slot = processor.assignModulation (src, target);
            if (slot < 0)
                showFooterMessage (de ("ALLE 8 MODULATIONS-SLOTS BELEGT. ENTFERNE ERST EINE ZUWEISUNG (× IN DER ROUTING-LISTE)."), 6);
            else
                showFooterMessage (modSourceName (src) + juce::String::fromUTF8 (" → ") + knobs[(size_t) target].title
                                       + de (":  TIEFE IN DER ROUTING-LISTE ZIEHEN, RECHTSKLICK AM REGLER ENTFERNT."), 6);
            updateModRings();
        };
        k.slider.modInfo = [this, target] { return modInfoFor (target); };
        k.slider.onRemoveMenu = [this, target] { showKnobModMenu (target); };
    }

    sections[secMod].body.addAndMakeVisible (modPanel);
    for (auto& s : sections)
        addChildComponent (s.curtain); // über allem anderen

    // INFO-Knopf: Erklärungen beim Hovern an/aus, gespeichert für alle Instanzen
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Prizm";
        o.folderName = "Brain Bugz";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    infoButton.setClickingTogglesState (true);
    infoButton.onClick = [this] { setInfoEnabled (infoButton.getToggleState(), true); };
    infoButton.setTooltip (de ("INFO\nErklärungen beim Hovern an/aus. Die Einstellung gilt für alle Prizm-Instanzen."));
    addAndMakeVisible (infoButton);
    setInfoEnabled (settings->getBoolValue ("showInfo", true), false);

    processor.setEditorOpen (true);
    setSize (kWidth, targetEditorHeight());
    // Passt das gespeicherte Layout nicht auf den Bildschirm, klappen wir von unten her zu
    for (int i = kNumSections - 1; i > 0 && targetEditorHeight() > availableHeight(); --i)
        if (sections[(size_t) i].open)
            setSectionOpen (i, false, false);
    setSize (kWidth, targetEditorHeight());
    setTimerRate (30);
    timerCallback();
}

PrizmAudioProcessorEditor::~PrizmAudioProcessorEditor()
{
    processor.setEditorOpen (false);
    stopTimer();
    setLookAndFeel (nullptr);
}

void PrizmAudioProcessorEditor::setTimerRate (int hz)
{
    if (hz != timerRate)
    {
        timerRate = hz;
        startTimerHz (hz);
    }
}

void PrizmAudioProcessorEditor::setInfoEnabled (bool shouldShow, bool remember)
{
    // Erklärungen erscheinen erst nach 1,2 s Stillstand der Maus, damit sie beim Arbeiten nicht stören
    if (shouldShow && tooltips == nullptr)
        tooltips = std::make_unique<juce::TooltipWindow> (this, 1200);
    else if (! shouldShow)
        tooltips.reset();

    infoButton.setToggleState (shouldShow, juce::dontSendNotification);
    infoButton.setButtonText (shouldShow ? "INFO: ON" : "INFO: OFF");
    if (remember && settings != nullptr)
    {
        settings->setValue ("showInfo", shouldShow);
        settings->saveIfNeeded();
    }
}

void PrizmAudioProcessorEditor::refreshPresetBox()
{
    processor.refreshUserPresets();
    presetBox.clear (juce::dontSendNotification);
    const auto names = processor.getPresetNames();
    const auto& factory = factoryPresets();
    const char* group = nullptr;
    for (int i = 0; i < names.size(); ++i)
    {
        if (i < (int) factory.size())
        {
            if (group == nullptr || juce::String (group) != factory[(size_t) i].group)
            {
                group = factory[(size_t) i].group;
                presetBox.addSectionHeading (group);
            }
        }
        else if (i == (int) factory.size())
            presetBox.addSectionHeading ("USER");
        presetBox.addItem (names[i], i + 1);
    }
    shownPreset = processor.getCurrentPreset();
    presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
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

void PrizmAudioProcessorEditor::showFooterMessage (const juce::String& text, int seconds)
{
    footerMessage = text;
    footerMessageTicks = seconds * 1000;
    repaint (footerArea);
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

//==============================================================================
// Modulation an den Reglern

void PrizmAudioProcessorEditor::updateModRings()
{
    auto& apvts = processor.apvts;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& sl = knobs[i].slider;
        float lo = 0.0f, hi = 0.0f;
        bool has = false;
        int firstSrc = srcOff;
        bool mixed = false;
        for (int s = 0; s < kNumModSlots; ++s)
        {
            const int src = (int) apvts.getRawParameterValue (ParamIDs::modSrc (s))->load();
            if (src == srcOff || (int) apvts.getRawParameterValue (ParamIDs::modDst (s))->load() != (int) i)
                continue;
            const float a = apvts.getRawParameterValue (ParamIDs::modAmt (s))->load() * 0.01f;
            has = true;
            if (firstSrc == srcOff)
                firstSrc = src;
            else if (firstSrc != src)
                mixed = true;
            if (src == srcEnv) // ENV ist 0..1, LFOs schwingen um den Wert
            {
                lo += juce::jmin (0.0f, a);
                hi += juce::jmax (0.0f, a);
            }
            else
            {
                lo -= std::abs (a);
                hi += std::abs (a);
            }
        }
        auto& props = sl.getProperties();
        const bool had = (bool) props.getWithDefault ("hasMod", false);
        if (! has)
        {
            if (had)
            {
                props.set ("hasMod", false);
                sl.repaint();
            }
            continue;
        }
        float base = 0.0f;
        if (auto* p = apvts.getParameter (kModTargetIds[i]))
            base = p->getValue();
        const float newLo = juce::jlimit (0.0f, 1.0f, base + lo);
        const float newHi = juce::jlimit (0.0f, 1.0f, base + hi);
        const float now = idleTicks <= 6 ? processor.getModNow ((int) i) : base;
        const auto col = mixed ? Col::ink : modSourceColour (firstSrc);
        if (! had || std::abs ((float) props["modLo"] - newLo) > 1.0e-4f || std::abs ((float) props["modHi"] - newHi) > 1.0e-4f
            || std::abs ((float) props["modNow"] - now) > 1.0e-4f || (juce::int64) props["modCol"] != (juce::int64) col.getARGB())
        {
            props.set ("hasMod", true);
            props.set ("modLo", newLo);
            props.set ("modHi", newHi);
            props.set ("modNow", now);
            props.set ("modCol", (juce::int64) col.getARGB());
            sl.repaint();
        }
    }
}

juce::String PrizmAudioProcessorEditor::modInfoFor (int target) const
{
    auto& apvts = processor.apvts;
    juce::StringArray parts;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const int src = (int) apvts.getRawParameterValue (ParamIDs::modSrc (s))->load();
        if (src == srcOff || (int) apvts.getRawParameterValue (ParamIDs::modDst (s))->load() != target)
            continue;
        const int amt = juce::roundToInt (apvts.getRawParameterValue (ParamIDs::modAmt (s))->load());
        parts.add (modSourceName (src) + " " + (amt > 0 ? "+" : "") + juce::String (amt) + " %");
    }
    if (parts.isEmpty())
        return de ("\nModulieren: LFO 1, LFO 2 oder ENV aus dem Bereich MODULATION auf diesen Regler ziehen.");
    return de ("\nMODULIERT VON: ") + parts.joinIntoString (", ") + de (". Rechtsklick: Modulation entfernen.");
}

void PrizmAudioProcessorEditor::showKnobModMenu (int target)
{
    auto& apvts = processor.apvts;
    juce::PopupMenu menu;
    menu.addSectionHeader (knobs[(size_t) target].title);
    int count = 0;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const int src = (int) apvts.getRawParameterValue (ParamIDs::modSrc (s))->load();
        if (src == srcOff || (int) apvts.getRawParameterValue (ParamIDs::modDst (s))->load() != target)
            continue;
        menu.addItem (1 + s, modSourceName (src) + de (" entfernen"));
        ++count;
    }
    if (count == 0)
    {
        menu.addItem (100, de ("Keine Modulation. Zum Zuweisen LFO 1, LFO 2 oder ENV hierher ziehen."), false);
        if (! sections[secMod].open)
            menu.addItem (101, de ("Bereich MODULATION aufklappen"));
    }
    else if (count > 1)
        menu.addItem (99, de ("Alle entfernen"));

    juce::Component::SafePointer<PrizmAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&knobs[(size_t) target].slider), [safe, target] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        if (result == 101)
        {
            safe->setSectionOpen (secMod, true, true);
            return;
        }
        auto& ap = safe->processor.apvts;
        for (int s = 0; s < kNumModSlots; ++s)
        {
            const bool match = (int) ap.getRawParameterValue (ParamIDs::modSrc (s))->load() != srcOff
                               && (int) ap.getRawParameterValue (ParamIDs::modDst (s))->load() == target;
            if (match && (result == 99 || result == 1 + s))
                safe->processor.clearModSlot (s);
        }
        safe->updateModRings();
    });
}

void PrizmAudioProcessorEditor::dragOperationStarted (const juce::DragAndDropTarget::SourceDetails& d)
{
    if (! d.description.toString().startsWith ("mod:"))
        return;
    for (auto& k : knobs)
    {
        k.slider.getProperties().set ("dropReady", true);
        k.slider.repaint();
    }
}

void PrizmAudioProcessorEditor::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
{
    for (auto& k : knobs)
    {
        k.slider.getProperties().set ("dropReady", false);
        k.slider.getProperties().set ("dropHover", 0);
        k.slider.repaint();
    }
}

//==============================================================================
// Aufklappbare Bereiche

namespace
{
    float smooth (float t) { t = juce::jlimit (0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); }
    constexpr float kGrowTime = 0.18f, kRevealTime = 0.35f, kCoverTime = 0.22f, kShrinkTime = 0.18f;
}

int PrizmAudioProcessorEditor::panelHeight (const Section& s) const
{
    switch (s.phase)
    {
        case Section::growing:   return juce::roundToInt (juce::jmap (smooth (s.t), (float) kCollapsedHeight, (float) s.fullHeight));
        case Section::shrinking: return juce::roundToInt (juce::jmap (smooth (s.t), (float) s.fullHeight, (float) kCollapsedHeight));
        case Section::revealing:
        case Section::covering:  return s.fullHeight;
        case Section::idle:
        default:                 return s.open ? s.fullHeight : kCollapsedHeight;
    }
}

int PrizmAudioProcessorEditor::targetEditorHeight() const
{
    int h = kTop;
    for (auto& s : sections)
        h += (s.open ? s.fullHeight : kCollapsedHeight) + kGap;
    return h - kGap + kFooterSpace;
}

int PrizmAudioProcessorEditor::availableHeight() const
{
    // Platz auf dem Bildschirm, abzüglich Titelleiste des Plugin-Fensters
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* d = isShowing() ? displays.getDisplayForRect (getScreenBounds()) : displays.getPrimaryDisplay();
    return d != nullptr ? d->userArea.getHeight() - 60 : 100000;
}

bool PrizmAudioProcessorEditor::anyAnimating() const
{
    for (auto& s : sections)
        if (s.phase != Section::idle)
            return true;
    return false;
}

void PrizmAudioProcessorEditor::setSectionOpen (int index, bool open, bool animate)
{
    auto& s = sections[(size_t) index];
    if (s.open == open)
        return;
    s.open = open;
    processor.apvts.state.setProperty ("open_" + s.id, open, nullptr);

    if (! animate)
        s.phase = Section::idle;
    else if (open)
    {
        if (s.phase == Section::covering)       { s.phase = Section::revealing; s.t = 1.0f - s.t; }
        else if (s.phase == Section::shrinking) { s.phase = Section::growing;   s.t = 1.0f - s.t; }
        else                                    { s.phase = Section::growing;   s.t = 0.0f; }
    }
    else
    {
        if (s.phase == Section::revealing)      { s.phase = Section::covering;  s.t = 1.0f - s.t; }
        else if (s.phase == Section::growing)   { s.phase = Section::shrinking; s.t = 1.0f - s.t; }
        else                                    { s.phase = Section::covering;  s.t = 0.0f; }
    }

    if (open)
    {
        // Passt es nicht mehr auf den Bildschirm, klappen andere Bereiche (von unten her) sofort zu
        juce::StringArray closed;
        for (int i = kNumSections - 1; i >= 0 && targetEditorHeight() > availableHeight(); --i)
            if (i != index && sections[(size_t) i].open)
            {
                setSectionOpen (i, false, false);
                closed.add (sections[(size_t) i].tag.fromFirstOccurrenceOf ("// ", false, false));
            }
        if (! closed.isEmpty())
            showFooterMessage (de ("PLATZ KNAPP AUF DEM BILDSCHIRM: ") + closed.joinIntoString (", ") + de (" EINGEKLAPPT."), 5);
    }

    // Fenster wächst sofort, schrumpft erst, wenn die Animation durch ist
    const int target = targetEditorHeight();
    if (target > getHeight() || (! anyAnimating() && target != getHeight()))
        setSize (getWidth(), target);
    layoutSections();
    if (anyAnimating())
        setTimerRate (60);
    repaint();
}

void PrizmAudioProcessorEditor::advanceSections (float dt)
{
    if (! anyAnimating())
        return;
    ++animFrame;
    for (auto& s : sections)
    {
        if (s.phase == Section::idle)
            continue;
        const float dur = s.phase == Section::growing ? kGrowTime
                        : s.phase == Section::revealing ? kRevealTime
                        : s.phase == Section::covering ? kCoverTime : kShrinkTime;
        s.t += dt / dur;
        if (s.t >= 1.0f)
        {
            s.t = 0.0f;
            if (s.phase == Section::growing)
                s.phase = Section::revealing;
            else if (s.phase == Section::covering)
                s.phase = Section::shrinking;
            else
                s.phase = Section::idle;
        }
    }
    if (! anyAnimating())
    {
        setTimerRate (30);
        const int target = targetEditorHeight();
        if (target != getHeight())
            setSize (getWidth(), target); // ruft layoutSections über resized()
    }
    layoutSections();
    // alles ab dem obersten animierten Bereich neu zeichnen (darunter verschiebt sich alles)
    for (auto& s : sections)
        if (s.phase != Section::idle || s.curtain.isVisible())
        {
            repaint (getLocalBounds().withTop (s.bounds.getY() - 2));
            break;
        }
    repaint (footerArea.expanded (0, 40));
}

void PrizmAudioProcessorEditor::layoutSections()
{
    int y = kTop;
    const int w = getWidth() - 40;
    for (auto& s : sections)
    {
        const int h = panelHeight (s);
        s.bounds = { 20, y, w, h };
        const auto bodyFull = juce::Rectangle<int> (20, y + kBodyTop, w, s.fullHeight - kBodyTop);

        float reveal = 0.0f;
        if (s.phase == Section::idle)
            reveal = s.open ? 1.0f : 0.0f;
        else if (s.phase == Section::revealing)
            reveal = smooth (s.t);
        else if (s.phase == Section::covering)
            reveal = 1.0f - smooth (s.t);

        const bool show = reveal > 0.0f;
        if (show)
            s.body.setBounds (bodyFull.withHeight (juce::jmax (1, juce::roundToInt (reveal * (float) bodyFull.getHeight()))));
        if (s.body.isVisible() != show)
            s.body.setVisible (show);

        const bool curtain = s.phase == Section::revealing || s.phase == Section::covering;
        if (curtain)
        {
            s.curtain.setBounds (bodyFull);
            s.curtain.setEdge (reveal, animFrame);
            // Holo-Flackern beim Materialisieren
            const float flicker = reveal < 0.85f ? 0.55f + 0.45f * reveal + (juce::Random ((juce::int64) animFrame * 31 + y).nextFloat() - 0.5f) * 0.25f : 1.0f;
            s.body.setAlpha (juce::jlimit (0.3f, 1.0f, flicker));
        }
        else if (s.body.getAlpha() < 1.0f)
            s.body.setAlpha (1.0f);
        if (s.curtain.isVisible() != curtain)
            s.curtain.setVisible (curtain);

        y += h + kGap;
    }

    // Kopfzeilen-Bedienung
    const auto& keyB = sections[secKey].bounds;
    presetBox.setBounds (keyB.getRight() - 20 - 64 - 6 - 220, keyB.getY() + 5, 220, 24);
    savePresetButton.setBounds (keyB.getRight() - 20 - 64, keyB.getY() + 5, 64, 24);
    const auto& rangeB = sections[secRange].bounds;
    stereoBox.setBounds (rangeB.getRight() - 20 - 150, rangeB.getY() + 5, 150, 24);

    footerArea = { 24, y - kGap + 12, getWidth() - 48, 20 };
    infoButton.setBounds (footerArea.getX(), footerArea.getY(), 84, footerArea.getHeight());
}

int PrizmAudioProcessorEditor::headerAt (juce::Point<int> p) const
{
    for (int i = 0; i < kNumSections; ++i)
    {
        const auto& b = sections[(size_t) i].bounds;
        if (b.withHeight (kBodyTop).contains (p))
            return i;
    }
    return -1;
}

void PrizmAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const int h = headerAt (e.getPosition());
    if (h >= 0)
        setSectionOpen (h, ! sections[(size_t) h].open, true);
}

void PrizmAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const int h = headerAt (e.getPosition());
    if (h != hoverHeader)
    {
        if (hoverHeader >= 0)
            repaint (sections[(size_t) hoverHeader].bounds.withHeight (kBodyTop + 4));
        hoverHeader = h;
        if (h >= 0)
            repaint (sections[(size_t) h].bounds.withHeight (kBodyTop + 4));
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
}

juce::String PrizmAudioProcessorEditor::sectionSummary (int index) const
{
    auto& apvts = processor.apvts;
    auto val = [&apvts] (const char* id) { return apvts.getParameter (id)->getCurrentValueAsText().toUpperCase(); };
    const juce::String sep ("  //  ");
    switch (index)
    {
        case secKey:
        {
            const int count = juce::BigInteger ((int) processor.getScaleMask()).countNumberOfSetBits();
            const juce::String src = sourceShown == 1 ? "MIDI KEY" : (sourceShown == 2 ? "SIDECHAIN" : val (ParamIDs::key) + " " + val (ParamIDs::scale));
            return src + sep + juce::String (count) + "/12";
        }
        case secSignal:
            return "STR " + val (ParamIDs::strength) + sep + "COL " + val (ParamIDs::color) + sep + "CLR " + val (ParamIDs::clear)
                   + sep + "MIX " + val (ParamIDs::mix) + sep + "OUT " + val (ParamIDs::output);
        case secRange:
            return "LOW " + val (ParamIDs::rangeLow) + sep + "HIGH " + val (ParamIDs::rangeHigh) + sep + "SHIFT " + val (ParamIDs::shift);
        case secMod:
        {
            int count = 0;
            for (int s = 0; s < kNumModSlots; ++s)
                if ((int) apvts.getRawParameterValue (ParamIDs::modSrc (s))->load() != srcOff)
                    ++count;
            auto lfoText = [&apvts] (int n)
            {
                const bool sync = apvts.getRawParameterValue (ParamIDs::lfoSync (n))->load() > 0.5f;
                return "LFO " + juce::String (n) + " " + apvts.getParameter (ParamIDs::lfoShape (n))->getCurrentValueAsText().toUpperCase() + " "
                       + (sync ? apvts.getParameter (ParamIDs::lfoDiv (n))->getCurrentValueAsText() : apvts.getParameter (ParamIDs::lfoRate (n))->getCurrentValueAsText()).toUpperCase()
                       + ((int) apvts.getRawParameterValue (ParamIDs::lfoMode (n))->load() == lfoFree ? juce::String()
                                                                                                       : " " + apvts.getParameter (ParamIDs::lfoMode (n))->getCurrentValueAsText().toUpperCase());
            };
            return (count == 0 ? juce::String ("NO ROUTING") : juce::String (count) + (count == 1 ? " ROUTING" : " ROUTINGS")) + sep + lfoText (1) + sep + lfoText (2);
        }
        default:
            return {};
    }
}

//==============================================================================
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
    const float dt = 1.0f / (float) juce::jmax (1, timerRate);
    advanceSections (dt);

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
        learnMessageTicks = 8000;
        if (! sections[secKey].open)
            showFooterMessage (learnMessage, 8);
        repaint();
    }
    else if (learnMessageTicks > 0 && (learnMessageTicks -= juce::roundToInt (dt * 1000.0f)) <= 0)
    {
        learnMessageTicks = 0;
        learnMessage = {};
        repaint();
    }
    if (footerMessageTicks > 0 && (footerMessageTicks -= juce::roundToInt (dt * 1000.0f)) <= 0)
    {
        footerMessageTicks = 0;
        footerMessage = {};
        repaint (footerArea);
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

    animPhase += dt;
    prism.setPhase (animPhase);

    // Scan-Linien über den offenen Panels: nur alte und neue Streifen neu zeichnen
    {
        const auto& keyS = sections[secKey];
        const auto& sigS = sections[secSignal];
        const bool keyIdle = keyS.open && keyS.phase == Section::idle;
        const bool sigIdle = sigS.open && sigS.phase == Section::idle;
        const auto a = keyIdle ? scanBand (keyS.bounds, animPhase) : juce::Rectangle<int>();
        const auto b = sigIdle ? scanBand (sigS.bounds, animPhase + 3.0f) : juce::Rectangle<int>();
        repaint (lastScanA.getUnion (a).getIntersection (keyS.bounds));
        repaint (lastScanB.getUnion (b).getIntersection (sigS.bounds));
        lastScanA = a;
        lastScanB = b;
    }

    const bool midi = processor.isMidiControllingKey();
    const int source = midi ? 1 : (processor.isSidechainControllingKey() ? 2 : 0);
    if (sections[secKey].body.isVisible())
    {
        const int scaleIndex = (int) apvts.getRawParameterValue (ParamIDs::scale)->load();
        const int key = (int) apvts.getRawParameterValue (ParamIDs::key)->load();
        const bool chromatic = scaleIndex >= 0 && scaleIndex < (int) kScales.size() && kScales[(size_t) scaleIndex].mask == kAllPitches;
        const int root = (source != 0 || scaleIndex == kCustomScaleIndex || chromatic) ? -1 : key;
        keyStrip.setState (processor.getScaleMask(), root, source); // Tasten = gewählte Tonleiter, Umleitungen zeigt die Matrix
        updateEnergy();
        updateMatrix (mode);
        pitchMatrix.setLevels (shownInput, shownEnergy, animPhase);
    }

    if (sections[secMod].body.isVisible())
        modPanel.tick();
    updateModRings();

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

    // Zusammenfassungen eingeklappter Bereiche aktuell halten (Werte können sich per Automation ändern)
    if ((animFrame++ % 8) == 0)
        for (auto& s : sections)
            if (! s.open && s.phase == Section::idle)
                repaint (s.bounds.withHeight (kBodyTop + 4));

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
        presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
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
    holo::cornerBrackets (g, r, juce::jmin (16.0f, r.getHeight() * 0.4f), Col::holoBright, 2.0f);

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

    // nasse Tech-Oberfläche, feste Größe ab oben, damit sie beim Auf-/Zuklappen nicht springt
    g.fillAll (Col::bg);
    if (texture.isValid())
        g.drawImage (texture, Rectangle<float> (0.0f, 0.0f, (float) kWidth, (float) kTextureHeight), RectanglePlacement::fillDestination);

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
}

void PrizmAudioProcessorEditor::paintKnobTitles (juce::Graphics& g, size_t first, size_t last, int panelTop)
{
    using namespace juce;
    auto* body = knobs[first].slider.getParentComponent();
    if (body == nullptr)
        return;
    const int bottom = (int) (body == &sections[secSignal].body ? sections[secSignal].fullHeight : sections[secRange].fullHeight) - kBodyTop;
    g.setFont (display (13.5f, true).withExtraKerningFactor (0.12f));
    for (size_t i = first; i < last; ++i)
    {
        const auto& k = knobs[i];
        g.setColour (Col::ink.withAlpha (0.85f));
        g.drawText (k.title, k.slider.getX(), panelTop, k.slider.getWidth(), 18, Justification::centred);
        if (i != first)
        {
            const float x = (float) k.slider.getX() - 6.0f;
            ColourGradient div (Col::holo.withAlpha (0.0f), x, (float) panelTop, Col::holo.withAlpha (0.0f), x, (float) bottom - 14.0f, false);
            div.addColour (0.5, Col::holo.withAlpha (0.35f));
            g.setGradientFill (div);
            g.fillRect (x, (float) panelTop, 1.0f, (float) (bottom - 14 - panelTop));
        }
    }
}

void PrizmAudioProcessorEditor::paintKeyBody (juce::Graphics& g)
{
    using namespace juce;
    g.setFont (display (12.5f, true).withExtraKerningFactor (0.1f));
    g.setColour (Col::muted);
    g.drawText ("KEY", keyLabelArea, Justification::centredLeft);
    g.drawText ("SCALE", scaleLabelArea, Justification::centredLeft);

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

void PrizmAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const float scale = jmax (1.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
    if (staticLayer.isNull() || staticLayer.getWidth() != roundToInt ((float) getWidth() * scale)
        || staticLayer.getHeight() != roundToInt ((float) getHeight() * scale))
        renderStaticLayer (scale);
    g.drawImage (staticLayer, getLocalBounds().toFloat());

    // Panels (gecacht pro Höhe) und Kopfzeilen
    for (int i = 0; i < kNumSections; ++i)
    {
        auto& s = sections[(size_t) i];
        const auto b = s.bounds;
        if (s.cache.isNull() || s.cacheH != b.getHeight() || s.cacheScale != scale)
        {
            s.cache = Image (Image::ARGB, roundToInt ((float) b.getWidth() * scale), roundToInt ((float) (b.getHeight() + kPoolSpace) * scale), true);
            Graphics cg (s.cache);
            cg.addTransform (AffineTransform::scale (scale));
            drawHoloPanel (cg, Rectangle<float> (0.0f, 0.0f, (float) b.getWidth(), (float) b.getHeight()), {}, {});
            s.cacheH = b.getHeight();
            s.cacheScale = scale;
        }
        g.drawImage (s.cache, Rectangle<float> ((float) b.getX(), (float) b.getY(), (float) b.getWidth(), (float) (b.getHeight() + kPoolSpace)));

        // Kopfzeile: Pfeil, Titel, offen = Kennung, zu = Zusammenfassung
        const bool hover = i == hoverHeader;
        const auto header = b.withHeight (kBodyTop).toFloat();
        if (hover)
        {
            g.setColour (Col::holo.withAlpha (0.07f));
            g.fillRect (header.reduced (1.0f, 1.5f));
        }
        const bool openish = s.open;
        const float cx = header.getX() + 24.0f, cy = header.getY() + 16.0f;
        Path chevron;
        if (openish)
            chevron.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.0f);
        else
            chevron.addTriangle (cx - 2.5f, cy - 4.5f, cx - 2.5f, cy + 4.5f, cx + 3.0f, cy);
        g.setColour ((hover ? Col::ink : Col::holoBright).withAlpha (0.35f));
        g.strokePath (chevron, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (hover ? Col::ink : Col::holoBright);
        g.fillPath (chevron);

        g.setFont (mono (12.0f));
        g.setColour (hover ? Col::ink : Col::holoBright);
        const int tagX = (int) header.getX() + 36;
        const int tagW = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s.tag) + 4;
        g.drawText (s.tag, tagX, (int) header.getY() + 9, tagW, 14, Justification::centredLeft);

        int right = (int) header.getRight() - 20;
        if (i == secKey)
            right = presetBox.getX() - 70;
        else if (i == secRange)
            right = stereoBox.getX() - 76;
        const int textX = tagX + tagW + 20;
        if (! s.open && s.phase == Section::idle)
        {
            g.setColour (Col::muted);
            g.setFont (mono (11.5f));
            g.drawText (sectionSummary (i), textX, (int) header.getY() + 9, right - textX, 14, Justification::centredRight, true);
        }
        else if (s.code.isNotEmpty())
        {
            g.setColour (Col::dim);
            g.drawText (s.code, textX, (int) header.getY() + 9, right - textX - 48, 14, Justification::centredRight);
            for (int k = 0; k < 4; ++k)
            {
                g.setColour (k < 3 ? Col::holo.withAlpha (0.85f) : Col::holoDim);
                g.fillRect ((float) right - 40.0f + (float) k * 10.0f, header.getY() + 11.0f, 7.0f, 10.0f);
            }
        }
        else if (i == secMod)
        {
            g.setColour (Col::dim);
            g.drawText ("DRAG A SOURCE ONTO A KNOB", textX, (int) header.getY() + 9, right - textX, 14, Justification::centredRight);
        }
    }
    g.setFont (mono (12.0f));
    g.setColour (Col::dim);
    g.drawText ("PRESET", presetBox.getX() - 62, presetBox.getY(), 56, presetBox.getHeight(), Justification::centredRight);
    g.drawText ("PROCESS", stereoBox.getX() - 70, stereoBox.getY(), 64, stereoBox.getHeight(), Justification::centredRight);

    // scan sweeps over the hologram panels
    auto drawScan = [&g] (Rectangle<int> panel, Rectangle<int> band)
    {
        if (band.isEmpty())
            return;
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (panel.reduced (1));
        const auto b = band.toFloat();
        ColourGradient sweep (Col::holo.withAlpha (0.0f), 0.0f, b.getY(), Col::holo.withAlpha (0.0f), 0.0f, b.getBottom(), false);
        sweep.addColour (0.85, Col::holo.withAlpha (0.07f));
        sweep.addColour (0.97, Col::holoBright.withAlpha (0.16f));
        g.setGradientFill (sweep);
        g.fillRect (b);
    };
    if (sections[secKey].open && sections[secKey].phase == Section::idle)
        drawScan (sections[secKey].bounds, scanBand (sections[secKey].bounds, animPhase));
    if (sections[secSignal].open && sections[secSignal].phase == Section::idle)
        drawScan (sections[secSignal].bounds, scanBand (sections[secSignal].bounds, animPhase + 3.0f));

    // latency under the mode switch
    g.setFont (mono (12.0f));
    g.setColour (Col::muted);
    g.drawText (latencyText, modeSwitch.getRight() - 300, modeSwitch.getBottom() + 2, 295, 16, Justification::centredRight);

    // Fußzeile: Hinweis oder Meldung (z. B. wofür ein Preset gedacht ist)
    const auto footerText = footerArea.withTrimmedLeft (infoButton.getWidth() + 12);
    if (footerMessage.isNotEmpty())
    {
        g.setFont (display (13.0f, true));
        g.setColour (Col::holoBright);
        g.drawFittedText (footerMessage, footerText, Justification::centredLeft, 1, 0.85f);
    }
    else
    {
        g.setFont (mono (11.5f));
        g.setColour (Col::dim);
        g.drawText ("DRAG = ADJUST  //  DOUBLE-CLICK = DEFAULT  //  CLICK A HEADER = FOLD", footerText, Justification::centredLeft);
        g.drawText ("BRAIN BUGZ  //  PRIZM  v0.2", footerArea, Justification::centredRight);
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

    // Inhalte in Bereichs-Koordinaten: Panel-Koordinaten minus Kopfzeile
    auto local = [] (int x, int y, int w, int h) { return juce::Rectangle<int> (x, y - kBodyTop, w, h); };
    const int panelW = getWidth() - 40;

    // KEY + PITCH MATRIX
    {
        const int rowY = 34;
        keyLabelArea = local (20, rowY, 100, 16);
        keyBox.setBounds (local (20, rowY + 18, 100, 34));
        scaleLabelArea = local (132, rowY, 200, 16);
        scaleBox.setBounds (local (132, rowY + 18, 200, 34));
        midiButton.setBounds (local (344, rowY + 18, 104, 34));
        scButton.setBounds (local (456, rowY + 18, 104, 34));
        learnButton.setBounds (local (568, rowY + 18, 104, 34));
        readoutArea = local (680, rowY, panelW - 20 - 680, 52);
        stripLabelArea = local (20, 94, panelW - 40, 16);
        keyStrip.setBounds (local (10, 110, panelW - 20, 70));
        pitchMatrix.setBounds (keyStrip.getX(), keyStrip.getBottom(), keyStrip.getWidth(), 112);
        const int legendY = pitchMatrix.getBottom() + 6;
        legendArea = { 20, legendY, 440, 22 };
        auto btnRow = juce::Rectangle<int> (panelW - 20 - 3 * 100 - 2 * 8, legendY, 3 * 100 + 2 * 8, 22);
        majToMinButton.setBounds (btnRow.removeFromLeft (100));
        btnRow.removeFromLeft (8);
        minToMajButton.setBounds (btnRow.removeFromLeft (100));
        btnRow.removeFromLeft (8);
        resetMapButton.setBounds (btnRow);
    }

    // SIGNAL
    {
        auto area = local (14, 50, panelW - 28, kSignalHeight - 60);
        const int w = area.getWidth() / 5;
        for (size_t i = 0; i < 5; ++i)
            knobs[i].slider.setBounds (area.removeFromLeft (w).reduced (12, 0));
    }

    // RANGE + SPECTRAL
    {
        auto area = local (14, 48, panelW - 28, kRangeHeight - 56);
        const int w = area.getWidth() / 6;
        for (size_t i = 5; i < 11; ++i)
            knobs[i].slider.setBounds (area.removeFromLeft (w).reduced (8, 0));
    }

    // MODULATION
    modPanel.setBounds (local (20, 40, panelW - 40, 174));

    layoutSections();
}
