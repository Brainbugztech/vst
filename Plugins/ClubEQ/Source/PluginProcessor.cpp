#include "PluginProcessor.h"
#include "PluginEditor.h"

ClubEQProcessor::ClubEQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ClubEQ", createLayout())
{
    pHi    = apvts.getRawParameterValue (ParamIDs::hi);
    pMid   = apvts.getRawParameterValue (ParamIDs::mid);
    pLow   = apvts.getRawParameterValue (ParamIDs::low);
    pIso   = apvts.getRawParameterValue (ParamIDs::iso);
    pXLow  = apvts.getRawParameterValue (ParamIDs::xlow);
    pXHigh = apvts.getRawParameterValue (ParamIDs::xhigh);
}

juce::AudioProcessorValueTreeState::ParameterLayout ClubEQProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Bänder als Knopfposition (0…1): MIDI-Controller-Mitte = 0 dB, Automation verhält sich wie der echte Drehregler.
    // Anzeige in Ableton trotzdem in dB, passend zum EQ/ISO-Modus.
    auto band = [this, &layout] (const char* id, const char* name)
    {
        auto toText = [this] (float v, int) { return ceq::formatDb (v, isoMode()); };
        auto fromText = [this] (const juce::String& t)
        {
            // Texteingabe in dB -> nächstliegende Knopfposition
            const float target = t.trim().startsWithChar ('-') && t.containsChar (0x221e) ? -200.0f : t.getFloatValue();
            float best = 0.5f, err = 1.0e9f;
            for (int i = 0; i <= 1000; ++i)
            {
                const float p = (float) i / 1000.0f;
                const float d = std::abs (juce::jmax (-200.0f, ceq::positionToDb (p, isoMode())) - target);
                if (d < err) { err = d; best = p; }
            }
            return best;
        };
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)
                                                 .withValueFromStringFunction (fromText)));
    };
    band (ParamIDs::hi, "Hi");
    band (ParamIDs::mid, "Mid");
    band (ParamIDs::low, "Low");

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::iso, 1 }, "EQ Curve", juce::StringArray { "EQ (-26 dB)", "Isolator (kill)" }, 0));

    auto hz = [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; };
    juce::NormalisableRange<float> rLow (100.0f, 600.0f);
    rLow.setSkewForCentre ((float) ceq::kDefaultXLow);
    juce::NormalisableRange<float> rHigh (1500.0f, 8000.0f);
    rHigh.setSkewForCentre ((float) ceq::kDefaultXHigh);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::xlow, 1 }, "Crossover Low/Mid", rLow, (float) ceq::kDefaultXLow,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz).withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::xhigh, 1 }, "Crossover Mid/Hi", rHigh, (float) ceq::kDefaultXHigh,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz).withLabel ("Hz")));
    return layout;
}

bool ClubEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void ClubEQProcessor::pushParams (bool snap)
{
    const bool iso = isoMode();
    engine.setGains (ceq::positionToGain (pLow->load(), iso, ceq::bandLow),
                     ceq::positionToGain (pMid->load(), iso, ceq::bandMid),
                     ceq::positionToGain (pHi->load(), iso, ceq::bandHigh), snap);
    engine.setCrossovers (pXLow->load(), pXHigh->load(), snap);
}

void ClubEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    setLatencySamples (0);
    pushParams (true);
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void ClubEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    pushParams (false);
    engine.process (buffer.getArrayOfWritePointers(), getTotalNumOutputChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* ClubEQProcessor::createEditor()
{
    return new ClubEQEditor (*this);
}

void ClubEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ClubEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClubEQProcessor();
}
