#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "PluginProcessor.h"
#include "BrainBugzLook.h"
#include "BraincutAssets.h"
#include "ui/Look.h"
#include "ui/Widgets.h"

class BraincutEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BraincutEditor (BraincutProcessor&);
    ~BraincutEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 960, baseH = 640;

private:
    // Alle Bedienelemente liegen in einer Fläche mit fester Größe, die beim Skalieren des Fensters mitgezoomt wird
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void paintCard (juce::Graphics&, juce::Rectangle<float>, const juce::String& title, juce::Colour accent, bool on);
    void paintGlitch (juce::Graphics&);
    void syncFromParams();
    bool flag (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }

    BraincutProcessor& proc;
    bcui::Look lnf;
    Content content;
    juce::Image bg;
    double timeSec = 0.0, lastMs = 0.0;

    bcui::SliceView sliceView;

    bcui::KnobBox repeatK, loopK, subdivK, seedK;
    bcui::ValueBar phMinB, phMaxB, repMinB, repMaxB;
    bcui::KnobBox strAmtK, strMixK;
    bcui::KnobBox repChanceK, repRateK, repGateK, repDecayK;
    bcui::KnobBox gliAmtK, gliMixK;
    bcui::KnobBox mixK, outK;

    juce::TextButton cutBtn { "CUT" }, warpBtn { "WARP" }, lockBtn { "LOCK" }, rollBtn { "ROLL" },
                     prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton strOnBtn { "ON" }, repOnBtn { "ON" }, gliOnBtn { "ON" };
    juce::TextButton bitBtn { "BIT" }, revBtn { "REV" }, stopBtn { "STOP" }, frzBtn { "FREEZE" }, pitBtn { "PITCH" };
    juce::ComboBox presetBox;

    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<BA> lockAtt, strOnAtt, repOnAtt, gliOnAtt, bitAtt, revAtt, stopAtt, frzAtt, pitAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BraincutEditor)
};
