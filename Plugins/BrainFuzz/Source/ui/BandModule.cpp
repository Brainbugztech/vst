#include "BandModule.h"

namespace bfz::ui
{

// ----------------------------------------------------------------- StereoScope

void StereoScope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto c = col::band (b);
    g.setColour (col::bg0);
    g.fillRoundedRectangle (r, 6.0f);

    const float cx = r.getCentreX(), base = r.getBottom() - 12.0f;
    const float rad = std::min (r.getWidth() * 0.5f - 6.0f, base - r.getY() - 4.0f);

    // Halbkreis-Raster
    g.setColour (col::grid);
    juce::Path arc;
    arc.addCentredArc (cx, base, rad, rad, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    g.strokePath (arc, juce::PathStrokeType (1.0f));
    for (float a : { -0.785f, 0.0f, 0.785f })
        g.drawLine (cx, base, cx + std::sin (a) * rad, base - std::cos (a) * rad, 0.8f);
    g.drawHorizontalLine ((int) base, cx - rad, cx + rad);

    // Punkte: x = Seite, y = Mitte (nach oben)
    proc.scopeL[(size_t) b].read (bufL.data(), (int) bufL.size());
    proc.scopeR[(size_t) b].read (bufR.data(), (int) bufR.size());
    float peak = 1e-4f;
    double lr = 0, ll = 0, rr = 0;
    for (size_t i = 0; i < bufL.size(); ++i)
    {
        peak = std::max (peak, std::max (std::abs (bufL[i]), std::abs (bufR[i])));
        lr += bufL[i] * bufR[i]; ll += bufL[i] * bufL[i]; rr += bufR[i] * bufR[i];
    }
    const float scale = rad * 0.92f / std::max (peak, 0.05f);
    const float newCorr = (ll > 1e-9 && rr > 1e-9) ? (float) (lr / std::sqrt (ll * rr)) : 1.0f;
    corr += (newCorr - corr) * 0.25f;

    g.setColour (c.withAlpha (0.65f));
    for (size_t i = 0; i < bufL.size(); ++i)
    {
        const float m = (bufL[i] + bufR[i]) * 0.7071f, s = (bufL[i] - bufR[i]) * 0.7071f;
        const float x = cx - s * scale, y = base - std::abs (m) * scale;
        if (y < r.getY()) continue;
        g.fillRect (x, y, 1.6f, 1.6f);
    }

    // Korrelation
    g.setFont (fonts::mono (10.0f));
    g.setColour (corr < 0.0f ? col::red : col::inkMute);
    g.drawText ("L", juce::Rectangle<float> (r.getX() + 4.0f, base - 10.0f, 12.0f, 10.0f), juce::Justification::left);
    g.drawText ("R", juce::Rectangle<float> (r.getRight() - 16.0f, base - 10.0f, 12.0f, 10.0f), juce::Justification::right);
    g.drawText ("Korr " + juce::String (corr, 2), juce::Rectangle<float> (r.getX(), base + 1.0f, r.getWidth(), 11.0f), juce::Justification::centred);
}

// ----------------------------------------------------------------- FxTab

void FxTab::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool sel = getToggleState();
    g.setColour (sel ? col::bg3 : col::bg0);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (sel ? colour : over ? col::inkMute : col::line);
    g.drawRoundedRectangle (r, 5.0f, sel ? 1.3f : 1.0f);
    // LED
    const float d = 7.0f;
    juce::Rectangle<float> led (r.getX() + 7.0f, r.getCentreY() - d * 0.5f, d, d);
    if (shownOn)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
        g.setColour (colour);
    }
    else g.setColour (col::line);
    g.fillEllipse (led);
    g.setColour (sel ? col::ink : col::inkDim);
    g.setFont (fonts::display (11.5f, true));
    g.drawText (getButtonText(), r.withTrimmedLeft (17.0f), juce::Justification::centred);
}

// ----------------------------------------------------------------- FxPage

FxPage::FxPage (BrainfuzzProcessor& p, int band, int effect) : proc (p), b (band), fx (effect), accent (col::band (band))
{
    const int onP[3] { bCrushOn, bChorusOn, bVerbOn };
    const int posP[3] { bCrushPos, bChorusPos, bVerbPos };
    const int tgtP[3] { bCrushTarget, bChorusTarget, bVerbTarget };
    power = std::make_unique<Toggle> (p, bandIndex (b, onP[fx]), "ON", accent);
    pos = std::make_unique<ChoiceButtons> (p, bandIndex (b, posP[fx]), juce::StringArray { "PRE", "POST" }, accent);
    target = std::make_unique<ChoiceButtons> (p, bandIndex (b, tgtP[fx]), juce::StringArray { "BAND", "WIDTH", "BOTH" }, accent);
    pos->setTooltip ("Vor oder nach der Saettigung (gilt fuer das Band)");
    target->setTooltip ("Auf dem Band, nur auf der zusaetzlichen Breite (Spread) oder beides");
    addAndMakeVisible (*power);
    addAndMakeVisible (*pos);
    addAndMakeVisible (*target);

    auto K = [&] (int prm, const char* cap) { addAndMakeVisible (knobs.add (new Knob (p, bandIndex (b, prm), cap, accent))); };
    if (fx == 0) { K (bCrushBits, "Bits"); K (bCrushDown, "Down"); K (bCrushMix, "Mix"); }
    if (fx == 1)
    {
        voices = std::make_unique<ChoiceButtons> (p, bandIndex (b, bChorusVoices), juce::StringArray { "1", "2", "3" }, accent);
        voices->setTooltip ("Stimmen");
        addAndMakeVisible (*voices);
        K (bChorusRate, "Rate"); K (bChorusDepth, "Depth"); K (bChorusMix, "Mix");
    }
    if (fx == 2)
    {
        K (bVerbSize, "Size"); K (bVerbLength, "Length"); K (bVerbWidth, "Width"); K (bVerbPre, "Pre");
        K (bVerbLow, "Lo Cut"); K (bVerbHigh, "Hi Cut"); K (bVerbMix, "Mix");
    }
    for (auto* k : knobs) k->setValueBoxHeight (13);
    update();
}

void FxPage::paint (juce::Graphics& g)
{
    if (fx == 1)
    {
        g.setColour (col::inkDim);
        g.setFont (fonts::display (10.5f, true));
        g.drawText ("VOICES", juce::Rectangle<int> (0, 26, 44, 12), juce::Justification::centred);
    }
}

void FxPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (20);
    power->setBounds (top.removeFromLeft (30));
    top.removeFromLeft (4);
    pos->setBounds (top.removeFromLeft (64));
    top.removeFromLeft (4);
    target->setBounds (top);
    r.removeFromTop (4);

    if (fx == 0)
    {
        const int w = r.getWidth() / 3;
        for (int i = 0; i < 3; ++i) knobs[i]->setBounds (r.getX() + i * w, r.getY() + 8, w, std::min (84, r.getHeight() - 8));
    }
    else if (fx == 1)
    {
        voices->setBounds (2, r.getY() + 18, 40, 58);
        auto rest = r.withTrimmedLeft (46);
        const int w = rest.getWidth() / 3;
        for (int i = 0; i < 3; ++i) knobs[i]->setBounds (rest.getX() + i * w, rest.getY() + 8, w, std::min (84, rest.getHeight() - 8));
    }
    else
    {
        const int rowH = r.getHeight() / 2;
        const int w4 = r.getWidth() / 4;
        for (int i = 0; i < 4; ++i) knobs[i]->setBounds (r.getX() + i * w4, r.getY(), w4, rowH);
        const int w3 = r.getWidth() / 3;
        for (int i = 0; i < 3; ++i) knobs[4 + i]->setBounds (r.getX() + i * w3 + (w3 - w4) / 2, r.getY() + rowH, w4, rowH);
    }
}

void FxPage::update()
{
    const int onP[3] { bCrushOn, bChorusOn, bVerbOn };
    const bool on = proc.value (bandIndex (b, onP[fx])) > 0.5f;
    if (on != lastOn)
    {
        lastOn = on;
        for (auto* k : knobs) k->setAlpha (on ? 1.0f : 0.55f);
        pos->setAlpha (on ? 1.0f : 0.55f);
        target->setAlpha (on ? 1.0f : 0.55f);
        if (voices) voices->setAlpha (on ? 1.0f : 0.55f);
    }
}

// ----------------------------------------------------------------- BandModule

BandModule::BandModule (BrainfuzzProcessor& p, int band)
    : proc (p), b (band), accent (col::band (band)),
      solo (p, bandIndex (band, bSolo), "S", col::yellow),
      mute (p, bandIndex (band, bMute), "M", col::red),
      bypass (p, bandIndex (band, bBypass), "B", col::cyan),
      drive (p, bandIndex (band, bDrive), "Drive", col::band (band)),
      level (p, bandIndex (band, bLevel), "Level", col::band (band), true),
      width (p, bandIndex (band, bWidth), "Width", col::band (band)),
      spread (p, bandIndex (band, bSpread), "Spread", col::band (band)),
      scope (p, band)
{
    solo.setTooltip ("Solo: nur dieses Band hoeren");
    mute.setTooltip ("Mute: Band stumm");
    bypass.setTooltip ("Bypass: Band sauber (ohne Saettigung, Effekte und Breite)");
    width.slider.setTooltip ("Width: vorhandene Stereo-Breite des Bandes (100 % = original, 0 % = mono)");
    spread.slider.setTooltip ("Spread: zusaetzliche Breite aus der Mitte, mono-sicher, auch bei Mono-Quellen");
    for (auto* c : std::initializer_list<juce::Component*> { &solo, &mute, &bypass, &drive, &level, &width, &spread, &scope })
        addAndMakeVisible (c);
    drive.setValueBoxHeight (16);

    const char* names[3] { "CRUSH", "CHORUS", "REVERB" };
    const int onP[3] { bCrushOn, bChorusOn, bVerbOn };
    for (int i = 0; i < 3; ++i)
    {
        tabs[(size_t) i] = std::make_unique<FxTab> (p, b, bandIndex (b, onP[i]), names[i], accent);
        tabs[(size_t) i]->setTooltip ("Klick = anzeigen, Doppelklick = an/aus");
        tabs[(size_t) i]->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (*tabs[(size_t) i]);
        pages[(size_t) i] = std::make_unique<FxPage> (p, b, i);
        addChildComponent (*pages[(size_t) i]);
    }
    showTab (juce::jlimit (0, 2, proc.fxTab[(size_t) b]));
}

void BandModule::showTab (int t)
{
    tab = t;
    proc.fxTab[(size_t) b] = t;
    for (int i = 0; i < 3; ++i)
    {
        tabs[(size_t) i]->setToggleState (i == t, juce::dontSendNotification);
        pages[(size_t) i]->setVisible (i == t);
    }
}

void BandModule::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r, {}, accent);
    // Kopf
    auto head = r.removeFromTop (30.0f).reduced (10.0f, 0.0f);
    const float d = 9.0f;
    g.setColour (accent.withAlpha (0.25f + 0.6f * juce::jlimit (0.0f, 1.0f, shownLevel)));
    g.fillEllipse (head.getX() - 2.0f, head.getCentreY() - d * 0.5f - 2.0f, d + 4.0f, d + 4.0f);
    g.setColour (accent);
    g.fillEllipse (head.getX(), head.getCentreY() - d * 0.5f, d, d);
    g.setFont (fonts::display (14.0f, true));
    g.setColour (col::ink);
    g.drawText (juce::String (b + 1) + "  " + juce::String (bandName (b)).toUpperCase(),
                head.withTrimmedLeft (16.0f), juce::Justification::centredLeft);

    // Trennlinien
    g.setColour (col::line.withAlpha (0.6f));
    g.drawHorizontalLine (148, 10.0f, (float) getWidth() - 10.0f);
    g.drawHorizontalLine (220, 10.0f, (float) getWidth() - 10.0f);
}

void BandModule::resized()
{
    const int w = getWidth();
    solo.setBounds (w - 82, 6, 22, 20);
    mute.setBounds (w - 57, 6, 22, 20);
    bypass.setBounds (w - 32, 6, 22, 20);

    drive.setBounds (4, 32, 118, 114);
    level.setBounds (122, 52, w - 126, 90);
    width.setBounds (8, 152, (w - 16) / 2, 66);
    spread.setBounds (8 + (w - 16) / 2, 152, (w - 16) / 2, 66);

    const int tw = (w - 16 - 6) / 3;
    for (int i = 0; i < 3; ++i) tabs[(size_t) i]->setBounds (8 + i * (tw + 3), 226, tw, 22);
    for (auto& pg : pages) pg->setBounds (8, 252, w - 16, 132);
    scope.setBounds (8, 388, w - 16, getHeight() - 396);
}

void BandModule::update()
{
    const int onP[3] { bCrushOn, bChorusOn, bVerbOn };
    for (int i = 0; i < 3; ++i)
    {
        const bool on = proc.value (bandIndex (b, onP[i])) > 0.5f;
        if (on != tabs[(size_t) i]->shownOn) { tabs[(size_t) i]->shownOn = on; tabs[(size_t) i]->repaint(); }
        pages[(size_t) i]->update();
    }
    const float lvl = proc.bandLevel[(size_t) b].load();
    if (std::abs (lvl - shownLevel) > 0.02f) { shownLevel = lvl; repaint (0, 0, 40, 30); }
    scope.update();
}

} // namespace bfz::ui
