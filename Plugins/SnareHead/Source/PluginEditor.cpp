#include "PluginEditor.h"
#include "SnareHeadAssets.h"
#include <complex>

using namespace sh;

namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;

    juce::String noteName (float hz)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const int n = (int) std::round (midi);
        const int cents = (int) std::round ((midi - (float) n) * 100.0f);
        juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
        s << (cents >= 0 ? " +" : " ") << cents << "ct";
        return s;
    }

    // |H| eines RBJ-Peaking-Filters in dB
    float peakingDb (float f, float fc, float q, float gainDb, float fs = 48000.0f)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * kPi * fc / fs, al = std::sin (w0) / (2.0f * q), c = std::cos (w0);
        const float b0 = 1 + al * A, b1 = -2 * c, b2 = 1 - al * A, a0 = 1 + al / A, a1 = -2 * c, a2 = 1 - al / A;
        const std::complex<float> z = std::polar (1.0f, -2.0f * kPi * f / fs);
        const auto num = b0 + b1 * z + b2 * z * z;
        const auto den = a0 + a1 * z + a2 * z * z;
        return 20.0f * std::log10 (std::abs (num / den) + 1e-9f);
    }

    void setParam (juce::AudioProcessorValueTreeState& s, const char* id, float plain)
    {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }
    void gesture (juce::AudioProcessorValueTreeState& s, const char* id, bool begin)
    {
        if (auto* p = s.getParameter (id)) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); }
    }

    juce::Image asset (const char* name)
    {
        int size = 0;
        if (auto* data = SnareHeadAssets::getNamedResource (name, size))
            return juce::ImageCache::getFromMemory (data, size);
        return {};
    }

    juce::Typeface::Ptr loadFont (const char* name)
    {
        int size = 0;
        if (auto* data = SnareHeadAssets::getNamedResource (name, size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        return nullptr;
    }

    juce::Font withTf (juce::Typeface::Ptr tf, float h, juce::Font fallback)
    {
        if (tf != nullptr) return juce::Font (juce::FontOptions (tf).withHeight (h));
        return fallback;
    }

    // Rechteck mit abgeschraegten Ecken (oben rechts, unten links) wie im Design
    juce::Path cutRect (juce::Rectangle<float> r, float c)
    {
        juce::Path p;
        p.startNewSubPath (r.getX(), r.getY());
        p.lineTo (r.getRight() - c, r.getY());
        p.lineTo (r.getRight(), r.getY() + c);
        p.lineTo (r.getRight(), r.getBottom());
        p.lineTo (r.getX() + c, r.getBottom());
        p.lineTo (r.getX(), r.getBottom() - c);
        p.closeSubPath();
        return p;
    }

    void drawChartFrame (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (col::panelDeep.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::border.withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    juce::String versionText()
    {
       #ifdef JucePlugin_VersionString
        auto v = juce::StringArray::fromTokens (JucePlugin_VersionString, ".", "");
        return "v" + v[0] + "." + (v.size() > 1 ? v[1] : juce::String ("0"));
       #else
        return "v1.0";
       #endif
    }
}

juce::Font sh::fonts::heading (float h)   { static auto tf = loadFont ("RajdhaniBold_ttf");     return withTf (tf, h, bb::fonts::display (h, true)); }
juce::Font sh::fonts::label (float h)     { static auto tf = loadFont ("RajdhaniSemiBold_ttf"); return withTf (tf, h, bb::fonts::display (h)); }
juce::Font sh::fonts::value (float h)     { static auto tf = loadFont ("OxaniumRegular_ttf");   return withTf (tf, h, bb::fonts::mono (h)); }
juce::Font sh::fonts::valueBold (float h) { static auto tf = loadFont ("OxaniumSemiBold_ttf");  return withTf (tf, h, bb::fonts::mono (h)); }

// ================================================================ LookAndFeel

sh::LookAndFeel::LookAndFeel()
{
    knobSmall = asset ("knob_small_64_png");
    knobLarge = asset ("knob_large_64_png");

    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::headerTextColourId, col::magenta);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::violet.withAlpha (0.55f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::ink);
    setColour (juce::AlertWindow::outlineColourId, col::purple);
    setColour (juce::TextEditor::backgroundColourId, col::panelDeep);
    setColour (juce::TextEditor::textColourId, col::ink);
    setColour (juce::TextEditor::outlineColourId, col::border);
    setColour (juce::TextEditor::focusedOutlineColourId, col::magenta);
    setColour (juce::TextEditor::highlightColourId, col::violet.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, col::magenta);
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, col::panel);
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::TooltipWindow::outlineColourId, col::border);
}

void sh::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float, float, juce::Slider& s)
{
    const bool big = s.getProperties().getWithDefault ("big", false);
    const auto& img = big ? knobLarge : knobSmall;
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();

    if (s.isMouseOverOrDragging())
    {
        g.setColour (col::magenta.withAlpha (s.isMouseButtonDown() ? 0.22f : 0.12f));
        g.fillEllipse (r.reduced (r.getWidth() * 0.04f));
    }

    if (img.isValid())
    {
        const int fw = img.getWidth();
        const int frames = std::max (1, img.getHeight() / fw);
        const int frame = juce::jlimit (0, frames - 1, juce::roundToInt (pos * (float) (frames - 1)));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, x, y, w, h, 0, frame * fw, fw, fw);
        return;
    }

    // Notfall ohne Sprite
    g.setColour (col::panel);
    g.fillEllipse (r.reduced (6));
    juce::Path arc;
    arc.addCentredArc (r.getCentreX(), r.getCentreY(), r.getWidth() * 0.42f, r.getWidth() * 0.42f, 0.0f,
                       -2.4f, -2.4f + pos * 4.8f, true);
    g.setColour (col::magenta);
    g.strokePath (arc, juce::PathStrokeType (4.0f));
}

void sh::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const float cut = std::min (12.0f, r.getHeight() * 0.32f);
    const bool on = b.getToggleState();
    const auto shape = cutRect (r, cut);

    if (on)
    {
        g.setColour (col::cyan.withAlpha (0.18f));
        g.strokePath (cutRect (r.expanded (2.0f), cut + 2.0f), juce::PathStrokeType (4.0f));
    }
    juce::ColourGradient fill (juce::Colour (down ? 0xff1a1028 : 0xff241834), 0, r.getY(),
                               juce::Colour (0xff0b0714), 0, r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillPath (shape);

    // Metallrand
    juce::ColourGradient rim (col::metal.withAlpha (over ? 0.85f : 0.6f), r.getX(), r.getY(),
                              col::border.withAlpha (0.7f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (rim);
    g.strokePath (shape, juce::PathStrokeType (1.6f));

    // Leuchtakzent oben rechts bzw. unten (aktiv = cyan)
    const auto accent = on ? col::cyan : col::magenta;
    g.setColour (accent.withAlpha (on ? 1.0f : (over ? 0.95f : 0.75f)));
    juce::Path a;
    a.startNewSubPath (r.getRight() - cut - std::min (60.0f, r.getWidth() * 0.25f), r.getY() + 1.5f);
    a.lineTo (r.getRight() - cut, r.getY() + 1.5f);
    a.lineTo (r.getRight() - 1.5f, r.getY() + cut);
    g.strokePath (a, juce::PathStrokeType (2.0f));
    if (on || r.getWidth() > 160.0f)
    {
        g.setColour (accent.withAlpha (on ? 0.9f : 0.35f));
        g.drawLine (r.getX() + cut + 6.0f, r.getBottom() - 4.0f, r.getRight() - 10.0f, r.getBottom() - 4.0f, on ? 2.0f : 1.2f);
    }
}

void sh::LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool small = (bool) b.getProperties().getWithDefault ("small", false);
    g.setFont (fonts::heading ((float) b.getHeight() * (small ? 0.66f : 0.5f)));
    g.setColour (b.getToggleState() ? juce::Colours::white : col::ink);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void sh::LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.5f);
    const float cut = std::min (12.0f, r.getHeight() * 0.32f);
    const auto shape = cutRect (r, cut);
    juce::ColourGradient fill (juce::Colour (0xff1f1530), 0, r.getY(), juce::Colour (0xff09060f), 0, r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillPath (shape);
    juce::ColourGradient rim (col::metal.withAlpha (box.isMouseOver (true) ? 0.85f : 0.6f), r.getX(), r.getY(),
                              col::border.withAlpha (0.7f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (rim);
    g.strokePath (shape, juce::PathStrokeType (1.6f));
    g.setColour (col::magenta.withAlpha (0.45f));
    g.drawLine (r.getX() + cut + 8.0f, r.getBottom() - 4.0f, r.getRight() - 12.0f, r.getBottom() - 4.0f, 1.2f);

    juce::Path arrow;
    const float ax = (float) w - 30.0f, ay = (float) h * 0.5f, s = (float) h * 0.13f;
    arrow.addTriangle (ax - s * 1.4f, ay - s * 0.8f, ax + s * 1.4f, ay - s * 0.8f, ax, ay + s * 1.0f);
    g.setColour (col::magenta.withAlpha (0.4f));
    g.strokePath (arrow, juce::PathStrokeType (4.0f));
    g.setColour (col::magenta);
    g.fillPath (arrow);
}

juce::Font sh::LookAndFeel::getComboBoxFont (juce::ComboBox& b) { return fonts::heading ((float) b.getHeight() * 0.5f); }
juce::Font sh::LookAndFeel::getPopupMenuFont() { return fonts::label (21.0f); }

void sh::LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (14, 0, box.getWidth() - 56, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void sh::LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::panel);
    g.setColour (col::border);
    g.drawRect (0, 0, w, h, 1);
}

// ================================================================ Knob

double NoteSlider::snapValue (double v, DragMode)
{
    if (! noteMode || ! noteMode() || v <= 0.0) return v;
    int midi = (int) std::round (69.0 + 12.0 * std::log2 (v / 440.0));
    auto hzOf = [] (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); };
    while (hzOf (midi) < getMinimum() - 0.01) ++midi;
    while (hzOf (midi) > getMaximum() + 0.01) --midi;
    return juce::jlimit (getMinimum(), getMaximum(), hzOf (midi));
}

Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& label, bool big)
    : param (state.getParameter (paramId)), name (label), isBig (big)
{
    setInterceptsMouseClicks (false, true);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (big ? 320 : 240);
    slider.setVelocityBasedMode (false);
    slider.getProperties().set ("big", big);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    // Wert anklicken = eintippen
    valueLabel.setEditable (true, true, false);
    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (fonts::value (big ? 22.0f : 20.0f));
    valueLabel.setColour (juce::Label::textColourId, col::value);
    valueLabel.setColour (juce::Label::textWhenEditingColourId, col::ink);
    valueLabel.setColour (juce::Label::backgroundWhenEditingColourId, col::panelDeep);
    valueLabel.setColour (juce::Label::outlineWhenEditingColourId, col::magenta);
    valueLabel.setMouseCursor (juce::MouseCursor::IBeamCursor);
    valueLabel.setTooltip ("Klicken und Wert eintippen");
    valueLabel.onEditorShow = [this]
    {
        if (auto* ed = valueLabel.getCurrentTextEditor())
        {
            ed->setJustification (juce::Justification::centred);
            ed->setFont (fonts::value (isBig ? 22.0f : 20.0f));
            ed->selectAll();
        }
    };
    valueLabel.onTextChange = [this]
    {
        if (param == nullptr) return;
        const auto t = valueLabel.getText().trim();
        if (t.isNotEmpty())
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->getValueForText (t));
            param->endChangeGesture();
        }
        refreshValue();
    };
    addAndMakeVisible (valueLabel);

    slider.onValueChange = [this] { refreshValue(); };
    refreshValue();
}

void Knob::place (int cx, int cy, int diameter, int labelDy, int valueDy, int labelDx)
{
    diam = diameter; lDy = labelDy; vDy = valueDy; lDx = labelDx;
    const int w = std::max (diameter, 124);
    setBounds (cx - w / 2, cy - labelDy - 16, w, labelDy + valueDy + 32);
}

void Knob::resized()
{
    const int w = getWidth(), cy = lDy + 16;
    slider.setBounds (w / 2 - diam / 2, cy - diam / 2, diam, diam);
    valueLabel.setBounds (w / 2 - 62, cy + vDy - 15, 124, 30);
}

void Knob::enableNoteMode (SnareHeadProcessor& p, const juce::String& stateKey)
{
    proc = &p;
    noteKey = stateKey;
    slider.noteMode = [this] { return isNoteMode(); };
    noteBtn.setTooltip ("Anzeige Hz / Note umschalten (Note = rastet auf Halbtoene)");
    noteBtn.getProperties().set ("small", true);
    noteBtn.onClick = [this]
    {
        proc->setNoteMode (noteKey, ! isNoteMode());
        if (isNoteMode())
            slider.setValue (slider.snapValue (slider.getValue(), juce::Slider::notDragging), juce::sendNotificationSync);
        refreshValue();
    };
    refreshValue();
}

void Knob::refreshValue()
{
    if (param == nullptr) return;
    const bool note = isNoteMode();
    const juce::String txt = note ? noteNameForHz ((float) slider.getValue(), true) : param->getCurrentValueAsText();
    if (! valueLabel.isBeingEdited())
        valueLabel.setText (txt, juce::dontSendNotification);
    if (proc != nullptr)
    {
        noteBtn.setButtonText (note ? "NOTE" : "Hz");
        noteBtn.setToggleState (note, juce::dontSendNotification);
    }
    repaint();
}

void Knob::setDimmed (bool d)
{
    if (d == dimmed) return;
    dimmed = d;
    slider.setAlpha (d ? 0.4f : 1.0f);
    valueLabel.setAlpha (d ? 0.4f : 1.0f);
    repaint();
}

void Knob::paint (juce::Graphics& g)
{
    g.setFont (fonts::label (isBig ? 22.0f : 20.0f));
    g.setColour (col::ink.withAlpha (dimmed ? 0.45f : 1.0f));
    g.drawText (name.toUpperCase(), lDx, 0, getWidth(), 30, juce::Justification::centred);
}

// ================================================================ EqView

juce::Rectangle<float> EqView::plot() const
{
    return getLocalBounds().toFloat().withTrimmedTop (34.0f).withTrimmedBottom (26.0f).reduced (8.0f, 0.0f);
}
float EqView::hzToX (float hz) const
{
    const auto p = plot();
    const float lo = std::log (250.0f), hi = std::log (2100.0f);
    return p.getX() + (std::log (hz) - lo) / (hi - lo) * p.getWidth();
}
float EqView::xToHz (float x) const
{
    const auto p = plot();
    const float lo = std::log (250.0f), hi = std::log (2100.0f);
    return std::exp (lo + juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()) * (hi - lo));
}

void EqView::paint (juce::Graphics& g)
{
    const auto p = proc.readParams();
    drawChartFrame (g, getLocalBounds().toFloat());
    const auto pl = plot();
    const float top = pl.getY(), bottom = pl.getBottom();
    auto dbToY = [&] (float dbv) { return juce::jmap (dbv, -3.0f, 20.0f, bottom, top); };

    // Raster
    g.setFont (fonts::value (14.0f));
    for (float f : { 300.0f, 500.0f, 700.0f, 1000.0f, 1200.0f, 1600.0f, 2000.0f })
    {
        const float x = hzToX (f);
        g.setColour (col::border.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, top, bottom);
        g.setColour (col::inkDim.withAlpha (0.85f));
        g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 1) + "k" : juce::String ((int) f), (int) x - 24, (int) bottom + 3, 48, 18,
                    juce::Justification::centred);
    }
    g.setColour (col::border.withAlpha (0.5f));
    g.drawHorizontalLine ((int) dbToY (0.0f), pl.getX(), pl.getRight());

    // Ring-Teiltoene
    float ratio[snarehead::Voice::kPartials], amp[snarehead::Voice::kPartials];
    snarehead::Voice::ringPartials (p, ratio, amp);
    const float tune = proc.lastNoteHz.load() > 0 && p.keyTrack ? proc.lastNoteHz.load() : p.tune;
    for (int k = 0; k < snarehead::Voice::kPartials; ++k)
    {
        const float f = tune * ratio[k];
        if (f < 250.0f || f > 2100.0f) continue;
        const float x = hzToX (f);
        const float hgt = juce::jlimit (0.15f, 1.0f, amp[k] * 4.0f) * (bottom - top) * 0.82f * std::max (0.25f, p.ringLevel);
        juce::ColourGradient bar (col::metal.withAlpha (0.85f), x, bottom - hgt, col::violet.withAlpha (0.25f), x, bottom, false);
        g.setGradientFill (bar);
        g.fillRect (x - 2.5f, bottom - hgt, 5.0f, hgt);
    }

    // Glocke
    const float fc = p.eqLink ? tune : p.eqFreq;
    juce::Path curve, fill;
    for (float x = pl.getX(); x <= pl.getRight(); x += 2.0f)
    {
        const float y = dbToY (peakingDb (xToHz (x), fc, p.eqQ, p.eqGain));
        if (curve.isEmpty()) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, dbToY (0.0f)); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (pl.getRight(), dbToY (0.0f));
    fill.closeSubPath();
    juce::ColourGradient grad (col::magenta.withAlpha (0.22f), 0, top, col::purple.withAlpha (0.0f), 0, dbToY (0.0f), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (col::magenta.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::magenta);
    g.strokePath (curve, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Griff
    const juce::Point<float> h (hzToX (fc), dbToY (p.eqGain));
    g.setColour (col::magenta.withAlpha (0.35f));
    g.fillEllipse (h.x - 13, h.y - 13, 26, 26);
    g.setColour (juce::Colours::white);
    g.fillEllipse (h.x - 7, h.y - 7, 14, 14);

    // Info
    g.setFont (fonts::label (20.0f));
    g.setColour (col::ink);
    juce::String info = noteName (fc);
    if (p.eqLink) info = "LINK > TUNE  " + info;
    g.drawText (info, 20, 6, getWidth() / 2, 24, juce::Justification::centredLeft);
    g.drawText ("RING " + noteName (tune), getWidth() / 2, 6, getWidth() / 2 - 20, 24, juce::Justification::centredRight);
}

void EqView::setFromMouse (juce::Point<float> pt)
{
    const auto p = proc.readParams();
    const auto pl = plot();
    if (! p.eqLink)
        setParam (proc.apvts, ParamIDs::eqFreq, juce::jlimit (kFreqMin, 1600.0f, xToHz (pt.x)));
    setParam (proc.apvts, ParamIDs::eqGain, juce::jlimit (0.0f, 18.0f, juce::jmap (pt.y, pl.getBottom(), pl.getY(), -3.0f, 20.0f)));
}

void EqView::mouseDown (const juce::MouseEvent& e)
{
    gesture (proc.apvts, ParamIDs::eqFreq, true);
    gesture (proc.apvts, ParamIDs::eqGain, true);
    setFromMouse (e.position);
}
void EqView::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }
void EqView::mouseUp (const juce::MouseEvent&)
{
    gesture (proc.apvts, ParamIDs::eqFreq, false);
    gesture (proc.apvts, ParamIDs::eqGain, false);
}
void EqView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const auto p = proc.readParams();
    setParam (proc.apvts, ParamIDs::eqQ, juce::jlimit (0.5f, 12.0f, p.eqQ * std::pow (1.25f, w.deltaY * 4.0f)));
}

// ================================================================ GateView

juce::Rectangle<float> GateView::plot() const
{
    const float topPad = reverb ? 36.0f : 30.0f, bottomPad = reverb ? 26.0f : 22.0f;
    return getLocalBounds().toFloat().withTrimmedTop (topPad).withTrimmedBottom (bottomPad).reduced (reverb ? 16.0f : 12.0f, 0.0f);
}

void GateView::paint (juce::Graphics& g)
{
    const auto p = proc.readParams();
    drawChartFrame (g, getLocalBounds().toFloat());
    const auto pl = plot();

    const float bpm = proc.hostBpm.load();
    const float gateMs = snarehead::SnareEngine::gateLengthMs (p, bpm);
    const bool gateOn = reverb && gateMs < 1999.0f;
    const float cutMs = (0.0008f + p.verbCut * p.verbCut * 0.2f) * 1000.0f;
    const float dryLen = p.hold + p.release;
    const float t60 = (0.35f + p.verbSize * p.verbSize * 5.5f) * 1000.0f;

    float span = dryLen * 1.35f;
    if (reverb)
        span = std::max (span, gateOn ? (gateMs + cutMs) * 1.3f : (p.verbMix > 0.01f ? std::min (t60 * 0.7f, 2500.0f) : span));
    spanMs = juce::jlimit (120.0f, 2500.0f, span);

    const float top = pl.getY(), bottom = pl.getBottom();
    auto tx = [&] (float t) { return pl.getX() + t / spanMs * pl.getWidth(); };
    auto vy = [&] (float v) { return bottom - v * (bottom - top); };

    // Raster
    const float step = spanMs > 1200 ? 250.0f : spanMs > 500 ? 100.0f : 50.0f;
    g.setFont (fonts::value (reverb ? 14.0f : 13.0f));
    for (float t = 0; t <= spanMs; t += step)
    {
        g.setColour (col::border.withAlpha (0.35f));
        g.drawVerticalLine ((int) tx (t), top, bottom);
        g.setColour (col::inkDim.withAlpha (0.85f));
        g.drawText (juce::String ((int) t), (int) tx (t) - 22, (int) bottom + 2, 44, 18, juce::Justification::centred);
    }
    g.setColour (col::border.withAlpha (0.6f));
    g.drawHorizontalLine ((int) bottom, pl.getX(), pl.getRight());

    const float holdTau = p.hold * 3.0f;
    auto dry = [&] (float t)
    {
        if (t < p.hold) return std::exp (-t / holdTau);
        const float x = (t - p.hold) / std::max (1.0f, p.release);
        if (x >= 1.0f) return 0.0f;
        const float u = 1.0f - x;
        return std::exp (-p.hold / holdTau) * u * u * u;
    };
    auto gateGain = [&] (float t)
    {
        if (! gateOn || t < gateMs) return 1.0f;
        const float v = std::max (0.0f, 1.0f - (t - gateMs) / cutMs);
        return v * v * (3 - 2 * v);
    };
    const float wetAmt = reverb ? std::min (1.0f, 2.0f * p.verbMix) : 0.0f;
    auto wet = [&] (float t)
    {
        const float build = 1.0f - std::exp (-t / 25.0f);
        return 0.85f * wetAmt * build * std::pow (10.0f, -3.0f * t / t60) * gateGain (t);
    };

    auto area = [&] (auto fn, juce::Colour c, float fillAlpha)
    {
        juce::Path path, edge;
        path.startNewSubPath (tx (0), vy (0));
        for (int i = 0; i <= 240; ++i)
        {
            const float t = spanMs * (float) i / 240.0f;
            path.lineTo (tx (t), vy (fn (t)));
            if (i == 0) edge.startNewSubPath (tx (t), vy (fn (t))); else edge.lineTo (tx (t), vy (fn (t)));
        }
        path.lineTo (tx (spanMs), vy (0));
        path.closeSubPath();
        juce::ColourGradient grad (c.withAlpha (fillAlpha), 0, top, c.withAlpha (0.1f), 0, bottom, false);
        g.setGradientFill (grad);
        g.fillPath (path);
        g.setColour (c.brighter (0.4f));
        g.strokePath (edge, juce::PathStrokeType (2.0f));
    };

    if (wetAmt > 0.001f) area (wet, col::magenta, 0.55f);
    area (dry, col::graphFill, 0.85f);

    if (gateOn)
    {
        const float gx = tx (gateMs), cx = tx (gateMs + cutMs);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (cx, top - 4.0f, pl.getRight() - cx, bottom - top + 4.0f);
        g.setColour (col::cyan.withAlpha (0.3f));
        g.fillRect (gx - 3.0f, top - 4.0f, 6.0f, bottom - top + 4.0f);
        g.setColour (col::cyan);
        g.fillRect (gx - 1.0f, top - 4.0f, 2.0f, bottom - top + 4.0f);
    }

    g.setFont (fonts::label (reverb ? 20.0f : 18.0f));
    g.setColour (col::ink);
    const int tY = reverb ? 6 : 4, tH = 24;
    if (reverb)
    {
        if (gateOn)
        {
            const juce::String lbl = p.verbSync > 0 ? juce::String ("GATE ") + snarehead::kSyncNames[p.verbSync]
                                                    : "GATE " + juce::String ((int) std::round (gateMs)) + " ms";
            g.setColour (col::cyan);
            g.drawText (lbl, 20, tY, 220, tH, juce::Justification::centredLeft);
            g.setColour (col::ink);
        }
        else
            g.drawText ("GATE OFF", 20, tY, 200, tH, juce::Justification::centredLeft);
        g.drawText ("ziehen = Gate", getWidth() - 220, tY, 200, tH, juce::Justification::centredRight);
    }
    else
        g.drawText (juce::String ((int) std::round (dryLen)) + " ms", 16, tY, 140, tH, juce::Justification::centredLeft);

    if (sinceHitMs < spanMs)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawVerticalLine ((int) tx (sinceHitMs), top, bottom);
    }
}

void GateView::setGateFromX (float x)
{
    if (! reverb) return;
    const auto pl = plot();
    const float t = (x - pl.getX()) / pl.getWidth() * spanMs;
    setParam (proc.apvts, ParamIDs::verbSync, 0.0f);
    setParam (proc.apvts, ParamIDs::verbGate, juce::jlimit (30.0f, 1990.0f, t));
}
void GateView::mouseDown (const juce::MouseEvent& e)
{
    if (! reverb) { proc.audition(); return; }
    gesture (proc.apvts, ParamIDs::verbGate, true);
    setGateFromX (e.position.x);
}
void GateView::mouseDrag (const juce::MouseEvent& e) { setGateFromX (e.position.x); }
void GateView::mouseUp (const juce::MouseEvent&) { if (reverb) gesture (proc.apvts, ParamIDs::verbGate, false); }

// ================================================================ MascotView

MascotView::MascotView (SnareHeadProcessor& p) : proc (p)
{
    img = asset ("mascot_png");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MascotView::tick (bool hit, float level)
{
    const float noisy = proc.readParams().noisy;
    if (hit) { pulse = 1.0f; glow = 1.0f; }
    const float shake = noisy * pulse * 7.0f;
    shakeX = (rng.nextFloat() * 2.0f - 1.0f) * shake;
    shakeY = (rng.nextFloat() * 2.0f - 1.0f) * shake;
    rot = (rng.nextFloat() * 2.0f - 1.0f) * noisy * pulse * 0.03f;
    pulse *= 0.84f;
    glow = std::max (glow * 0.9f, std::min (1.0f, level * 1.3f));
    if (pulse > 0.002f || glow > 0.002f || hit)
        repaint();
}

void MascotView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float base = std::min (r.getWidth(), r.getHeight());

    if (glow > 0.01f)
    {
        juce::ColourGradient halo (col::magenta.withAlpha (0.35f * glow), c.x, c.y,
                                   col::magenta.withAlpha (0.0f), c.x + base * 0.55f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (r.reduced (base * 0.02f));
    }

    if (img.isValid())
    {
        const float s = std::min (r.getWidth() / (float) img.getWidth(), r.getHeight() / (float) img.getHeight()) * (1.0f + 0.035f * pulse);
        auto t = juce::AffineTransform::translation (-(float) img.getWidth() * 0.5f, -(float) img.getHeight() * 0.5f)
                     .scaled (s).rotated (rot).translated (c.x + shakeX, c.y + shakeY);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (img, t);
    }
}

void MascotView::mouseDown (const juce::MouseEvent& e)
{
    // weiter unten geklickt = haerter angeschlagen
    const float vel = juce::jlimit (0.3f, 1.0f, 0.45f + 0.6f * e.position.y / (float) getHeight());
    proc.audition (vel);
}

// ================================================================ AuditionStrip

void AuditionStrip::paint (juce::Graphics& g)
{
    const auto mr = juce::Rectangle<float> (4.0f, 6.0f, (float) getWidth() - 8.0f, 12.0f);
    g.setColour (col::panelDeep);
    g.fillRoundedRectangle (mr, 5.0f);
    g.setColour (col::border.withAlpha (0.6f));
    g.drawRoundedRectangle (mr, 5.0f, 1.0f);
    const float lv = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (meter, -48.0f), -48.0f, 0.0f, 0.0f, 1.0f));
    if (lv > 0.001f)
    {
        juce::ColourGradient mg (col::violet, mr.getX(), 0, col::magenta, mr.getRight(), 0, false);
        g.setGradientFill (mg);
        g.fillRoundedRectangle (mr.withWidth (mr.getWidth() * lv), 5.0f);
    }
    g.setFont (fonts::value (18.0f));
    g.setColour (isMouseOver() ? col::ink : col::inkDim);
    g.drawText ("Logo = Anspielen", 0, 24, getWidth(), 24, juce::Justification::centred);
}

// ================================================================ Editor

SnareHeadEditor::SnareHeadEditor (SnareHeadProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      mascot (p), eqView (p), envView (p, false), gateView (p, true), audition (p),
      tune (p.apvts, ParamIDs::tune, "Tune", true),
      spread (p.apvts, ParamIDs::spread, "Spread"),
      metal (p.apvts, ParamIDs::metal, "Metal"),
      punch (p.apvts, ParamIDs::punch, "Punch"),
      ring (p.apvts, ParamIDs::ring, "Ring"),
      noise (p.apvts, ParamIDs::noise, "Level"),
      noiseTone (p.apvts, ParamIDs::noiseTone, "Tone"),
      hold (p.apvts, ParamIDs::hold, "Hold"),
      release (p.apvts, ParamIDs::release, "Release"),
      eqFreq (p.apvts, ParamIDs::eqFreq, "Freq"),
      eqGain (p.apvts, ParamIDs::eqGain, "Boost"),
      eqQ (p.apvts, ParamIDs::eqQ, "Q"),
      verbSize (p.apvts, ParamIDs::verbSize, "Size"),
      verbMix (p.apvts, ParamIDs::verbMix, "Mix"),
      verbGate (p.apvts, ParamIDs::verbGate, "Gate"),
      verbCut (p.apvts, ParamIDs::verbCut, "Cut"),
      noisy (p.apvts, ParamIDs::noisy, "Amount", true),
      width (p.apvts, ParamIDs::width, "Width"),
      volume (p.apvts, ParamIDs::volume, "Volume")
{
    // gespeicherte Fenstergroesse merken, bevor setResizeLimits sie ueberschreibt
    const int savedWidth = juce::jlimit (790, 1974, (int) proc.apvts.state.getProperty ("uiWidth", 1105));
    setLookAndFeel (&lnf);
    tune.enableNoteMode (p, "tuneNoteMode");
    eqFreq.enableNoteMode (p, "eqNoteMode");

    addAndMakeVisible (canvas);
    for (auto* c : std::initializer_list<juce::Component*> { &mascot, &eqView, &envView, &gateView, &audition,
             &tune, &spread, &metal, &punch, &ring, &noise, &noiseTone, &hold, &release,
             &eqFreq, &eqGain, &eqQ, &verbSize, &verbMix, &verbGate, &verbCut, &noisy, &width, &volume,
             &tune.noteBtn, &eqFreq.noteBtn,
             &linkBtn, &keyBtn, &syncBox, &presetBox, &prevBtn, &nextBtn, &saveBtn, &moreBtn })
        canvas.addAndMakeVisible (c);

    linkBtn.setClickingTogglesState (true);
    keyBtn.setClickingTogglesState (true);
    linkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::eqLink, linkBtn);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::keyTrack, keyBtn);
    linkBtn.setTooltip ("Res-EQ folgt dem Ring-Ton");
    keyBtn.setTooltip ("MIDI-Note stimmt die Snare um (C3 = Tune)");

    for (int i = 0; i < 10; ++i)
        syncBox.addItem (i == 0 ? juce::String ("Gate frei [ms]") : juce::String ("Sync ") + snarehead::kSyncNames[i], i + 1);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::verbSync, syncBox);

    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentPreset()) { proc.loadPreset (idx); proc.audition (0.85f); }
        refreshPresetBox();
    };
    auto step = [this] (int d)
    {
        const int total = proc.getPresetNames().size();
        if (total == 0) return;
        const int cur = proc.getCurrentPreset();
        proc.loadPreset (((cur < 0 ? 0 : cur + d) % total + total) % total);
        proc.audition (0.85f);
        refreshPresetBox();
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };
    saveBtn.onClick = [this] { savePreset (proc.getCurrentPreset() < proc.getNumFactoryPresets()); };
    moreBtn.onClick = [this] { showPresetMenu(); };
    prevBtn.setTooltip ("Voriges Preset");
    nextBtn.setTooltip ("Naechstes Preset");

    layout();
    buildStaticLayer();

    // stufenlos skalierbar, Seitenverhaeltnis fest; Start in mittlerer Groesse
    setResizable (true, true);
    setResizeLimits (790, 498, 1974, 1245);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) sh::kDesignW / (double) sh::kDesignH);
    setSize (savedWidth, juce::roundToInt ((float) savedWidth * (float) sh::kDesignH / (float) sh::kDesignW));

    refreshPresetBox();
    startTimerHz (60);
}

SnareHeadEditor::~SnareHeadEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SnareHeadEditor::layout()
{
    canvas.setBounds (0, 0, sh::kDesignW, sh::kDesignH);

    // Kopfzeile
    prevBtn.setBounds (865, 21, 54, 49);
    presetBox.setBounds (922, 21, 384, 49);
    nextBtn.setBounds (1309, 21, 54, 49);
    saveBtn.setBounds (1368, 21, 110, 49);
    moreBtn.setBounds (1483, 21, 68, 49);

    mascot.setBounds (20, 101, 466, 423);

    // Ring
    tune.place (619, 282, 176, 105, 107, -21);
    tune.noteBtn.setBounds (642, 160, 68, 30);
    spread.place (788, 213, 96, 53, 61);
    metal.place (940, 213, 96, 53, 61);
    punch.place (789, 355, 96, 53, 62);
    ring.place (944, 355, 96, 53, 62);

    // Noise
    noise.place (1148, 213, 96, 53, 61);
    noiseTone.place (1148, 355, 96, 53, 62);

    // Shape
    envView.setBounds (1282, 152, 246, 127);
    hold.place (1350, 355, 96, 55, 62);
    release.place (1475, 355, 96, 55, 62);

    // Xtreme Noisy + Out
    noisy.place (142, 714, 176, 100, 102);
    width.place (319, 663, 96, 54, 66);
    volume.place (431, 663, 96, 54, 66);
    keyBtn.setBounds (282, 756, 195, 46);
    audition.setBounds (282, 866, 194, 50);

    // Res EQ
    eqView.setBounds (528, 511, 470, 209);
    eqFreq.place (605, 788, 96, 53, 62, -13);
    eqFreq.noteBtn.setBounds (630, 722, 68, 30);
    eqGain.place (768, 788, 96, 53, 62);
    eqQ.place (928, 788, 96, 53, 62);
    linkBtn.setBounds (522, 866, 475, 43);

    // Reverb
    gateView.setBounds (1058, 511, 470, 209);
    verbSize.place (1106, 788, 96, 53, 62);
    verbMix.place (1233, 788, 96, 53, 62);
    verbGate.place (1355, 788, 96, 53, 62);
    verbCut.place (1475, 788, 96, 53, 62);
    syncBox.setBounds (1051, 866, 476, 44);
}

void SnareHeadEditor::buildStaticLayer()
{
    staticLayer = juce::Image (juce::Image::ARGB, sh::kDesignW, sh::kDesignH, true);
    juce::Graphics g (staticLayer);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    auto draw = [&] (const char* name, int x, int y)
    {
        auto im = asset (name);
        if (im.isValid()) g.drawImageAt (im, x, y);
    };

    g.fillAll (col::canvas);
    draw ("bg_base_png", 0, 0);
    draw ("rail_header_png", 0, 0);
    draw ("rail_footer_png", 0, sh::kDesignH - 56);
    draw ("panel_ring_png", 494, 96);
    draw ("panel_noise_png", 1028, 96);
    draw ("panel_shape_png", 1259, 96);
    draw ("panel_xtreme_noisy_png", 20, 522);
    draw ("panel_out_png", 259, 522);
    draw ("panel_res_eq_png", 492, 451);
    draw ("panel_reverb_png", 1027, 451);
    draw ("wordmark_png", 40, 8);

    // Neon-Kanten unten an den Panels (wie im Konzept)
    for (auto pr : { juce::Rectangle<int> (494, 96, 529, 348), { 1028, 96, 224, 348 }, { 1259, 96, 297, 348 },
                     { 20, 522, 236, 408 }, { 259, 522, 224, 408 }, { 492, 451, 531, 480 }, { 1027, 451, 531, 480 } })
    {
        const auto r = pr.toFloat();
        const float y = r.getBottom() - 9.0f, x0 = r.getX() + 26.0f, x1 = r.getRight() - 26.0f;
        juce::ColourGradient glow (col::magenta.withAlpha (0.0f), x0, y, col::magenta.withAlpha (0.0f), x1, y, false);
        glow.addColour (0.25, col::magenta.withAlpha (0.9f));
        glow.addColour (0.75, col::magenta.withAlpha (0.9f));
        g.setGradientFill (glow);
        g.setOpacity (0.22f);
        g.fillRect (x0, y - 5.0f, x1 - x0, 10.0f);
        g.setOpacity (0.9f);
        g.fillRect (x0, y - 1.0f, x1 - x0, 2.0f);
        g.setOpacity (1.0f);
    }

    // Untertitel
    g.setColour (col::inkDim);
    g.setFont (fonts::value (17.0f).withExtraKerningFactor (0.12f));
    g.drawText ("BRAIN BUGZ  //  SNARE SYNTH  " + versionText(), 462, 30, 380, 26, juce::Justification::centredLeft);

    // Paneltitel
    auto title = [&] (const juce::String& t, int x, int cy, bool bar, float size = 32.0f, bool centred = false, int w = 0)
    {
        if (bar)
        {
            g.setColour (col::magenta);
            g.fillRect (x - 10, cy - 12, 3, 24);
        }
        g.setFont (fonts::heading (size));
        g.setColour (col::magenta.withAlpha (0.35f));
        if (centred) g.drawText (t, x, cy - 20 + 2, w, 40, juce::Justification::centred);
        else         g.drawText (t, x, cy - 20 + 2, 400, 40, juce::Justification::centredLeft);
        g.setColour (col::ink);
        if (centred) g.drawText (t, x, cy - 20, w, 40, juce::Justification::centred);
        else         g.drawText (t, x, cy - 20, 400, 40, juce::Justification::centredLeft);
    };
    title ("RING", 546, 126, true);
    title ("NOISE", 1080, 126, true);
    title ("SHAPE", 1312, 126, true);
    title ("RES EQ", 538, 485, false);
    title ("REVERB", 1071, 485, false);
    title ("XTREME NOISY", 20, 559, false, 30.0f, true, 236);
    title ("OUT", 290, 559, false);

    // Xtreme-Noisy-Effekte
    g.setFont (fonts::value (17.0f));
    g.setColour (col::inkDim);
    g.drawText ("Comb", 73, 846, 60, 24, juce::Justification::centred);
    g.drawText ("Ringmod", 141, 846, 72, 24, juce::Justification::centred);
    g.drawText ("Drive", 79, 867, 60, 24, juce::Justification::centred);
    g.drawText ("Crush", 147, 867, 60, 24, juce::Justification::centred);
}

void SnareHeadEditor::paint (juce::Graphics& g)
{
    g.fillAll (sh::col::canvas);
}

void SnareHeadEditor::resized()
{
    const float s = (float) getWidth() / (float) sh::kDesignW;
    canvas.setTransform (juce::AffineTransform::scale (s));
    proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}

void SnareHeadEditor::Canvas::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (owner.staticLayer.isValid())
        g.drawImageAt (owner.staticLayer, 0, 0);
    else
        g.fillAll (col::canvas);
}

void SnareHeadEditor::Canvas::paintOverChildren (juce::Graphics& g)
{
    // XTREME NOISY glueht mit dem Amount
    if (owner.noisyGlow > 0.01f)
    {
        const auto r = juce::Rectangle<float> (20, 522, 236, 408).reduced (6.0f);
        const float a = owner.noisyGlow;
        g.setColour (col::magenta.withAlpha (0.18f * a));
        g.drawRoundedRectangle (r, 14.0f, 10.0f);
        g.setColour (col::magenta.withAlpha (0.65f * a));
        g.drawRoundedRectangle (r, 14.0f, 2.0f);
    }
}

void SnareHeadEditor::refreshPresetBox()
{
    const auto names = proc.getPresetNames();
    if (names.size() != shownPresetCount)
    {
        presetBox.clear (juce::dontSendNotification);
        const int nf = proc.getNumFactoryPresets();
        presetBox.addSectionHeading ("WERK");
        for (int i = 0; i < nf; ++i)
        {
            if (i == 12) presetBox.addSectionHeading ("XTREME NOISY");
            if (i == 20) presetBox.addSectionHeading ("ACASNARE");
            presetBox.addItem (names[i], i + 1);
        }
        if (names.size() > nf)
        {
            presetBox.addSectionHeading ("EIGENE");
            for (int i = nf; i < names.size(); ++i) presetBox.addItem (names[i], i + 1);
        }
        shownPresetCount = names.size();
    }
    const int cur = proc.getCurrentPreset();
    const bool mod = proc.isPresetModified();
    presetBox.setSelectedId (cur + 1, juce::dontSendNotification);
    presetBox.setText (proc.getCurrentPresetName() + (mod ? " *" : ""), juce::dontSendNotification);
    shownPresetName = proc.getCurrentPresetName();
    shownModified = mod;
}

void SnareHeadEditor::savePreset (bool askName)
{
    if (! askName)
    {
        proc.saveUserPreset (proc.getCurrentPresetName());
        refreshPresetBox();
        return;
    }
    auto* w = new juce::AlertWindow ("Preset speichern", "Name fuer dein Preset:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", proc.getCurrentPresetName() + (proc.getCurrentPreset() < proc.getNumFactoryPresets() ? " (eigen)" : ""));
    w->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<SnareHeadEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1) return;
        const auto name = w->getTextEditorContents ("name");
        if (! safe->proc.saveUserPreset (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "BrainBugz Snarehead", "Preset konnte nicht gespeichert werden.");
        safe->refreshPresetBox();
    }), true);
}

void SnareHeadEditor::showPresetMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    const bool isUser = proc.getCurrentPreset() >= proc.getNumFactoryPresets();
    m.addItem (1, "Speichern unter ...");
    m.addItem (2, "Preset loeschen", isUser);
    m.addSeparator();
    m.addItem (3, "Preset-Ordner oeffnen");
    m.addItem (4, "Liste neu laden");
    juce::Component::SafePointer<SnareHeadEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreBtn), [safe] (int r)
    {
        if (safe == nullptr) return;
        auto& pr = safe->proc;
        if (r == 1) safe->savePreset (true);
        else if (r == 2) pr.deleteUserPreset (pr.getCurrentPreset());
        else if (r == 3) { auto f = pr.getPresetFolder(); f.createDirectory(); f.startAsProcess(); }
        else if (r == 4) pr.refreshUserPresets();
        safe->shownPresetCount = -1;
        safe->refreshPresetBox();
    });
}

void SnareHeadEditor::timerCallback()
{
    const int hits = proc.hitCounter.load();
    const bool hit = hits != lastHits;
    lastHits = hits;
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (hit) lastHitTime = now;

    const float level = proc.vizLevel.load();
    mascot.tick (hit, level);
    const float m = std::max (level, audition.meter * 0.9f);
    if (std::abs (m - audition.meter) > 0.0005f) { audition.meter = m; audition.repaint(); }

    const float since = lastHitTime < 0 ? 1e9f : (float) (now - lastHitTime);
    envView.sinceHitMs = since;
    gateView.sinceHitMs = since;
    eqView.repaint();
    envView.repaint();
    gateView.repaint();

    const auto p = proc.readParams();
    verbGate.setDimmed (p.verbSync > 0);
    eqFreq.setDimmed (p.eqLink);

    const float glowTarget = juce::jlimit (0.0f, 1.0f, p.noisy * 1.6f);
    if (std::abs (glowTarget - noisyGlow) > 0.002f)
    {
        noisyGlow += (glowTarget - noisyGlow) * 0.25f;
        canvas.repaint (juce::Rectangle<int> (20, 522, 236, 408).expanded (8));
    }

    if (proc.getCurrentPresetName() != shownPresetName || proc.isPresetModified() != shownModified
        || proc.getPresetNames().size() != shownPresetCount || presetBox.getSelectedId() != proc.getCurrentPreset() + 1)
        refreshPresetBox();
}
