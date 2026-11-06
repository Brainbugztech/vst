#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GainLockLook.h"
#include "GainLockAssets.h"

// Zeiger-Instrument: zeigt die Korrektur (dB) wie ein analoges Messgerät
class NeedleMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setValues (float gainDb, float targetDb, float rangeDb, bool holding, bool active, float chainDiffDb);
    void tick (float dt);
    void paint (juce::Graphics&) override;

private:
    float shown = 0.0f, vel = 0.0f, value = 0.0f, target = 0.0f, range = 18.0f, chainDiff = 0.0f;
    bool hold = false, live = false;
    float glint = -1.0f, glintTimer = 4.0f;   // seltener Lichtreflex über das Glas
};

// Pegelbalken: Source und Output, dazu der unkorrigierte Pegel als Marke
class LevelBars : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setValues (float refLufs, float postLufs, float outLufs, float refPk, float outPk, bool refActive, bool lufs);
    void paint (juce::Graphics&) override;
    void setSourceMode (bool s, juce::String caption = "INPUT") { sourceMode = s; singleCaption = caption; repaint(); }

private:
    float ref = -180, post = -180, out = -180;
    float refPeakShown = 0, outPeakShown = 0;
    bool refOn = false, isLufs = true, sourceMode = false;
    juce::String singleCaption { "INPUT" };
};

// Verlauf der Korrektur über die letzten ~24 s
class HistoryGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    void push (float db, float rangeDb);
    void paint (juce::Graphics&) override;

private:
    static constexpr int kLen = 360;
    std::array<float, kLen> data {};
    int head = 0, count = 0;
    float range = 18.0f;
};

// Segment-Schalter für Auswahl-Parameter
class SegmentSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentSwitch (juce::RangedAudioParameter& p, juce::StringArray labels, juce::String caption = {}, int valueOffset = 0);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray labels;
    juce::String caption;
    int offset = 0, index = 0;
    juce::ParameterAttachment attachment;
};

// Monitor-Reihe: Final / Original / P1-P3 / Delta
class MonitorStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MonitorStrip (GainLockProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();

private:
    int slotAt (juce::Point<float> pos) const;
    juce::Rectangle<float> slotBounds (int i) const;

    GainLockProcessor& proc;
    juce::ParameterAttachment attachment;
    int selected = 0, previous = 1, active = 0;
    bool blindRunning = false;
    std::array<bool, GainLockProcessor::kNumMon> avail {};
    std::array<juce::String, GainLockProcessor::kNumTaps> names;
};

// Blind-Vorliebetest
class BlindPanel : public juce::Component
{
public:
    explicit BlindPanel (GainLockProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::function<void()> onClose;

private:
    void fillSourceBoxes();
    juce::String verdictHeadline (const GainLockProcessor::BlindResult& r) const;
    juce::String verdictDetail (const GainLockProcessor::BlindResult& r) const;

    GainLockProcessor& proc;
    juce::ComboBox aBox, bBox, roundsBox;
    juce::TextButton startBtn { "START TEST" }, closeBtn { "CLOSE" }, xBtn { "X" }, yBtn { "Y" },
                     prefX { "X IS BETTER" }, prefY { "Y IS BETTER" }, noneBtn { "NO DIFFERENCE" },
                     stopBtn { "STOP" }, againBtn { "NEW TEST" }, clearBtn { "CLEAR LOG" };
    GainLockProcessor::BlindState state;
    std::vector<GainLockProcessor::BlindResult> history;
    juce::String availKey;
};

class GainLockEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit GainLockEditor (GainLockProcessor&);
    ~GainLockEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateVisibility();
    void applyTooltips();
    void drawWordmark (juce::Graphics&, juce::Rectangle<float> area, bool locked);
    juce::String statusText() const;

    GainLockProcessor& proc;
    gl::GainLockLook look;
    std::unique_ptr<juce::TooltipWindow> tips;

    juce::Path logoLetters, logoOutline;   // Brain-Bugz-Logo als Vektor (Assets/bb_logo.svg)
    juce::Image logoPrinted;
    NeedleMeter needle;
    LevelBars bars;
    HistoryGraph history;
    MonitorStrip monitor;
    BlindPanel blindPanel;

    SegmentSwitch roleSwitch, speedSwitch, metricSwitch, pointSwitch;
    juce::ComboBox groupBox;
    juce::Slider trimKnob, rangeKnob;
    juce::TextButton holdButton { "HOLD" }, blindButton { "BLIND TEST" }, helpButton { "?" };
    juce::TextEditor pointName;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> trimAtt, rangeAtt;
    std::unique_ptr<BA> holdAtt;
    std::unique_ptr<CA> groupAtt;

    int shownRole = -1;
    bool blindOpen = false, shownBlind = false;
    float uiScale = 1.0f;
    float chainDiffShown = 0.0f;
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainLockEditor)
};
