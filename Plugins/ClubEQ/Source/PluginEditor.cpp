#include "PluginEditor.h"
#include "BrainBugzLook.h"
#include "ClubEQAssets.h"

using namespace ceq::ui;

namespace
{
constexpr int kW = 300, kH = 600;
constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;   // 7 Uhr
constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;   // 5 Uhr
constexpr int kRowTop = 190, kRowH = 112;

juce::Font display (float h, bool bold = false) { return bb::fonts::display (h, bold); }
juce::Font mono (float h) { return bb::fonts::mono (h); }
}

//==============================================================================
Look::Look()
{
    setColour (juce::Label::textColourId, col::ink);
    setColour (juce::TextButton::textColourOffId, col::inkDim);
    setColour (juce::TextButton::textColourOnId, col::ink);
}

juce::Font Look::getLabelFont (juce::Label&) { return mono (15.0f); }
juce::Font Look::getTextButtonFont (juce::TextButton&, int) { return display (13.0f, true); }

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (s.getProperties().contains ("xover")) { drawCrossover (g, area, s); return; }

    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float rTrack = size * 0.46f, rCap = size * 0.33f;
    const float centreA = (startAngle + endAngle) * 0.5f;
    const float a = startAngle + pos * (endAngle - startAngle);
    const bool detent = ceq::travel (pos) == 0.0f;

    // Skala: feine Striche, 12 Uhr betont
    for (int i = 0; i <= 10; ++i)
    {
        const float ta = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const bool mid = i == 5;
        const float r0 = rTrack + size * 0.035f, r1 = r0 + size * (mid ? 0.05f : 0.03f);
        juce::Line<float> l (c.getPointOnCircumference (r0, ta), c.getPointOnCircumference (r1, ta));
        g.setColour (mid ? (detent ? col::purpleHi : col::inkDim) : col::inkFaint);
        g.drawLine (l, mid ? 1.6f : 1.0f);
    }

    // Spur
    juce::Path track;
    track.addCentredArc (c.x, c.y, rTrack, rTrack, 0.0f, startAngle, endAngle, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Wertbogen ab 12 Uhr, mit weichem Schein
    if (! detent)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rTrack, rTrack, 0.0f, juce::jmin (a, centreA), juce::jmax (a, centreA), true);
        g.setColour (col::purple.withAlpha (0.16f));
        g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::purple);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Kappe: Schatten, gedrehtes Metall, Kante
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (rCap * 2.1f, rCap * 2.1f).withCentre (c.translated (0.0f, size * 0.03f)));

    auto cap = juce::Rectangle<float> (rCap * 2.0f, rCap * 2.0f).withCentre (c);
    juce::ColourGradient body (juce::Colour (0xff46484f), c.x - rCap * 0.6f, c.y - rCap * 0.8f,
                               juce::Colour (0xff1b1c20), c.x + rCap * 0.5f, c.y + rCap, true);
    g.setGradientFill (body);
    g.fillEllipse (cap);

    auto inner = cap.reduced (rCap * 0.16f);
    juce::ColourGradient face (juce::Colour (0xff34363c), inner.getX(), inner.getY(),
                               juce::Colour (0xff222327), inner.getRight(), inner.getBottom(), false);
    g.setGradientFill (face);
    g.fillEllipse (inner);

    g.setColour (juce::Colours::white.withAlpha (0.07f));   // Lichtkante oben
    juce::Path rim;
    rim.addCentredArc (c.x, c.y, rCap - 0.8f, rCap - 0.8f, 0.0f, -1.1f, 1.1f, true);
    g.strokePath (rim, juce::PathStrokeType (1.2f));
    g.setColour (juce::Colour (0xff0d0e10));
    g.drawEllipse (cap, 1.0f);

    // Zeiger mit leuchtender Spitze
    const auto p0 = c.getPointOnCircumference (rCap * 0.30f, a);
    const auto p1 = c.getPointOnCircumference (rCap * 0.86f, a);
    g.setColour (col::ink.withAlpha (0.92f));
    g.drawLine ({ p0, p1 }, 2.2f);
    const auto tip = c.getPointOnCircumference (rCap * 0.86f, a);
    g.setColour (col::purple.withAlpha (s.isMouseOverOrDragging() ? 0.40f : 0.25f));
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (tip));
    g.setColour (col::purpleHi);
    g.fillEllipse (juce::Rectangle<float> (3.6f, 3.6f).withCentre (tip));
}

void Look::drawCrossover (juce::Graphics& g, juce::Rectangle<float> r, juce::Slider& s)
{
    const bool active = s.isMouseOverOrDragging();
    g.setColour (active ? col::lineSoft.brighter (0.1f) : col::lineSoft);
    g.fillRoundedRectangle (r.reduced (0.5f), 4.0f);
    g.setColour (active ? col::purpleLo : col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (active ? col::ink : col::inkDim);
    g.setFont (mono (13.0f));
    g.drawFittedText (s.getTextFromValue (s.getValue()), r.toNearestInt(), juce::Justification::centred, 1);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? col::purple.withAlpha (0.18f) : (over ? col::lineSoft.brighter (0.08f) : col::lineSoft));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? col::purple : col::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    if (on)   // kleine LED
    {
        g.setColour (col::purple.withAlpha (0.3f));
        g.fillEllipse (r.getX() + 6.0f, r.getCentreY() - 4.5f, 9.0f, 9.0f);
        g.setColour (col::purpleHi);
        g.fillEllipse (r.getX() + 8.5f, r.getCentreY() - 2.0f, 4.0f, 4.0f);
    }
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? col::ink : col::inkDim);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

//==============================================================================
void CurveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float top = 8.0f, bottom = -30.0f;
    const auto& vt = proc.apvts;
    const bool iso = proc.isoMode();
    auto gainOf = [&] (const char* id, int band) { return ceq::positionToGain (vt.getRawParameterValue (id)->load(), iso, band); };
    const float gL = gainOf (ParamIDs::low, ceq::bandLow), gM = gainOf (ParamIDs::mid, ceq::bandMid), gH = gainOf (ParamIDs::hi, ceq::bandHigh);
    const double f1 = vt.getRawParameterValue (ParamIDs::xlow)->load();
    const double f2 = vt.getRawParameterValue (ParamIDs::xhigh)->load();

    auto xOf = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    auto yOf = [&] (float db) { return juce::jmap (juce::jlimit (bottom, top, db), top, bottom, r.getY() + 6.0f, r.getBottom() - 4.0f); };

    // Raster
    g.setColour (col::lineSoft);
    for (double f : { 100.0, 1000.0, 10000.0 })
        g.drawVerticalLine (juce::roundToInt (xOf (f)), r.getY(), r.getBottom());
    for (float db : { 6.0f, -12.0f, -24.0f })
        g.drawHorizontalLine (juce::roundToInt (yOf (db)), r.getX(), r.getRight());
    g.setColour (col::line);
    g.drawHorizontalLine (juce::roundToInt (yOf (0.0f)), r.getX(), r.getRight());

    // Trennfrequenzen
    g.setColour (col::purpleLo.withAlpha (0.55f));
    for (double f : { f1, f2 })
    {
        const float x = xOf (f);
        for (float yy = r.getY(); yy < r.getBottom(); yy += 6.0f)
            g.drawVerticalLine (juce::roundToInt (x), yy, yy + 3.0f);
    }

    juce::Path curve, fill;
    const int n = juce::jmax (2, (int) r.getWidth());
    for (int i = 0; i <= n; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / n);
        const float x = r.getX() + r.getWidth() * (float) i / (float) n;
        const float y = yOf (ceq::responseDb (f, f1, f2, gL, gM, gH));
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, yOf (0.0f)); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (r.getRight(), yOf (0.0f));
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (col::purple.withAlpha (0.20f), 0.0f, r.getY(),
                                             col::purple.withAlpha (0.04f), 0.0f, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col::purple.withAlpha (0.18f));
    g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::purpleHi);
    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (mono (10.0f));
    g.setColour (col::inkFaint);
    g.drawText ("100", juce::Rectangle<float> (xOf (100.0) + 3.0f, r.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::left);
    g.drawText ("1k", juce::Rectangle<float> (xOf (1000.0) + 3.0f, r.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::left);
    g.drawText ("10k", juce::Rectangle<float> (xOf (10000.0) + 3.0f, r.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::left);
}

//==============================================================================
ClubEQEditor::ClubEQEditor (ClubEQProcessor& p)
    : AudioProcessorEditor (p), proc (p), curve (p)
{
    setLookAndFeel (&look);
    setOpaque (true);

    if (auto xml = juce::parseXML (juce::String::fromUTF8 (ClubEQAssets::bb_logo_svg, ClubEQAssets::bb_logo_svgSize)))
        for (auto* e : xml->getChildIterator())
            if (e->hasTagName ("path"))
            {
                auto path = juce::Drawable::parseSVGPath (e->getStringAttribute ("d"));
                path.setUsingNonZeroWinding (false);
                (e->getStringAttribute ("id") == "outline" ? logoOutline : logoLetters) = path;
            }

    const char* ids[]   = { ParamIDs::hi, ParamIDs::mid, ParamIDs::low };
    const char* names[] = { "HI", "MID", "LOW" };
    const char* freqs[] = { "13 kHz", "1 kHz", "70 Hz" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = bands[(size_t) i];
        b.id = ids[i]; b.name = names[i]; b.freq = freqs[i];
        b.knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        b.knob.setRotaryParameters (kStart, kEnd, true);
        b.knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        b.knob.setMouseDragSensitivity (220);
        b.knob.setDoubleClickReturnValue (true, 0.5);
        b.knob.setScrollWheelEnabled (true);
        b.knob.setTooltip ("Drag up/down, double-click = 0 dB");
        addAndMakeVisible (b.knob);
        b.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, b.id, b.knob);

        b.value.setJustificationType (juce::Justification::centredRight);
        b.value.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (b.value);
    }

    for (auto* s : { &xLow, &xHigh })
    {
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setMouseDragSensitivity (300);
        s->getProperties().set ("xover", true);
        addAndMakeVisible (*s);
    }
    xLow.setDoubleClickReturnValue (true, ceq::kDefaultXLow);
    xHigh.setDoubleClickReturnValue (true, ceq::kDefaultXHigh);
    xLowAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::xlow, xLow);
    xHighAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::xhigh, xHigh);

    auto setIso = [this] (bool on)
    {
        auto* prm = proc.apvts.getParameter (ParamIDs::iso);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (on ? 1.0f : 0.0f);
        prm->endChangeGesture();
    };
    eqButton.onClick = [setIso] { setIso (false); };
    isoButton.onClick = [setIso] { setIso (true); };
    eqButton.setTooltip ("DJM-900 curve: -26 dB to +6 dB");
    isoButton.setTooltip ("Isolator curve: full left = band off");
    addAndMakeVisible (eqButton);
    addAndMakeVisible (isoButton);

    curve.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (curve);

    for (auto* id : { ParamIDs::hi, ParamIDs::mid, ParamIDs::low, ParamIDs::iso, ParamIDs::xlow, ParamIDs::xhigh })
        proc.apvts.addParameterListener (id, this);

    setSize (kW, kH);
    handleAsyncUpdate();
}

ClubEQEditor::~ClubEQEditor()
{
    cancelPendingUpdate();
    for (auto* id : { ParamIDs::hi, ParamIDs::mid, ParamIDs::low, ParamIDs::iso, ParamIDs::xlow, ParamIDs::xhigh })
        proc.apvts.removeParameterListener (id, this);
    setLookAndFeel (nullptr);
}

void ClubEQEditor::handleAsyncUpdate()
{
    const bool iso = proc.isoMode();
    for (auto& b : bands)
    {
        const float pos = proc.apvts.getRawParameterValue (b.id)->load();
        b.value.setText (ceq::formatDb (pos, iso), juce::dontSendNotification);
        b.value.setColour (juce::Label::textColourId, ceq::travel (pos) == 0.0f ? col::inkDim : col::ink);
    }
    eqButton.setToggleState (! iso, juce::dontSendNotification);
    isoButton.setToggleState (iso, juce::dontSendNotification);
    curve.repaint();
}

void ClubEQEditor::resized()
{
    curve.setBounds (16, 76, kW - 32, 100);
    for (int i = 0; i < 3; ++i)
    {
        const int y = kRowTop + i * kRowH;
        bands[(size_t) i].knob.setBounds (kW / 2 - 50, y + 6, 100, 100);
        bands[(size_t) i].value.setBounds (kW - 104, y + 62, 88, 20);
    }
    const int fy = kRowTop + 3 * kRowH + 16;
    eqButton.setBounds (16, fy + 18, 60, 26);
    isoButton.setBounds (80, fy + 18, 60, 26);
    xLow.setBounds (kW - 16 - 136, fy + 18, 66, 26);
    xHigh.setBounds (kW - 16 - 66, fy + 18, 66, 26);
    renderBackground();
}

void ClubEQEditor::renderBackground()
{
    // in Bildschirmauflösung vorberechnen (scharf auf HiDPI), danach nur noch kopieren
    bgScale = juce::jmax (1.0f, (float) juce::Component::getApproximateScaleFactorForComponent (this));
    background = juce::Image (juce::Image::ARGB, juce::roundToInt (kW * bgScale), juce::roundToInt (kH * bgScale), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (bgScale));

    g.setGradientFill (juce::ColourGradient (col::bgTop, 0.0f, 0.0f, col::bgBottom, 0.0f, (float) kH, false));
    g.fillAll();

    // feine gebürstete Struktur, einmalig berechnet
    juce::Random rnd (7);
    for (int y = 0; y < kH; y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (0.006f + rnd.nextFloat() * 0.010f));
        g.drawHorizontalLine (y, 0.0f, (float) kW);
    }
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (0, 0.0f, (float) kW);

    // Logo: Buchstaben elfenbein, Umriss violett
    if (! logoLetters.isEmpty())
    {
        auto all = logoLetters.getBounds().getUnion (logoOutline.getBounds());
        auto lr = juce::Rectangle<float> (16.0f, 16.0f, 104.0f, 40.0f);
        auto t = juce::RectanglePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid).getTransformToFit (all, lr);
        g.setColour (col::purple.withAlpha (0.85f));
        g.fillPath (logoOutline, t);
        g.setColour (col::ink.withAlpha (0.92f));
        g.fillPath (logoLetters, t);
    }

    g.setColour (col::ink);
    g.setFont (display (24.0f, true));
    g.drawText ("CLUB EQ", juce::Rectangle<int> (120, 14, kW - 136, 28), juce::Justification::right);
    g.setColour (col::purple);
    g.setFont (mono (11.0f));
    g.drawText ("3-BAND  |  DJM CURVE", juce::Rectangle<int> (120, 42, kW - 136, 14), juce::Justification::right);

    // Kurvenfeld: vertieft
    auto well = juce::Rectangle<float> (12.0f, 72.0f, (float) kW - 24.0f, 108.0f);
    g.setColour (col::well);
    g.fillRoundedRectangle (well, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (well.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) well.getBottom(), well.getX() + 6.0f, well.getRight() - 6.0f);

    // Bandzeilen
    for (int i = 0; i < 3; ++i)
    {
        const int y = kRowTop + i * kRowH;
        if (i > 0)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawHorizontalLine (y, 24.0f, (float) kW - 24.0f);
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.drawHorizontalLine (y + 1, 24.0f, (float) kW - 24.0f);
        }
        g.setColour (col::ink);
        g.setFont (display (22.0f, true));
        g.drawText (bands[(size_t) i].name, juce::Rectangle<int> (20, y + 34, 80, 26), juce::Justification::left);
        g.setColour (col::inkDim);
        g.setFont (mono (12.0f));
        g.drawText (bands[(size_t) i].freq, juce::Rectangle<int> (20, y + 60, 80, 16), juce::Justification::left);
    }

    // Fußleiste
    const int fy = kRowTop + 3 * kRowH + 16;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawHorizontalLine (fy - 8, 16.0f, (float) kW - 16.0f);
    g.setFont (mono (10.0f));
    g.setColour (col::inkFaint);
    g.drawText ("CURVE", juce::Rectangle<int> (16, fy, 120, 14), juce::Justification::left);
    g.drawText ("CROSSOVER", juce::Rectangle<int> (kW - 152, fy, 136, 14), juce::Justification::right);
}

void ClubEQEditor::paint (juce::Graphics& g)
{
    const float sc = juce::jmax (1.0f, (float) juce::Component::getApproximateScaleFactorForComponent (this));
    if (! background.isValid() || std::abs (sc - bgScale) > 0.01f) renderBackground();
    g.drawImageTransformed (background, juce::AffineTransform::scale (1.0f / bgScale));
}
