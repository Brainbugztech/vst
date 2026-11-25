#include "PluginEditor.h"
#include "BoothEQAssets.h"

using namespace booth;

namespace
{
constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;   // 7 Uhr
constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;   // 5 Uhr
}

//==============================================================================
void BoothEQCurve::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f, 2.0f);
    const float top = 8.0f, bottom = -40.0f;
    const auto& vt = proc.apvts;
    const bool iso = proc.isoCurve();

    // Zielwerte (inkl. gedrückter Kills), nicht der Audio-Zustand: Anzeige folgt sofort
    auto knob = [&] (const char* id) { return beq::positionToGain (vt.getRawParameterValue (id)->load(), iso); };
    auto kill = [&] (const char* id) { return vt.getRawParameterValue (id)->load() > 0.5f ? beq::kKillGain : 1.0; };
    const double gL = knob (ParamIDs::low) * kill (ParamIDs::killLow);
    const double gM = knob (ParamIDs::mid) * kill (ParamIDs::killMid);
    const double gH = knob (ParamIDs::high);
    const double f1 = vt.getRawParameterValue (ParamIDs::xlow)->load();
    const double f2 = vt.getRawParameterValue (ParamIDs::xhigh)->load();

    auto xOf = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    auto yOf = [&] (float db) { return juce::jmap (juce::jlimit (bottom, top, db), top, bottom, r.getY() + 4.0f, r.getBottom() - 2.0f); };

    g.setColour (col::lineSoft);
    for (double f : { 100.0, 1000.0, 10000.0 })
        g.drawVerticalLine (juce::roundToInt (xOf (f)), r.getY(), r.getBottom());
    for (float db : { 6.0f, -12.0f, -24.0f, -36.0f })
        g.drawHorizontalLine (juce::roundToInt (yOf (db)), r.getX(), r.getRight());
    g.setColour (col::line);
    g.drawHorizontalLine (juce::roundToInt (yOf (0.0f)), r.getX(), r.getRight());

    g.setColour (accent.lo.withAlpha (0.6f));
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
        const float y = yOf (beq::responseDb (f, f1, f2, gL, gM, gH));
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, yOf (0.0f)); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (r.getRight(), yOf (0.0f));
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (accent.main.withAlpha (0.20f), 0.0f, r.getY(),
                                             accent.main.withAlpha (0.04f), 0.0f, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (accent.main.withAlpha (0.18f));
    g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent.hi);
    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (mono (10.0f));
    g.setColour (col::inkFaint);
    for (auto [f, t] : { std::pair { 100.0, "100" }, std::pair { 1000.0, "1k" }, std::pair { 10000.0, "10k" } })
        g.drawText (t, juce::Rectangle<float> (xOf (f) + 3.0f, r.getBottom() - 13.0f, 30.0f, 12.0f), juce::Justification::left);
}

//==============================================================================
BoothEQEditor::BoothEQEditor (BoothEQProcessor& p)
    : AudioProcessorEditor (p), proc (p), curve (p, accent)
{
    setLookAndFeel (&look);
    setOpaque (true);
    loadLogo (BoothEQAssets::bb_logo_svg, BoothEQAssets::bb_logo_svgSize, logoOutline, logoLetters);

    strip.name = "EQ";
    strip.subtitle = "3-BAND  |  ISOLATOR";
    strip.knobNames = { "LOW", "MID", "HIGH" };
    strip.knobHints = { "< 180 Hz", "180 Hz-2.5k", "> 2.5 kHz" };
    strip.faderNote = "track fader in Live";
    strip.footerLeft = "CURVE";
    strip.footerRight = "CROSSOVER";

    const char* ids[] = { ParamIDs::low, ParamIDs::mid, ParamIDs::high };
    const char* tips[] = { "Low band. Centre = 0 dB (detent), left = kill, right = +6 dB. Double-click resets.",
                           "Mid band. Centre = 0 dB (detent), left = kill, right = +6 dB. Double-click resets.",
                           "High band. Centre = 0 dB (detent), left = kill, right = +6 dB. Double-click resets." };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = bands[(size_t) i];
        b.id = ids[i];
        b.knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        b.knob.setRotaryParameters (kStart, kEnd, true);
        b.knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        b.knob.setMouseDragSensitivity (220);
        b.knob.setDoubleClickReturnValue (true, 0.5);
        b.knob.getProperties().set ("bipolar", true);
        b.knob.setTooltip (tips[i]);
        addAndMakeVisible (b.knob);
        b.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, b.id, b.knob);

        b.value.setJustificationType (juce::Justification::centredRight);
        b.value.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (b.value);
    }

    killLow = std::make_unique<HoldPad> (*proc.apvts.getParameter (ParamIDs::killLow), "LOW KILL", "B1", accent);
    killMid = std::make_unique<HoldPad> (*proc.apvts.getParameter (ParamIDs::killMid), "MID KILL", "B2", accent);
    killLow->setTooltip ("Hold to kill the low band (5 ms fade). Release = back.");
    killMid->setTooltip ("Hold to kill the mid band (5 ms fade). Release = back.");
    addAndMakeVisible (*killLow);
    addAndMakeVisible (*killMid);

    auto setCurve = [this] (int idx)
    {
        auto* prm = proc.apvts.getParameter (ParamIDs::curve);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx));
        prm->endChangeGesture();
    };
    isoButton.onClick = [setCurve] { setCurve (0); };
    eqButton.onClick = [setCurve] { setCurve (1); };
    isoButton.setTooltip ("Isolator curve: full left = band off");
    eqButton.setTooltip ("DJM-style curve: full left = -26 dB");
    addAndMakeVisible (isoButton);
    addAndMakeVisible (eqButton);

    for (auto* s : { &xLow, &xHigh })
    {
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setMouseDragSensitivity (300);
        s->getProperties().set ("chip", true);
        addAndMakeVisible (*s);
    }
    xLow.setDoubleClickReturnValue (true, beq::kDefaultXLow);
    xHigh.setDoubleClickReturnValue (true, beq::kDefaultXHigh);
    xLow.setTooltip ("Low/mid crossover. Drag to change, double-click = 180 Hz.");
    xHigh.setTooltip ("Mid/high crossover. Drag to change, double-click = 2.5 kHz.");
    xLowAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::xlow, xLow);
    xHighAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::xhigh, xHigh);

    help = std::make_unique<HelpToggle> (proc.apvts.state, *this, accent);
    addAndMakeVisible (*help);

    curve.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (curve);

    for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::high, ParamIDs::killLow, ParamIDs::killMid,
                      ParamIDs::curve, ParamIDs::xlow, ParamIDs::xhigh })
        proc.apvts.addParameterListener (id, this);

    setSize (grid::width, grid::height);
    handleAsyncUpdate();
}

BoothEQEditor::~BoothEQEditor()
{
    cancelPendingUpdate();
    for (auto* id : { ParamIDs::low, ParamIDs::mid, ParamIDs::high, ParamIDs::killLow, ParamIDs::killMid,
                      ParamIDs::curve, ParamIDs::xlow, ParamIDs::xhigh })
        proc.apvts.removeParameterListener (id, this);
    help.reset();
    setLookAndFeel (nullptr);
}

void BoothEQEditor::handleAsyncUpdate()
{
    const bool iso = proc.isoCurve();
    const bool kills[] = { proc.apvts.getRawParameterValue (ParamIDs::killLow)->load() > 0.5f,
                           proc.apvts.getRawParameterValue (ParamIDs::killMid)->load() > 0.5f, false };
    for (size_t i = 0; i < bands.size(); ++i)
    {
        auto& b = bands[i];
        const float pos = proc.apvts.getRawParameterValue (b.id)->load();
        b.value.setText (kills[i] ? "KILL" : beq::formatDb (pos, iso), juce::dontSendNotification);
        b.value.setColour (juce::Label::textColourId, kills[i] ? accent.hi : (beq::travel (pos) == 0.0f ? col::inkDim : col::ink));
    }
    isoButton.setToggleState (iso, juce::dontSendNotification);
    eqButton.setToggleState (! iso, juce::dontSendNotification);
    curve.repaint();
}

void BoothEQEditor::resized()
{
    curve.setBounds (grid::screen.reduced (4));
    for (int i = 0; i < 3; ++i)
    {
        bands[(size_t) i].knob.setBounds (grid::knob (i));
        bands[(size_t) i].value.setBounds (grid::knobValue (i));
    }
    killLow->setBounds (grid::pad1);
    killMid->setBounds (grid::pad2);

    const int fy = grid::footerTop + 18;
    isoButton.setBounds (16, fy, 46, 26);
    eqButton.setBounds (66, fy, 46, 26);
    help->setBounds (123, fy + 1, 24, 24);
    xLow.setBounds (grid::width - 16 - 128, fy, 62, 26);
    xHigh.setBounds (grid::width - 16 - 62, fy, 62, 26);
    background = {};
}

void BoothEQEditor::paint (juce::Graphics& g)
{
    const float sc = juce::jmax (1.0f, (float) juce::Component::getApproximateScaleFactorForComponent (this));
    if (! background.isValid() || std::abs (sc - bgScale) > 0.01f)
    {
        bgScale = sc;
        background = renderBackground (strip, accent, logoOutline, logoLetters, bgScale);
    }
    g.drawImageTransformed (background, juce::AffineTransform::scale (1.0f / bgScale));
}
