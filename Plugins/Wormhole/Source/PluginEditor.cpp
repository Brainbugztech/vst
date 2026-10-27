#include "PluginEditor.h"

namespace whl
{
static constexpr float W = 960.0f, H = 600.0f;
static juce::String u8 (const char* t) { return juce::String::fromUTF8 (t); }

static juce::String fmtTime (double sec)
{
    if (sec >= 3600.0)
    {
        const int t = juce::roundToInt (sec), h = t / 3600, m = (t / 60) % 60, s2 = t % 60;
        return juce::String (h) + ":" + juce::String (m).paddedLeft ('0', 2) + ":" + juce::String (s2).paddedLeft ('0', 2);
    }
    const int m = (int) (sec / 60.0);
    return juce::String (m) + ":" + juce::String (sec - m * 60.0, 1).paddedLeft ('0', 4);
}

static juce::String fmtTempo (double pct)
{
    return juce::String (pct, pct < 1.0 ? 3 : pct < 10.0 ? 2 : 1);
}

// ---------------------------------------------------------------------------
void PillButton::paintButton (juce::Graphics& g, bool hover, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float al = isEnabled() ? 1.0f : 0.35f;
    const float rad = r.getHeight() * 0.5f;
    if (style == Style::primary)
    {
        auto gr = juce::ColourGradient (juce::Colour (0xff7a2df0), r.getX(), 0, juce::Colour (0xff0aa8e0), r.getRight(), 0, false);
        if (progress >= 0.0f)
        {
            g.setColour (juce::Colour (0xff15161c).withMultipliedAlpha (al));
            g.fillRoundedRectangle (r, rad);
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, progress)).getSmallestIntegerContainer());
            g.setGradientFill (gr);
            g.fillRoundedRectangle (r, rad);
        }
        else
        {
            gr.multiplyOpacity (al * (down ? 0.85f : 1.0f));
            if (hover && isEnabled())
            {
                auto glow = gr; glow.multiplyOpacity (0.25f);
                g.setGradientFill (glow);
                g.fillRoundedRectangle (r.expanded (3.0f, 3.0f).translated (0, 2), rad + 3.0f);
            }
            g.setGradientFill (gr);
            g.fillRoundedRectangle (r, rad);
        }
        g.setColour (juce::Colours::white.withAlpha (0.18f * al));
        g.drawRoundedRectangle (r, rad, 1.0f);
        caps (g, getButtonText(), r, 10.5f, juce::Colours::white.withAlpha (al), juce::Justification::centred, 0.24f);
    }
    else if (style == Style::outline)
    {
        g.setColour (juce::Colour (0xff15161c).withMultipliedAlpha (al));
        g.fillRoundedRectangle (r, rad);
        g.setColour (juce::Colours::white.withAlpha ((hover && isEnabled() ? 0.24f : 0.12f) * al));
        g.drawRoundedRectangle (r, rad, 1.0f);
        const float ix = r.getX() + 18.0f, iy = r.getCentreY();
        if (icon == Icon::dragOut)
        {
            juce::Path arrow;
            arrow.startNewSubPath (ix + 7, iy + 5); arrow.lineTo (ix, iy - 2);
            arrow.startNewSubPath (ix, iy - 2); arrow.lineTo (ix + 5, iy - 2);
            arrow.startNewSubPath (ix, iy - 2); arrow.lineTo (ix, iy + 3);
            g.setColour (col::ink.withAlpha (al));
            g.strokePath (arrow, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (icon == Icon::play)
        {
            juce::Path tri; tri.addTriangle (ix, iy - 5, ix, iy + 5, ix + 8, iy);
            { auto gr = accent (ix, 0, ix + 8, 0); gr.multiplyOpacity (al); g.setGradientFill (gr); }
            g.fillPath (tri);
        }
        else if (icon == Icon::stop)
        {
            g.setGradientFill (accent (ix, 0, ix + 8, 0));
            g.fillRoundedRectangle (ix, iy - 4.5f, 9.0f, 9.0f, 1.5f);
        }
        caps (g, getButtonText(), r.withTrimmedLeft (icon == Icon::none ? 0.0f : 22.0f), 10.5f, col::ink.withAlpha (al), juce::Justification::centred, 0.24f);
    }
    else
    {
        const auto c = (hover && isEnabled()) ? col::ink : col::dim;
        caps (g, getButtonText(), r, 8.5f, c.withMultipliedAlpha (al), juce::Justification::centred, 0.24f);
        if (hover && isEnabled())
        {
            g.setGradientFill (accent (r.getX(), 0, r.getRight(), 0));
            g.fillRect (r.getX() + 4.0f, r.getBottom() - 2.0f, r.getWidth() - 8.0f, 1.0f);
        }
    }
}

// ---------------------------------------------------------------------------
void ModeSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::well);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    auto inner = r.reduced (2.0f);
    auto half = inner.withWidth (inner.getWidth() * 0.5f);
    auto on = renderMode ? half.withX (inner.getCentreX()) : half;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2c37), 0, on.getY(), juce::Colour (0xff1c1d25), 0, on.getBottom(), false));
    g.fillRoundedRectangle (on, on.getHeight() * 0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (on.reduced (0.5f), on.getHeight() * 0.5f, 1.0f);

    g.setFont (fonts::semi (10.0f, 0.28f));
    for (int i = 0; i < 2; ++i)
    {
        auto tr = i == 0 ? half : half.withX (inner.getCentreX());
        const bool active = (i == 1) == renderMode;
        if (active) g.setGradientFill (accent (tr.getX() + 20, 0, tr.getRight() - 20, 0));
        else g.setColour (col::dim);
        g.drawText (i == 0 ? "LIVE" : "RENDER", tr, juce::Justification::centred, false);
    }
}

// ---------------------------------------------------------------------------
BpmField::BpmField()
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (fonts::medium (13.0f, 0.03f));
    label.setEditable (false, true, false);
    label.setColour (juce::Label::textColourId, col::ink);
    label.onTextChange = [this]
    {
        const double v = label.getText().replace (",", ".").getDoubleValue();
        if (v >= 20.0 && v <= 400.0 && onEdit) onEdit (v);
    };
    label.onEditorShow = [this]
    {
        if (auto* ed = label.getCurrentTextEditor())
        {
            ed->setJustification (juce::Justification::centred);
            ed->setInputRestrictions (7, "0123456789.,");
            ed->selectAll();
        }
    };
    addAndMakeVisible (label);
}

void BpmField::setValue (double bpm)
{
    if (label.isBeingEdited()) return;
    label.setText (bpm > 0.0 ? juce::String (bpm, 2) : juce::String ("--"), juce::dontSendNotification);
}

void BpmField::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::well);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.translated (0, 1.0f).reduced (1.0f), 5.0f, 1.0f);
    g.setColour (accentOutline ? col::cyan.withAlpha (0.45f) : juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

// ---------------------------------------------------------------------------
WaveView::WaveView (WormholeProcessor& p) : proc (p)
{
    for (auto* b : { &loadBtn, &captureBtn })
        addAndMakeVisible (b);
    loadBtn.onClick = [this] { if (onLoad) onLoad(); };
    captureBtn.onClick = [this] { if (onCapture) onCapture(); };
}

void WaveView::resized()
{
    loadBtn.setBounds (getWidth() - 182, getHeight() - 22, 70, 18);
    captureBtn.setBounds (getWidth() - 106, getHeight() - 22, 92, 18);
}

void WaveView::update()
{
    auto c = proc.worker.getClip();
    auto r = proc.worker.getResult();
    const auto s = proc.currentRenderSettings();
    const bool st = r != nullptr && r->settings != s;
    const bool lv = proc.isLiveMode(), cap = proc.isCapturing();
    const float pr = proc.worker.getStatus() == wh::Worker::Status::rendering ? proc.worker.getProgress() : -1.0f;
    const bool changed = c != clip || r != result || st != stale || lv != live || cap != capturing
                      || std::abs (s.timeRatio - predictedRatio) > 1e-6 || std::abs (pr - progress) > 0.004f || cap;
    const double pp = proc.preview.isPlaying() ? proc.preview.getPositionFraction() : -1.0;
    const bool moved = std::abs (pp - playPos) > 1e-5;
    clip = c; result = r; stale = st; live = lv; capturing = cap; predictedRatio = s.timeRatio; progress = pr; playPos = pp;
    canDragOut = result != nullptr && ! stale && result->file.existsAsFile() && ! live;
    loadBtn.setVisible (! live && ! capturing);
    captureBtn.setVisible (! live);
    captureBtn.setButtonText (capturing ? "STOP" : "AUFNEHMEN");
    if (changed || moved) repaint();
}

void WaveView::mouseUp (const juce::MouseEvent& e)
{
    if (dragDone || live || capturing || clip == nullptr || ! onSeek) return;
    if (e.getDistanceFromDragStart() > 6) return;
    if (loadBtn.getBounds().contains (e.getPosition()) || captureBtn.getBounds().contains (e.getPosition())) return;
    onSeek (juce::jlimit (0.0, 0.999, (double) (e.position.x - waveX0) / juce::jmax (1.0f, waveW)));
}

void WaveView::mouseMove (const juce::MouseEvent&)
{
    setMouseCursor (canDragOut ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (canDragOut && ! dragDone && e.getDistanceFromDragStart() > 6 && onDragOut)
    {
        dragDone = true;
        onDragOut();
    }
}

void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0e0f14));
    g.fillRoundedRectangle (r, 10.0f);
    // eingelassene Kante
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (1.0f).translated (0, 1.0f), 9.0f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    const float mid = r.getCentreY(), x0 = 14.0f, x1 = r.getWidth() - 14.0f;

    if (live)
    {
        caps (g, u8 ("LIVE  \xc2\xb7  PITCH UND FORMANT LAUFEN IN ECHTZEIT AUF DER SPUR"), r.withTrimmedBottom (14), 9.5f, col::dim);
        const double ms = proc.getLatencySamples() / juce::jmax (1.0, proc.getPreparedRate()) * 1000.0;
        caps (g, "LATENZ " + juce::String (juce::roundToInt (ms)) + u8 (" MS  \xc2\xb7  TEMPO NUR IM RENDER-MODUS"), r.withTrimmedTop (26), 8.5f, col::faint);
        return;
    }
    if (capturing)
    {
        // Laufende Aufnahme: pulsierender Punkt + Zeit
        const float t = (float) proc.captureSeconds();
        const float pulse = 0.6f + 0.4f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.006f);
        g.setColour (juce::Colour (0xffff3d6e).withAlpha (pulse));
        g.fillEllipse (r.getCentreX() - 120.0f, mid - 4.0f, 8.0f, 8.0f);
        caps (g, u8 ("AUFNAHME L\xc3\x84UFT  \xc2\xb7  ") + fmtTime (t) + u8 ("  \xc2\xb7  PLAY IN ABLETON DR\xc3\x9c" "CKEN"), r.withTrimmedLeft (20), 9.5f, col::ink);
        return;
    }
    if (clip == nullptr)
    {
        caps (g, "AUDIO HIERHER ZIEHEN", r.withTrimmedBottom (16), 11.0f, col::ink.withAlpha (0.8f), juce::Justification::centred, 0.32f);
        caps (g, u8 ("WAV \xc2\xb7 AIFF \xc2\xb7 FLAC \xc2\xb7 MP3   ODER DIREKT VON DER SPUR AUFNEHMEN"), r.withTrimmedTop (24), 8.5f, col::faint);
        return;
    }

    const double inLen = clip->lengthSeconds();
    const bool haveRes = result != nullptr;
    const double ratio = haveRes && ! stale ? result->settings.timeRatio : predictedRatio;
    const double outLen = inLen * ratio;
    const double span = juce::jmax (inLen, outLen);
    float wIn = (float) ((x1 - x0) * inLen / span), wOut = (float) ((x1 - x0) * outLen / span);
    const bool farOut = ratio > 4.0 || ratio < 0.25;   // Extrem: beide über die volle Breite, Faktor als Text
    if (farOut) { wIn = x1 - x0; wOut = x1 - x0; }
    waveX0 = x0; waveW = wOut;
    const float amp = r.getHeight() * 0.36f;

    // Original
    const int n = (int) clip->peaks.size();
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    for (int i = 0; i < n; i += 2)
    {
        const float x = x0 + wIn * i / n, h = clip->peaks[(size_t) i] * amp;
        g.fillRect (x, mid - h, 1.2f, h * 2.0f);
    }

    // Ergebnis (oder Vorschau, solange nicht gerendert)
    const std::vector<float>& pk = haveRes && ! stale ? result->peaks : clip->peaks;
    const int m = (int) pk.size();
    auto gr = accent (x0, 0, x0 + wOut, 0);
    gr.multiplyOpacity (haveRes && ! stale ? 0.9f : 0.28f);
    g.setGradientFill (gr);
    const float step = juce::jmax (1.94f, wOut / 460.0f);
    for (float x = x0; x < x0 + wOut; x += step)
    {
        const int idx = juce::jlimit (0, m - 1, (int) ((x - x0) / wOut * m));
        const float h = pk[(size_t) idx] * amp * 0.88f;
        g.fillRect (x, mid - h, 1.2f, h * 2.0f);
    }
    // Attack-Marker
    if (haveRes && ! stale)
    {
        g.setColour (col::cyan.withAlpha (0.5f));
        for (float t : result->transients) g.fillRect (x0 + wOut * t, 8.0f, 1.0f, 4.0f);
    }
    // Ende-Markierung
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillRect (x0 + wOut, 10.0f, 1.0f, r.getHeight() - 34.0f);
    if (! farOut)
        caps (g, haveRes && ! stale ? "ENDE NEU" : "VORSCHAU", { x0 + wOut + 6.0f, 6.0f, 90.0f, 14.0f }, 8.5f,
              juce::Colours::white.withAlpha (0.35f), juce::Justification::centredLeft, 0.1f);
    if (stale) caps (g, u8 ("EINSTELLUNG GE\xc3\x84NDERT  \xc2\xb7  NEU RENDERN"), { x0, 6.0f, 300.0f, 14.0f }, 8.5f, col::cyan.withAlpha (0.7f), juce::Justification::centredLeft, 0.14f);
    caps (g, canDragOut ? u8 ("CLIP  \xc2\xb7  ZUM ZIEHEN ANFASSEN") : "CLIP", { x0, r.getHeight() - 20.0f, 300.0f, 14.0f }, 8.5f,
          col::dim, juce::Justification::centredLeft, 0.1f);

    if (farOut)
        caps (g, u8 ("\xc3\x97") + juce::String (ratio, ratio >= 100 ? 0 : 1) + u8 ("  \xc2\xb7  ") + fmtTime (outLen), { x1 - 226.0f, 6.0f, 220.0f, 14.0f }, 8.5f,
              col::cyan.withAlpha (0.7f), juce::Justification::centredRight, 0.14f);
    // Vorhör-Position
    if (playPos >= 0.0)
    {
        const float px = x0 + wOut * (float) playPos;
        g.setColour (col::cyan.withAlpha (0.18f)); g.fillRect (px - 2.0f, 6.0f, 5.0f, r.getHeight() - 12.0f);
        g.setColour (col::ice); g.fillRect (px, 6.0f, 1.0f, r.getHeight() - 12.0f);
    }
    else if (clip != nullptr)
        caps (g, u8 ("KLICK = AB HIER VORH\xc3\x96REN"), { x0, r.getHeight() - 20.0f, x1 - x0 - 190.0f, 14.0f }, 8.5f,
              col::faint, juce::Justification::centred, 0.14f);

    if (progress >= 0.0f)
    {
        g.setGradientFill (accent (x0, 0, x1, 0));
        g.fillRect (x0, 2.0f, (x1 - x0) * progress, 1.5f);
    }
}

// ---------------------------------------------------------------------------
Panel::Panel (WormholeProcessor& p) : proc (p), wave (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (tunnel);
    addAndMakeVisible (logo);
    addAndMakeVisible (modeSwitch);
    modeSwitch.onChange = [this] (bool r) { setMode (r); };

    auto knob = [this] (juce::Slider& s, bool bipolar, double resetValue)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.getProperties().set ("bipolar", bipolar);
        s.setDoubleClickReturnValue (true, resetValue);
        s.setMouseDragSensitivity (300);
        s.setVelocityModeParameters (0.6, 1, 0.05, true, juce::ModifierKeys::ctrlModifier);
        s.onValueChange = [this, &s]
        {
            refreshTexts(); repaint();
            tunnel.kick (&s == &tempo ? 0.5f : 0.35f);
        };
        addAndMakeVisible (s);
    };
    knob (tempo, false, 100.0); knob (pitch, true, 0.0); knob (fine, true, 0.0); knob (formant, true, 0.0); knob (texture, false, 0.0);
    aTempo = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "tempo", tempo);
    aPitch = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "pitch", pitch);
    aFine = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "fine", fine);
    aFormant = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "formant", formant);
    aTexture = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "texture", texture);
    texture.setTooltip (u8 ("Fensterl\xc3\xa4nge im Extrem-Modus: kurz = k\xc3\xb6rniger, lang = weicher. Ganz links = automatisch."));
    tempo.setTooltip (u8 ("Unter 25 % arbeitet der Extrem-Modus (bis 0,1 %). Strg + Ziehen = fein."));
    addAndMakeVisible (formantLock); addAndMakeVisible (attackLock);
    aFormantLock = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "formantLock", formantLock);
    aAttackLock = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "attackLock", attackLock);

    addAndMakeVisible (srcBpm); addAndMakeVisible (dstBpm);
    dstBpm.accentOutline = true;
    srcBpm.onEdit = [this] (double v) { proc.setSourceBpm (v); refreshTexts(); repaint(); };
    dstBpm.onEdit = [this] (double v)
    {
        const double src = proc.getSourceBpm();
        if (src > 0.0) proc.setTempoPercent (v / src * 100.0);
    };
    addAndMakeVisible (hostBtn);
    hostBtn.onClick = [this]
    {
        const double h = proc.getHostBpm(), src = proc.getSourceBpm();
        if (h > 0.0 && src > 0.0) proc.setTempoPercent (h / src * 100.0);
    };

    addAndMakeVisible (wave);
    wave.onLoad = [this] { chooseFile(); };
    wave.onCapture = [this] { toggleCapture(); };
    wave.onDragOut = [this] { dragResultOut(); };
    wave.onSeek = [this] (double fr)
    {
        if (auto c = proc.worker.getClip())
        {
            lastPreviewSettings = proc.currentRenderSettings();
            proc.preview.play (c, lastPreviewSettings, fr);
            tunnel.kick (0.6f);
        }
    };
    addAndMakeVisible (previewBtn);
    previewBtn.onClick = [this] { togglePreview(); };
    previewBtn.setTooltip (u8 ("Ergebnis vorh\xc3\xb6ren, ohne zu rendern. Regler wirken sofort. Klick in die Wellenform startet an der Stelle."));

    addAndMakeVisible (dragBtn);
    dragBtn.onDragOut = [this] { dragResultOut(); };
    dragBtn.onClick = [this] { if (auto r = proc.worker.getResult()) { proc.worker.markExported (r->file); r->file.revealToUser(); } };
    dragBtn.setTooltip ("Anfassen und in eine Ableton-Spur ziehen. Klick zeigt die Datei im Explorer.");

    addAndMakeVisible (renderBtn);
    renderBtn.onClick = [this] { startOrCancelRender(); };
    lastRenderCount = proc.worker.getRenderCount();

    setSize ((int) W, (int) H);
    timerCallback();
    startTimerHz (15);
}

Panel::~Panel() { setLookAndFeel (nullptr); }

void Panel::resized()
{
    tunnel.setBounds (getLocalBounds());
    logo.setBounds ((int) W - 28 - 150, 13, 150, 52);
    modeSwitch.setBounds (386, 18, 188, 28);

    tempo.setBounds (80, 107, 170, 170);
    pitch.setBounds (710, 107, 170, 170);
    fine.setBounds (723, 336, 46, 46);
    formant.setBounds (812, 336, 46, 46);
    texture.setBounds (300, 336, 46, 46);

    srcBpm.setBounds (70, 333, 78, 30);
    dstBpm.setBounds (180, 333, 78, 30);
    hostBtn.setBounds (186, 368, 80, 20);

    wave.setBounds (28, 428, 904, 92);
    formantLock.setBounds (28, 541, 160, 32);
    attackLock.setBounds (194, 541, 150, 32);
    renderBtn.setBounds (839, 540, 93, 34);
    dragBtn.setBounds (644, 540, 186, 34);
    previewBtn.setBounds (540, 540, 96, 34);
}

void Panel::paint (juce::Graphics& g)
{
    // Hintergrund
    juce::ColourGradient bg (col::bgCentre, W * 0.5f, H * 0.45f, col::bgEdge, W * 0.5f + 620.0f, H * 0.45f, true);
    bg.addColour (0.45, col::bgMid);
    g.setGradientFill (bg);
    g.fillAll();

    // Kopf
    g.setColour (col::ink);
    g.setFont (fonts::display (21.0f, 0.46f));
    g.drawText ("WORMHOLE", juce::Rectangle<float> (28, 14, 300, 26), juce::Justification::centredLeft, false);
    caps (g, "TIME & PITCH", { 28, 40, 200, 12 }, 8.5f, col::dim, juce::Justification::centredLeft, 0.38f);
    {
        juce::ColourGradient sep (juce::Colours::transparentWhite, 28, 0, juce::Colours::transparentWhite, W - 28, 0, false);
        sep.addColour (0.15, juce::Colours::white.withAlpha (0.12f));
        sep.addColour (0.85, juce::Colours::white.withAlpha (0.12f));
        g.setGradientFill (sep);
        g.fillRect (28.0f, 64.0f, W - 56.0f, 1.0f);
    }
}

void Panel::paintOverChildren (juce::Graphics& g)
{
    const auto s = proc.currentRenderSettings();
    const float liveAlpha = live ? 0.35f : 1.0f;

    // TEMPO
    if (extreme && ! live)
    {
        // TEMPO · EXTREM (EXTREM im Akzentverlauf)
        g.setFont (fonts::semi (9.5f, 0.34f));
        const float w1 = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), u8 ("TEMPO  \xc2\xb7  ")),
                    w2 = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "EXTREM");
        const float x = 165.0f - (w1 + w2) * 0.5f;
        g.setColour (col::dim); g.drawText (u8 ("TEMPO  \xc2\xb7  "), juce::Rectangle<float> (x, 82, w1 + 2, 14), juce::Justification::centredLeft, false);
        g.setGradientFill (accent (x + w1, 0, x + w1 + w2, 0));
        g.drawText ("EXTREM", juce::Rectangle<float> (x + w1, 82, w2 + 4, 14), juce::Justification::centredLeft, false);
    }
    else
        caps (g, "TEMPO", { 40, 82, 250, 14 }, 9.5f, col::dim.withMultipliedAlpha (liveAlpha));
    {
        const juce::String v = fmtTempo (proc.getTempoPercent());
        auto big = fonts::display (34.0f, 0.01f), small = fonts::light (15.0f);
        const float wv = juce::GlyphArrangement::getStringWidth (big, v), ws = juce::GlyphArrangement::getStringWidth (small, "%");
        const float x = 165.0f - (wv + 6.0f + ws) * 0.5f;
        g.setColour (col::ink.withMultipliedAlpha (liveAlpha));
        g.setFont (big); g.drawText (v, juce::Rectangle<float> (x, 284, wv + 2, 40), juce::Justification::centredLeft, false);
        g.setColour (col::dim.withMultipliedAlpha (liveAlpha));
        g.setFont (small); g.drawText ("%", juce::Rectangle<float> (x + wv + 6.0f, 296, ws + 4, 22), juce::Justification::centredLeft, false);
    }
    if (live)
        caps (g, "NUR IM RENDER-MODUS", { 40, 340, 250, 14 }, 8.5f, col::faint);
    else
    {
        g.setColour (col::faint);
        g.setFont (fonts::light (12.0f));
        g.drawText (u8 ("\xe2\x86\x92"), juce::Rectangle<float> (148, 333, 32, 30), juce::Justification::centred, false);
        g.setFont (fonts::medium (11.0f, 0.06f));
        g.setColour (col::dim);
        g.drawText (bpmNote, juce::Rectangle<float> (40, 368, 146, 20), juce::Justification::centredRight, false);
    }

    // PITCH
    caps (g, "PITCH", { 670, 82, 250, 14 });
    {
        const float semis = (float) pitch.getValue(), cents = (float) fine.getValue();
        const juce::String a = (semis > 0 ? "+" : semis < 0 ? u8 ("\xe2\x88\x92") : juce::String()) + juce::String (std::abs ((int) semis));
        const juce::String b = (cents > 0 ? "+" : cents < 0 ? u8 ("\xe2\x88\x92") : juce::String()) + juce::String (std::abs ((int) cents));
        auto big = fonts::display (34.0f), mid = fonts::display (20.0f), small = fonts::light (15.0f);
        const float w1 = juce::GlyphArrangement::getStringWidth (big, a), w2 = juce::GlyphArrangement::getStringWidth (small, "st"),
                    w3 = juce::GlyphArrangement::getStringWidth (mid, b), w4 = juce::GlyphArrangement::getStringWidth (small, "ct");
        float x = 795.0f - (w1 + 5 + w2 + 18 + w3 + 5 + w4) * 0.5f;
        g.setFont (big); g.setColour (col::ink); g.drawText (a, juce::Rectangle<float> (x, 284, w1 + 2, 40), juce::Justification::centredLeft, false); x += w1 + 5;
        g.setFont (small); g.setColour (col::dim); g.drawText ("st", juce::Rectangle<float> (x, 296, w2 + 4, 22), juce::Justification::centredLeft, false); x += w2 + 18;
        g.setFont (mid); g.setColour (col::dim); g.drawText (b, juce::Rectangle<float> (x, 290, w3 + 2, 30), juce::Justification::centredLeft, false); x += w3 + 5;
        g.setFont (small); g.drawText ("ct", juce::Rectangle<float> (x, 296, w4 + 4, 22), juce::Justification::centredLeft, false);
    }
    caps (g, "FINE", { 706, 386, 80, 12 }, 8.5f);
    caps (g, "FORMANT", { 795, 386, 80, 12 }, 8.5f);
    {
        const double fv = formant.getValue();
        caps (g, std::abs (fv) < 0.05 ? juce::String ("0.0") : ((fv > 0 ? "+" : "") + juce::String (fv, 1)),
              { 795, 399, 80, 12 }, 8.5f, col::faint, juce::Justification::centred, 0.1f);
    }

    if (extreme && ! live)
    {
        caps (g, "TEXTUR", { 283, 386, 80, 12 }, 8.5f);
        const double tv = texture.getValue();
        caps (g, tv < 0.04 ? juce::String ("AUTO") : juce::String (tv, 2) + " S", { 283, 399, 80, 12 }, 8.5f, col::faint, juce::Justification::centred, 0.1f);
    }

    // Länge
    if (! live)
    {
        juce::String t = u8 ("L\xc3\x84NGE  \xc3\x97") + juce::String (s.timeRatio, s.timeRatio >= 100.0 ? 0 : s.timeRatio >= 10.0 ? 1 : 3);
        bool tooLong = false;
        if (auto c = proc.worker.getClip())
        {
            const double outS = c->lengthSeconds() * s.timeRatio;
            tooLong = outS > wh::maxRenderSeconds;
            t << u8 ("  \xc2\xb7  ") << fmtTime (c->lengthSeconds()) << u8 (" \xe2\x86\x92 ") << fmtTime (outS);
            if (tooLong) t << u8 ("  \xc2\xb7  RENDER MAX. 2 H, H\xc3\x96REN GEHT");
        }
        caps (g, t, { 230, 410, 500, 14 }, 9.5f, tooLong ? col::cyan.withAlpha (0.8f) : col::dim, juce::Justification::centred, 0.3f);
    }

    // Statuszeile
    g.setFont (fonts::medium (11.0f, 0.06f));
    g.setColour (col::dim);
    g.drawText (metaText, juce::Rectangle<float> (352, 540, live ? 560.0f : 182.0f, 34), juce::Justification::centredLeft, true);

    // Rahmen
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (1.0f, 1.0f, W - 2.0f, 1.0f);

    if (dropHover)
    {
        auto r = wave.getBounds().toFloat().expanded (2.0f);
        g.setGradientFill (accent (r.getX(), 0, r.getRight(), 0));
        g.drawRoundedRectangle (r, 12.0f, 1.5f);
    }
}

void Panel::refreshTexts()
{
    const double src = proc.getSourceBpm();
    srcBpm.setValue (src);
    dstBpm.setValue (src > 0.0 ? src * proc.getTempoPercent() / 100.0 : 0.0);
    const double host = proc.getHostBpm();
    hostBtn.setButtonText (host > 0.0 ? "PROJEKT " + juce::String (host, host == std::floor (host) ? 0 : 1) : "PROJEKT");
    hostBtn.setEnabled (host > 0.0 && src > 0.0);
    auto c = proc.worker.getClip();
    bpmNote = src > 0.0 ? (c != nullptr && std::abs (c->detectedBpm - src) < 0.01 ? u8 ("BPM  \xc2\xb7  erkannt") : "BPM")
                        : "BPM eingeben";
    tunnel.setTargets (live ? 1.0 : proc.currentRenderSettings().timeRatio, pitch.getValue() + fine.getValue() * 0.01, formant.getValue());
}

void Panel::setMode (bool render)
{
    if (auto* p = proc.apvts.getParameter ("mode"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (render ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

void Panel::timerCallback()
{
    live = proc.isLiveMode();
    if (modeSwitch.renderMode == live) { modeSwitch.renderMode = ! live; modeSwitch.repaint(); }

    const auto st = proc.worker.getStatus();
    const bool rendering = st == wh::Worker::Status::rendering;
    const int gen = proc.worker.getGeneration();
    if (gen != lastGeneration)
    {
        lastGeneration = gen;
        auto c = proc.worker.getClip();
        if (c != nullptr && c->detectedBpm > 0.0 && (userLoadPending || proc.getSourceBpm() <= 0.0))
            proc.setSourceBpm (c->detectedBpm);
        if (c != nullptr && st != wh::Worker::Status::loading) userLoadPending = false;
    }

    tempo.setEnabled (! live);
    attackLock.setEnabled (! live);
    for (juce::Component* comp : { (juce::Component*) &srcBpm, (juce::Component*) &dstBpm, (juce::Component*) &hostBtn,
                                   (juce::Component*) &dragBtn, (juce::Component*) &renderBtn })
        comp->setVisible (! live);

    auto clip = proc.worker.getClip();
    auto res = proc.worker.getResult();
    const bool stale = res == nullptr || res->settings != proc.currentRenderSettings();
    renderBtn.setEnabled (clip != nullptr && ! proc.isCapturing() && st != wh::Worker::Status::loading);
    renderBtn.setButtonText (rendering ? "STOP" : "RENDER");
    renderBtn.progress = rendering ? proc.worker.getProgress() : -1.0f;
    renderBtn.repaint();
    dragBtn.setEnabled (! stale && res != nullptr && res->file.existsAsFile());

    if (live)
        metaText = u8 ("Live-Pitch  \xc2\xb7  Formant ") + juce::String (formantLock.getToggleState() ? "fest" : "frei");
    else if (st == wh::Worker::Status::error)
        metaText = proc.worker.getMessage();
    else if (st == wh::Worker::Status::loading)
        metaText = "Lade ...";
    else if (rendering)
        metaText = "Rendere ...  " + juce::String (juce::roundToInt (proc.worker.getProgress() * 100.0f)) + " %";
    else if (clip != nullptr)
        metaText = clip->name + u8 ("  \xc2\xb7  ") + fmtTime (clip->lengthSeconds()) + u8 ("  \xc2\xb7  ") + juce::String (clip->sampleRate / 1000.0, 1) + " kHz";
    else
        metaText = "Kein Clip geladen";

    // Extrem-Bereich: Textur-Regler einblenden
    extreme = ! live && proc.currentRenderSettings().isExtreme();
    texture.setVisible (extreme);
    attackLock.setEnabled (! live && ! extreme);

    // Vorhören
    const bool playingNow = proc.preview.isPlaying();
    if (live && playingNow) proc.preview.stop();
    previewBtn.setVisible (! live);
    previewBtn.setEnabled (clip != nullptr && ! proc.isCapturing());
    previewBtn.icon = playingNow ? PillButton::Icon::stop : PillButton::Icon::play;
    previewBtn.setButtonText (playingNow ? "STOP" : juce::String::fromUTF8 ("H\xc3\x96REN"));
    if (playingNow)
    {
        // Regler geändert: kurz warten, dann an der aktuellen Stelle neu starten
        const auto cur = proc.currentRenderSettings();
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (cur != lastPreviewSettings)
        {
            if (previewDirtySince < 0.0) previewDirtySince = now;
            if (now - previewDirtySince > 120.0)
            {
                lastPreviewSettings = cur; previewDirtySince = -1.0;
                proc.preview.update (cur);
            }
        }
    }
    previewBtn.repaint();

    // Wurmloch füttern
    tunnel.setRendering (rendering, proc.worker.getProgress());
    tunnel.setPreview (playingNow, proc.preview.getLevel(), (float) (live ? 1.0 : proc.currentRenderSettings().timeRatio));
    if (proc.worker.getRenderCount() != lastRenderCount) { lastRenderCount = proc.worker.getRenderCount(); tunnel.flash(); }
    wave.update();
    refreshTexts();
    repaint (0, 280, (int) W, 140);
    repaint (0, 76, 340, 24);
    repaint (340, 536, 200, 40);
}

bool Panel::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (live) return false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void Panel::filesDropped (const juce::StringArray& files, int, int)
{
    dropHover = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"))
        {
            userLoadPending = true;
            proc.preview.stop();
            proc.worker.loadFile (juce::File (f));
            break;
        }
    repaint();
}

void Panel::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Audio laden", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.existsAsFile()) { userLoadPending = true; proc.preview.stop(); proc.worker.loadFile (f); }
                          });
}

void Panel::toggleCapture()
{
    if (proc.isCapturing()) { userLoadPending = true; proc.stopCapture(); }
    else { proc.preview.stop(); proc.startCapture(); }
}

void Panel::startOrCancelRender()
{
    if (proc.worker.getStatus() == wh::Worker::Status::rendering) { proc.worker.cancel(); return; }
    proc.worker.render (proc.currentRenderSettings());
}

void Panel::togglePreview()
{
    if (proc.preview.isPlaying()) { proc.preview.stop(); return; }
    if (auto c = proc.worker.getClip())
    {
        lastPreviewSettings = proc.currentRenderSettings();
        proc.preview.play (c, lastPreviewSettings, 0.0);
        tunnel.kick (0.6f);
    }
}

void Panel::dragResultOut()
{
    auto r = proc.worker.getResult();
    if (r == nullptr || ! r->file.existsAsFile() || r->settings != proc.currentRenderSettings()) return;
    proc.worker.markExported (r->file);
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ r->file.getFullPathName() }, false, this);
}
} // namespace whl

// ---------------------------------------------------------------------------
WormholeEditor::WormholeEditor (WormholeProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (720, 450, 1920, 1200);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (whl::W / whl::H);
    setSize ((int) whl::W, (int) whl::H);
}

void WormholeEditor::resized()
{
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / whl::W));
}
