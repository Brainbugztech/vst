#include "PluginEditor.h"
#include "PrizmAssets.h"

#include <cmath>

using namespace prizm;

namespace
{
    constexpr int kWidth = 800;
    constexpr int kHeight = 814;
    constexpr float kTitleSize = 48.0f;

    juce::Font display (float size, bool bold = false) { return bb::fonts::display (size, bold); }
    juce::Font mono (float size) { return bb::fonts::mono (size); }
    juce::Font titleFont() { return display (kTitleSize, true).withExtraKerningFactor (0.18f); }

    bool isSharp (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

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
void KeyStrip::setState (PitchMask newMask, int newRoot, bool newMidi)
{
    if (newMask != mask || newRoot != root || newMidi != midi)
    {
        mask = newMask;
        root = newRoot;
        midi = newMidi;
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
    const auto onColour = midi ? Col::pink : Col::tox;
    const auto onInk = midi ? juce::Colours::white : Col::toxInk;

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
            ColourGradient fill (onColour.brighter (0.15f + 0.3f * e), 0.0f, cell.getY(), onColour.darker (0.45f), 0.0f, cell.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (cell, corner);
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

        g.setColour (on ? onInk : (isSharp (pc) ? Col::dim : Col::muted));
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
    const int pc = cellAt (e.x);
    if (pc >= 0 && onToggle)
        onToggle (pc);
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
    modeSwitch.setTooltip ("Zero Latency: resonator bank, no delay. High Quality: spectral, moves out-of-key partials onto the key.");
    addAndMakeVisible (modeSwitch);

    for (auto* n : kNoteNames)
        keyBox.addItem (n, keyBox.getNumItems() + 1);
    for (auto& s : kScales)
        scaleBox.addItem (s.name, scaleBox.getNumItems() + 1);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::key, keyBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::scale, scaleBox);

    midiButton.setClickingTogglesState (true);
    midiButton.setTooltip ("Play notes into the plugin's MIDI input: the last chord becomes the key.");
    addAndMakeVisible (midiButton);
    midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamIDs::midiKey, midiButton);

    learnButton.setTooltip ("Listens to the input for a few seconds and sets Key, Scale and Tune. Click again to cancel.");
    learnButton.onClick = [this]
    {
        if (processor.isLearning())
            processor.cancelLearn();
        else
            processor.startLearn();
    };
    addAndMakeVisible (learnButton);

    // Noten-Map: pro Note ein Ziel (High Quality), OFF = stumm (beide Modi)
    for (int pc = 0; pc < 12; ++pc)
    {
        auto& box = mapBoxes[(size_t) pc];
        box.addItem ("--", 1);
        for (int t = 0; t < 12; ++t)
            box.addItem (kNoteNames[(size_t) t], t + 2);
        box.addItem ("OFF", 14);
        box.setTooltip (juce::String ("Map ") + kNoteNames[(size_t) pc] + ": move this note to another one (High Quality), or switch it OFF. -- = normal.");
        addAndMakeVisible (box);
        mapAttachments[(size_t) pc] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamIDs::mapId (pc), box);
    }
    majToMinButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::majorToMinor); };
    minToMajButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::minorToMajor); };
    resetMapButton.onClick = [this] { processor.applyMapPreset (PrizmAudioProcessor::MapPreset::reset); };
    majToMinButton.setTooltip ("Turns major into minor, relative to KEY: 3rd, 6th and 7th go down a semitone (High Quality).");
    minToMajButton.setTooltip ("Turns minor into major, relative to KEY (High Quality).");
    resetMapButton.setTooltip ("All notes back to normal.");
    for (auto* b : { &majToMinButton, &minToMajButton, &resetMapButton })
        addAndMakeVisible (*b);

    keyStrip.onToggle = [this] (int pc) { processor.togglePitchClass (pc); };
    addAndMakeVisible (keyStrip);

    struct Def { const char* id; const char* title; const char* hint; };
    const std::array<Def, 11> defs {{
        { ParamIDs::strength, "STRENGTH", "How hard the sound is pulled into the key." },
        { ParamIDs::color,    "COLOR",    "More tonal. Above 100 % (pink) a coloured decay tail is added." },
        { ParamIDs::clear,    "CLEAR",    "Keeps transients dry and damps old ringing on new hits." },
        { ParamIDs::mix,      "MIX",      "Dry / wet. In High Quality the dry signal is latency-aligned." },
        { ParamIDs::output,   "OUTPUT",   "Output level. A soft limiter prevents hard clipping." },
        { ParamIDs::rangeLow, "RANGE LOW",  "Below this frequency the signal stays untouched (keeps kick and sub clean). OFF = full range." },
        { ParamIDs::rangeHigh,"RANGE HIGH", "Above this frequency the signal stays untouched (keeps cymbals and air). OFF = full range." },
        { ParamIDs::gate,     "GATE",     "Pushes quiet notes down relative to the loudest one. Cleans up noise between the tones." },
        { ParamIDs::shift,    "SHIFT",    "High Quality only: moves out-of-key partials onto the nearest note instead of just filtering them." },
        { ParamIDs::formant,  "FORMANT",  "High Quality only: keeps the tone colour (formants) when notes are moved, so voices don't sound like chipmunks." },
        { ParamIDs::tune,     "TUNE A4",  "Tuning reference. Old samples and vinyl are often not at 440 Hz. LEARN sets this automatically." },
    }};

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.title = defs[i].title;
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 24);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        k.slider.setTooltip (defs[i].hint);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, defs[i].id, k.slider);
        if (juce::String (defs[i].id) == ParamIDs::color)
            k.slider.getProperties().set ("overdrive", true); // über 100 % pink
        if (auto* param = apvts.getParameter (defs[i].id))
            k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    setSize (kWidth, kHeight);
    timerCallback();
    startTimerHz (30);
}

PrizmAudioProcessorEditor::~PrizmAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
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
    keyStrip.setEnergy (shownEnergy);
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
    for (auto& box : mapBoxes)
    {
        const int id = box.getSelectedId();
        const auto col = id == 14 ? Col::pink : (id > 1 ? (mode == highQuality ? Col::tox : Col::dim) : Col::dim);
        if (box.findColour (juce::ComboBox::textColourId) != col)
            box.setColour (juce::ComboBox::textColourId, col);
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
    const int scaleIndex = (int) apvts.getRawParameterValue (ParamIDs::scale)->load();
    const int key = (int) apvts.getRawParameterValue (ParamIDs::key)->load();
    const bool chromatic = scaleIndex >= 0 && scaleIndex < (int) kScales.size() && kScales[(size_t) scaleIndex].mask == kAllPitches;
    const int root = (midi || scaleIndex == kCustomScaleIndex || chromatic) ? -1 : key;
    keyStrip.setState (processor.getDisplayMask(), root, midi);
    updateEnergy();

    juce::String text = "LATENCY 0 SMP";
    if (mode == highQuality)
    {
        const int latency = processor.getLatencySamples();
        const double sr = processor.getSampleRate();
        text = "LATENCY " + juce::String (latency) + " SMP";
        if (sr > 0.0)
            text << " / " << juce::String (juce::roundToInt (1000.0 * latency / sr)) << " MS";
    }

    if (text != latencyText || midi != midiShown)
    {
        latencyText = text;
        midiShown = midi;
        repaint();
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
    drawHoloPanel (g, keyPanel.toFloat(), "// KEY MATRIX", "SCALE LOCK  //  PRZ-01");
    drawHoloPanel (g, knobPanel.toFloat(), "// SIGNAL", "SPECTRAL CORE  //  PRZ-02");
    drawHoloPanel (g, extraPanel.toFloat(), "// RANGE + SPECTRAL", "FILTER BAND  //  PRZ-03");

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
        g.drawText (k.title, k.slider.getX(), panel.getY() + 32, k.slider.getWidth(), 18, Justification::centred);
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
        const int count = BigInteger ((int) processor.getDisplayMask()).countNumberOfSetBits();
        g.setFont (display (12.5f, true).withExtraKerningFactor (0.1f));
        g.setColour (Col::muted);
        g.drawText ("IN KEY", r.removeFromTop (16), Justification::centredRight);
        const auto box = r.withTrimmedTop (2).removeFromRight (62).withHeight (34).toFloat();
        holo::box (g, box);
        g.setFont (mono (18.0f));
        g.setColour (midiShown ? Col::pink : Col::ink);
        g.drawText (String (count) + "/12", box, Justification::centred);
    }

    g.setFont (display (12.5f, true).withExtraKerningFactor (0.1f));
    g.setFont (mono (11.0f));
    g.setColour (Col::dim);
    {
        const bool hqMode = (int) processor.apvts.getRawParameterValue (ParamIDs::mode)->load() == highQuality;
        g.setColour (hqMode ? Col::muted : Col::dim);
        g.drawText (hqMode ? "MAP  -  MOVE A NOTE TO ANOTHER ONE OR SWITCH IT OFF"
                           : "MAP  -  MOVING NOTES NEEDS HIGH QUALITY, OFF WORKS IN BOTH MODES",
                    mapLabelArea, Justification::centredLeft);
    }
    g.setFont (display (12.5f, true).withExtraKerningFactor (0.1f));
    if (learnMessage.isNotEmpty())
    {
        g.setColour (Col::tox);
        g.drawText (learnMessage, stripLabelArea, Justification::centredLeft);
    }
    else
    {
        g.setColour (midiShown ? Col::pink : Col::muted);
        g.drawText (midiShown ? "KEY SET BY MIDI  -  LAST CHORD" : "NOTES  -  CLICK TO EDIT, GLOW = WHAT YOU HEAR", stripLabelArea, Justification::centredLeft);
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
    keyLabelArea = { keyPanel.getX() + 20, rowY, 110, 16 };
    keyBox.setBounds (keyPanel.getX() + 20, rowY + 18, 110, 34);
    scaleLabelArea = { keyPanel.getX() + 144, rowY, 236, 16 };
    scaleBox.setBounds (keyPanel.getX() + 144, rowY + 18, 236, 34);
    midiButton.setBounds (keyPanel.getX() + 394, rowY + 18, 132, 34);
    learnButton.setBounds (keyPanel.getX() + 538, rowY + 18, 132, 34);
    readoutArea = { keyPanel.getX() + 680, rowY, keyPanel.getRight() - 20 - (keyPanel.getX() + 680), 52 };

    stripLabelArea = { keyPanel.getX() + 20, keyPanel.getY() + 96, keyPanel.getWidth() - 40, 16 };
    keyStrip.setBounds (keyPanel.getX() + 10, keyPanel.getY() + 112, keyPanel.getWidth() - 20, 78);

    // Map-Zeile direkt unter den Tasten, gleiche Spalten wie die Noten-Leiste
    {
        const float areaX = (float) keyStrip.getX() + 10.0f;
        const float areaW = (float) keyStrip.getWidth() - 20.0f;
        const float gap = 7.0f, w = (areaW - gap * 11.0f) / 12.0f;
        const int y = keyStrip.getBottom() + 4;
        for (int pc = 0; pc < 12; ++pc)
            mapBoxes[(size_t) pc].setBounds (juce::roundToInt (areaX + (float) pc * (w + gap)), y, juce::roundToInt (w), 26);
        const int mapRowY = y + 34;
        mapLabelArea = { keyPanel.getX() + 20, mapRowY, 430, 22 };
        auto btnRow = juce::Rectangle<int> (keyPanel.getRight() - 20 - 3 * 100 - 2 * 8, mapRowY, 3 * 100 + 2 * 8, 22);
        majToMinButton.setBounds (btnRow.removeFromLeft (100));
        btnRow.removeFromLeft (8);
        minToMajButton.setBounds (btnRow.removeFromLeft (100));
        btnRow.removeFromLeft (8);
        resetMapButton.setBounds (btnRow);
    }

    knobPanel = { 20, keyPanel.getBottom() + 12, getWidth() - 40, 212 };
    auto knobArea = knobPanel.reduced (14, 0);
    knobArea.removeFromTop (54);
    knobArea.removeFromBottom (14);
    const int w = knobArea.getWidth() / 5;
    for (size_t i = 0; i < 5; ++i)
        knobs[i].slider.setBounds (knobArea.removeFromLeft (w).reduced (12, 0));

    extraPanel = { 20, knobPanel.getBottom() + 12, getWidth() - 40, 172 };
    auto extraArea = extraPanel.reduced (14, 0);
    extraArea.removeFromTop (52);
    extraArea.removeFromBottom (10);
    const int w2 = extraArea.getWidth() / 6;
    for (size_t i = 5; i < 11; ++i)
        knobs[i].slider.setBounds (extraArea.removeFromLeft (w2).reduced (8, 0));

    footerArea = { 24, getHeight() - 30, getWidth() - 48, 20 };
}
