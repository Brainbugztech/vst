#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

using namespace prizm;

namespace
{
    constexpr int kDefaultCustomMask = 2741; // C major

    float softSafety (float y)
    {
        // Transparent below -0.9 dBFS, gently rounds off anything above so tails can't clip hard.
        const float a = std::abs (y);
        if (a <= 0.9f)
            return y;
        return std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f), y);
    }
} // namespace

PrizmAudioProcessor::PrizmAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PrizmState", createParameterLayout())
{
    keyParam        = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::key));
    scaleParam      = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::scale));
    customMaskParam = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (ParamIDs::customMask));
    midiKeyParam    = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::midiKey));
    modeParam       = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::mode));
    strengthParam   = apvts.getRawParameterValue (ParamIDs::strength);
    colorParam      = apvts.getRawParameterValue (ParamIDs::color);
    clearParam      = apvts.getRawParameterValue (ParamIDs::clear);
    mixParam        = apvts.getRawParameterValue (ParamIDs::mix);
    outputParam     = apvts.getRawParameterValue (ParamIDs::output);

    jassert (keyParam && scaleParam && customMaskParam && midiKeyParam && modeParam);
}

juce::AudioProcessorValueTreeState::ParameterLayout PrizmAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray keys, scales;
    for (auto* n : kNoteNames)
        keys.add (n);
    for (auto& s : kScales)
        scales.add (s.name);

    auto percent = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v)) + " %"; });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::key, 1 }, "Key", keys, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::scale, 1 }, "Scale", scales, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::customMask, 1 }, "Custom Scale", 1, 4095, kDefaultCustomMask));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::midiKey, 1 }, "MIDI Key", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Mode",
                                                        StringArray { "Zero Latency", "High Quality" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::strength, 1 }, "Strength",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::color, 1 }, "Color",
                                                       NormalisableRange<float> (0.0f, 200.0f, 0.1f), 60.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::clear, 1 }, "Clear",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " dB"; })));
    return layout;
}

bool PrizmAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PrizmAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int block = juce::jmax (1, samplesPerBlock);
    filterBank.prepare (sampleRate, block);
    spectral.prepare (sampleRate);

    dryBuffer.setSize (2, block, false, false, true);
    for (auto& d : dryDelay)
        d.assign ((size_t) spectral.getLatencySamples(), 0.0f);
    dryDelayPos = 0;

    mixSmoothed.reset (sampleRate, 0.05);
    outputSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);
    outputSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    currentMode = -1; // forces latency report + engine reset on the first block
}

void PrizmAudioProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    bool changed = false;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            heldNotes[(size_t) msg.getNoteNumber()] = true;
            changed = true;
        }
        else if (msg.isNoteOff())
        {
            heldNotes[(size_t) msg.getNoteNumber()] = false;
            changed = true;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            heldNotes.fill (false);
            changed = true;
        }
    }

    if (! changed)
        return;

    PitchMask held = 0;
    for (int n = 0; n < 128; ++n)
        if (heldNotes[(size_t) n])
            held |= (PitchMask) (1u << (n % 12));

    // Latch: the last chord stays active until a new one is played.
    if (held != 0)
        latchedMidiMask = held;
}

PitchMask PrizmAudioProcessor::computeMask()
{
    const int scaleIndex = scaleParam->getIndex();
    PitchMask mask = (scaleIndex == kCustomScaleIndex)
                         ? (PitchMask) (customMaskParam->get() & kAllPitches)
                         : rotateToKey (kScales[(size_t) scaleIndex].mask, keyParam->getIndex());

    const bool useMidi = midiKeyParam->get() && latchedMidiMask != 0;
    midiActive.store (useMidi);
    if (useMidi)
        mask = latchedMidiMask;

    return mask == 0 ? kAllPitches : mask;
}

void PrizmAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numCh = juce::jmin (2, totalIn, buffer.getNumChannels());
    if (numCh == 0 || numSamples == 0)
        return;

    handleMidi (midi);
    blockCounter.fetch_add (1, std::memory_order_relaxed);

    EngineParams p;
    p.mask = computeMask();
    p.strength = strengthParam->load() * 0.01f;
    p.color = colorParam->load() * 0.01f;
    p.clear = clearParam->load() * 0.01f;
    displayMask.store ((int) p.mask);

    const int mode = modeParam->getIndex();
    if (mode != currentMode)
    {
        currentMode = mode;
        filterBank.reset();
        spectral.reset();
        for (auto& d : dryDelay)
            std::fill (d.begin(), d.end(), 0.0f);
        dryDelayPos = 0;
        setLatencySamples (mode == highQuality ? spectral.getLatencySamples() : 0);
    }

    mixSmoothed.setTargetValue (mixParam->load() * 0.01f);
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    // Work in chunks no larger than the prepared block size.
    const int maxChunk = dryBuffer.getNumSamples();
    const int delayLength = (int) dryDelay[0].size();

    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, numSamples - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numCh, start, n);

        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom (ch, 0, chunk, ch, 0, n);

        if (mode == highQuality)
            spectral.process (chunk, n, p);
        else
            filterBank.process (chunk, n, p);

        std::array<float, 12> energy {};
        const bool fresh = (mode == highQuality) ? spectral.takePitchEnergy (energy)
                                                 : filterBank.takePitchEnergy (energy);
        if (fresh)
            for (size_t pc = 0; pc < 12; ++pc)
                pitchEnergy[pc].store (energy[pc] / (float) n);

        for (int i = 0; i < n; ++i)
        {
            const float m = mixSmoothed.getNextValue();
            const float g = outputSmoothed.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                float dry = dryBuffer.getSample (ch, i);

                if (mode == highQuality && delayLength > 0)
                {
                    auto& d = dryDelay[(size_t) ch];
                    const float delayed = d[(size_t) dryDelayPos];
                    d[(size_t) dryDelayPos] = dry;
                    dry = delayed;
                }

                const float wet = chunk.getSample (ch, i);
                chunk.setSample (ch, i, softSafety ((dry * (1.0f - m) + wet * m) * g));
            }

            if (mode == highQuality && delayLength > 0)
                dryDelayPos = (dryDelayPos + 1) % delayLength;
        }
    }
}

void PrizmAudioProcessor::togglePitchClass (int pitchClass)
{
    if (pitchClass < 0 || pitchClass > 11)
        return;

    const int scaleIndex = scaleParam->getIndex();
    int mask = (scaleIndex == kCustomScaleIndex)
                   ? customMaskParam->get()
                   : (int) rotateToKey (kScales[(size_t) scaleIndex].mask, keyParam->getIndex());

    mask ^= (1 << pitchClass);
    if ((mask & kAllPitches) == 0)
        return; // keep at least one note

    customMaskParam->beginChangeGesture();
    *customMaskParam = mask;
    customMaskParam->endChangeGesture();

    if (scaleIndex != kCustomScaleIndex)
    {
        scaleParam->beginChangeGesture();
        *scaleParam = kCustomScaleIndex;
        scaleParam->endChangeGesture();
    }
}

void PrizmAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PrizmAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PrizmAudioProcessor::createEditor()
{
    return new PrizmAudioProcessorEditor (*this);
}

#ifndef PRIZM_HEADLESS_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrizmAudioProcessor();
}
#endif
