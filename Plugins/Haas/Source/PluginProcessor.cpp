#include "PluginProcessor.h"
#include "PluginEditor.h"

HaasProcessor::HaasProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzHaas", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout HaasProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::width, 1 }, "Width", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 0.8f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return String (roundToInt (v * 100.0f)) + "%" + (v > 1.0f ? " KRASS" : "");
        })));

    auto msAttr = AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int)
    {
        return String (v, 1) + " ms";
    });
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lms, 1 }, "Delay L (bei 100%)",
                                                       NormalisableRange<float> (0.0f, 40.0f, 0.01f), 6.0f, msAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::rms, 1 }, "Delay R (bei 100%)",
                                                       NormalisableRange<float> (0.0f, 40.0f, 0.01f), 30.0f, msAttr));

    NormalisableRange<float> bassRange (0.0f, 400.0f, 1.0f);
    bassRange.setSkewForCentre (110.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::monoBass, 1 }, "Mono Bass", bassRange, 120.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
        {
            return v < 30.0f ? String ("aus") : String (roundToInt (v)) + " Hz";
        })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::flip, 1 }, "L/R tauschen", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::mono, 1 }, "Mono-Check", false));
    return layout;
}

bb::HaasParams HaasProcessor::readParams() const
{
    bb::HaasParams p;
    p.width    = apvts.getRawParameterValue (ParamIDs::width)->load();
    p.lBase    = apvts.getRawParameterValue (ParamIDs::lms)->load();
    p.rBase    = apvts.getRawParameterValue (ParamIDs::rms)->load();
    p.monoBass = apvts.getRawParameterValue (ParamIDs::monoBass)->load();
    p.flip     = apvts.getRawParameterValue (ParamIDs::flip)->load() > 0.5f;
    p.mono     = apvts.getRawParameterValue (ParamIDs::mono)->load() > 0.5f;
    return p;
}

void HaasProcessor::setParam (const char* id, float plainValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}

void HaasProcessor::setDisplayedDelay (int side, float ms)
{
    ms = juce::jlimit (0.0f, bb::HaasEngine::kMaxDelayMs, ms);
    auto p = readParams();
    if (p.width < 0.01f)
    {
        setParam (ParamIDs::width, 1.0f);
        p.width = 1.0f;
    }
    const bool isLeftBase = (side == 0) != p.flip;
    setParam (isLeftBase ? ParamIDs::lms : ParamIDs::rms, ms / std::min (1.0f, p.width));
}

void HaasProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
    env = sLL = sRR = sLR = 0.0;
}

bool HaasProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void HaasProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    // Takt-Position für die Animation
    double bpm = 120.0;
    bool playing = false;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    }
    if (playing) freeBeats = ppq + n * bpm / 60.0 / sr;
    else         freeBeats += n * bpm / 60.0 / sr;
    vizBeats.store (freeBeats);
    vizBpm.store (bpm);
    vizPlaying.store (playing);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    engine.process (L, R, n, readParams());

    // Pegel, Korrelation und Vektorskop-Daten
    const double kAtt = 0.01, kRel = 0.0004;
    for (int i = 0; i < n; ++i)
    {
        const double l = L[i], r = R[i];
        const double ax = std::max (std::abs (l), std::abs (r));
        env += (ax - env) * (ax > env ? kAtt : kRel);
        sLL = sLL * 0.9995 + l * l;
        sRR = sRR * 0.9995 + r * r;
        sLR = sLR * 0.9995 + l * r;
        if (++scopeDecim >= 4)
        {
            scopeDecim = 0;
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scope[(size_t) w * 2].store ((float) l, std::memory_order_relaxed);
            scope[(size_t) w * 2 + 1].store ((float) r, std::memory_order_relaxed);
            scopeWrite.store ((w + 1) % kScopeSize, std::memory_order_relaxed);
        }
    }
    vizEnv.store ((float) env);
    const double den = std::sqrt (sLL * sRR) + 1e-9;
    vizCorr.store ((float) juce::jlimit (-1.0, 1.0, sLR / den));
}

juce::AudioProcessorEditor* HaasProcessor::createEditor()
{
    return new HaasEditor (*this);
}

void HaasProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HaasProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HaasProcessor();
}
