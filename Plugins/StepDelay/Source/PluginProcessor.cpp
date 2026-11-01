#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

StepDelayProcessor::StepDelayProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzStepDelay", createLayout())
{
    for (int k = 0; k < kMaxSteps; ++k)
        levelPtr[(size_t) k] = apvts.getRawParameterValue (ParamIDs::level (k));
    pSteps = apvts.getRawParameterValue (ParamIDs::steps);
    pSync  = apvts.getRawParameterValue (ParamIDs::sync);
    pRate  = apvts.getRawParameterValue (ParamIDs::rate);
    pMode  = apvts.getRawParameterValue (ParamIDs::mode);
    pTime  = apvts.getRawParameterValue (ParamIDs::timeMs);
    pMix   = apvts.getRawParameterValue (ParamIDs::mix);
    pOut   = apvts.getRawParameterValue (ParamIDs::out);
    pLo    = apvts.getRawParameterValue (ParamIDs::loCut);
    pHi    = apvts.getRawParameterValue (ParamIDs::hiCut);
}

juce::AudioProcessorValueTreeState::ParameterLayout StepDelayProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::steps, 1 }, "Steps", 1, kMaxSteps, 8));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::sync, 1 }, "Tempo-Sync", true));

    StringArray rates, modes;
    for (auto* r : bb::stepgrid::rateNames) rates.add (r);
    for (auto* m : bb::stepgrid::modeNames) modes.add (m);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::rate, 1 }, "Raster", rates, 3));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Modus", modes, 0));

    NormalisableRange<float> timeRange (10.0f, 2000.0f, 0.1f);
    timeRange.setSkewForCentre (250.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::timeMs, 1 }, "Step-Zeit (frei)", timeRange, 250.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int)
        {
            return v < 100.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms";
        })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return String (roundToInt (v * 100.0f)) + "%";
        })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::out, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
        {
            return (v > 0.04f ? "+" : "") + String (v, 1) + " dB";
        })));

    auto hzText = [] (float v, int)
    {
        return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
    };
    NormalisableRange<float> loRange (20.0f, 2000.0f, 1.0f);
    loRange.setSkewForCentre (200.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::loCut, 1 }, "Lo Cut (Echos)", loRange, 20.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([hzText] (float v, int i)
        {
            return v <= 20.5f ? String ("aus") : hzText (v, i);
        })));

    NormalisableRange<float> hiRange (1000.0f, 20000.0f, 1.0f);
    hiRange.setSkewForCentre (5000.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::hiCut, 1 }, "Hi Cut (Echos)", hiRange, 20000.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([hzText] (float v, int i)
        {
            return v >= 19999.0f ? String ("aus") : hzText (v, i);
        })));

    // Start-Muster: 8 Echos, die gleichmäßig ausklingen
    const auto& init = bb::stepdelay::presets().front().levels;
    for (int k = 0; k < kMaxSteps; ++k)
    {
        const float def = k < (int) init.size() ? init[(size_t) k] : 0.0f;
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::level (k), 1 }, "Step " + String (k + 1),
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            {
                return v < 0.001f ? String ("stumm") : String (roundToInt (v * 100.0f)) + "%";
            })));
    }
    return layout;
}

//==============================================================================
void StepDelayProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}

float StepDelayProcessor::getParam (const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

void StepDelayProcessor::randomizePattern()
{
    const int n = (int) pSteps->load();
    for (int k = 0; k < n; ++k)
    {
        // etwa jedes dritte Echo stumm, der Rest mit leichtem Ausklingen
        const bool mute = rng.nextFloat() < 0.33f;
        const float tilt = 1.0f - 0.5f * (float) k / (float) std::max (1, n - 1);
        setLevel (k, mute ? 0.0f : (0.25f + 0.75f * rng.nextFloat()) * tilt);
    }
}

void StepDelayProcessor::resetPattern()
{
    const int n = (int) pSteps->load();
    for (int k = 0; k < kMaxSteps; ++k)
        setLevel (k, k < n ? 0.8f * std::pow (0.8f, (float) k * 8.0f / (float) std::max (8, n)) : 0.0f);
}

void StepDelayProcessor::invertPattern()
{
    const int n = (int) pSteps->load();
    for (int k = 0; k < n; ++k)
        setLevel (k, 1.0f - getLevel (k));
}

void StepDelayProcessor::shiftPattern (int dir)
{
    const int n = (int) pSteps->load();
    if (n < 2) return;
    std::vector<float> v ((size_t) n);
    for (int k = 0; k < n; ++k) v[(size_t) k] = getLevel (k);
    for (int k = 0; k < n; ++k)
        setLevel (k, v[(size_t) ((k - dir + n) % n)]);
}

double StepDelayProcessor::stepLengthMs() const
{
    if (pSync->load() < 0.5f)
        return pTime->load();
    const double beats = bb::stepgrid::beats ((int) pRate->load(), (int) pMode->load());
    return beats * 60000.0 / std::max (1.0, vizBpm.load());
}

juce::String StepDelayProcessor::gridName() const
{
    if (pSync->load() < 0.5f) return "frei";
    return juce::String (bb::stepgrid::rateNames[(int) pRate->load()]) + bb::stepgrid::modeShort[(int) pMode->load()];
}

//==============================================================================
int StepDelayProcessor::getNumPrograms() { return (int) bb::stepdelay::presets().size(); }

const juce::String StepDelayProcessor::getProgramName (int index)
{
    const auto& p = bb::stepdelay::presets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String::fromUTF8 (p[(size_t) index].name) : juce::String();
}

void StepDelayProcessor::setCurrentProgram (int index)
{
    const auto& list = bb::stepdelay::presets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    currentPreset = index;
    const auto& p = list[(size_t) index];
    setParam (ParamIDs::steps, (float) p.steps);
    setParam (ParamIDs::sync, 1.0f);
    setParam (ParamIDs::rate, (float) p.rate);
    setParam (ParamIDs::mode, (float) p.mode);
    for (int k = 0; k < kMaxSteps; ++k)
        setLevel (k, k < (int) p.levels.size() ? p.levels[(size_t) k] : 0.0f);
}

//==============================================================================
void StepDelayProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
}

bool StepDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void StepDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    double bpm = vizBpm.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = juce::jlimit (20.0, 999.0, *b);
    vizBpm.store (bpm);

    params.numSteps = juce::jlimit (1, kMaxSteps, (int) pSteps->load());
    if (pSync->load() > 0.5f)
    {
        const double beats = bb::stepgrid::beats ((int) pRate->load(), (int) pMode->load());
        params.stepSamples = beats * 60.0 / bpm * sr;
    }
    else
        params.stepSamples = pTime->load() * 0.001 * sr;

    for (int k = 0; k < kMaxSteps; ++k)
        params.level[(size_t) k] = levelPtr[(size_t) k]->load();
    params.mix     = pMix->load();
    params.outGain = juce::Decibels::decibelsToGain (pOut->load());
    params.loCutHz = pLo->load();
    params.hiCutHz = pHi->load();

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, params);

    for (int k = 0; k < kMaxSteps; ++k)
        vizTap[(size_t) k].store (engine.getTapPeak (k), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* StepDelayProcessor::createEditor()
{
    return new StepDelayEditor (*this);
}

void StepDelayProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StepDelayProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StepDelayProcessor();
}
