#include "PluginEditor.h"
#include "TTSAssets.h"

using namespace ttsui;

namespace
{
juce::Image loadImage (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); }

constexpr int knobBox (float r) { return (int) (r * 2.0f + 24.0f); }

juce::Rectangle<int> centred (int cx, int cy, int size) { return { cx - size / 2, cy - size / 2, size, size }; }

juce::Path wavePolygon (float w, float h)
{
    juce::Path p;
    p.startNewSubPath (0.01f * w, 0.01f * h);
    p.lineTo (0.894f * w, 0.01f * h);
    p.lineTo (0.996f * w, 0.47f * h);
    p.lineTo (0.948f * w, 0.99f * h);
    p.lineTo (0.025f * w, 0.99f * h);
    p.lineTo (0.003f * w, 0.86f * h);
    p.lineTo (0.003f * w, 0.13f * h);
    p.closeSubPath();
    return p;
}

juce::String mb (juce::int64 bytes) { return juce::String (juce::roundToInt ((double) bytes / 1048576.0)); }
} // namespace

// ---------------------------------------------------------------- Editor

TtsEditor::TtsEditor (TtsProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    bg = loadImage (TTSAssets::tts_bg_jpg, TTSAssets::tts_bg_jpgSize);
    bugGlow = loadImage (TTSAssets::tts_bugglow_png, TTSAssets::tts_bugglow_pngSize);
    capBig = loadImage (TTSAssets::tts_knob_big_png, TTSAssets::tts_knob_big_pngSize);
    capSmall = loadImage (TTSAssets::tts_knob_small_png, TTSAssets::tts_knob_small_pngSize);

    addAndMakeVisible (content);
    content.setBounds (0, 0, baseW, baseH);
    content.painter = [this] (juce::Graphics& g) { paintContent (g); };
    content.overPainter = [this] (juce::Graphics& g) { paintOver (g); };

    // ---- Stimme / Akzent
    accentBox.addItem ("US English", 1);
    accentBox.addItem ("UK English", 2);
    accentBox.setSelectedId (proc.isBritish() ? 2 : 1, juce::dontSendNotification);
    accentBox.onChange = [this]
    {
        const bool gb = accentBox.getSelectedId() == 2;
        if (gb == proc.isBritish()) return;
        proc.setBritish (gb);
        // erste passende Stimme nehmen
        for (auto& v : ttsVoices)
            if (v.british == gb) { proc.setVoice (v.id); break; }
        fillVoices();
        if (proc.hasAudio()) proc.generate();
    };
    voiceBox.onChange = [this]
    {
        const int idx = voiceBox.getSelectedId() - 1;
        if (idx < 0 || idx >= (int) std::size (ttsVoices)) return;
        if (proc.getVoice() == ttsVoices[idx].id) return;
        proc.setVoice (ttsVoices[idx].id);
        if (proc.hasAudio()) proc.generate();
    };
    fillVoices();
    content.addAndMakeVisible (voiceBox);
    content.addAndMakeVisible (accentBox);
    voiceBox.setBounds (790, 68, 246, 42);
    accentBox.setBounds (1205, 68, 216, 42);

    content.addAndMakeVisible (gearBtn);
    gearBtn.setBounds (1444, 61, 50, 50);
    gearBtn.onClick = [this]
    {
        settings.setVisible (! settings.isVisible());
        if (settings.isVisible()) settings.toFront (true);
    };

    // ---- Text
    textBox.setMultiLine (true, true);
    textBox.setReturnKeyStartsNewLine (true);
    textBox.setScrollbarsShown (true);
    textBox.setFont (mono (24.0f));
    textBox.setIndents (14, 12);
    textBox.setBorder (juce::BorderSize<int> (0));
    textBox.setTextToShowWhenEmpty ("Enter your text here...", col::dim);
    textBox.setText (proc.getText(), juce::dontSendNotification);
    textBox.onTextChange = [this] { proc.setText (textBox.getText()); };
    content.addAndMakeVisible (textBox);
    textBox.setBounds (576, 146, 862, 187);

    content.addAndMakeVisible (scrollUp);
    content.addAndMakeVisible (scrollDown);
    scrollUp.setBounds (1442, 147, 33, 32);
    scrollDown.setBounds (1442, 298, 33, 32);
    // die gemalten Pfeile der Scrollleiste bewegen den Cursor zeilenweise
    scrollUp.onClick = [this] { textBox.moveCaretUp (false); };
    scrollDown.onClick = [this] { textBox.moveCaretDown (false); };

    // ---- Transport
    for (auto* b : { &playBtn, &stopBtn, &genBtn, &saveBtn }) content.addAndMakeVisible (b);
    playBtn.setBounds (573, 366, 183, 62);
    stopBtn.setBounds (786, 366, 180, 62);
    genBtn.setBounds (997, 366, 244, 62);
    saveBtn.setBounds (1272, 366, 207, 62);
    playBtn.onClick = [this] { if (proc.hasAudio()) proc.play(); else { toast = "Hit GENERATE first"; toastUntil = timeSec + 1.8; } };
    stopBtn.onClick = [this] { proc.stop(); };
    genBtn.onClick = [this]
    {
        proc.setText (textBox.getText());
        if (textBox.getText().trim().isEmpty()) { toast = "Enter some text first"; toastUntil = timeSec + 1.8; textBox.grabKeyboardFocus(); return; }
        if (! proc.engineHolder().isReady()) { toast = "Voice engine not ready yet"; toastUntil = timeSec + 2.0; }
        proc.generate();
    };
    saveBtn.onClick = [this] { saveWav(); };

    // ---- Regler
    auto& s = proc.apvts;
    pitchK = std::make_unique<Knob> (capBig, 44.0f, 48.5f, 8.0f, true, false);
    speedK = std::make_unique<Knob> (capBig, 44.0f, 48.5f, 8.0f, true, false);
    volumeK = std::make_unique<Knob> (capBig, 44.0f, 48.5f, 8.0f, false, false);
    formantK = std::make_unique<Knob> (capBig, 44.0f, 48.5f, 8.0f, true, false);
    reverbK = std::make_unique<Knob> (capSmall, 21.0f, 23.5f, 5.0f, false, true);
    delayK = std::make_unique<Knob> (capSmall, 21.0f, 23.5f, 5.0f, false, true);
    eqK = std::make_unique<Knob> (capSmall, 21.0f, 23.5f, 5.0f, true, true);
    pitchA = std::make_unique<SA> (s, TtsProcessor::pPitch, *pitchK);
    speedA = std::make_unique<SA> (s, TtsProcessor::pSpeed, *speedK);
    volumeA = std::make_unique<SA> (s, TtsProcessor::pVolume, *volumeK);
    formantA = std::make_unique<SA> (s, TtsProcessor::pFormant, *formantK);
    reverbA = std::make_unique<SA> (s, TtsProcessor::pReverb, *reverbK);
    delayA = std::make_unique<SA> (s, TtsProcessor::pDelay, *delayK);
    eqA = std::make_unique<SA> (s, TtsProcessor::pEq, *eqK);
    struct Def { Knob* k; float def; };
    for (auto d : { Def { pitchK.get(), 0.0f }, Def { speedK.get(), 1.0f }, Def { volumeK.get(), 0.0f }, Def { formantK.get(), 1.0f },
                    Def { reverbK.get(), 20.0f }, Def { delayK.get(), 15.0f }, Def { eqK.get(), 0.0f } })
    {
        d.k->setDoubleClickReturnValue (true, d.def);
        d.k->onValueChange = [this] { content.repaint (440, 660, 1060, 100); };
        content.addAndMakeVisible (*d.k);
    }
    const int bb = knobBox (48.5f + 4.0f), sb = knobBox (23.5f + 3.0f);
    pitchK->setBounds (centred (532, 579, bb));
    speedK->setBounds (centred (700, 579, bb));
    volumeK->setBounds (centred (870, 579, bb));
    formantK->setBounds (centred (1038, 579, bb));
    reverbK->setBounds (centred (1346, 582, sb));
    delayK->setBounds (centred (1346, 653, sb));
    eqK->setBounds (centred (1346, 722, sb));

    // ---- Effekte an/aus
    fxAtt = std::make_unique<juce::ParameterAttachment> (*s.getParameter (TtsProcessor::pFxOn), [this] (float v)
    {
        fxOn = v > 0.5f;
        for (auto* k : { reverbK.get(), delayK.get(), eqK.get() }) k->setAlpha (fxOn ? 1.0f : 0.38f);
        content.repaint (1160, 460, 350, 320);
    });
    fxAtt->sendInitialUpdate();
    content.addAndMakeVisible (fxBtn);
    fxBtn.setBounds (1430, 480, 48, 48);
    fxBtn.onClick = [this] { fxAtt->setValueAsCompleteGesture (fxOn ? 0.0f : 1.0f); };

    // ---- Wellenform + Download
    content.addAndMakeVisible (wave);
    wave.setBounds (150, 784, 1074, 176);
    downloadBtn.setButtonText ("DOWNLOAD VOICE ENGINE  (" + mb (tts::EngineHolder::totalDownloadSize()) + " MB, free)");
    downloadBtn.onClick = [this] { proc.engineHolder().startDownload(); };
    cancelBtn.onClick = [this] { proc.engineHolder().cancel(); };
    cancelBtn.getProperties().set ("purple", true);
    retryBtn.onClick = [this] { proc.engineHolder().retry(); };
    folderBtn.onClick = [] { auto d = tts::EngineHolder::dataDir(); d.createDirectory(); d.startAsProcess(); };
    folderBtn.getProperties().set ("purple", true);
    for (auto* b : { &downloadBtn, &cancelBtn, &retryBtn, &folderBtn })
    {
        content.addChildComponent (b);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    downloadBtn.setBounds (420, 878, 560, 54);
    cancelBtn.setBounds (620, 902, 160, 42);
    retryBtn.setBounds (500, 902, 180, 42);
    folderBtn.setBounds (700, 902, 220, 42);

    // ---- Einstellungen
    content.addChildComponent (settings);
    settings.setBounds (1074, 122, 426, 300);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (baseW / 2, baseH / 2, baseW, baseH);
    }
    setSize (juce::roundToInt (baseW * 0.7), juce::roundToInt (baseH * 0.7));
    setWantsKeyboardFocus (true);

    lastMs = juce::Time::getMillisecondCounterHiRes();
    updateStatusButtons();
    startTimerHz (30);
}

TtsEditor::~TtsEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TtsEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (s));
}

void TtsEditor::fillVoices()
{
    voiceBox.clear (juce::dontSendNotification);
    const bool gb = proc.isBritish();
    voiceBox.addSectionHeading ("FEMALE");
    for (int i = 0; i < (int) std::size (ttsVoices); ++i)
        if (ttsVoices[i].british == gb && ttsVoices[i].female)
            voiceBox.addItem (juce::String (ttsVoices[i].name) + " (F)", i + 1);
    voiceBox.addSectionHeading ("MALE");
    for (int i = 0; i < (int) std::size (ttsVoices); ++i)
        if (ttsVoices[i].british == gb && ! ttsVoices[i].female)
            voiceBox.addItem (juce::String (ttsVoices[i].name) + " (M)", i + 1);
    for (int i = 0; i < (int) std::size (ttsVoices); ++i)
        if (proc.getVoice() == ttsVoices[i].id) voiceBox.setSelectedId (i + 1, juce::dontSendNotification);
}

void TtsEditor::syncFromProcessor()
{
    // Text kann vom Host (Projekt laden) gesetzt werden
    if (! textBox.hasKeyboardFocus (true) && textBox.getText() != proc.getText())
        textBox.setText (proc.getText(), juce::dontSendNotification);
    const int acc = proc.isBritish() ? 2 : 1;
    if (accentBox.getSelectedId() != acc) { accentBox.setSelectedId (acc, juce::dontSendNotification); fillVoices(); }
}

void TtsEditor::updateStatusButtons()
{
    using S = tts::EngineHolder::State;
    const auto st = proc.engineHolder().getState();
    downloadBtn.setVisible (st == S::missing);
    cancelBtn.setVisible (st == S::downloading);
    retryBtn.setVisible (st == S::failed);
    folderBtn.setVisible (st == S::failed);
}

void TtsEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    timeSec += (now - lastMs) / 1000.0;
    lastMs = now;

    syncFromProcessor();
    updateStatusButtons();

    genBtn.busy = proc.isRendering();
    genBtn.busyPhase = (float) (timeSec * 8.0);
    if (genBtn.busy) genBtn.repaint();

    if (proc.getAudioVersion() != lastAudioVersion)
    {
        lastAudioVersion = proc.getAudioVersion();
        peaks = proc.getPeaks (150);
    }

    content.repaint (0, 230, 460, 560);   // Käfer
    content.repaint (60, 780, 1170, 185);  // Wellenform + Pegel
    content.repaint (1260, 300, 120, 30);  // Zeichenzähler
    if (toastUntil > 0.0) content.repaint (400, 0, 736, 70);
}

// ---------------------------------------------------------------- Zeichnen

void TtsEditor::paintValueBox (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, float size, float alpha)
{
    g.setFont (mono (size));
    g.setColour (col::purple.withAlpha (0.45f * alpha));
    g.drawText (text, r.translated (0, 1), juce::Justification::centred, false);
    g.setColour (col::violet.withAlpha (alpha));
    g.drawText (text, r, juce::Justification::centred, false);
}

void TtsEditor::paintContent (juce::Graphics& g)
{
    g.drawImageAt (bg, 0, 0);

    // pulsierender Käfer
    const bool talking = proc.isPlaying();
    const double period = talking ? 0.42 : 2.6;
    const float pulse = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * timeSec / period - juce::MathConstants<double>::halfPi);
    g.setOpacity (0.12f + 0.73f * pulse);
    g.drawImageAt (bugGlow, 0, 240);
    g.setOpacity (1.0f);
    {
        const float r = 60.0f * (0.9f + 0.18f * pulse);
        juce::ColourGradient eye (juce::Colour (0xd978f0ff).withMultipliedAlpha (0.15f + 0.75f * pulse), 318.0f, 434.0f,
                                  juce::Colour (0x007840ff), 318.0f + r, 434.0f, true);
        eye.addColour (0.38, juce::Colour (0x5a3c78ff).withMultipliedAlpha (0.15f + 0.75f * pulse));
        g.setGradientFill (eye);
        g.fillEllipse (312.0f - r, 440.0f - r, r * 2.0f, r * 2.0f);
    }

    // Beschriftung "Accent:" (im Bild stand "Language:")
    g.setFont (mono (23.0f));
    g.setColour (col::label);
    g.drawText ("Accent:", 1078, 70, 120, 32, juce::Justification::centredLeft, false);

    // Generate-Icon (Waveform + Funke)
    {
        juce::Path p;
        const float x = 1034.0f, y = 376.0f;
        auto bar = [&] (float bx, float y0, float y1) { p.addRoundedRectangle (x + bx - 1.5f, y + y0, 3.0f, y1 - y0, 1.5f); };
        bar (6, 19, 29); bar (12, 13, 35); bar (18, 17, 31); bar (24, 21, 27);
        juce::Path star;
        star.startNewSubPath (x + 34, y + 5); star.lineTo (x + 36, y + 11); star.lineTo (x + 42, y + 13); star.lineTo (x + 36, y + 15);
        star.lineTo (x + 34, y + 21); star.lineTo (x + 32, y + 15); star.lineTo (x + 26, y + 13); star.lineTo (x + 32, y + 11); star.closeSubPath();
        g.setColour (col::cyan.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (5.0f));
        g.strokePath (star, juce::PathStrokeType (4.0f));
        g.setColour (col::cyan);
        g.fillPath (p);
        g.fillPath (star);
    }

    // Werte in den Feldern unter den Reglern
    auto txt = [this] (const char* id) { return proc.apvts.getParameter (id)->getCurrentValueAsText(); };
    paintValueBox (g, { 476, 680, 117, 32 }, txt (TtsProcessor::pPitch), 24.0f, 1.0f);
    paintValueBox (g, { 644, 680, 117, 32 }, txt (TtsProcessor::pSpeed), 24.0f, 1.0f);
    paintValueBox (g, { 813, 680, 117, 32 }, txt (TtsProcessor::pVolume), 24.0f, 1.0f);
    paintValueBox (g, { 981, 680, 117, 32 }, txt (TtsProcessor::pFormant), 24.0f, 1.0f);
    const float fa = fxOn ? 1.0f : 0.38f;
    paintValueBox (g, { 1397, 568, 74, 30 }, txt (TtsProcessor::pReverb), 18.0f, fa);
    paintValueBox (g, { 1397, 639, 74, 30 }, txt (TtsProcessor::pDelay), 18.0f, fa);
    paintValueBox (g, { 1397, 709, 74, 30 }, txt (TtsProcessor::pEq), 18.0f, fa);
    if (! fxOn)
    {
        g.setColour (juce::Colour (0xb30b1424));
        g.fillEllipse (1431.0f, 481.0f, 46.0f, 46.0f);
    }

    // Zeichenzähler
    g.setFont (mono (15.0f));
    g.setColour (juce::Colour (0xff6d64a0));
    g.drawText (juce::String (textBox.getTotalNumChars()) + " chars", 1250, 304, 120, 20, juce::Justification::centredRight, false);

    // Pegelanzeige
    {
        const float lvl = juce::jlimit (0.0f, 1.0f, proc.getLevel());
        const int segs = 9;
        const float h = 150.0f, gap = 4.0f, sh = (h - gap * (segs - 1)) / segs;
        for (int i = 0; i < segs; ++i)
        {
            const bool on = (float) (i + 1) / segs <= lvl + 0.001f;
            const bool top = i >= 7;
            const float y = 804.0f + h - (float) (i + 1) * sh - (float) i * gap;
            juce::Rectangle<float> r (71.0f, y, 34.0f, sh);
            if (on)
            {
                const auto c = top ? juce::Colour (0xffb45cff) : juce::Colour (0xff22e6f5);
                g.setColour (c.withAlpha (0.3f));
                g.fillRect (r.expanded (2.0f));
                g.setColour (c);
            }
            else
                g.setColour (juce::Colour (0x47286e82));
            g.fillRect (r);
        }
    }
}

void TtsEditor::paintOver (juce::Graphics& g)
{
    if (toastUntil > 0.0 && timeSec < toastUntil)
    {
        g.setFont (mono (19.0f));
        const int w = juce::jmax (200, (int) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), toast) + 40);
        juce::Rectangle<float> r ((float) (baseW - w) / 2.0f, 14.0f, (float) w, 40.0f);
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::cyan2);
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
        g.setColour (col::cyan);
        g.drawText (toast, r, juce::Justification::centred, false);
    }
    else if (toastUntil > 0.0)
        toastUntil = 0.0;
}

// ---------------------------------------------------------------- Wellenform / Status

bool TtsEditor::WaveView::hitTest (int x, int y)
{
    return wavePolygon ((float) getWidth(), (float) getHeight()).contains ((float) x, (float) y);
}

void TtsEditor::WaveView::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const auto poly = wavePolygon (w, h);
    g.reduceClipRegion (poly);

    // Raster
    g.setColour (juce::Colour (0x145a46dc));
    for (float x = 0; x < w; x += 24.0f) g.drawVerticalLine ((int) x, 0.0f, h);
    for (float y = 16.0f; y < h; y += 24.0f) g.drawHorizontalLine ((int) y, 0.0f, w);
    g.setColour (col::purple.withAlpha (0.35f));
    g.fillRect (0.0f, h * 0.5f - 1.0f, w, 2.0f);

    using S = tts::EngineHolder::State;
    auto& holder = ed.proc.engineHolder();
    const auto st = holder.getState();
    const auto t = ed.timeSec;
    const juce::String dots = juce::String::repeatedString (".", 1 + ((int) (t * 3.0)) % 3);

    auto bar = [&] (float frac, juce::Rectangle<float> r, bool indeterminate)
    {
        g.setColour (juce::Colour (0xff120c24));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (col::purple.withAlpha (0.6f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);
        auto fill = r.reduced (3.0f);
        if (indeterminate)
        {
            const float seg = fill.getWidth() * 0.25f;
            const float x = fill.getX() + (float) std::fmod (t * 0.45, 1.0) * (fill.getWidth() + seg) - seg;
            fill = fill.withX (juce::jmax (fill.getX(), x)).withRight (juce::jmin (r.getRight() - 3.0f, x + seg));
        }
        else
            fill = fill.withWidth (fill.getWidth() * juce::jlimit (0.0f, 1.0f, frac));
        if (fill.getWidth() <= 0) return;
        juce::ColourGradient grad (col::cyan2, fill.getX(), 0, col::purple, r.getRight(), 0, false);
        g.setColour (col::cyan.withAlpha (0.25f));
        g.fillRoundedRectangle (fill.expanded (3.0f), fill.getHeight() * 0.5f + 3.0f);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, fill.getHeight() * 0.5f);
        // Glanzlicht läuft durch
        const float gx = fill.getX() + (float) std::fmod (t * 0.8, 1.0) * fill.getWidth();
        juce::ColourGradient shine (juce::Colours::white.withAlpha (0.0f), gx - 60.0f, 0, juce::Colours::white.withAlpha (0.0f), gx + 60.0f, 0, false);
        shine.addColour (0.5, juce::Colours::white.withAlpha (0.35f));
        g.setGradientFill (shine);
        g.fillRoundedRectangle (fill, fill.getHeight() * 0.5f);
    };

    g.setFont (mono (21.0f));
    if (st == S::missing)
    {
        g.setColour (col::violet);
        g.drawText ("VOICE ENGINE NOT INSTALLED  -  ONE CLICK, ONE TIME", 0, 24, (int) w, 30, juce::Justification::centred);
        g.setFont (mono (15.0f));
        g.setColour (col::dim);
        auto err = holder.getError();
        g.drawText (err.isNotEmpty() ? err : "Kokoro voices, runs offline after the download", 0, 154, (int) w, 18, juce::Justification::centred);
        return;
    }
    if (st == S::downloading)
    {
        const auto total = tts::EngineHolder::totalDownloadSize();
        const auto got = holder.getDownloaded();
        const float frac = total > 0 ? (float) got / (float) total : 0.0f;
        g.setColour (col::cyan);
        g.drawText ("DOWNLOADING VOICE ENGINE" + dots, 40, 22, (int) w - 80, 30, juce::Justification::centredLeft);
        g.setColour (col::violet);
        g.drawText (mb (got) + " / " + mb (total) + " MB  (" + juce::String (juce::roundToInt (frac * 100.0f)) + " %)",
                    40, 22, (int) w - 120, 30, juce::Justification::centredRight);
        bar (frac, { 40.0f, 64.0f, w - 120.0f, 26.0f }, false);
        return;
    }
    if (st == S::loading)
    {
        g.setColour (col::cyan);
        g.drawText ("LOADING VOICE ENGINE" + dots, 0, 30, (int) w, 30, juce::Justification::centred);
        bar (0.0f, { 160.0f, 80.0f, w - 320.0f, 22.0f }, true);
        return;
    }
    if (st == S::failed)
    {
        g.setColour (juce::Colour (0xffff5f9e));
        g.drawText ("SOMETHING WENT WRONG", 0, 16, (int) w, 28, juce::Justification::centred);
        g.setFont (mono (16.0f));
        g.setColour (col::violet);
        g.drawFittedText (holder.getError(), 60, 46, (int) w - 120, 40, juce::Justification::centred, 2);
        return;
    }

    if (ed.proc.isRendering())
    {
        const float f = ed.proc.getRenderProgress();
        g.setColour (col::cyan);
        g.drawText ("GENERATING" + dots, 60, 34, (int) w - 120, 28, juce::Justification::centredLeft);
        g.setColour (col::violet);
        g.drawText (juce::String (juce::roundToInt (f * 100.0f)) + " %", 60, 34, (int) w - 160, 28, juce::Justification::centredRight);
        bar (f, { 60.0f, 76.0f, w - 160.0f, 22.0f }, f <= 0.001f);
        return;
    }
    const auto err = ed.proc.getRenderError();
    if (ed.peaks.empty())
    {
        g.setColour (err.isNotEmpty() ? juce::Colour (0xffff5f9e) : juce::Colour (0x99a096d7));
        g.drawText (err.isNotEmpty() ? err : juce::String ("Type text and hit GENERATE"), 0, 64, (int) w, 30, juce::Justification::centred);
        return;
    }

    // Balken
    const int n = (int) ed.peaks.size();
    const float x0 = 30.0f, x1 = w - 80.0f, step = (x1 - x0) / (float) n;
    const float head = ed.proc.isPlaying() ? ed.proc.getPlayhead() : -1.0f;
    const float vol = juce::Decibels::decibelsToGain (ed.proc.apvts.getRawParameterValue (TtsProcessor::pVolume)->load());
    for (int i = 0; i < n; ++i)
    {
        const float hh = juce::jmin (0.95f, ed.peaks[(size_t) i] * vol) * 80.0f;
        const bool played = head >= 0.0f && (float) i / (float) n <= head;
        const auto c = played ? juce::Colour (0xff5ff6ff) : juce::Colour (0xffa24cff);
        const float x = x0 + (float) i * step;
        g.setColour (c.withAlpha (0.25f));
        g.fillRoundedRectangle (x - 2.0f, h * 0.5f - hh - 2.0f, 7.0f, hh * 2.0f + 4.0f, 3.0f);
        g.setColour (c);
        g.fillRoundedRectangle (x, h * 0.5f - hh, 3.0f, hh * 2.0f, 1.0f);
    }
    if (head >= 0.0f)
    {
        const float x = x0 + head * (x1 - x0);
        g.setColour (col::cyan.withAlpha (0.3f));
        g.fillRect (x - 4.0f, 4.0f, 8.0f, h - 8.0f);
        g.setColour (col::cyan);
        g.fillRect (x - 1.0f, 4.0f, 2.0f, h - 8.0f);
    }
    g.setFont (mono (15.0f));
    g.setColour (juce::Colour (0xff6d64a0));
    const double secs = ed.proc.getAudioSeconds();
    const juce::String len = secs >= 60.0 ? juce::String ((int) secs / 60) + ":" + juce::String ((int) secs % 60).paddedLeft ('0', 2) + " min"
                                          : juce::String (secs, 1) + " s";
    g.drawText (len + "   |   DRAG TO DAW", 0, 10, (int) w - 150, 18, juce::Justification::centredRight);
}

void TtsEditor::WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged || e.getDistanceFromDragStart() < 8 || ed.peaks.empty() || ed.proc.isRendering()) return;
    dragged = true;
    auto f = ed.writeDragFile();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

void TtsEditor::WaveView::mouseUp (const juce::MouseEvent&)
{
    if (dragged || ed.peaks.empty()) return;
    if (ed.proc.isPlaying()) ed.proc.stop();
    else ed.proc.play();
}

juce::File TtsEditor::writeDragFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("BrainbugzTTS");
    dir.createDirectory();
    auto f = dir.getChildFile (proc.suggestedFileName());
    juce::String err;
    if (! proc.exportWav (f, err)) { toast = err; toastUntil = timeSec + 2.5; return {}; }
    return f;
}

void TtsEditor::saveWav()
{
    if (! proc.hasAudio()) { toast = "Nothing to save yet"; toastUntil = timeSec + 1.8; return; }
    auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (proc.suggestedFileName());
    chooser = std::make_unique<juce::FileChooser> ("Save voice as WAV", start, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
                              juce::String err;
                              if (proc.exportWav (f, err)) toast = "Saved: " + f.getFileName();
                              else toast = "Save failed: " + err;
                              toastUntil = timeSec + 2.5;
                          });
}

// ---------------------------------------------------------------- Einstellungen

TtsEditor::Settings::Settings (TtsEditor& e) : ed (e)
{
    blendVoice.addItem ("- none -", 1);
    for (int i = 0; i < (int) std::size (ttsVoices); ++i)
        blendVoice.addItem (juce::String (ttsVoices[i].name) + (ttsVoices[i].female ? " (F" : " (M") + (ttsVoices[i].british ? ", UK)" : ", US)"), i + 2);
    blendVoice.setSelectedId (1, juce::dontSendNotification);
    for (int i = 0; i < (int) std::size (ttsVoices); ++i)
        if (ed.proc.getBlendVoice() == ttsVoices[i].id) blendVoice.setSelectedId (i + 2, juce::dontSendNotification);
    blendVoice.onChange = [this]
    {
        const int id = blendVoice.getSelectedId();
        ed.proc.setBlendVoice (id >= 2 ? juce::String (ttsVoices[id - 2].id) : juce::String());
        if (ed.proc.hasAudio()) ed.proc.generate();
    };
    blend.setRange (0.0, 100.0, 1.0);
    blend.setValue (ed.proc.getBlend() * 100.0, juce::dontSendNotification);
    blend.onValueChange = [this] { ed.proc.setBlend ((float) blend.getValue() / 100.0f); repaint(); };
    blend.onDragEnd = [this] { if (ed.proc.hasAudio() && ed.proc.getBlendVoice().isNotEmpty()) ed.proc.generate(); };
    folder.onClick = [] { auto d = tts::EngineHolder::dataDir(); d.createDirectory(); d.startAsProcess(); };
    folder.getProperties().set ("purple", true);
    close.onClick = [this] { setVisible (false); };
    for (juce::Component* c : { (juce::Component*) &blendVoice, (juce::Component*) &blend, (juce::Component*) &folder, (juce::Component*) &close })
        addAndMakeVisible (c);
}

void TtsEditor::Settings::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xf70b0718));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::cyan2.withAlpha (0.35f));
    g.drawRoundedRectangle (r.expanded (2.0f), 10.0f, 4.0f);
    g.setColour (col::cyan2);
    g.drawRoundedRectangle (r, 8.0f, 2.0f);
    g.setFont (mono (24.0f));
    g.setColour (col::cyan);
    g.drawText ("SETTINGS", 20, 14, 300, 30, juce::Justification::centredLeft);
    g.setFont (mono (18.0f));
    g.setColour (juce::Colour (0xffa39ad6));
    g.drawText ("Voice blend", 20, 62, 160, 36, juce::Justification::centredLeft);
    g.drawText ("Blend amount", 20, 110, 160, 30, juce::Justification::centredLeft);
    g.setColour (col::violet);
    g.drawText (juce::String (juce::roundToInt (blend.getValue())) + "%", 340, 110, 66, 30, juce::Justification::centredRight);
    g.setFont (mono (14.0f));
    g.setColour (col::dim);
    g.drawFittedText ("Mixes the main voice with a second Kokoro voice (e.g. 70 / 30).", 20, 150, 386, 36,
                      juce::Justification::centredLeft, 2);
}

void TtsEditor::Settings::resized()
{
    blendVoice.setBounds (180, 62, 226, 36);
    blend.setBounds (180, 110, 160, 30);
    folder.setBounds (20, 240, 250, 40);
    close.setBounds (286, 240, 120, 40);
}
