#include "PluginProcessor.h"
#include "PluginEditor.h"

BoothEQProcessor::BoothEQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BoothEQ", createLayout ([this] { return isoCurve(); }))
{
    pLow     = apvts.getRawParameterValue (ParamIDs::low);
    pMid     = apvts.getRawParameterValue (ParamIDs::mid);
    pHigh    = apvts.getRawParameterValue (ParamIDs::high);
    pKillLow = apvts.getRawParameterValue (ParamIDs::killLow);
    pKillMid = apvts.getRawParameterValue (ParamIDs::killMid);
    pCurve   = apvts.getRawParameterValue (ParamIDs::curve);
    pXLow    = apvts.getRawParameterValue (ParamIDs::xlow);
    pXHigh   = apvts.getRawParameterValue (ParamIDs::xhigh);
    apvts.state.setProperty ("help", true, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout BoothEQProcessor::createLayout (std::function<bool()> isIso)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Bänder als Knopfposition (0…1): XL-Knopf-Mitte = 0 dB, Automation verhält sich wie der Drehregler.
    // Ableton zeigt trotzdem dB, passend zur gewählten Kurve.
    auto band = [&layout, isIso] (const char* id, const char* name)
    {
        auto toText = [isIso] (float v, int) { return beq::formatDb (v, isIso()); };
        auto fromText = [isIso] (const juce::String& t)
        {
            const bool killText = t.containsChar (0x221e) || t.containsIgnoreCase ("inf") || t.containsIgnoreCase ("kill");
            const float target = killText ? -200.0f : t.getFloatValue();
            float best = 0.5f, err = 1.0e9f;
            for (int i = 0; i <= 1000; ++i)
            {
                const float p = (float) i / 1000.0f;
                const float d = std::abs (juce::jmax (-200.0f, beq::positionToDb (p, isIso())) - target);
                if (d < err) { err = d; best = p; }
            }
            return best;
        };
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)
                                                 .withValueFromStringFunction (fromText)));
    };
    band (ParamIDs::low, "Low");
    band (ParamIDs::mid, "Mid");
    band (ParamIDs::high, "High");

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::killLow, 1 }, "Low Kill", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::killMid, 1 }, "Mid Kill", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::curve, 1 }, "Curve", juce::StringArray { "Isolator (kill)", "EQ (-26 dB)" }, 0));

    auto hz = [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; };
    juce::NormalisableRange<float> rLow (80.0f, 400.0f);
    rLow.setSkewForCentre ((float) beq::kDefaultXLow);
    juce::NormalisableRange<float> rHigh (1200.0f, 6000.0f);
    rHigh.setSkewForCentre ((float) beq::kDefaultXHigh);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::xlow, 1 }, "Crossover Low/Mid", rLow, (float) beq::kDefaultXLow,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz).withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::xhigh, 1 }, "Crossover Mid/High", rHigh, (float) beq::kDefaultXHigh,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz).withLabel ("Hz")));
    return layout;
}

bool BoothEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void BoothEQProcessor::pushParams (bool snap)
{
    const bool iso = isoCurve();
    const bool curveChanged = iso != lastIso;
    lastIso = iso;
    engine.setKnobGains (beq::positionToGain (pLow->load(), iso),
                         beq::positionToGain (pMid->load(), iso),
                         beq::positionToGain (pHigh->load(), iso), curveChanged, snap);
    engine.setKills (pKillLow->load() > 0.5f, pKillMid->load() > 0.5f, snap);
    engine.setCrossovers (pXLow->load(), pXHigh->load(), snap);
}

void BoothEQProcessor::prepareToPlay (double sampleRate, int)
{
    setLatencySamples (0);
    lastIso = isoCurve();
    engine.prepare (sampleRate);
    pushParams (true);
    engine.prepare (sampleRate);
}

void BoothEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    pushParams (false);
    engine.process (buffer.getArrayOfWritePointers(), getTotalNumOutputChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* BoothEQProcessor::createEditor()
{
    return new BoothEQEditor (*this);
}

void BoothEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BoothEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            // Hold-Taster nie gedrückt aus einem gespeicherten Set laden
            for (auto* id : { ParamIDs::killLow, ParamIDs::killMid })
                if (auto* p = apvts.getParameter (id))
                    p->setValueNotifyingHost (0.0f);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BoothEQProcessor();
}
