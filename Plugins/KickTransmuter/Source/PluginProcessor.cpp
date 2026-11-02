#include "PluginProcessor.h"
#include "PluginEditor.h"

KickTransmuterProcessor::KickTransmuterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzKickTransmuter", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout KickTransmuterProcessor::createLayout()
{
    using namespace juce;
    using P = ParameterID;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto signedPercent = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int)
    {
        const int i = roundToInt (v);
        return (i > 0 ? "+" : "") + String (i) + " %";
    });
    auto percent = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int)
    {
        return String (roundToInt (v)) + " %";
    });
    auto decibel = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
    {
        return (v > 0.04f ? "+" : "") + String (v, 1) + " dB";
    });

    auto add = [&] (const char* id, const char* name, float lo, float hi, float def, AudioParameterFloatAttributes attr)
    {
        layout.add (std::make_unique<AudioParameterFloat> (P { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def, attr));
    };

    add (ParamIDs::attack,  "Attack",     -100.0f, 100.0f, 0.0f, signedPercent);
    add (ParamIDs::sustain, "Sustain",    -100.0f, 100.0f, 0.0f, signedPercent);
    add (ParamIDs::punch,   "Punch",         0.0f, 100.0f, 0.0f, percent);
    add (ParamIDs::sat,     "Saturation",    0.0f, 100.0f, 0.0f, percent);
    add (ParamIDs::lowEnd,  "Low End",       0.0f, 100.0f, 0.0f, percent);
    add (ParamIDs::mix,     "Mix",           0.0f, 100.0f, 100.0f, percent);
    add (ParamIDs::output,  "Output",      -18.0f,  18.0f, 0.0f, decibel);
    layout.add (std::make_unique<AudioParameterBool> (P { ParamIDs::autoGain, 1 }, "Auto Gain", false));

    return layout;
}

ktm::Params KickTransmuterProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    ktm::Params p;
    p.attack     = f (ParamIDs::attack) * 0.01f;
    p.sustain    = f (ParamIDs::sustain) * 0.01f;
    p.punch      = f (ParamIDs::punch) * 0.01f;
    p.saturation = f (ParamIDs::sat) * 0.01f;
    p.lowEnd     = f (ParamIDs::lowEnd) * 0.01f;
    p.mix        = f (ParamIDs::mix) * 0.01f;
    p.outDb      = f (ParamIDs::output);
    p.autoGain   = f (ParamIDs::autoGain) > 0.5f;
    return p;
}

void KickTransmuterProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatency());
    for (auto& d : bypassDelay)
        d.assign ((size_t) engine.getLatency(), 0.0f);
    bypassPos = 0;
}

bool KickTransmuterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void KickTransmuterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    if (numIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, readParams());
}

void KickTransmuterProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (buffer.getNumChannels() < 2 || n == 0 || bypassDelay[0].empty())
        return;

    if (numIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    const int len = (int) bypassDelay[0].size();
    int pos = bypassPos;
    for (int c = 0; c < 2; ++c)
    {
        auto* x = buffer.getWritePointer (c);
        pos = bypassPos;
        for (int i = 0; i < n; ++i)
        {
            const float out = bypassDelay[c][(size_t) pos];
            bypassDelay[c][(size_t) pos] = x[i];
            x[i] = out;
            if (++pos >= len) pos = 0;
        }
    }
    bypassPos = pos;
}

juce::AudioProcessorEditor* KickTransmuterProcessor::createEditor()
{
    return new KickTransmuterEditor (*this);
}

void KickTransmuterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    // Auto-Gain-Wert mitspeichern: beim Laden stimmt die Lautstärke schon ab der ersten Kick
    state.setProperty ("autoGainDb", engine.getAutoGainDb(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KickTransmuterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            if (tree.hasProperty ("autoGainDb"))
                engine.restoreAutoGainDb ((float) tree.getProperty ("autoGainDb"));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KickTransmuterProcessor();
}
