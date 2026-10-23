#include "SidePanels.h"

namespace bfz::ui
{

// ----------------------------------------------------------------- CurveView

void CurveView::update()
{
    const int t = juce::roundToInt (proc.value (pSatType));
    const float bite = proc.value (pBite);
    if (t != lastType || std::abs (bite - lastBite) > 0.01f) { lastType = t; lastBite = bite; repaint(); }
}

void CurveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg0);
    g.fillRoundedRectangle (r, 6.0f);
    auto plot = r.reduced (8.0f);
    g.setColour (col::grid);
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    g.drawVerticalLine ((int) plot.getCentreX(), plot.getY(), plot.getBottom());
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (col::grid.withAlpha (0.5f));
        g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * i / 4.0f), plot.getX(), plot.getRight());
        g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * i / 4.0f), plot.getY(), plot.getBottom());
    }
    // Diagonale (sauber)
    g.setColour (col::inkMute.withAlpha (0.5f));
    g.drawLine (plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.0f);

    Shaper sh;
    sh.set ((SatType) juce::jlimit (0, 6, juce::roundToInt (proc.value (pSatType))), proc.value (pBite) * 0.01f);
    // Darstellung mit 12 dB Drive, damit der Charakter sichtbar wird
    const float drv = 4.0f;
    float peak = 0.01f;
    for (int i = 0; i <= 200; ++i) peak = std::max (peak, std::abs (sh ((i / 100.0f - 1.0f) * drv)));
    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float x = i / 100.0f - 1.0f;
        const float y = sh (x * drv) / peak;
        const float px = plot.getX() + (x + 1.0f) * 0.5f * plot.getWidth();
        const float py = plot.getCentreY() - y * plot.getHeight() * 0.5f * 0.92f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (col::pink.withAlpha (0.2f));
    g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::pink.withAlpha (0.45f));
    g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (col::sunset (plot.getX(), plot.getY(), plot.getRight(), plot.getBottom()));
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

// ----------------------------------------------------------------- SaturationPanel

static juce::StringArray satNames()
{
    juce::StringArray s;
    for (int i = 0; i < 7; ++i) s.add (juce::String (satTypeName (i)).toUpperCase());
    return s;
}

SaturationPanel::SaturationPanel (BrainfuzzProcessor& p)
    : proc (p), types (p, pSatType, satNames(), col::pink, 4), bite (p, pBite, "Bite", col::pink), curve (p)
{
    addAndMakeVisible (types);
    addAndMakeVisible (bite);
    addAndMakeVisible (curve);
    bite.slider.setTooltip ("Bite: wie hart der Typ zubeisst");
    for (int i = 0; i < 7; ++i) types.button (i)->setTooltip (satTypeInfo (i));
    types.onChange = [this] (int) { repaint(); };
}

void SaturationPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), "SATURATION", col::pink);
    g.setColour (col::inkDim);
    g.setFont (fonts::display (12.0f));
    g.drawText (satTypeInfo (types.getSelected()), juce::Rectangle<int> (12, 80, getWidth() - 24, 16), juce::Justification::centredLeft);
    g.setFont (fonts::mono (10.0f));
    g.setColour (col::inkMute);
    g.drawText ("Hart -> warm", juce::Rectangle<int> (getWidth() - 120, 6, 108, 20), juce::Justification::centredRight);
}

void SaturationPanel::resized()
{
    types.setBounds (12, 28, getWidth() - 24, 48);
    curve.setBounds (12, 102, 160, getHeight() - 112);
    bite.setBounds (182, 112, getWidth() - 194, 120);
}

// ----------------------------------------------------------------- FilterPanel

FilterPanel::FilterPanel (BrainfuzzProcessor& p)
    : type (p, pXType), slope (p, pSlope),
      tightHp (p, pTightHp, "HP", col::cyan), tightLp (p, pTightLp, "LP", col::cyan),
      tightSlope (p, pTightSlope, juce::StringArray { "24", "48" }, col::cyan),
      mono (p, pMonoBass, "MONO BASS", col::violet)
{
    type.setTooltip ("Filtertyp der Weichen: jeder hat seinen eigenen Uebergang");
    slope.setTooltip ("Steilheit der Weichen");
    tightHp.setTooltip ("Tight HP: entfernt Verzerrungs-Anteile unterhalb jedes Bandes");
    tightLp.setTooltip ("Tight LP: entfernt Verzerrungs-Anteile oberhalb jedes Bandes");
    tightSlope.setTooltip ("Steilheit der Tight-Filter (dB/Okt.)");
    mono.setTooltip ("Mono Bass: alles unterhalb der Frequenz bleibt in der Mitte");
    monoFreq.setSliderStyle (juce::Slider::LinearHorizontal);
    monoFreq.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);
    monoFreq.setTextBoxIsEditable (true);
    setAccent (monoFreq, col::violet);
    monoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramSpecs()[pMonoFreq].id, monoFreq);
    monoFreq.setDoubleClickReturnValue (true, paramSpecs()[pMonoFreq].def);
    for (auto* c : std::initializer_list<juce::Component*> { &type, &slope, &tightHp, &tightLp, &tightSlope, &mono, &monoFreq })
        addAndMakeVisible (c);
}

void FilterPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), {}, col::cyan);
    g.setFont (fonts::display (13.0f, true));
    g.setColour (col::cyan);
    g.drawText ("FILTER", juce::Rectangle<int> (12, 6, 60, 22), juce::Justification::centredLeft);
    g.drawText ("TIGHT", juce::Rectangle<int> (12, 38, 60, 22), juce::Justification::centredLeft);
    g.setFont (fonts::mono (10.0f));
    g.setColour (col::inkMute);
    g.drawText ("dB", juce::Rectangle<int> (getWidth() - 30, 38, 20, 22), juce::Justification::centredLeft);
}

void FilterPanel::resized()
{
    type.setBounds (72, 6, 136, 22);
    slope.setBounds (214, 6, getWidth() - 226, 22);
    tightHp.setBounds (72, 38, 50, 22);
    tightLp.setBounds (126, 38, 50, 22);
    tightSlope.setBounds (196, 38, getWidth() - 232, 22);
    mono.setBounds (12, 72, 86, 22);
    monoFreq.setBounds (104, 72, getWidth() - 114, 22);
}

// ----------------------------------------------------------------- Meter

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float w = (r.getWidth() - 2.0f) * 0.5f;
    auto bar = [&] (juce::Rectangle<float> b, float v)
    {
        g.setColour (col::bg0);
        g.fillRoundedRectangle (b, 2.0f);
        const float db = juce::Decibels::gainToDecibels (v, -60.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        auto f = b.withTop (b.getBottom() - b.getHeight() * t);
        juce::ColourGradient grad (col::cyan, 0, b.getBottom(), col::red, 0, b.getY(), false);
        grad.addColour (0.65, col::violet);
        grad.addColour (0.88, col::pink);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (f, 2.0f);
    };
    bar (r.withWidth (w), lv);
    bar (r.withTrimmedLeft (w + 2.0f), rv);
}

// ----------------------------------------------------------------- MasterPanel

MasterPanel::MasterPanel (BrainfuzzProcessor& p)
    : proc (p),
      input (p, pIn, "Input", col::cyan, true), output (p, pOut, "Output", col::cyan, true),
      limit (p, pLimit, "LIMIT", col::pink),
      mCrush (p, pMCrush, "Crush", col::orange), mChorus (p, pMChorus, "Chorus", col::orange), mVerb (p, pMVerb, "Reverb", col::orange),
      sat (p, pSatBlend, "Sat", col::violet), spread (p, pSpreadBlend, "Spread", col::violet), mix (p, pMix, "Mix", col::violet)
{
    for (auto* c : std::initializer_list<juce::Component*> { &input, &output, &link, &limit, &mCrush, &mChorus, &mVerb,
                                                             &sat, &spread, &mix, &inMeter, &outMeter })
        addAndMakeVisible (c);
    setAccent (link, col::cyan);
    link.setClickingTogglesState (true);
    link.setToggleState (proc.ioLink, juce::dontSendNotification);
    link.onClick = [this] { proc.ioLink = link.getToggleState(); };
    link.setTooltip ("Link: Input hoch = Output runter (gleich laut vergleichen)");
    limit.setTooltip ("Limit: Ausgang wird bei -0.1 dBFS begrenzt");
    mCrush.slider.setTooltip ("Master Crush: alle Bit Crusher zusammen lauter/leiser");
    mChorus.slider.setTooltip ("Master Chorus: alle Chorus zusammen");
    mVerb.slider.setTooltip ("Master Reverb: alle Reverbs zusammen");
    sat.slider.setTooltip ("Sat: Saettigung parallel zum sauberen Signal mischen");
    spread.slider.setTooltip ("Spread: gesamte zusaetzliche Breite (alle Baender)");
    mix.slider.setTooltip ("Mix: gesamter Effekt gegen das Original");

    lastIn = proc.value (pIn);
    input.slider.onValueChange = [this]
    {
        const float v = (float) input.slider.getValue();
        const float d = v - lastIn;
        lastIn = v;
        if (proc.ioLink && juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown() && input.slider.isMouseButtonDown())
            proc.setValue (pOut, juce::jlimit (-24.0f, 24.0f, proc.value (pOut) - d));
    };
}

void MasterPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), "MASTER", col::cyan);
    g.setFont (fonts::display (11.0f, true));
    g.setColour (col::orange);
    g.drawText ("FX MASTER", juce::Rectangle<int> (12, 134, 120, 14), juce::Justification::centredLeft);
    g.setColour (col::violet);
    g.drawText ("PARALLEL", juce::Rectangle<int> (12, 230, 120, 14), juce::Justification::centredLeft);
    g.setColour (col::line);
    g.drawHorizontalLine (130, 10.0f, getWidth() - 10.0f);
    g.drawHorizontalLine (226, 10.0f, getWidth() - 10.0f);
    // Gain-Reduction des Limiters
    g.setFont (fonts::mono (11.0f));
    g.setColour (gr > 0.05f ? col::pink : col::inkMute);
    g.drawText ("GR " + juce::String (-gr, 1) + " dB", juce::Rectangle<int> (getWidth() - 182, 6, 90, 20), juce::Justification::centredRight);
    g.setColour (col::inkMute);
    g.setFont (fonts::mono (9.0f));
    g.drawText ("IN", juce::Rectangle<int> (8, 118, 18, 10), juce::Justification::centred);
    g.drawText ("OUT", juce::Rectangle<int> (getWidth() - 28, 118, 22, 10), juce::Justification::centred);
}

void MasterPanel::resized()
{
    const int w = getWidth();
    limit.setBounds (w - 80, 5, 68, 22);
    inMeter.setBounds (10, 34, 14, 82);
    outMeter.setBounds (w - 24, 34, 14, 82);
    input.setBounds (28, 30, 96, 96);
    output.setBounds (w - 124, 30, 96, 96);
    link.setBounds (w / 2 - 22, 66, 44, 22);
    const int cw = (w - 20) / 3;
    mCrush.setBounds (10, 146, cw, 78);
    mChorus.setBounds (10 + cw, 146, cw, 78);
    mVerb.setBounds (10 + 2 * cw, 146, cw, 78);
    sat.setBounds (10, 242, cw, 80);
    spread.setBounds (10 + cw, 242, cw, 80);
    mix.setBounds (10 + 2 * cw, 242, cw, 80);
}

void MasterPanel::update()
{
    inMeter.set (proc.inPeak[0].load(), proc.inPeak[1].load());
    outMeter.set (proc.outPeak[0].load(), proc.outPeak[1].load());
    const float g = -juce::Decibels::gainToDecibels (proc.limiterGain.load(), -60.0f);
    if (std::abs (g - gr) > 0.05f) { gr = g; repaint (getWidth() - 190, 4, 100, 24); }
    if (link.getToggleState() != proc.ioLink) link.setToggleState (proc.ioLink, juce::dontSendNotification);
}

} // namespace bfz::ui
