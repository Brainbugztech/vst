#include "PluginEditor.h"
#include "TTSAssets.h"

using namespace ttsui;
using namespace ttsparams;

namespace
{
juce::Image loadImage (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); }
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

const juce::Rectangle<float> bigPanel { 420.0f, 459.0f, 1082.0f, 308.0f };
const juce::Rectangle<float> eqRect { 436.0f, 474.0f, 492.0f, 280.0f };
const juce::Rectangle<float> dlRect { 940.0f, 474.0f, 274.0f, 280.0f };
const juce::Rectangle<float> rvRect { 1226.0f, 474.0f, 264.0f, 280.0f };
const juce::Rectangle<float> cloudRect { 446.0f, 512.0f, 386.0f, 240.0f };
const juce::Colour bandColour[3] = { juce::Colour (0xff2ff3ff), juce::Colour (0xffb06cff), juce::Colour (0xffff5fc8) };
} // namespace

// ================================================================ Aufbau

void TtsEditor::addParamKnob (Page& page, std::unique_ptr<ParamKnob>& k, const char* id, const juce::String& label,
                              juce::Rectangle<int> r, bool bipolar, float capR)
{
    k = std::make_unique<ParamKnob> (proc.apvts, id, label, capSmall, bipolar, capR);
    page.addAndMakeVisible (*k);
    k->setBounds (r);
}

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
    auto& s = proc.apvts;

    // ---------------- immer sichtbar: Stimme, Text, Transport
    accentBox.addItem ("US English", 1);
    accentBox.addItem ("UK English", 2);
    accentBox.setSelectedId (proc.isBritish() ? 2 : 1, juce::dontSendNotification);
    accentBox.onChange = [this]
    {
        const bool gb = accentBox.getSelectedId() == 2;
        if (gb == proc.isBritish()) return;
        proc.setBritish (gb);
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
    scrollUp.onClick = [this] { textBox.moveCaretUp (false); };
    scrollDown.onClick = [this] { textBox.moveCaretDown (false); };

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

    // ---------------- Tabs
    const char* tabNames[] = { "MAIN", "GRAIN", "FX" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = tabBtn[i];
        b.setButtonText (tabNames[i]);
        b.getProperties().set ("tab", true);
        b.getProperties().set ("textScale", 0.6);
        b.setRadioGroupId (77);
        b.setClickingTogglesState (true);
        b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        b.onClick = [this, i] { showTab (i); };
        content.addAndMakeVisible (b);
        b.setBounds (440 + i * 112, 433, 104, 30);
    }

    // ---------------- Presets (rechts neben den Tabs)
    for (auto* b : { &prevPreset, &nextPreset, &savePresetBtn, &presetMenuBtn })
    {
        b->getProperties().set ("tab", true);
        b->getProperties().set ("textScale", 0.6);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        content.addAndMakeVisible (b);
    }
    savePresetBtn.getProperties().set ("purple", true);
    presetMenuBtn.getProperties().set ("purple", true);
    presetBox.getProperties().set ("framed", true);
    presetBox.setTextWhenNothingSelected ("-");
    content.addAndMakeVisible (presetBox);
    prevPreset.setBounds (800, 433, 34, 30);
    presetBox.setBounds (838, 433, 352, 30);
    nextPreset.setBounds (1194, 433, 34, 30);
    savePresetBtn.setBounds (1238, 433, 92, 30);
    presetMenuBtn.setBounds (1336, 433, 46, 30);
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    savePresetBtn.onClick = [this] { savePreset (proc.getCurrentPreset() < proc.getNumFactoryPresets()); };
    presetMenuBtn.onClick = [this] { showPresetMenu(); };
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id - 1 != proc.getCurrentPreset()) { proc.loadPreset (id - 1); refreshPresetBox (true); }
    };
    refreshPresetBox (true);

    for (auto* pg : { &mainPage, &grainPage, &fxPage })
    {
        content.addChildComponent (pg);
        pg->setBounds (0, 0, baseW, baseH);
    }
    mainPage.painter = [this] (juce::Graphics& g) { paintMainPage (g); };
    grainPage.painter = [this] (juce::Graphics& g) { paintGrainPage (g); };
    fxPage.painter = [this] (juce::Graphics& g) { paintFxPage (g); };

    // ---------------- MAIN: die vier großen Regler aus dem Entwurf
    pitchK = std::make_unique<Knob> (capBig, 44.0f, 48.5f, 8.0f, true, false);
    speedK = std::make_unique<Knob> (capBig, 44.0f, 48.5f, 8.0f, true, false);
    volumeK = std::make_unique<Knob> (capBig, 44.0f, 48.5f, 8.0f, false, false);
    formantK = std::make_unique<Knob> (capBig, 44.0f, 48.5f, 8.0f, true, false);
    pitchA = std::make_unique<SA> (s, pitch, *pitchK);
    speedA = std::make_unique<SA> (s, speed, *speedK);
    volumeA = std::make_unique<SA> (s, volume, *volumeK);
    formantA = std::make_unique<SA> (s, formant, *formantK);
    struct Def { Knob* k; float def; };
    for (auto d : { Def { pitchK.get(), 0.0f }, Def { speedK.get(), 1.0f }, Def { volumeK.get(), 0.0f }, Def { formantK.get(), 1.0f } })
    {
        d.k->setDoubleClickReturnValue (true, d.def);
        d.k->onValueChange = [this] { content.repaint (440, 660, 700, 60); };
        mainPage.addAndMakeVisible (*d.k);
    }
    const int bb = 113;
    pitchK->setBounds (centred (532, 579, bb));
    speedK->setBounds (centred (700, 579, bb));
    volumeK->setBounds (centred (870, 579, bb));
    formantK->setBounds (centred (1038, 579, bb));

    // MAIN: Voice-Mix-Panel (statt Effects)
    blendVoiceBox.getProperties().set ("framed", true);
    blendVoiceBox.addItem ("- none -", 1);
    for (int i = 0; i < (int) std::size (ttsVoices); ++i)
        blendVoiceBox.addItem (juce::String (ttsVoices[i].name) + (ttsVoices[i].female ? " (F" : " (M") + (ttsVoices[i].british ? ", UK)" : ", US)"), i + 2);
    blendVoiceBox.setSelectedId (1, juce::dontSendNotification);
    for (int i = 0; i < (int) std::size (ttsVoices); ++i)
        if (proc.getBlendVoice() == ttsVoices[i].id) blendVoiceBox.setSelectedId (i + 2, juce::dontSendNotification);
    blendVoiceBox.onChange = [this]
    {
        const int id = blendVoiceBox.getSelectedId();
        proc.setBlendVoice (id >= 2 ? juce::String (ttsVoices[id - 2].id) : juce::String());
        if (proc.getBlend() < 0.01f && id >= 2) { proc.setBlend (0.3f); blendK->setValue (30.0, juce::dontSendNotification); }
        mainPage.repaint();
        if (proc.hasAudio()) proc.generate();
    };
    mainPage.addAndMakeVisible (blendVoiceBox);
    blendVoiceBox.setBounds (1192, 584, 288, 38);
    blendK = std::make_unique<Knob> (capSmall, 26.0f, 29.5f, 6.0f, false, true);
    blendK->setRange (0.0, 100.0, 1.0);
    blendK->setValue (proc.getBlend() * 100.0, juce::dontSendNotification);
    blendK->setDoubleClickReturnValue (true, 30.0);
    blendK->onValueChange = [this] { proc.setBlend ((float) blendK->getValue() / 100.0f); mainPage.repaint (1170, 630, 330, 120); };
    blendK->onDragEnd = [this] { if (proc.hasAudio() && proc.getBlendVoice().isNotEmpty()) proc.generate(); };
    mainPage.addAndMakeVisible (*blendK);
    blendK->setBounds (centred (1250, 686, 84));

    // ---------------- GRAIN
    grainPage.addAndMakeVisible (grainPower);
    grainPower.setBounds (790, 466, 40, 40);
    grainPowerA = std::make_unique<BA> (s, grOn, grainPower);
    freezeBtn.setClickingTogglesState (true);
    freezeBtn.getProperties().set ("purple", true);
    freezeBtn.getProperties().set ("textScale", 0.55);
    freezeBtn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    grainPage.addAndMakeVisible (freezeBtn);
    freezeBtn.setBounds (676, 470, 104, 32);
    freezeA = std::make_unique<BA> (s, grFreeze, freezeBtn);
    grainPage.addAndMakeVisible (cloud);
    cloud.setBounds (cloudRect.toNearestInt());
    auto gk = [&] (std::unique_ptr<ParamKnob>& k, const char* id, const char* label, int col, int row)
    { addParamKnob (grainPage, k, id, label, juce::Rectangle<int> (856 + col * 106, row == 0 ? 474 : 616, 106, 140), false, 27.0f); };
    gk (gSize, grSize, "SIZE", 0, 0);
    gk (gFlux, grFlux, "FLUX", 1, 0);
    gk (gDensity, grDensity, "DENSITY", 2, 0);
    gk (gSpray, grSpray, "SPRAY", 3, 0);
    gk (gDrift, grDrift, "DRIFT", 4, 0);
    gk (gMix, grMix, "MIX", 5, 0);
    gk (gPitch, grPitch, "PITCH", 0, 1);
    gk (gOctave, grOctave, "OCTAVE", 1, 1);
    gk (gWidth, grWidth, "WIDTH", 2, 1);
    gk (gReverse, grReverse, "REVERSE", 3, 1);
    gk (gLfoRate, grLfoRate, "LFO RATE", 4, 1);
    gk (gLfoDepth, grLfoDepth, "LFO DEPTH", 5, 1);

    // ---------------- FX: EQ
    fxPage.addAndMakeVisible (eqPower);
    eqPower.setBounds ((int) eqRect.getRight() - 46, (int) eqRect.getY() + 4, 38, 38);
    eqPowerA = std::make_unique<BA> (s, eqOn, eqPower);
    fxPage.addAndMakeVisible (eqGraph);
    eqGraph.setBounds (450, 516, 464, 126);
    for (int b = 0; b < 3; ++b)
    {
        auto& bt = bandBtn[b];
        bt.setButtonText (juce::String (b + 1));
        bt.setRadioGroupId (78);
        bt.setClickingTogglesState (true);
        bt.getProperties().set ("textScale", 0.6);
        bt.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        bt.onClick = [this, b] { selectBand (b); };
        fxPage.addAndMakeVisible (bt);
        bt.setBounds (450 + b * 44, 652, 38, 32);

        fxPage.addChildComponent (bandPower[b]);
        bandPower[b].setBounds (586, 650, 36, 36);
        bandPowerA[b] = std::make_unique<BA> (s, eqId (b, "On"), bandPower[b]);

        bandType[b].getProperties().set ("framed", true);
        bandType[b].addItemList (eqTypeNames(), 1);
        fxPage.addChildComponent (bandType[b]);
        bandType[b].setBounds (450, 698, 176, 36);
        bandTypeA[b] = std::make_unique<CA> (s, eqId (b, "Type"), bandType[b]);
        bandType[b].onChange = [this] { updateSectionAlpha(); eqGraph.repaint(); };

        const auto fId = eqId (b, "Freq"), gId = eqId (b, "Gain"), qId = eqId (b, "Q");
        bandFreq[b] = std::make_unique<ParamKnob> (s, fId, "FREQ", capSmall, false, 22.0f);
        bandGain[b] = std::make_unique<ParamKnob> (s, gId, "GAIN", capSmall, true, 22.0f);
        bandQ[b] = std::make_unique<ParamKnob> (s, qId, "Q", capSmall, false, 22.0f);
        int x = 636;
        for (auto* k : { bandFreq[b].get(), bandGain[b].get(), bandQ[b].get() })
        {
            fxPage.addChildComponent (*k);
            k->setBounds (x, 646, 94, 106);
            k->knob.onValueChange = [this, k] { k->repaint(); eqGraph.repaint(); };
            x += 94;
        }
    }

    // FX: Delay
    fxPage.addAndMakeVisible (dlPower);
    dlPower.setBounds ((int) dlRect.getRight() - 46, (int) dlRect.getY() + 4, 38, 38);
    dlPowerA = std::make_unique<BA> (s, dlOn, dlPower);
    const int dx = (int) dlRect.getX() + 6;
    addParamKnob (fxPage, dTimeSync, dlDiv, "TIME", { dx, 516, 88, 108 });
    addParamKnob (fxPage, dTimeMs, dlTime, "TIME", { dx, 516, 88, 108 });
    addParamKnob (fxPage, dFeedback, dlFeedback, "FEEDBACK", { dx + 88, 516, 88, 108 });
    addParamKnob (fxPage, dMix, dlMix, "MIX", { dx + 176, 516, 88, 108 });
    addParamKnob (fxPage, dLow, dlLow, "LOW CUT", { dx, 634, 88, 108 });
    addParamKnob (fxPage, dHigh, dlHigh, "HIGH CUT", { dx + 88, 634, 88, 108 });
    for (auto* b : { &syncBtn, &pingBtn })
    {
        b->setClickingTogglesState (true);
        b->getProperties().set ("textScale", 0.5);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        fxPage.addAndMakeVisible (b);
    }
    syncBtn.setBounds (dx + 182, 652, 80, 32);
    pingBtn.setBounds (dx + 182, 696, 80, 32);
    syncA = std::make_unique<BA> (s, dlSync, syncBtn);
    pingA = std::make_unique<BA> (s, dlPing, pingBtn);
    syncBtn.onStateChange = [this] { const bool on = syncBtn.getToggleState(); dTimeSync->setVisible (on); dTimeMs->setVisible (! on); };

    // FX: Reverb
    fxPage.addAndMakeVisible (rvPower);
    rvPower.setBounds ((int) rvRect.getRight() - 46, (int) rvRect.getY() + 4, 38, 38);
    rvPowerA = std::make_unique<BA> (s, rvOn, rvPower);
    const int rx = (int) rvRect.getX() + 4;
    addParamKnob (fxPage, rSize, rvSize, "SIZE", { rx, 516, 86, 108 });
    addParamKnob (fxPage, rDamp, rvDamp, "DAMPING", { rx + 86, 516, 86, 108 });
    addParamKnob (fxPage, rMix, rvMix, "MIX", { rx + 172, 516, 86, 108 });
    addParamKnob (fxPage, rPre, rvPre, "PRE-DLY", { rx, 634, 86, 108 });
    addParamKnob (fxPage, rWidth, rvWidth, "WIDTH", { rx + 86, 634, 86, 108 });
    addParamKnob (fxPage, rLow, rvLow, "LOW CUT", { rx + 172, 634, 86, 108 });

    for (auto* b : { &eqPower, &dlPower, &rvPower, &grainPower, &bandPower[0], &bandPower[1], &bandPower[2] })
        b->onStateChange = [this] { updateSectionAlpha(); eqGraph.repaint(); };

    // ---------------- Wellenform + Download
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

    content.addChildComponent (settings);
    settings.setBounds (1074, 122, 426, 280);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (baseW / 2, baseH / 2, baseW, baseH);
    }
    setSize (juce::roundToInt (baseW * 0.7), juce::roundToInt (baseH * 0.7));
    setWantsKeyboardFocus (true);

    selectBand (0);
    showTab (juce::jlimit (0, 2, proc.getUiTab()));
    syncBtn.onStateChange();
    updateSectionAlpha();
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
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
}

void TtsEditor::showTab (int t)
{
    tab = t;
    proc.setUiTab (t);
    for (int i = 0; i < 3; ++i) tabBtn[i].setToggleState (i == t, juce::dontSendNotification);
    mainPage.setVisible (t == 0);
    grainPage.setVisible (t == 1);
    fxPage.setVisible (t == 2);
    content.repaint();
}

void TtsEditor::selectBand (int b)
{
    selBand = b;
    for (int i = 0; i < 3; ++i)
    {
        bandBtn[i].setToggleState (i == b, juce::dontSendNotification);
        const bool v = i == b;
        bandPower[i].setVisible (v);
        bandType[i].setVisible (v);
        bandFreq[i]->setVisible (v);
        bandGain[i]->setVisible (v);
        bandQ[i]->setVisible (v);
    }
    updateSectionAlpha();
    eqGraph.repaint();
    fxPage.repaint();
}

void TtsEditor::updateSectionAlpha()
{
    const float off = 0.42f;
    const bool eq = eqPower.getToggleState(), band = bandPower[selBand].getToggleState();
    for (int b = 0; b < 3; ++b)
    {
        const float a = (eq && bandPower[b].getToggleState()) ? 1.0f : off;
        const int type = bandType[b].getSelectedItemIndex();
        const bool cut = type == 0 || type == 4; // Low/High Cut: Gain spielt keine Rolle
        bandType[b].setAlpha (a);
        bandFreq[b]->setAlpha (a);
        bandGain[b]->setAlpha (cut ? juce::jmin (a, 0.3f) : a);
        bandQ[b]->setAlpha (a);
    }
    juce::ignoreUnused (band);
    eqGraph.setAlpha (eq ? 1.0f : 0.5f);
    const float d = dlPower.getToggleState() ? 1.0f : off;
    for (juce::Component* c : { (juce::Component*) dTimeSync.get(), (juce::Component*) dTimeMs.get(), (juce::Component*) dFeedback.get(),
                                (juce::Component*) dMix.get(), (juce::Component*) dLow.get(), (juce::Component*) dHigh.get(),
                                (juce::Component*) &syncBtn, (juce::Component*) &pingBtn })
        c->setAlpha (d);
    const float r = rvPower.getToggleState() ? 1.0f : off;
    for (auto* c : { rSize.get(), rDamp.get(), rMix.get(), rPre.get(), rWidth.get(), rLow.get() }) c->setAlpha (r);
    const float g = grainPower.getToggleState() ? 1.0f : off;
    for (auto* c : { gSize.get(), gFlux.get(), gDensity.get(), gSpray.get(), gDrift.get(), gMix.get(), gPitch.get(), gOctave.get(),
                      gWidth.get(), gReverse.get(), gLfoRate.get(), gLfoDepth.get() })
        c->setAlpha (g);
    freezeBtn.setAlpha (g);
    fxPage.repaint();
    grainPage.repaint();
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
    const int acc = proc.isBritish() ? 2 : 1;
    if (accentBox.getSelectedId() != acc) { accentBox.setSelectedId (acc, juce::dontSendNotification); fillVoices(); }
    if (! textBox.hasKeyboardFocus (true) && textBox.getText() != proc.getText())
        textBox.setText (proc.getText(), juce::dontSendNotification);
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
    refreshPresetBox();

    genBtn.busy = proc.isRendering();
    genBtn.busyPhase = (float) (timeSec * 8.0);
    if (genBtn.busy) genBtn.repaint();

    {
        juce::File f;
        juce::String key, err;
        if (proc.takeExportResult (f, key, err))
        {
            if (err.isNotEmpty()) toast = (key == "save" ? "Save failed: " : "WAV failed: ") + err;
            else if (key == "save") toast = "Saved: " + f.getFileName();
            else { dragFile = f; dragKey = key; toast = "WAV ready - drag the waveform into your DAW"; }
            toastUntil = timeSec + 3.0;
        }
    }

    if (proc.getAudioVersion() != lastAudioVersion)
    {
        lastAudioVersion = proc.getAudioVersion();
        peaks = proc.getPeaks (150);
    }

    content.repaint (0, 230, 460, 560);   // Käfer
    content.repaint (60, 780, 1170, 185);  // Wellenform + Pegel
    content.repaint (1260, 300, 120, 30);  // Zeichenzähler
    if (tab == 1) cloud.repaint();
    if (toastUntil > 0.0) content.repaint (400, 0, 736, 70);
}

// ================================================================ Presets

void TtsEditor::refreshPresetBox (bool force)
{
    const auto& list = proc.getPresetList();
    const bool modified = proc.isPresetModified();
    if (! force && (int) list.size() == shownPresetCount && proc.getCurrentPreset() == shownPreset && modified == shownModified) return;
    if (force || (int) list.size() != shownPresetCount)
    {
        presetBox.clear (juce::dontSendNotification);
        juce::String group;
        for (int i = 0; i < (int) list.size(); ++i)
        {
            if (list[(size_t) i].group != group)
            {
                group = list[(size_t) i].group;
                presetBox.addSectionHeading (group.toUpperCase());
            }
            presetBox.addItem (list[(size_t) i].name, i + 1);
        }
        shownPresetCount = (int) list.size();
    }
    shownPreset = proc.getCurrentPreset();
    shownModified = modified;
    presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
    presetBox.setText (proc.getCurrentPresetName() + (modified ? " *" : ""), juce::dontSendNotification);
}

void TtsEditor::stepPreset (int d)
{
    const int total = (int) proc.getPresetList().size();
    if (total == 0) return;
    const int cur = proc.getCurrentPreset();
    proc.loadPreset (((cur < 0 ? 0 : cur + d) % total + total) % total);
    refreshPresetBox (true);
}

void TtsEditor::savePreset (bool askName)
{
    if (! askName)
    {
        proc.saveUserPreset (proc.getCurrentPresetName());
        toast = "Preset saved";
        toastUntil = timeSec + 1.5;
        refreshPresetBox (true);
        return;
    }
    auto* w = new juce::AlertWindow ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    const bool factory = proc.getCurrentPreset() >= 0 && proc.getCurrentPreset() < proc.getNumFactoryPresets();
    w->addTextEditor ("name", proc.getCurrentPresetName() + (factory ? " (mine)" : ""));
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TtsEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto name = w->getTextEditorContents ("name");
        if (safe->proc.saveUserPreset (name)) { safe->toast = "Preset saved: " + name; }
        else safe->toast = "Could not save the preset";
        safe->toastUntil = safe->timeSec + 2.0;
        safe->refreshPresetBox (true);
    }), true);
}

void TtsEditor::showPresetMenu()
{
    juce::PopupMenu m;
    const bool isUser = proc.getCurrentPreset() >= proc.getNumFactoryPresets();
    m.addItem (1, "Save preset as ...");
    m.addItem (2, "Delete preset", isUser);
    m.addItem (3, "Open preset folder");
    m.addItem (4, "Reload preset list");
    m.addSeparator();
    m.addItem (5, "Brainbugz TTS " JucePlugin_VersionString " - Brain Bugz", false);
    juce::Component::SafePointer<TtsEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetMenuBtn), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& pr = safe->proc;
        if (r == 1) safe->savePreset (true);
        else if (r == 2) pr.deleteUserPreset (pr.getCurrentPreset());
        else if (r == 3) { auto f = pr.getPresetFolder(); f.createDirectory(); f.startAsProcess(); }
        else if (r == 4) pr.refreshUserPresets();
        safe->refreshPresetBox (true);
    });
}

// ================================================================ Zeichnen

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
    const bool talking = proc.isVoicePlaying();
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

    g.setFont (mono (15.0f));
    g.setColour (juce::Colour (0xff6d64a0));
    g.drawText (juce::String (textBox.getTotalNumChars()) + " chars", 1250, 304, 120, 20, juce::Justification::centredRight, false);

    // Pegelanzeige
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

void TtsEditor::paintMainPage (juce::Graphics& g)
{
    // Werte unter den großen Reglern
    auto txt = [this] (const char* id) { return proc.apvts.getParameter (id)->getCurrentValueAsText(); };
    paintValueBox (g, { 476, 680, 117, 32 }, txt (pitch), 24.0f, 1.0f);
    paintValueBox (g, { 644, 680, 117, 32 }, txt (speed), 24.0f, 1.0f);
    paintValueBox (g, { 813, 680, 117, 32 }, txt (volume), 24.0f, 1.0f);
    paintValueBox (g, { 981, 680, 117, 32 }, txt (formant), 24.0f, 1.0f);

    // Effects-Panel aus dem Entwurf wird zum Voice-Mix-Panel
    auto inner = chamfered ({ 1173.0f, 465.0f, 322.0f, 296.0f }, 16.0f);
    g.setColour (juce::Colour (0xff0d0a1a));
    g.fillPath (inner);
    g.setFont (mono (28.0f));
    g.setColour (col::cyan.withAlpha (0.35f));
    g.drawText ("Voice Mix", 1199, 486, 260, 40, juce::Justification::centredLeft, false);
    g.setColour (col::cyan);
    g.drawText ("Voice Mix", 1198, 485, 260, 40, juce::Justification::centredLeft, false);
    g.setColour (col::purple.withAlpha (0.8f));
    g.fillRect (1182.0f, 536.0f, 306.0f, 1.5f);
    g.setFont (mono (16.0f));
    g.setColour (col::label);
    g.drawText ("Second voice", 1194, 554, 280, 24, juce::Justification::centredLeft, false);
    const bool hasB = proc.getBlendVoice().isNotEmpty();
    g.setColour (hasB ? col::label : col::dim);
    g.drawText ("BLEND", 1302, 652, 170, 24, juce::Justification::centredLeft, false);
    g.setFont (mono (24.0f));
    g.setColour (hasB ? col::violet : col::dim);
    g.drawText (juce::String (juce::roundToInt (proc.getBlend() * 100.0f)) + "%", 1302, 676, 170, 30, juce::Justification::centredLeft, false);
    g.setFont (mono (13.0f));
    g.setColour (col::dim);
    g.drawText ("MIDI: C3 = original pitch", 1186, 730, 300, 18, juce::Justification::centredLeft, false);
}

void TtsEditor::paintGrainPage (juce::Graphics& g)
{
    drawNeonPanel (g, bigPanel);
    const bool on = grainPower.getToggleState();
    g.setFont (mono (26.0f));
    g.setColour (on ? col::cyan : col::dim);
    g.drawText ("GRAIN ENGINE", 448, 470, 240, 34, juce::Justification::centredLeft, false);
    // Trennlinie zwischen Wolke und Reglern
    g.setColour (col::purple.withAlpha (0.45f));
    g.fillRect (848.0f, 480.0f, 1.5f, 268.0f);
}

void TtsEditor::paintFxPage (juce::Graphics& g)
{
    drawNeonPanel (g, bigPanel);
    drawSection (g, eqRect, "EQ", eqPower.getToggleState());
    drawSection (g, dlRect, "DELAY", dlPower.getToggleState());
    drawSection (g, rvRect, "REVERB", rvPower.getToggleState());
    // Band-Kennfarben unter den Band-Tasten
    for (int b = 0; b < 3; ++b)
    {
        g.setColour (bandColour[b].withAlpha (bandPower[b].getToggleState() ? 0.9f : 0.3f));
        g.fillRect (452.0f + (float) b * 44.0f, 686.0f, 34.0f, 3.0f);
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

// ================================================================ Grain-Wolke

void TtsEditor::GrainCloud::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07050f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colour (0x18785adc));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8.0f), r.getY(), r.getBottom());
    for (int i = 1; i < 6; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 6.0f), r.getX(), r.getRight());

    // die Stimme als Hintergrund (x = Position in der Stimme)
    const auto& pk = ed.peaks;
    const float cy = r.getCentreY();
    if (! pk.empty())
    {
        const int n = (int) pk.size();
        g.setColour (col::purple.withAlpha (0.28f));
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + 6.0f + (r.getWidth() - 12.0f) * (float) i / (float) n;
            const float h = pk[(size_t) i] * r.getHeight() * 0.42f;
            g.fillRect (x, cy - h, juce::jmax (1.0f, (r.getWidth() - 12.0f) / (float) n - 1.0f), h * 2.0f);
        }
    }
    g.setColour (col::purple.withAlpha (0.4f));
    g.fillRect (r.getX(), cy - 0.75f, r.getWidth(), 1.5f);

    const bool on = ed.param (grOn) > 0.5f;
    const bool frozen = ed.param (grFreeze) > 0.5f;
    auto xFor = [&] (float pos) { return r.getX() + 6.0f + juce::jlimit (0.0f, 1.0f, pos) * (r.getWidth() - 12.0f); };
    if (ed.proc.isVoicePlaying())
    {
        const float x = xFor (ed.proc.getPlayhead());
        g.setColour ((frozen ? col::pink : col::cyan).withAlpha (0.35f));
        g.fillRect (x - 3.0f, r.getY() + 4.0f, 6.0f, r.getHeight() - 8.0f);
        g.setColour (frozen ? col::pink : col::cyan);
        g.fillRect (x - 1.0f, r.getY() + 4.0f, 2.0f, r.getHeight() - 8.0f);
    }
    tts::dsp::GrainVis vis[tts::dsp::GrainEngine::maxGrains];
    const int n = on ? ed.proc.grainSnapshot (vis, tts::dsp::GrainEngine::maxGrains) : 0;
    for (int i = 0; i < n; ++i)
    {
        const auto& v = vis[i];
        const float x = xFor (v.pos);
        const float y = cy - juce::jlimit (-1.0f, 1.0f, v.pitch / 24.0f) * (r.getHeight() * 0.42f) + v.pan * 12.0f;
        const float rad = 4.0f + 18.0f * juce::jlimit (0.0f, 1.0f, v.size);
        const auto c = v.reverse ? col::pink : col::cyan;
        g.setColour (c.withAlpha (0.12f * v.amp));
        g.fillEllipse (x - rad * 1.8f, y - rad * 1.8f, rad * 3.6f, rad * 3.6f);
        g.setColour (c.withAlpha (0.25f + 0.65f * v.amp));
        g.fillEllipse (x - rad * 0.5f, y - rad * 0.5f, rad, rad);
    }
    if (! on)
    {
        g.setFont (mono (17.0f));
        g.setColour (col::dim);
        g.drawText ("GRAIN OFF - press the power button", r, juce::Justification::centred, false);
    }
    else if (frozen)
    {
        g.setFont (mono (15.0f));
        g.setColour (col::pink);
        g.drawText ("FROZEN", r.reduced (10.0f, 6.0f), juce::Justification::topLeft, false);
    }
    g.setColour (col::purple.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.2f);
}

// ================================================================ EQ-Kurve

float TtsEditor::EqGraph::xForFreq (float f) const
{
    return (float) getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f);
}
float TtsEditor::EqGraph::freqForX (float x) const
{
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, x / (float) getWidth()));
}
float TtsEditor::EqGraph::yForDb (float db) const
{
    return (float) getHeight() * (0.5f - juce::jlimit (-20.0f, 20.0f, db) / 40.0f);
}
float TtsEditor::EqGraph::dbForY (float y) const
{
    return juce::jlimit (-18.0f, 18.0f, (0.5f - y / (float) getHeight()) * 40.0f);
}

juce::Point<float> TtsEditor::EqGraph::nodePos (int b) const
{
    const auto type = (tts::dsp::EqType) juce::roundToInt (ed.param (eqId (b, "Type")));
    const float f = ed.param (eqId (b, "Freq"));
    const bool cut = type == tts::dsp::EqType::lowCut || type == tts::dsp::EqType::highCut;
    return { xForFreq (f), cut ? yForDb (0.0f) : yForDb (ed.param (eqId (b, "Gain"))) };
}

int TtsEditor::EqGraph::hitNode (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 16.0f;
    for (int b = 0; b < 3; ++b)
    {
        const float d = nodePos (b).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void TtsEditor::EqGraph::setParam (const juce::String& id, float value)
{
    if (auto* p = ed.proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void TtsEditor::EqGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07050f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setFont (mono (11.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (juce::Colour (0x22785adc));
        g.drawVerticalLine ((int) x, 0.0f, r.getHeight());
        g.setColour (col::dim.withAlpha (0.7f));
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f), (int) x + 2, (int) r.getHeight() - 14, 30, 12,
                    juce::Justification::left, false);
    }
    for (float db : { -12.0f, -6.0f, 6.0f, 12.0f })
    {
        g.setColour (juce::Colour (0x18785adc));
        g.drawHorizontalLine ((int) yForDb (db), 0.0f, r.getWidth());
    }
    g.setColour (col::purple.withAlpha (0.45f));
    g.drawHorizontalLine ((int) yForDb (0.0f), 0.0f, r.getWidth());

    // Gesamtkurve
    const bool eqActive = ed.param (eqOn) > 0.5f;
    juce::dsp::IIR::Coefficients<float>::Ptr coefs[3];
    for (int b = 0; b < 3; ++b)
    {
        if (ed.param (eqId (b, "On")) < 0.5f) continue;
        tts::dsp::EqBand band;
        band.type = (tts::dsp::EqType) juce::roundToInt (ed.param (eqId (b, "Type")));
        band.freq = ed.param (eqId (b, "Freq"));
        band.gainDb = ed.param (eqId (b, "Gain"));
        band.q = ed.param (eqId (b, "Q"));
        coefs[b] = tts::dsp::makeEqCoefficients (48000.0, band);
    }
    juce::Path curve, fill;
    const int w = getWidth();
    for (int x = 0; x <= w; x += 2)
    {
        const double f = freqForX ((float) x);
        double mag = 1.0;
        for (auto& c : coefs) if (c != nullptr) mag *= c->getMagnitudeForFrequency (f, 48000.0);
        const float y = yForDb ((float) juce::Decibels::gainToDecibels (mag, -40.0));
        if (x == 0) { curve.startNewSubPath ((float) x, y); fill.startNewSubPath ((float) x, yForDb (0.0f)); }
        else curve.lineTo ((float) x, y);
        fill.lineTo ((float) x, y);
    }
    fill.lineTo ((float) w, yForDb (0.0f));
    fill.closeSubPath();
    g.setColour (col::cyan.withAlpha (eqActive ? 0.12f : 0.05f));
    g.fillPath (fill);
    g.setColour (col::cyan.withAlpha (eqActive ? 0.3f : 0.12f));
    g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour ((eqActive ? col::cyan : col::dim));
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Punkte
    for (int b = 0; b < 3; ++b)
    {
        const auto p = nodePos (b);
        const bool on = ed.param (eqId (b, "On")) > 0.5f;
        const auto c = bandColour[b].withAlpha (on ? 1.0f : 0.35f);
        const float rad = b == ed.selBand ? 11.0f : 9.0f;
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (p.x - rad - 4, p.y - rad - 4, (rad + 4) * 2, (rad + 4) * 2);
        g.setColour (juce::Colour (0xff0b0718));
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2, rad * 2);
        g.setColour (c);
        g.drawEllipse (p.x - rad, p.y - rad, rad * 2, rad * 2, b == ed.selBand ? 2.5f : 1.5f);
        g.setFont (mono (13.0f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (p.x - rad, p.y - rad, rad * 2, rad * 2), juce::Justification::centred, false);
    }
    g.setColour (col::purple.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);
}

void TtsEditor::EqGraph::mouseDown (const juce::MouseEvent& e)
{
    dragBand = hitNode (e.position);
    if (dragBand < 0) return;
    ed.selectBand (dragBand);
    for (auto* what : { "Freq", "Gain" })
        if (auto* p = ed.proc.apvts.getParameter (eqId (dragBand, what))) p->beginChangeGesture();
}

void TtsEditor::EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;
    setParam (eqId (dragBand, "Freq"), freqForX (e.position.x));
    const auto type = (tts::dsp::EqType) juce::roundToInt (ed.param (eqId (dragBand, "Type")));
    if (type != tts::dsp::EqType::lowCut && type != tts::dsp::EqType::highCut)
        setParam (eqId (dragBand, "Gain"), dbForY (e.position.y));
    repaint();
}

void TtsEditor::EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragBand < 0) return;
    for (auto* what : { "Freq", "Gain" })
        if (auto* p = ed.proc.apvts.getParameter (eqId (dragBand, what))) p->endChangeGesture();
    dragBand = -1;
}

void TtsEditor::EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = hitNode (e.position);
    if (b < 0) return;
    if (auto* p = ed.proc.apvts.getParameter (eqId (b, "On")))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
    repaint();
}

void TtsEditor::EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = hitNode (e.position);
    if (b < 0) b = ed.selBand;
    const auto id = eqId (b, "Q");
    if (auto* p = ed.proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * 0.15f));
        p->endChangeGesture();
    }
    repaint();
}

// ================================================================ Wellenform / Status

bool TtsEditor::WaveView::hitTest (int x, int y)
{
    return wavePolygon ((float) getWidth(), (float) getHeight()).contains ((float) x, (float) y);
}

void TtsEditor::WaveView::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.reduceClipRegion (wavePolygon (w, h));
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
    if (ed.proc.isExporting())
    {
        const float f = ed.proc.getExportProgress();
        g.setColour (col::cyan);
        g.drawText ("RENDERING WAV" + dots, 60, 34, (int) w - 120, 28, juce::Justification::centredLeft);
        g.setColour (col::violet);
        g.drawText (juce::String (juce::roundToInt (f * 100.0f)) + " %", 60, 34, (int) w - 160, 28, juce::Justification::centredRight);
        bar (f, { 60.0f, 76.0f, w - 160.0f, 22.0f }, false);
        return;
    }
    const auto err = ed.proc.getRenderError();
    if (ed.peaks.empty())
    {
        g.setColour (err.isNotEmpty() ? juce::Colour (0xffff5f9e) : juce::Colour (0x99a096d7));
        g.drawText (err.isNotEmpty() ? err : juce::String ("Type text and hit GENERATE"), 0, 64, (int) w, 30, juce::Justification::centred);
        return;
    }

    const int n = (int) ed.peaks.size();
    const float x0 = 30.0f, x1 = w - 80.0f, step = (x1 - x0) / (float) n;
    const float head = ed.proc.isVoicePlaying() ? ed.proc.getPlayhead() : -1.0f;
    const float vol = juce::Decibels::decibelsToGain (ed.param (volume), -60.0f);
    for (int i = 0; i < n; ++i)
    {
        const float hh = juce::jmin (0.95f, ed.peaks[(size_t) i] * juce::jmin (1.4f, vol)) * 80.0f;
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
    const auto key = proc.exportKey();
    if (dragFile.existsAsFile() && dragKey == key) return dragFile;
    if (proc.isExporting()) { toast = "Still rendering the WAV..."; toastUntil = timeSec + 2.0; return {}; }
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("BrainbugzTTS");
    dir.createDirectory();
    auto f = dir.getChildFile (proc.suggestedFileName());
    // lange Stimmen (sehr langsamer Speed) im Hintergrund rendern, dann nochmal ziehen
    if (proc.getAudioSeconds() > 30.0)
    {
        proc.startExport (f, key);
        toast = "Rendering long WAV - drag again when it's ready";
        toastUntil = timeSec + 3.0;
        return {};
    }
    juce::String err;
    if (! proc.exportWav (f, err)) { toast = err; toastUntil = timeSec + 2.5; return {}; }
    dragFile = f;
    dragKey = key;
    return f;
}

void TtsEditor::saveWav()
{
    if (! proc.hasAudio()) { toast = "Nothing to save yet"; toastUntil = timeSec + 1.8; return; }
    if (proc.isExporting()) { toast = "Still rendering the WAV..."; toastUntil = timeSec + 2.0; return; }
    auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (proc.suggestedFileName());
    chooser = std::make_unique<juce::FileChooser> ("Save voice as WAV", start, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
                              proc.startExport (f, "save");
                              toast = "Saving...";
                              toastUntil = timeSec + 1.5;
                          });
}

// ================================================================ Einstellungen

TtsEditor::Settings::Settings (TtsEditor& e) : ed (e)
{
    folder.onClick = [] { auto d = tts::EngineHolder::dataDir(); d.createDirectory(); d.startAsProcess(); };
    folder.getProperties().set ("purple", true);
    close.onClick = [this] { setVisible (false); };
    addAndMakeVisible (folder);
    addAndMakeVisible (close);
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
    g.setFont (mono (15.0f));
    g.setColour (col::text);
    const char* lines[] = { "- Every knob can be automated in your DAW.",
                            "- MIDI notes retrigger the voice,",
                            "  C3 = original pitch.",
                            "- Drag the waveform into a track for a WAV.",
                            "- Voice engine: Kokoro (runs offline)." };
    int y = 58;
    for (auto* l : lines) { g.drawText (l, 20, y, 390, 22, juce::Justification::centredLeft); y += 26; }
}

void TtsEditor::Settings::resized()
{
    folder.setBounds (20, 222, 250, 40);
    close.setBounds (286, 222, 120, 40);
}
