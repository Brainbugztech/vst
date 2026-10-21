#include "PluginProcessor.h"
#include "PluginEditor.h"

BugzAlignProcessor::BugzAlignProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzBugzAlign", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BugzAlignProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto ms = [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::amount, 1 }, "Amount", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::offset, 1 }, "Offset", NormalisableRange<float> (-180.0f, 180.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel (String::fromUTF8 ("\xc2\xb0")).withStringFromValueFunction ([] (float v, int)
        {
            return (v > 0.05f ? "+" : "") + String (roundToInt (v)) + String::fromUTF8 ("\xc2\xb0");
        })));

    {
        NormalisableRange<float> r (60.0f, bb::AlignEngine::kFullHz, 1.0f);
        r.setSkewForCentre (160.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::split, 1 }, "Split", r, 180.0f,
            AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
            {
                return v >= bb::AlignEngine::kFullHz - 0.5f ? String ("FULL") : String (roundToInt (v)) + " Hz";
            })));
    }

    {
        NormalisableRange<float> r (10.0f, (float) bb::AlignEngine::kMaxWindowMs, 0.1f);
        r.setSkewForCentre (45.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::window, 1 }, "Window", r, 50.0f,
            AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (ms)));
    }

    {
        NormalisableRange<float> r (1.0f, (float) bb::AlignEngine::kMaxGlideMs, 0.1f);
        r.setSkewForCentre (12.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::glide, 1 }, "Glide", r, 15.0f,
            AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (ms)));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::sens, 1 }, "Kick Sensitivity", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::hold, 1 }, "Hold", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "A/B Vorher", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::subCheck, 1 }, "Sub Check", false));
    return layout;
}

bb::AlignParams BugzAlignProcessor::readParams() const
{
    bb::AlignParams p;
    p.amount    = apvts.getRawParameterValue (ParamIDs::amount)->load();
    p.offsetDeg = apvts.getRawParameterValue (ParamIDs::offset)->load();
    p.splitHz   = apvts.getRawParameterValue (ParamIDs::split)->load();
    p.windowMs  = apvts.getRawParameterValue (ParamIDs::window)->load();
    p.glideMs   = apvts.getRawParameterValue (ParamIDs::glide)->load();
    p.sens      = apvts.getRawParameterValue (ParamIDs::sens)->load();
    p.hold      = apvts.getRawParameterValue (ParamIDs::hold)->load() > 0.5f;
    p.bypass    = apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;
    p.subCheck  = apvts.getRawParameterValue (ParamIDs::subCheck)->load() > 0.5f;
    return p;
}

void BugzAlignProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
    latencyMs.store ((float) (1000.0 * engine.getLatencySamples() / sampleRate));
}

bool BugzAlignProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != out) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void BugzAlignProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    auto main = getBusBuffer (buffer, true, 0);
    const int numMain = std::min (2, main.getNumChannels());

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    if (n == 0 || numMain == 0)
        return;

    const float* const* kick = nullptr;
    int numKick = 0;
    juce::AudioBuffer<float> sc;
    if (getBusCount (true) > 1)
    {
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            sc = getBusBuffer (buffer, true, 1);
            numKick = std::min (2, sc.getNumChannels());
            if (numKick > 0) kick = sc.getArrayOfReadPointers();
        }
    }
    sidechainConnected.store (numKick > 0, std::memory_order_relaxed);

    engine.process (main.getArrayOfWritePointers(), numMain, kick, numKick, n, readParams());
}

juce::AudioProcessorEditor* BugzAlignProcessor::createEditor()
{
    return new BugzAlignEditor (*this);
}

void BugzAlignProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BugzAlignProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BugzAlignProcessor();
}
