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
        if (cents != 0) s << (cents > 0 ? " +" : " ") << cents << "ct";
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

    juce::Font display (float h, bool bold = false) { return bb::fonts::display (h, bold); }
    juce::Font mono (float h) { return bb::fonts::mono (h); }

    void setParam (juce::AudioProcessorValueTreeState& s, const char* id, float plain)
    {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }
    void gesture (juce::AudioProcessorValueTreeState& s, const char* id, bool begin)
    {
        if (auto* p = s.getParameter (id)) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); }
    }

    // kleiner Tropfen: Stiel, der unten in eine Kugel uebergeht
    void drawDrip (juce::Graphics& g, float x, float top, float len, float w, juce::Colour c)
    {
        const float stem = w * 0.55f, bulb = w;
        juce::Path p;
        p.startNewSubPath (x - stem * 0.9f, top);
        p.quadraticTo (x - stem * 0.5f, top + len * 0.35f, x - stem * 0.5f, top + len - bulb * 0.3f);
        p.lineTo (x + stem * 0.5f, top + len - bulb * 0.3f);
        p.quadraticTo (x + stem * 0.5f, top + len * 0.35f, x + stem * 0.9f, top);
        p.closeSubPath();
        p.addEllipse (x - bulb * 0.5f, top + len - bulb * 0.55f, bulb, bulb * 1.1f);
        g.setColour (c);
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.35f * c.getFloatAlpha()));
        g.fillEllipse (x - bulb * 0.28f, top + len - bulb * 0.35f, bulb * 0.24f, bulb * 0.28f);
    }
}

juce::Font sh::dripFont (float height)
{
    static juce::Typeface::Ptr tf = []
    {
        int size = 0;
        if (auto* data = SnareHeadAssets::getNamedResource ("CreepsterRegular_ttf", size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        return juce::Typeface::Ptr();
    }();
    if (tf != nullptr)
        return juce::Font (juce::FontOptions (tf).withHeight (height));
    return display (height * 0.85f, true);
}

// ================================================================ LookAndFeel

sh::LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::magenta);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::headerTextColourId, col::magenta);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::deep);
    setColour (juce::PopupMenu::highlightedTextColourId, col::ink);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::ink);
    setColour (juce::AlertWindow::outlineColourId, col::purple);
    setColour (juce::TextEditor::backgroundColourId, col::bg);
    setColour (juce::TextEditor::textColourId, col::ink);
    setColour (juce::TextEditor::outlineColourId, col::line);
    setColour (juce::TextEditor::focusedOutlineColourId, col::magenta);
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, col::magenta);
}

void sh::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float, float, juce::Slider& s)
{
    const float size = (float) std::min (w, h);
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float r = size * 0.40f, thick = size * 0.07f;
    const float a = startAngle + pos * (endAngle - startAngle);
    const bool noisyKnob = s.getProperties().getWithDefault ("noisy", false);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.003f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, a, true);
        const auto hot = noisyKnob ? col::magenta.interpolatedWith (juce::Colour (0xffff7a2f), pos) : col::magenta;
        juce::ColourGradient grad (col::purple, c.x - r, c.y + r, hot, c.x + r, c.y - r, false);
        g.setGradientFill (grad);
        g.setOpacity (0.28f);
        g.strokePath (arc, juce::PathStrokeType (thick * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setOpacity (1.0f);
        g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Bonbon
    const float rad = size * 0.31f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - rad * 0.95f, c.y - rad * 0.8f, rad * 1.9f, rad * 2.05f);

    const auto mid = noisyKnob ? col::purple.interpolatedWith (col::magenta, 0.35f) : col::purple;
    juce::ColourGradient candy (col::lilac, c.x - rad * 0.45f, c.y - rad * 0.55f, col::deep, c.x + rad * 0.7f, c.y + rad * 0.9f, true);
    candy.addColour (0.45, mid);
    g.setGradientFill (candy);
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);

    // Wirbel, dreht sich mit dem Wert
    for (int i = 0; i < 3; ++i)
    {
        const float s0 = a + (float) i * 2.0f * kPi / 3.0f;
        juce::Path sw;
        sw.addCentredArc (c.x, c.y, rad * 0.6f, rad * 0.6f, 0.0f, s0, s0 + 1.1f, true);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (sw, juce::PathStrokeType (rad * 0.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.fillEllipse (c.x - rad * 0.62f, c.y - rad * 0.7f, rad * 0.55f, rad * 0.32f);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, std::max (1.2f, size * 0.02f));

    // Zeiger
    const juce::Point<float> tip (c.x + std::sin (a) * rad * 0.7f, c.y - std::cos (a) * rad * 0.7f);
    const float d = std::max (3.0f, size * 0.07f);
    g.setColour (col::magenta.withAlpha (0.45f));
    g.fillEllipse (tip.x - d, tip.y - d, d * 2.0f, d * 2.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (tip.x - d * 0.55f, tip.y - d * 0.55f, d * 1.1f, d * 1.1f);
}

void sh::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = std::min (8.0f, r.getHeight() * 0.35f);
    const bool on = b.getToggleState();
    juce::ColourGradient grad (on ? col::deep.brighter (0.3f) : juce::Colour (0xff2a1840), 0, r.getY(),
                               on ? col::deep : juce::Colour (0xff150b22), 0, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, rad);
    if (on)
    {
        g.setColour (col::magenta.withAlpha (0.25f));
        g.drawRoundedRectangle (r.expanded (1.5f), rad + 1.5f, 3.0f);
    }
    g.setColour (on ? col::magenta : over ? col::purple : col::line);
    g.drawRoundedRectangle (r, rad, 1.3f);
    g.setColour (juce::Colours::white.withAlpha (down ? 0.04f : 0.1f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + rad, r.getRight() - rad);
}

void sh::LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool small = (bool) b.getProperties().getWithDefault ("small", false);
    g.setFont (small ? mono ((float) b.getHeight() * 0.82f) : display ((float) b.getHeight() * 0.5f, true));
    g.setColour (b.getToggleState() ? col::magenta : col::ink);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void sh::LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.isMouseOver (true) ? col::purple : col::line);
    g.drawRoundedRectangle (r, 7.0f, 1.3f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (col::magenta);
    g.fillPath (arrow);
}

juce::Font sh::LookAndFeel::getComboBoxFont (juce::ComboBox& b) { return display ((float) b.getHeight() * 0.52f, true); }
juce::Font sh::LookAndFeel::getPopupMenuFont() { return display (16.0f); }

void sh::LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
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
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (big ? 260 : 200);
    slider.setVelocityBasedMode (false);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    // Wert anklicken = eintippen
    valueLabel.setEditable (true, true, false);
    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (mono (isBig ? 16.0f : 13.5f));
    valueLabel.setColour (juce::Label::textColourId, isBig ? col::lilac : col::ink);
    valueLabel.setColour (juce::Label::textWhenEditingColourId, col::ink);
    valueLabel.setColour (juce::Label::backgroundWhenEditingColourId, col::bg);
    valueLabel.setColour (juce::Label::outlineWhenEditingColourId, col::magenta);
    valueLabel.setMouseCursor (juce::MouseCursor::IBeamCursor);
    valueLabel.setTooltip ("Klicken und Wert eintippen");
    valueLabel.onEditorShow = [this]
    {
        if (auto* ed = valueLabel.getCurrentTextEditor())
        {
            ed->setJustification (juce::Justification::centred);
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
    addAndMakeVisible (noteBtn);
    refreshValue();
    resized();
}

void Knob::refreshValue()
{
    if (param == nullptr) return;
    const bool note = isNoteMode();
    juce::String txt = note ? noteNameForHz ((float) slider.getValue(), true) : param->getCurrentValueAsText();
    if (! valueLabel.isBeingEdited())
        valueLabel.setText (txt, juce::dontSendNotification);
    valueLabel.setAlpha (dimmed ? 0.35f : 1.0f);
    if (proc != nullptr)
    {
        noteBtn.setButtonText (note ? "NOTE" : "Hz");
        noteBtn.setToggleState (note, juce::dontSendNotification);
    }
    repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (isBig ? 20 : 16);
    valueLabel.setBounds (r.removeFromBottom (isBig ? 20 : 16));
    slider.setBounds (r);
    if (proc != nullptr)
        noteBtn.setBounds (top.getRight() - 42, top.getY(), 42, top.getHeight());
}

void Knob::paint (juce::Graphics& g)
{
    const float a = dimmed ? 0.35f : 1.0f;
    auto r = getLocalBounds();
    auto top = r.removeFromTop (isBig ? 20 : 16);
    if (proc != nullptr) top = top.withTrimmedRight (34);
    g.setColour (col::inkDim.withAlpha (a));
    g.setFont (display (isBig ? 15.0f : 12.5f, true));
    g.drawFittedText (name.toUpperCase(), top, juce::Justification::centred, 1);
}

// ================================================================ EqView

float EqView::hzToX (float hz) const
{
    const float lo = std::log (520.0f), hi = std::log (2100.0f);
    return (std::log (hz) - lo) / (hi - lo) * (float) getWidth();
}
float EqView::xToHz (float x) const
{
    const float lo = std::log (520.0f), hi = std::log (2100.0f);
    return std::exp (lo + juce::jlimit (0.0f, 1.0f, x / (float) getWidth()) * (hi - lo));
}

void EqView::paint (juce::Graphics& g)
{
    const auto p = proc.readParams();
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 8.0f);

    const float top = 16.0f, bottom = r.getHeight() - 16.0f;
    auto dbToY = [&] (float dbv) { return juce::jmap (dbv, -3.0f, 20.0f, bottom, top); };

    // Boost-Bereich 700 Hz - 1,6 kHz
    const float x0 = hzToX (700.0f), x1 = hzToX (1600.0f);
    g.setColour (col::purple.withAlpha (0.07f));
    g.fillRect (x0, 0.0f, x1 - x0, r.getHeight());

    g.setFont (mono (11.0f));
    for (float f : { 600.0f, 700.0f, 800.0f, 1000.0f, 1200.0f, 1600.0f, 2000.0f })
    {
        const float x = hzToX (f);
        g.setColour (col::line.withAlpha (0.8f));
        g.drawVerticalLine ((int) x, top, bottom);
        g.setColour (col::inkDim.withAlpha (0.7f));
        g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 1) + "k" : juce::String ((int) f), (int) x - 20, (int) bottom + 1, 40, 14,
                    juce::Justification::centred);
    }
    g.setColour (col::line);
    g.drawHorizontalLine ((int) dbToY (0.0f), 0.0f, r.getWidth());

    // Ring-Teiltoene
    float ratio[snarehead::Voice::kPartials], amp[snarehead::Voice::kPartials];
    snarehead::Voice::ringPartials (p, ratio, amp);
    const float tune = proc.lastNoteHz.load() > 0 && p.keyTrack ? proc.lastNoteHz.load() : p.tune;
    for (int k = 0; k < snarehead::Voice::kPartials; ++k)
    {
        const float f = tune * ratio[k];
        if (f < 520.0f || f > 2100.0f) continue;
        const float x = hzToX (f);
        const float hgt = juce::jlimit (0.15f, 1.0f, amp[k] * 4.0f) * (bottom - top) * 0.75f * p.ringLevel;
        g.setColour (col::lilac.withAlpha (0.18f));
        g.fillRect (x - 3.0f, bottom - hgt, 6.0f, hgt);
        g.setColour (col::lilac.withAlpha (0.75f));
        g.fillRect (x - 0.75f, bottom - hgt, 1.5f, hgt);
    }

    // Glocke
    const float fc = p.eqLink ? tune : p.eqFreq;
    juce::Path curve, fill;
    for (int i = 0; i <= getWidth(); i += 2)
    {
        const float x = (float) i;
        const float y = dbToY (peakingDb (xToHz (x), fc, p.eqQ, p.eqGain));
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, dbToY (0.0f)); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (r.getWidth(), dbToY (0.0f));
    fill.closeSubPath();
    juce::ColourGradient grad (col::magenta.withAlpha (0.45f), 0, top, col::purple.withAlpha (0.05f), 0, dbToY (0.0f), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (col::magenta.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour (col::magenta);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Griff
    const juce::Point<float> h (hzToX (fc), dbToY (p.eqGain));
    g.setColour (col::magenta.withAlpha (0.35f));
    g.fillEllipse (h.x - 10, h.y - 10, 20, 20);
    g.setColour (juce::Colours::white);
    g.fillEllipse (h.x - 5, h.y - 5, 10, 10);

    // Info
    g.setFont (display (13.0f, true));
    g.setColour (col::ink);
    juce::String info = noteName (fc);
    if (p.eqLink) info = "LINK > TUNE  " + info;
    g.drawText (info, 8, 2, getWidth() - 16, 16, juce::Justification::centredLeft);
    g.setColour (col::inkDim);
    g.setFont (mono (12.0f));
    g.drawText ("RING " + noteName (tune), 8, 2, getWidth() - 16, 16, juce::Justification::centredRight);
}

void EqView::setFromMouse (juce::Point<float> pt)
{
    const auto p = proc.readParams();
    if (! p.eqLink)
        setParam (proc.apvts, ParamIDs::eqFreq, juce::jlimit (700.0f, 1600.0f, xToHz (pt.x)));
    const float top = 16.0f, bottom = (float) getHeight() - 16.0f;
    setParam (proc.apvts, ParamIDs::eqGain, juce::jlimit (0.0f, 18.0f, juce::jmap (pt.y, bottom, top, -3.0f, 20.0f)));
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

void GateView::paint (juce::Graphics& g)
{
    const auto p = proc.readParams();
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 8.0f);

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

    const float top = 18.0f, bottom = r.getHeight() - 16.0f;
    auto tx = [&] (float t) { return 6.0f + t / spanMs * (r.getWidth() - 12.0f); };
    auto vy = [&] (float v) { return bottom - v * (bottom - top); };

    // Raster
    const float step = spanMs > 1200 ? 250.0f : spanMs > 500 ? 100.0f : 50.0f;
    g.setFont (mono (10.5f));
    for (float t = 0; t <= spanMs; t += step)
    {
        g.setColour (col::line.withAlpha (0.7f));
        g.drawVerticalLine ((int) tx (t), top, bottom);
        g.setColour (col::inkDim.withAlpha (0.6f));
        g.drawText (juce::String ((int) t), (int) tx (t) - 18, (int) bottom + 1, 36, 13, juce::Justification::centred);
    }

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

    auto area = [&] (auto fn, juce::Colour c)
    {
        juce::Path path;
        path.startNewSubPath (tx (0), vy (0));
        for (int i = 0; i <= 240; ++i)
        {
            const float t = spanMs * (float) i / 240.0f;
            path.lineTo (tx (t), vy (fn (t)));
        }
        path.lineTo (tx (spanMs), vy (0));
        path.closeSubPath();
        juce::ColourGradient grad (c.withAlpha (0.55f), 0, top, c.withAlpha (0.08f), 0, bottom, false);
        g.setGradientFill (grad);
        g.fillPath (path);
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (1.6f));
    };

    if (wetAmt > 0.001f) area (wet, col::magenta);
    area (dry, col::purple.brighter (0.3f));

    if (gateOn)
    {
        const float gx = tx (gateMs), cx = tx (gateMs + cutMs);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (cx, top - 4.0f, r.getWidth() - cx, bottom - top + 4.0f);
        g.setColour (col::magenta.withAlpha (0.35f));
        g.fillRect (gx - 3.0f, top - 4.0f, 6.0f, bottom - top + 4.0f);
        g.setColour (col::magenta);
        g.fillRect (gx - 1.0f, top - 4.0f, 2.0f, bottom - top + 4.0f);
        const juce::String lbl = p.verbSync > 0 ? juce::String ("GATE ") + snarehead::kSyncNames[p.verbSync]
                                                : "GATE " + juce::String ((int) std::round (gateMs)) + " ms";
        g.setFont (display (12.5f, true));
        const bool leftSide = gx > r.getWidth() * 0.62f;
        g.setColour (col::magenta);
        if (leftSide) g.drawText (lbl, (int) gx - 112, 1, 106, 16, juce::Justification::centredRight);
        else          g.drawText (lbl, (int) gx + 6, 1, 106, 16, juce::Justification::centredLeft);
    }

    g.setFont (display (12.5f, true));
    g.setColour (col::inkDim);
    if (reverb)
    {
        if (! gateOn)
        {
            g.drawText ("GATE OFF", 8, 1, 100, 16, juce::Justification::centredLeft);
            g.drawText ("ziehen = Gate", getWidth() - 106, 1, 98, 16, juce::Justification::centredRight);
        }
    }
    else
        g.drawText (juce::String ((int) std::round (dryLen)) + " ms", 8, 1, 100, 16, juce::Justification::centredLeft);

    if (sinceHitMs < spanMs)
    {
        g.setColour (col::ink.withAlpha (0.8f));
        g.drawVerticalLine ((int) tx (sinceHitMs), top, bottom);
    }
}

void GateView::setGateFromX (float x)
{
    if (! reverb) return;
    const float t = (x - 6.0f) / ((float) getWidth() - 12.0f) * spanMs;
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

// ================================================================ LogoView

LogoView::LogoView (SnareHeadProcessor& p) : proc (p)
{
    logo = juce::ImageCache::getFromMemory (SnareHeadAssets::logo_png, SnareHeadAssets::logo_pngSize);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LogoView::tick (bool hit, float level)
{
    const float noisy = proc.readParams().noisy;
    if (hit)
    {
        pulse = 1.0f;
        glow = 1.0f;
        const int n = 2 + rng.nextInt (3) + (int) (noisy * 4.0f);
        for (int i = 0; i < n; ++i)
            drops.push_back ({ (0.3f + 0.4f * rng.nextFloat()) * (float) getWidth(),
                               (0.74f + 0.08f * rng.nextFloat()) * (float) getHeight(),
                               0.5f + rng.nextFloat() * 1.5f, 3.0f + rng.nextFloat() * 4.0f, 1.0f });
    }
    const float shake = noisy * pulse * 7.0f;
    shakeX = (rng.nextFloat() * 2.0f - 1.0f) * shake;
    shakeY = (rng.nextFloat() * 2.0f - 1.0f) * shake;
    rot = (rng.nextFloat() * 2.0f - 1.0f) * noisy * pulse * 0.04f;
    pulse *= 0.84f;
    glow = std::max (glow * 0.9f, std::min (1.0f, level * 1.3f));
    for (auto& d : drops) { d.vy += 0.45f; d.y += d.vy; d.life -= 0.012f; }
    drops.erase (std::remove_if (drops.begin(), drops.end(), [this] (const Drop& d) { return d.y > (float) getHeight() + 10.0f || d.life <= 0.0f; }),
                 drops.end());
    repaint();
}

void LogoView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float base = std::min (r.getWidth(), r.getHeight());

    juce::ColourGradient halo (col::magenta.withAlpha (0.1f + 0.4f * glow), c.x, c.y,
                               col::magenta.withAlpha (0.0f), c.x + base * 0.55f, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (r.reduced (-base * 0.05f));

    for (const auto& d : drops)
    {
        const float a = juce::jlimit (0.0f, 1.0f, d.life);
        const float stretch = 1.0f + std::min (1.2f, d.vy * 0.08f);
        g.setColour (col::purple.brighter (0.3f).withAlpha (a));
        g.fillEllipse (d.x - d.r, d.y - d.r * stretch, d.r * 2.0f, d.r * 2.0f * stretch);
        g.setColour (juce::Colours::white.withAlpha (0.4f * a));
        g.fillEllipse (d.x - d.r * 0.5f, d.y - d.r * stretch * 0.6f, d.r * 0.6f, d.r * 0.7f);
    }

    if (logo.isValid())
    {
        const float s = base * 0.98f * (1.0f + 0.055f * pulse) / (float) logo.getWidth();
        auto t = juce::AffineTransform::translation (-(float) logo.getWidth() * 0.5f, -(float) logo.getHeight() * 0.5f)
                     .scaled (s).rotated (rot).translated (c.x + shakeX, c.y + shakeY);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImageTransformed (logo, t);
    }
}

void LogoView::mouseDown (const juce::MouseEvent& e)
{
    // weiter unten geklickt = haerter angeschlagen
    const float vel = juce::jlimit (0.3f, 1.0f, 0.45f + 0.6f * e.position.y / (float) getHeight());
    proc.audition (vel);
}

// ================================================================ Editor

SnareHeadEditor::SnareHeadEditor (SnareHeadProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      logo (p), eqView (p), envView (p, false), gateView (p, true),
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
    setLookAndFeel (&lnf);
    noisy.slider.getProperties().set ("noisy", true);
    tune.enableNoteMode (p, "tuneNoteMode");
    eqFreq.enableNoteMode (p, "eqNoteMode");


    for (auto* c : std::initializer_list<juce::Component*> { &logo, &eqView, &envView, &gateView,
             &tune, &spread, &metal, &punch, &ring, &noise, &noiseTone, &hold, &release,
             &eqFreq, &eqGain, &eqQ, &verbSize, &verbMix, &verbGate, &verbCut, &noisy, &width, &volume,
             &linkBtn, &keyBtn, &syncBox, &presetBox, &prevBtn, &nextBtn, &saveBtn, &moreBtn })
        addAndMakeVisible (c);

    linkBtn.setClickingTogglesState (true);
    keyBtn.setClickingTogglesState (true);
    linkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::eqLink, linkBtn);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::keyTrack, keyBtn);
    linkBtn.setTooltip ("Res-EQ folgt dem Ring-Ton");
    keyBtn.setTooltip ("MIDI-Note stimmt die Snare um (C3 = Tune)");

    for (int i = 0; i < 10; ++i)
        syncBox.addItem (i == 0 ? juce::String ("Gate frei (ms)") : juce::String ("Sync ") + snarehead::kSyncNames[i], i + 1);
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

    setSize (1040, 660);
    refreshPresetBox();
    startTimerHz (60);
}

SnareHeadEditor::~SnareHeadEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
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
            if (i == 20) presetBox.addSectionHeading ("ACAJOU");
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
    const auto text = proc.getCurrentPresetName() + (mod ? " *" : "");
    presetBox.setText (text, juce::dontSendNotification);
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
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Snare Head", "Preset konnte nicht gespeichert werden.");
        safe->refreshPresetBox();
    }), true);
}

void SnareHeadEditor::showPresetMenu()
{
    juce::PopupMenu m;
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
    if (hit) { lastHitTime = now; stickerPulse = 1.0f; }

    const float level = proc.vizLevel.load();
    logo.tick (hit, level);
    meter = std::max (level, meter * 0.9f);
    stickerPulse *= 0.88f;

    const float since = lastHitTime < 0 ? 1e9f : (float) (now - lastHitTime);
    envView.sinceHitMs = since;
    gateView.sinceHitMs = since;
    eqView.repaint();
    envView.repaint();
    gateView.repaint();

    const auto p = proc.readParams();
    const bool gateDim = p.verbSync > 0;
    if (verbGate.dimmed != gateDim) { verbGate.dimmed = gateDim; verbGate.refreshValue(); }
    if (eqFreq.dimmed != p.eqLink) { eqFreq.dimmed = p.eqLink; eqFreq.refreshValue(); }

    if (proc.getCurrentPresetName() != shownPresetName || proc.isPresetModified() != shownModified
        || proc.getPresetNames().size() != shownPresetCount || presetBox.getSelectedId() != proc.getCurrentPreset() + 1)
        refreshPresetBox();

    repaint (rSticker.expanded (24));
    repaint (rOut);
}

void SnareHeadEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, bool hot)
{
    auto r = area.toFloat();
    juce::ColourGradient grad (col::panel2, 0, r.getY(), col::panel, 0, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour ((hot ? col::magenta : col::purple).withAlpha (0.18f));
    g.drawRoundedRectangle (r.expanded (1.5f), 13.5f, 3.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r, 12.0f, 1.3f);

    g.setFont (dripFont (24.0f));
    const auto tr = juce::Rectangle<float> (r.getX() + 14, r.getY() + 4, r.getWidth() - 28, 28);
    g.setColour ((hot ? col::magenta : col::purple).withAlpha (0.5f));
    g.drawText (title, tr.translated (0, 2), juce::Justification::centredLeft);
    g.setColour (hot ? col::magenta : col::lilac);
    g.drawText (title, tr, juce::Justification::centredLeft);
}

void SnareHeadEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (col::bg);
    juce::ColourGradient neb (col::deep.withAlpha (0.55f), (float) rLogo.getCentreX(), (float) rLogo.getCentreY(),
                              col::bg.withAlpha (0.0f), (float) rLogo.getCentreX() + 620.0f, (float) rLogo.getCentreY(), true);
    g.setGradientFill (neb);
    g.fillRect (b);

    // Sterne
    juce::Random rnd (7);
    for (int i = 0; i < 110; ++i)
    {
        const float x = rnd.nextFloat() * b.getWidth(), y = rnd.nextFloat() * b.getHeight(), s = 0.6f + rnd.nextFloat() * 1.6f;
        g.setColour (col::lilac.withAlpha (0.15f + rnd.nextFloat() * 0.45f));
        g.fillEllipse (x, y, s, s);
    }

    // Kopfzeile
    g.setFont (dripFont (30.0f));
    g.setColour (col::magenta.withAlpha (0.45f));
    g.drawText ("SNARE HEAD", 18, 12, 300, 36, juce::Justification::centredLeft);
    g.setColour (col::lilac);
    g.drawText ("SNARE HEAD", 16, 10, 300, 36, juce::Justification::centredLeft);
    g.setFont (mono (12.0f));
    g.setColour (col::inkDim);
    g.drawText ("BRAIN BUGZ  //  SNARE SYNTH  v0.3", 200, 19, 260, 18, juce::Justification::centredLeft);

    drawPanel (g, rRing, "Ring");
    drawPanel (g, rNoise, "Noise");
    drawPanel (g, rShape, "Shape");
    drawPanel (g, rEq, "Res EQ");
    drawPanel (g, rVerb, "Reverb");
    drawPanel (g, rNoisy, "Xtreme Noisy", proc.readParams().noisy > 0.3f);
    drawPanel (g, rOut, "Out");

    g.setFont (mono (11.0f));
    g.setColour (col::inkDim);
    g.drawFittedText ("Comb  Ringmod\nDrive  Crush", rNoisy.withTrimmedTop (196).withHeight (34).reduced (8, 0), juce::Justification::centred, 2);

    // Pegel
    const auto mr = juce::Rectangle<float> ((float) rOut.getX() + 12, (float) rOut.getBottom() - 30, (float) rOut.getWidth() - 24, 10);
    g.setColour (col::bg);
    g.fillRoundedRectangle (mr, 4.0f);
    const float lv = juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (meter, -48.0f), -48.0f, 0.0f, 0.0f, 1.0f));
    juce::ColourGradient mg (col::purple, mr.getX(), 0, col::magenta, mr.getRight(), 0, false);
    g.setGradientFill (mg);
    g.fillRoundedRectangle (mr.withWidth (mr.getWidth() * lv), 4.0f);
    g.setColour (col::inkDim);
    g.drawText ("Logo = Anspielen", rOut.getX(), rOut.getBottom() - 18, rOut.getWidth(), 14, juce::Justification::centred);
}

void SnareHeadEditor::paintOverChildren (juce::Graphics& g)
{
    // XTREME NOISY Sticker
    const float amt = proc.readParams().noisy;
    if (amt < 0.3f) return;
    const auto r = rSticker.toFloat();
    const float s = 1.0f + 0.08f * stickerPulse;
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (-0.16f, r.getCentreX(), r.getCentreY())
                        .scaled (s, s, r.getCentreX(), r.getCentreY()));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (3, 4), 9.0f);
    juce::ColourGradient grad (juce::Colour (0xffff7a2f), r.getX(), r.getY(), col::magenta, r.getRight(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 9.0f);
    drawDrip (g, r.getX() + r.getWidth() * 0.22f, r.getBottom() - 1, 10.0f, 7.0f, col::magenta);
    drawDrip (g, r.getX() + r.getWidth() * 0.71f, r.getBottom() - 1, 15.0f, 6.0f, col::magenta);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 9.0f, 3.0f);
    g.setFont (dripFont (r.getHeight() * 0.62f));
    g.setColour (juce::Colours::black);
    g.drawText ("XTREME NOISY", r.translated (1.5f, 2.0f), juce::Justification::centred);
    g.setColour (juce::Colours::white);
    g.drawText ("XTREME NOISY", r, juce::Justification::centred);
}

void SnareHeadEditor::resized()
{
    // Kopfzeile: Presets
    presetBox.setBounds (612, 14, 250, 32);
    prevBtn.setBounds (574, 14, 34, 32);
    nextBtn.setBounds (866, 14, 34, 32);
    saveBtn.setBounds (906, 14, 70, 32);
    moreBtn.setBounds (980, 14, 44, 32);

    rLogo = { 16, 60, 320, 324 };
    rNoisy = { 16, 396, 150, 248 };
    rOut = { 174, 396, 162, 248 };
    rRing = { 350, 64, 340, 226 };
    rNoise = { 698, 64, 140, 226 };
    rShape = { 846, 64, 178, 226 };
    rEq = { 350, 302, 340, 342 };
    rVerb = { 698, 302, 326, 342 };
    rSticker = { 196, 340, 150, 40 };

    logo.setBounds (rLogo);

    // Ring
    tune.setBounds (rRing.getX() + 8, rRing.getY() + 44, 118, 160);
    const int gx = rRing.getX() + 132, gy = rRing.getY() + 36;
    spread.setBounds (gx, gy, 100, 92);
    metal.setBounds (gx + 102, gy, 100, 92);
    punch.setBounds (gx, gy + 94, 100, 92);
    ring.setBounds (gx + 102, gy + 94, 100, 92);

    // Noise
    noise.setBounds (rNoise.getX() + 20, rNoise.getY() + 36, 100, 92);
    noiseTone.setBounds (rNoise.getX() + 20, rNoise.getY() + 130, 100, 92);

    // Shape
    envView.setBounds (rShape.getX() + 10, rShape.getY() + 38, rShape.getWidth() - 20, 82);
    hold.setBounds (rShape.getX() + 4, rShape.getY() + 126, 85, 96);
    release.setBounds (rShape.getX() + 89, rShape.getY() + 126, 85, 96);

    // Res EQ
    eqView.setBounds (rEq.getX() + 10, rEq.getY() + 38, rEq.getWidth() - 20, 150);
    const int ew = (rEq.getWidth() - 20) / 3;
    eqFreq.setBounds (rEq.getX() + 10, rEq.getY() + 194, ew, 104);
    eqGain.setBounds (rEq.getX() + 10 + ew, rEq.getY() + 194, ew, 104);
    eqQ.setBounds (rEq.getX() + 10 + 2 * ew, rEq.getY() + 194, ew, 104);
    linkBtn.setBounds (rEq.getX() + 10, rEq.getY() + 304, rEq.getWidth() - 20, 28);

    // Reverb
    gateView.setBounds (rVerb.getX() + 10, rVerb.getY() + 38, rVerb.getWidth() - 20, 150);
    const int vw = (rVerb.getWidth() - 20) / 4;
    verbSize.setBounds (rVerb.getX() + 10, rVerb.getY() + 194, vw, 104);
    verbMix.setBounds (rVerb.getX() + 10 + vw, rVerb.getY() + 194, vw, 104);
    verbGate.setBounds (rVerb.getX() + 10 + 2 * vw, rVerb.getY() + 194, vw, 104);
    verbCut.setBounds (rVerb.getX() + 10 + 3 * vw, rVerb.getY() + 194, vw, 104);
    syncBox.setBounds (rVerb.getX() + 10, rVerb.getY() + 304, rVerb.getWidth() - 20, 28);

    // Xtreme Noisy + Out
    noisy.setBounds (rNoisy.getX() + 10, rNoisy.getY() + 40, rNoisy.getWidth() - 20, 152);
    width.setBounds (rOut.getX() + 6, rOut.getY() + 38, 75, 100);
    volume.setBounds (rOut.getX() + 81, rOut.getY() + 38, 75, 100);
    keyBtn.setBounds (rOut.getX() + 12, rOut.getY() + 150, rOut.getWidth() - 24, 28);

}
