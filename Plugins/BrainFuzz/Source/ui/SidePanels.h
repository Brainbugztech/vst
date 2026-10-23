#pragma once

#include "Widgets.h"

namespace bfz::ui
{

// Kennlinie der aktuellen Saettigung (mit Drive des lautesten Bandes)
class CurveView : public juce::Component
{
public:
    explicit CurveView (BrainfuzzProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void update();
private:
    BrainfuzzProcessor& proc;
    int lastType = -1;
    float lastBite = -1.0f;
};

class SaturationPanel : public juce::Component
{
public:
    explicit SaturationPanel (BrainfuzzProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update() { curve.update(); }
private:
    BrainfuzzProcessor& proc;
    ChoiceButtons types;
    Knob bite;
    CurveView curve;
};

class FilterPanel : public juce::Component
{
public:
    explicit FilterPanel (BrainfuzzProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    ChoiceBox type, slope;
    Toggle tightHp, tightLp;
    ChoiceButtons tightSlope;
    Toggle mono;
    juce::Slider monoFreq;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> monoAtt;
};

class Meter : public juce::Component
{
public:
    void set (float l, float r) { lv = l; rv = r; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float lv = 0, rv = 0;
};

class MasterPanel : public juce::Component
{
public:
    explicit MasterPanel (BrainfuzzProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update();
private:
    BrainfuzzProcessor& proc;
    Knob input, output;
    juce::TextButton link { "LINK" };
    Toggle limit;
    Knob mCrush, mChorus, mVerb, sat, spread, mix;
    Meter inMeter, outMeter;
    float gr = 0.0f;
    float lastIn = 0.0f;
};

} // namespace bfz::ui
