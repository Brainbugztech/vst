#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GainLockLook.h"
#include "GainLockAssets.h"

// Zeiger-Instrument: zeigt die Korrektur (dB) wie ein analoges Messgerät
class NeedleMeter : public juce::Component
{
public:
    void setValues (float gainDb, float targetDb, float rangeDb, bool holding, bool active);
    void tick (float dt);
    void paint (juce::Graphics&) override;

private:
    float shown = 0.0f, vel = 0.0f, value = 0.0f, target = 0.0f, range = 18.0f;
    bool hold = false, live = false;
    float glint = -1.0f, glintTimer = 4.0f;   // seltener Lichtreflex über das Glas
};

// Zwei Pegelbalken: Referenz (Quelle) und Ausgang, dazu der unkorrigierte Pegel als Marke
class LevelBars : public juce::Component
{
public:
    void setValues (float refLufs, float postLufs, float outLufs, float refPk, float outPk, bool refActive, bool lufs);
    void paint (juce::Graphics&) override;
    void setSourceMode (bool s) { sourceMode = s; repaint(); }

private:
    float ref = -180, post = -180, out = -180, refPeak = 0, outPeak = 0;
    float refPeakShown = 0, outPeakShown = 0;
    bool refOn = false, isLufs = true, sourceMode = false;
};

// Verlauf der Korrektur über die letzten ~24 s
class HistoryGraph : public juce::Component
{
public:
    void push (float db, float rangeDb, bool holding);
    void paint (juce::Graphics&) override;

private:
    static constexpr int kLen = 360;
    std::array<float, kLen> data {};
    std::array<bool, kLen> held {};
    int head = 0, count = 0;
    float range = 18.0f;
};

// Segment-Schalter für Auswahl-Parameter (Rolle, Tempo, Messung)
class SegmentSwitch : public juce::Component
{
public:
    SegmentSwitch (juce::RangedAudioParameter& p, juce::StringArray labels, juce::String caption = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void resized() override {}

private:
    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    juce::String caption;
    int index = 0;
    juce::ParameterAttachment attachment;
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
    juce::String statusText() const;

    GainLockProcessor& proc;
    gl::GainLockLook look;

    juce::Image logo;
    NeedleMeter needle;
    LevelBars bars;
    HistoryGraph history;

    SegmentSwitch roleSwitch, speedSwitch, metricSwitch;
    juce::ComboBox groupBox;
    juce::Slider trimKnob, rangeKnob;
    juce::TextButton holdButton { "HOLD" }, abButton { "A/B" };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> trimAtt, rangeAtt;
    std::unique_ptr<BA> holdAtt, abAtt;
    std::unique_ptr<CA> groupAtt;

    int shownRole = -1;
    float uiScale = 1.0f;
    juce::String linkedName;
    int nameTimer = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainLockEditor)
};
