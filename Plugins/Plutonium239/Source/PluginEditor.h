#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PlutoLook.h"
#include "Widgets.h"

// Kleine Wellenform-Vorschau neben den Oszillator-Reglern (zeigt Wavetable-Position und Phase Distortion)
// Rechtsklick/Doppelklick = eigene Wavetable laden
class WavePreview : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<float (int table, float pos, float phase)> reader;
    std::function<void()> onLoadRequest;
    std::function<juce::String()> userName;

    WavePreview (juce::AudioProcessorValueTreeState& s, int osc)
    {
        auto rp = [this] { repaint(); };
        wave.attach (s, pluto::oid (osc, "wave"), rp);
        wt.attach (s, pluto::oid (osc, "wt"), rp);
        pwm.attach (s, pluto::oid (osc, "pwm"), rp);
        wtpos.attach (s, pluto::oid (osc, "wtpos"), rp);
        pdmode.attach (s, pluto::oid (osc, "pdmode"), rp);
        pdamt.attach (s, pluto::oid (osc, "pdamt"), rp);
        setTooltip ("Eigene Wavetable: .wav hier draufziehen oder Doppelklick/Rechtsklick zum Laden");
    }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && onLoadRequest) onLoadRequest(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onLoadRequest) onLoadRequest(); }

private:
    pluto::ParamLink wave, wt, pwm, wtpos, pdmode, pdamt;
};

// Tabellen-Auswahl: eingebaute Tables + Dateien aus dem verknuepften Ordner
class TableSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    std::function<juce::String()> currentName;
    void watch (juce::AudioProcessorValueTreeState& s, const juce::String& id) { link.attach (s, id, [this] { repaint(); }); }
    pluto::ParamLink link;
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setColour (isEnabled() ? pluto::col::combo : pluto::col::combo.darker (0.4f));
        g.fillRect (b);
        g.setColour (pluto::col::border);
        g.drawRect (b, 1.0f);
        Path tri;
        const float cx = b.getRight() - 7.0f, cy = b.getCentreY();
        tri.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
        g.setColour (pluto::col::text.withAlpha (isEnabled() ? 1.0f : 0.5f));
        g.fillPath (tri);
        g.setFont (pluto::PlutoLook::font (10.5f));
        g.drawText (currentName ? currentName() : String(), b.withTrimmedLeft (3).withTrimmedRight (13), Justification::centredLeft, true);
    }
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
};

class PlutoEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int baseW = 1180, baseH = 1034;

    explicit PlutoEditor (PlutoProcessor&);
    ~PlutoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Drag & Drop eigener Wavetables
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int x, int y) override { fileDragMove ({}, x, y); }
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override { setDropTarget (-1); }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    struct TextItem { juce::Rectangle<int> r; juce::String t; juce::Justification j; };
    struct BoxItem  { juce::Rectangle<int> r; juce::String t; bool section; };

    class Content : public juce::Component
    {
    public:
        std::vector<BoxItem> boxes;
        std::vector<TextItem> texts;
        std::vector<juce::Rectangle<int>> nameBoxes;
        juce::StringArray nameTexts;
        void paint (juce::Graphics&) override;
    };

    // liegt ueber allen Reglern: graut stumme Oszillatoren aus, zeigt Drop-Ziel (faengt keine Klicks ab)
    class Overlay : public juce::Component
    {
    public:
        Overlay() { setInterceptsMouseClicks (false, false); }
        std::array<juce::Rectangle<int>, pluto::numOsc> panels;
        std::array<bool, pluto::numOsc> muted {};
        juce::Rectangle<int> dropHighlight;
        juce::String dropText;
        void paint (juce::Graphics&) override;
    };

    PlutoProcessor& proc;
    pluto::PlutoLook look;
    Content content;
    Overlay overlay;
    juce::TooltipWindow tooltips { this, 700 };

    juce::OwnedArray<juce::Component> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;

    std::vector<pluto::LfoDisplay*> lfoDisplays;
    std::array<WavePreview*, pluto::numOsc> previews {};
    std::array<TableSelector*, pluto::numOsc> tableBoxes {};
    std::array<juce::Rectangle<int>, pluto::numOsc> oscRects;
    int dropOsc = -1, lastUserCounter = -1;
    std::unique_ptr<juce::FileChooser> chooser;

    int oscAt (int x, int y) const;
    void setDropTarget (int osc);
    void loadWavetable (int osc, const juce::File& f);
    void openWavetableChooser (int osc);
    void openFolderChooser();
    void showPlusMenu (int osc, juce::Component* target);
    void showTableMenu (int osc, juce::Component* target);
    juce::Array<juce::File> folderFiles;
    juce::File scannedFolder;
    void rescanFolder (bool force);
    void updateTableTooltips();
    void updateMuted();
    juce::ComboBox* presetBox = nullptr;
    int lastPresetCounter = -1;

    // Bausteine
    juce::Slider* knob (const juce::String& id, juce::Rectangle<int> r, const juce::String& label, bool textBox);
    juce::Component* vSlider (const juce::String& id, juce::Rectangle<int> r, bool textBox);
    juce::Slider* hSlider (const juce::String& id, juce::Rectangle<int> r, int textBoxW);
    juce::ComboBox* combo (const juce::String& id, juce::Rectangle<int> r);
    juce::ToggleButton* toggle (const juce::String& id, juce::Rectangle<int> r, const juce::String& text);
    void text (juce::Rectangle<int> r, const juce::String& t, juce::Justification j = juce::Justification::centred);
    template <typename T> T* own (T* c) { owned.add (c); content.addAndMakeVisible (c); return c; }

    void buildTopBar();
    void buildOsc (int o, int x, int y);
    void buildFilter (int f, int x, int y);
    void buildMixer (int x, int y);
    void buildSync (int x, int y);
    void buildMaster (int x, int y);
    void buildLfo (int l, int x, int y);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlutoEditor)
};
