#pragma once

#include "PluginProcessor.h"

class BugWobbleEditor : public juce::AudioProcessorEditor
{
public:
    explicit BugWobbleEditor (BugWobbleProcessor&);
    ~BugWobbleEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    BugWobbleProcessor& processor;

    // Reihenfolge wichtig: Attachments werden zuerst zerstört, danach die Controls.
    std::vector<std::unique_ptr<juce::Label>>     labels;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<SliderAtt>>       sliderAtts;
    std::vector<std::unique_ptr<ComboAtt>>        comboAtts;
    std::vector<std::unique_ptr<ButtonAtt>>       buttonAtts;

    void addCell (const juce::String& name, std::unique_ptr<juce::Component> control);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BugWobbleEditor)
};
