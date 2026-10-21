#include "PluginProcessor.h"
#include "PluginEditor.h"

BugFuzzProcessor::BugFuzzProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzBugFuzz", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BugFuzzProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return String (roundToInt (v * 100.0f)) + "%" + (v > 0.5f ? " KRASS" : "");
        })));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::sat, 1 }, "Saturation", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return String (roundToInt (v * 100.0f)) + "% " + (v < 0.25f ? "WARM" : v < 0.6f ? "FUZZ" : "HART");
        })));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::out, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
        {
            return (v > 0.04f ? "+" : "") + String (v, 1) + " dB";
        })));

    const char* bandNames[4] { "Drive Low", "Drive Lo-Mid", "Drive Hi-Mid", "Drive High" };
    for (int b = 0; b < 4; ++b)
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::band[b], 1 }, bandNames[b], NormalisableRange<float> (0.0f, 1.0f, 0.001f),
            kBandDefaults[b], AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    const char* xNames[3] { "Weiche Low/Lo-Mid", "Weiche Lo-Mid/Hi-Mid", "Weiche Hi-Mid/High" };
    for (int i = 0; i < 3; ++i)
    {
        NormalisableRange<float> r (bb::BugFuzzEngine::kXMin[i], bb::BugFuzzEngine::kXMax[i], 1.0f);
        r.setSkewForCentre (std::sqrt (bb::BugFuzzEngine::kXMin[i] * bb::BugFuzzEngine::kXMax[i]));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::xover[i], 1 }, xNames[i], r, kXoverDefaults[i],
            AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
            {
                return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz";
            })));
    }

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::octa, 1 }, "Octa", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoGain, 1 }, "Auto-Gain", true));
    return layout;
}

bb::FuzzParams BugFuzzProcessor::readParams() const
{
    bb::FuzzParams p;
    p.drive = apvts.getRawParameterValue (ParamIDs::drive)->load();
    p.sat   = apvts.getRawParameterValue (ParamIDs::sat)->load();
    p.mix   = apvts.getRawParameterValue (ParamIDs::mix)->load();
    p.outDb = apvts.getRawParameterValue (ParamIDs::out)->load();
    for (int b = 0; b < 4; ++b) p.band[(size_t) b] = apvts.getRawParameterValue (ParamIDs::band[b])->load();
    for (int i = 0; i < 3; ++i) p.xover[(size_t) i] = apvts.getRawParameterValue (ParamIDs::xover[i])->load();
    p.octa     = apvts.getRawParameterValue (ParamIDs::octa)->load() > 0.5f;
    p.autoGain = apvts.getRawParameterValue (ParamIDs::autoGain)->load() > 0.5f;
    return p;
}

void BugFuzzProcessor::setParam (const char* id, float plainValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}

void BugFuzzProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (32, samplesPerBlock);
    engine.prepare (sampleRate, maxBlock);
    setLatencySamples (engine.getLatencySamples());
    env = 0.0;
}

bool BugFuzzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void BugFuzzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    // in Stücken verarbeiten, falls der Host größere Blöcke schickt als angekündigt
    const auto prm = readParams();
    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = std::min (maxBlock, n - start);
        juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), 2, start, len);
        engine.process (sub, prm);
    }

    // Pegel und Oszilloskop-Daten
    const auto* L = buffer.getReadPointer (0);
    const auto* R = buffer.getReadPointer (1);
    const double kAtt = 0.01, kRel = 0.0004;
    for (int i = 0; i < n; ++i)
    {
        const double m = 0.5 * (L[i] + R[i]);
        const double ax = std::abs (m);
        env += (ax - env) * (ax > env ? kAtt : kRel);
        if (++scopeDecim >= 2)
        {
            scopeDecim = 0;
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scope[(size_t) w].store ((float) m, std::memory_order_relaxed);
            scopeWrite.store ((w + 1) % kScopeSize, std::memory_order_relaxed);
        }
    }
    vizEnv.store ((float) env);
    for (int b = 0; b < 4; ++b) vizBand[(size_t) b].store (engine.bandLevel[(size_t) b]);
}

juce::AudioProcessorEditor* BugFuzzProcessor::createEditor()
{
    return new BugFuzzEditor (*this);
}

void BugFuzzProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BugFuzzProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BugFuzzProcessor();
}
