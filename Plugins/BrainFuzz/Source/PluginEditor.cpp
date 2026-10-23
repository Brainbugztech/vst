#include "PluginEditor.h"

using namespace bfz;
using namespace bfz::ui;

// ----------------------------------------------------------------- Inhalt

BrainfuzzContent::BrainfuzzContent (BrainfuzzProcessor& p)
    : proc (p), analyser (p), satPanel (p), filterPanel (p), masterPanel (p)
{
    logo = juce::ImageCache::getFromMemory (BrainfuzzAssets::logo_png, BrainfuzzAssets::logo_pngSize);
    addAndMakeVisible (analyser);
    for (int b = 0; b < 4; ++b) addAndMakeVisible (bands.add (new BandModule (p, b)));
    addAndMakeVisible (satPanel);
    addAndMakeVisible (filterPanel);
    addAndMakeVisible (masterPanel);

    bypassBtn = std::make_unique<Toggle> (p, pBypass, "BYPASS", col::red);
    bypassBtn->setTooltip ("Ganzes Plugin umgehen (gleiche Latenz)");
    for (auto* b : { &lockBtn, &prevBtn, &nextBtn, &saveBtn, &undoBtn, &redoBtn, &optionsBtn })
    {
        setAccent (*b, col::pink);
        addAndMakeVisible (b);
    }
    addAndMakeVisible (*bypassBtn);
    addAndMakeVisible (presetBox);

    lockBtn.setTooltip ("Preset-Sperren: Input, Output, Effekte oder Tight beim Durchblaettern behalten");
    prevBtn.setTooltip ("Vorheriges Preset");
    nextBtn.setTooltip ("Naechstes Preset");
    saveBtn.setTooltip ("Preset speichern");
    undoBtn.setTooltip ("Rueckgaengig");
    redoBtn.setTooltip ("Wiederholen");

    lockBtn.onClick = [this] { showLockMenu(); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePreset (proc.getCurrentPreset() < proc.getNumFactoryPresets()); };
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    optionsBtn.onClick = [this] { showOptions(); };

    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentPreset()) proc.loadPreset (idx);
        shownPreset = -2;
        refreshPresetBox();
    };

    addMouseListener (this, true);
    refreshPresetBox();
}

BrainfuzzContent::~BrainfuzzContent()
{
    removeMouseListener (this);
}

void BrainfuzzContent::mouseDown (const juce::MouseEvent&)
{
    // jede neue Maus-Aktion = eigener Undo-Schritt
    proc.undo.beginNewTransaction();
}

void BrainfuzzContent::paint (juce::Graphics& g)
{
    // Hintergrund: Nachtverlauf mit leichtem Raster
    juce::ColourGradient bg (col::bg1, 0, 0, col::bg0, 0, (float) kH, false);
    g.setGradientFill (bg);
    g.fillAll();
    g.setColour (col::grid.withAlpha (0.25f));
    for (int x = 0; x < kW; x += 40) g.drawVerticalLine (x, 56.0f, (float) kH);
    for (int y = 56; y < kH; y += 40) g.drawHorizontalLine (y, 0.0f, (float) kW);

    // Kopfleiste
    juce::Rectangle<float> top (0, 0, (float) kW, 56.0f);
    juce::ColourGradient tg (col::bg2, 0, 0, col::bg1, 0, 56.0f, false);
    g.setGradientFill (tg);
    g.fillRect (top);
    g.setGradientFill (col::sunset (0, 56, (float) kW, 56));
    g.fillRect (0.0f, 55.0f, (float) kW, 1.5f);

    if (logo.isValid())
    {
        const float h = 46.0f, w = h * logo.getWidth() / (float) logo.getHeight();
        g.drawImage (logo, juce::Rectangle<float> (12.0f, 5.0f, w, h), juce::RectanglePlacement::centred);
    }

    // Schriftzug mit Sonnenuntergangs-Verlauf und Glow
    const juce::Rectangle<float> tr (124.0f, 6.0f, 250.0f, 44.0f);
    const auto font = fonts::title (31.0f);
    juce::GlyphArrangement ga;
    ga.addFittedText (font, "BRAINFUZZ", tr.getX(), tr.getY(), tr.getWidth(), tr.getHeight(), juce::Justification::centredLeft, 1);
    juce::Path tp;
    ga.createPath (tp);
    g.setColour (col::pink.withAlpha (0.18f));
    g.strokePath (tp, juce::PathStrokeType (5.0f));
    g.setGradientFill (col::sunset (0, tr.getY() + 6.0f, 0, tr.getBottom() - 6.0f));
    g.fillPath (tp);
    g.setColour (col::inkMute);
    g.setFont (fonts::mono (10.0f));
    g.drawText ("4-BAND FUZZ  /  WIDTH  /  FX", juce::Rectangle<float> (128.0f, 41.0f, 240.0f, 12.0f), juce::Justification::centredLeft);
}

void BrainfuzzContent::resized()
{
    // Kopfleiste
    lockBtn.setBounds (400, 15, 52, 26);
    prevBtn.setBounds (458, 15, 26, 26);
    presetBox.setBounds (488, 15, 280, 26);
    nextBtn.setBounds (772, 15, 26, 26);
    saveBtn.setBounds (804, 15, 52, 26);
    undoBtn.setBounds (882, 15, 52, 26);
    redoBtn.setBounds (938, 15, 52, 26);
    bypassBtn->setBounds (1028, 15, 84, 26);
    optionsBtn.setBounds (1118, 15, 108, 26);

    analyser.setBounds (16, 64, 900, 272);
    satPanel.setBounds (928, 64, 296, 272);
    filterPanel.setBounds (928, 344, 296, 104);
    masterPanel.setBounds (928, 456, 296, 328);
    for (int b = 0; b < 4; ++b) bands[b]->setBounds (16 + b * 227, 344, 219, 440);
}

void BrainfuzzContent::update()
{
    analyser.update();
    for (auto* b : bands) b->update();
    satPanel.update();
    masterPanel.update();
    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());
    const bool anyLock = std::any_of (proc.locks.begin(), proc.locks.end(), [] (bool l) { return l; });
    lockBtn.setToggleState (anyLock, juce::dontSendNotification);
    if (proc.getCurrentPreset() != shownPreset || proc.isPresetModified() != shownModified
        || proc.getCurrentPresetName() != shownName || (int) proc.getPresetList().size() != shownPresetCount)
        refreshPresetBox();
}

// ----------------------------------------------------------------- Presets

void BrainfuzzContent::refreshPresetBox()
{
    const auto& list = proc.getPresetList();
    if ((int) list.size() != shownPresetCount)
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
    shownModified = proc.isPresetModified();
    shownName = proc.getCurrentPresetName();
    presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
    presetBox.setText (shownName + (shownModified ? " *" : ""), juce::dontSendNotification);
}

void BrainfuzzContent::stepPreset (int d)
{
    const int total = (int) proc.getPresetList().size();
    if (total == 0) return;
    const int cur = proc.getCurrentPreset();
    proc.loadPreset (((cur < 0 ? 0 : cur + d) % total + total) % total);
    refreshPresetBox();
}

void BrainfuzzContent::savePreset (bool askName)
{
    if (! askName)
    {
        proc.saveUserPreset (proc.getCurrentPresetName());
        shownPresetCount = -1;
        refreshPresetBox();
        return;
    }
    auto* w = new juce::AlertWindow ("Preset speichern", "Name fuer dein Preset:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&getLookAndFeel());
    const bool factory = proc.getCurrentPreset() >= 0 && proc.getCurrentPreset() < proc.getNumFactoryPresets();
    w->addTextEditor ("name", proc.getCurrentPresetName() + (factory ? " (eigen)" : ""));
    w->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<BrainfuzzContent> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto name = w->getTextEditorContents ("name");
        if (! safe->proc.saveUserPreset (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Brainfuzz", "Preset konnte nicht gespeichert werden.");
        safe->shownPresetCount = -1;
        safe->refreshPresetBox();
    }), true);
}

void BrainfuzzContent::showLockMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("BEIM PRESET-WECHSEL BEHALTEN");
    const char* names[4] { "Input", "Output", "Effekte (Crush, Chorus, Reverb)", "Tight" };
    for (int i = 0; i < 4; ++i) m.addItem (i + 1, names[i], true, proc.locks[(size_t) i]);
    juce::Component::SafePointer<BrainfuzzContent> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&lockBtn), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        safe->proc.locks[(size_t) r - 1] = ! safe->proc.locks[(size_t) r - 1];
        safe->showLockMenu(); // offen lassen fuer mehrere Haken
    });
}

void BrainfuzzContent::showOptions()
{
    juce::PopupMenu size;
    for (int s : { 75, 90, 100, 125, 150, 175, 200 })
        size.addItem (100 + s, juce::String (s) + " %", true, std::abs (proc.uiScale * 100.0f - s) < 1.0f);
    juce::PopupMenu m;
    m.addSubMenu ("Fenstergroesse", size);
    m.addSeparator();
    const bool isUser = proc.getCurrentPreset() >= proc.getNumFactoryPresets();
    m.addItem (1, "Preset speichern unter ...");
    m.addItem (2, "Preset loeschen", isUser);
    m.addItem (3, "Preset-Ordner oeffnen");
    m.addItem (4, "Preset-Liste neu laden");
    m.addSeparator();
    m.addItem (5, "Brainfuzz " JucePlugin_VersionString " - Brain Bugz", false);
    juce::Component::SafePointer<BrainfuzzContent> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsBtn), [safe] (int r)
    {
        if (safe == nullptr) return;
        auto& pr = safe->proc;
        if (r > 100) { if (safe->onScaleRequest) safe->onScaleRequest ((r - 100) / 100.0f); return; }
        if (r == 1) safe->savePreset (true);
        else if (r == 2) pr.deleteUserPreset (pr.getCurrentPreset());
        else if (r == 3) { auto f = pr.getPresetFolder(); f.createDirectory(); f.startAsProcess(); }
        else if (r == 4) pr.refreshUserPresets();
        safe->shownPresetCount = -1;
        safe->refreshPresetBox();
    });
}

// ----------------------------------------------------------------- Editor

BrainfuzzEditor::BrainfuzzEditor (BrainfuzzProcessor& p)
    : AudioProcessorEditor (&p), proc (p), content (p)
{
    const float startScale = juce::jlimit (0.6f, 2.0f, proc.uiScale); // vor setResizeLimits merken
    setLookAndFeel (&look);
    content.setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setSize (BrainfuzzContent::kW, BrainfuzzContent::kH);
    content.onScaleRequest = [this] (float s)
    {
        setSize (juce::roundToInt (BrainfuzzContent::kW * s), juce::roundToInt (BrainfuzzContent::kH * s));
    };
    setResizable (true, true);
    setResizeLimits (BrainfuzzContent::kW * 6 / 10, BrainfuzzContent::kH * 6 / 10, BrainfuzzContent::kW * 2, BrainfuzzContent::kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) BrainfuzzContent::kW / BrainfuzzContent::kH);
    setSize (juce::roundToInt (BrainfuzzContent::kW * startScale), juce::roundToInt (BrainfuzzContent::kH * startScale));
    startTimerHz (30);
}

BrainfuzzEditor::~BrainfuzzEditor()
{
    stopTimer();
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void BrainfuzzEditor::paint (juce::Graphics& g) { g.fillAll (col::bg0); }

void BrainfuzzEditor::resized()
{
    const float s = (float) getWidth() / (float) BrainfuzzContent::kW;
    proc.uiScale = s;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setTopLeftPosition (0, 0);
}

void BrainfuzzEditor::timerCallback()
{
    content.update();
}
