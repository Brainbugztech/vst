#pragma once

#include "PluginProcessor.h"
#include "ui/Analyser.h"
#include "ui/BandModule.h"
#include "ui/SidePanels.h"

// Gesamte Oberflaeche in Basisgroesse 1240 x 800, skalierbar (Fensterecke ziehen oder Optionen)
class BrainfuzzContent : public juce::Component
{
public:
    explicit BrainfuzzContent (BrainfuzzProcessor& p);
    ~BrainfuzzContent() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void update();

    static constexpr int kW = 1240, kH = 800;
    std::function<void (float)> onScaleRequest;

private:
    void refreshPresetBox();
    void stepPreset (int d);
    void savePreset (bool askName);
    void showLockMenu();
    void showOptions();

    BrainfuzzProcessor& proc;
    juce::Image logo;
    bfz::ui::Analyser analyser;
    juce::OwnedArray<bfz::ui::BandModule> bands;
    bfz::ui::SaturationPanel satPanel;
    bfz::ui::FilterPanel filterPanel;
    bfz::ui::MasterPanel masterPanel;

    juce::TextButton lockBtn { "LOCK" }, prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" };
    juce::TextButton undoBtn { "UNDO" }, redoBtn { "REDO" }, optionsBtn { "OPTIONS" };
    std::unique_ptr<bfz::ui::Toggle> bypassBtn;
    juce::ComboBox presetBox;
    juce::TooltipWindow tooltips { this, 600 };

    int shownPresetCount = -1, shownPreset = -2;
    bool shownModified = false;
    juce::String shownName;
};

class BrainfuzzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BrainfuzzEditor (BrainfuzzProcessor&);
    ~BrainfuzzEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    BrainfuzzProcessor& proc;
    bfz::ui::Look look;
    BrainfuzzContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrainfuzzEditor)
};
