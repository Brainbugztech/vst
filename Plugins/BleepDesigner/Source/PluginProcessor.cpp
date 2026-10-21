#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---------------------------------------------------------------- TestVoice

void TestVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    phase = 0.0;
    inc = juce::MidiMessage::getMidiNoteInHertz (midiNote) / getSampleRate();
    level = velocity * 0.3f;
    adsr.setSampleRate (getSampleRate());
    adsr.setParameters ({ 0.002f, 0.18f, 0.0f, 0.05f });
    adsr.noteOn();
}

void TestVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
    }
}

void TestVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const int numCh = out.getNumChannels();
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = (float) std::sin (phase * juce::MathConstants<double>::twoPi) * level * adsr.getNextSample();
        for (int ch = 0; ch < numCh; ++ch)
            out.addSample (ch, startSample + i, s);

        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;

        if (! adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

// ----------------------------------------------------------- BleepProcessor

BleepProcessor::BleepProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzBleepDesigner", createLayout())
{
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new TestVoice());
    synth.addSound (new TestSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout BleepProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -3.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
        {
            return String (v, 1) + " dB";
        })));

    return layout;
}

void BleepProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    outGain.reset (sampleRate, 0.02);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::output)->load()));
    env = 0.0f;
}

bool BleepProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BleepProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0)
        return;

    // Letzte Note fuer die Anzeige merken
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            lastNote.store (msg.getNoteNumber(), std::memory_order_relaxed);
            noteCounter.fetch_add (1, std::memory_order_relaxed);
        }
    }

    synth.renderNextBlock (buffer, midi, 0, n);

    outGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::output)->load()));
    const int numCh = buffer.getNumChannels();
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[i] *= g;
            peak = std::max (peak, std::abs (d[i]));
        }
        env += (peak - env) * (peak > env ? 0.05f : 0.0005f);
    }
    vizLevel.store (env, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* BleepProcessor::createEditor()
{
    return new BleepEditor (*this);
}

void BleepProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BleepProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepProcessor();
}
