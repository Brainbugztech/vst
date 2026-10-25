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
    shiftParam      = apvts.getRawParameterValue (ParamIDs::shift);
    gateParam       = apvts.getRawParameterValue (ParamIDs::gate);
    rangeLowParam   = apvts.getRawParameterValue (ParamIDs::rangeLow);
    rangeHighParam  = apvts.getRawParameterValue (ParamIDs::rangeHigh);
    tuneParam       = apvts.getRawParameterValue (ParamIDs::tune);
    formantParam    = apvts.getRawParameterValue (ParamIDs::formant);
    for (int pc = 0; pc < 12; ++pc)
        mapParams[(size_t) pc] = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::mapId (pc)));

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
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::shift, 1 }, "Shift",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gate, 1 }, "Gate",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, percent));

    auto hzText = [] (float off) {
        return AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([off] (float v, int) {
            if (std::abs (v - off) < 0.5f)
                return String ("OFF");
            return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
        });
    };
    NormalisableRange<float> lowRange (20.0f, 2000.0f, 1.0f);
    lowRange.setSkewForCentre (200.0f);
    NormalisableRange<float> highRange (1000.0f, 20000.0f, 1.0f);
    highRange.setSkewForCentre (5000.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::rangeLow, 1 }, "Range Low", lowRange, 20.0f, hzText (20.0f)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::rangeHigh, 1 }, "Range High", highRange, 20000.0f, hzText (20000.0f)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::formant, 1 }, "Formant",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, percent));
    {
        StringArray targets { "Keep" };
        for (auto* n : kNoteNames)
            targets.add (String ("-> ") + n);
        targets.add ("Mute");
        for (int pc = 0; pc < 12; ++pc)
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mapId (pc), 1 },
                                                                String ("Map ") + kNoteNames[(size_t) pc], targets, 0));
    }
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::tune, 1 }, "Tune (A4)",
                                                       NormalisableRange<float> (415.0f, 466.0f, 0.1f), 440.0f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " Hz"; })));
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
    sampleRateHz = sampleRate;
    filterBank.prepare (sampleRate, block);
    spectral.prepare (sampleRate);
    keyAnalyzer.prepare (sampleRate);
    learnActive.store (false);
    for (auto* b : { &midBuffer, &dryMidBuffer, &outerBuffer })
        b->setSize (2, block, false, false, true);
    dryMidDelay.prepare (spectral.getLatencySamples());
    outerDelay.prepare (spectral.getLatencySamples());

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) block, 2 };
    for (auto* f : { &lowSplit, &highSplit, &lowAllpass })
        f->prepare (spec);
    lowAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

    auto coef = [sampleRate] (double s) { return (float) (1.0 - std::exp (-1.0 / (sampleRate * s))); };
    fastAtt = coef (0.001);
    fastRel = coef (0.020);
    slowAtt = coef (0.040);
    slowRel = coef (0.250);
    transientRel = coef (0.060);

    mixSmoothed.reset (sampleRate, 0.05);
    outputSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);
    outputSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    currentMode = -1; // erzwingt Latenzmeldung + Reset im ersten Block
}

void PrizmAudioProcessor::resetSignalPath()
{
    filterBank.reset();
    spectral.reset();
    dryMidDelay.clear();
    outerDelay.clear();
    for (auto* f : { &lowSplit, &highSplit, &lowAllpass })
        f->reset();
    inMeanSquare = wetMeanSquare = 0.0f;
    autoGain = 1.0f;
    envFast = envSlow = transient = 0.0f;
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

    // Learn: auf den unbearbeiteten Eingang hören
    if (const int cmd = learnCommand.exchange (0))
    {
        if (cmd == 1)
            keyAnalyzer.start();
        else
            keyAnalyzer.cancel();
    }
    if (keyAnalyzer.isActive())
    {
        if (keyAnalyzer.process (buffer.getArrayOfReadPointers(), numCh, numSamples))
        {
            const auto r = keyAnalyzer.getResult();
            learnedKey.store (r.key);
            learnedMinor.store (r.minor);
            learnedA4.store (r.a4);
            learnedConfidence.store (r.confidence);
            learnDone.store (true);
        }
    }
    learnActive.store (keyAnalyzer.isActive());
    learnProgress.store (keyAnalyzer.getProgress());

    EngineParams p;
    p.mask = computeMask();

    // Noten-Map: 0 = Keep, 1..12 = Ziel C..B, 13 = Mute
    const int modeNow = modeParam->getIndex();
    for (int pc = 0; pc < 12; ++pc)
    {
        const int idx = mapParams[(size_t) pc]->getIndex();
        p.map[(size_t) pc] = idx == 0 ? kMapKeep : (idx == 13 ? kMapMute : (int8_t) (idx - 1));
        if (p.map[(size_t) pc] == kMapMute)
            p.mask &= (PitchMask) ~(1u << pc);                 // stumm: in beiden Modi raus
        else if (p.map[(size_t) pc] >= 0 && modeNow == highQuality)
            p.mask |= (PitchMask) (1u << p.map[(size_t) pc]);  // Ziel darf durch den Filter
    }
    if (p.mask == 0)
        p.mask = kAllPitches;
    p.formant = formantParam->load() * 0.01f;
    p.color = colorParam->load() * 0.01f;
    p.clear = clearParam->load() * 0.01f;
    p.gate = gateParam->load() * 0.01f;
    p.shift = shiftParam->load() * 0.01f;
    p.a4 = tuneParam->load();
    displayMask.store ((int) p.mask);

    const float strength = juce::jlimit (0.0f, 1.0f, strengthParam->load() * 0.01f);
    const float clear = juce::jlimit (0.0f, 1.0f, p.clear);

    const int mode = modeParam->getIndex();
    if (mode != currentMode)
    {
        currentMode = mode;
        resetSignalPath();
        setLatencySamples (mode == highQuality ? spectral.getLatencySamples() : 0);
    }

    // Frequenzbereich: Linkwitz-Riley-Weichen, außerhalb bleibt das Signal trocken
    const float lowHz = rangeLowParam->load();
    const float highHz = juce::jmax (rangeHighParam->load(), lowHz * 2.0f);
    const bool wantSplit = lowHz > 20.5f || highHz < 19999.0f;
    if (wantSplit != splitActive)
    {
        splitActive = wantSplit;
        for (auto* f : { &lowSplit, &highSplit, &lowAllpass })
            f->reset();
    }
    if (splitActive)
    {
        const float nyquistSafe = (float) (sampleRateHz * 0.45);
        lowSplit.setCutoffFrequency (juce::jmin (lowHz, nyquistSafe));
        highSplit.setCutoffFrequency (juce::jmin (highHz, nyquistSafe));
        lowAllpass.setCutoffFrequency (juce::jmin (highHz, nyquistSafe));
    }

    mixSmoothed.setTargetValue (mixParam->load() * 0.01f);
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    const int maxChunk = midBuffer.getNumSamples();
    const bool hq = (mode == highQuality);

    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, numSamples - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numCh, start, n);

        // 1) Aufteilen: Mitte geht in die Engine, Bass/Höhen außerhalb des Bereichs bleiben trocken
        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float x = chunk.getSample (ch, i);
                float mid = x, outer = 0.0f;
                if (splitActive)
                {
                    float lo = 0.0f, rest = 0.0f, hi = 0.0f;
                    lowSplit.processSample (ch, x, lo, rest);
                    highSplit.processSample (ch, rest, mid, hi);
                    outer = lowAllpass.processSample (ch, lo) + hi;
                }
                midBuffer.setSample (ch, i, mid);
                dryMidBuffer.setSample (ch, i, mid);
                outerBuffer.setSample (ch, i, outer);
            }

        // 2) Engine (gleiche Filterkurve in beiden Modi)
        juce::AudioBuffer<float> wet (midBuffer.getArrayOfWritePointers(), numCh, 0, n);
        if (hq)
            spectral.process (wet, n, p);
        else
            filterBank.process (wet, n, p);

        std::array<float, 12> energy {};
        if (hq ? spectral.takePitchEnergy (energy) : filterBank.takePitchEnergy (energy))
            for (size_t pc = 0; pc < 12; ++pc)
                pitchEnergy[pc].store (energy[pc] / (float) n);

        // 3) Trockene Signale auf die Latenz der Engine ausrichten
        if (hq)
            for (int i = 0; i < n; ++i)
            {
                for (int ch = 0; ch < numCh; ++ch)
                {
                    dryMidBuffer.setSample (ch, i, dryMidDelay.process (ch, dryMidBuffer.getSample (ch, i)));
                    outerBuffer.setSample (ch, i, outerDelay.process (ch, outerBuffer.getSample (ch, i)));
                }
                dryMidDelay.advance();
                outerDelay.advance();
            }

        // 4) Pegelausgleich: gefiltertes Signal etwa so laut wie das Original
        double inSum = 0.0, wetSum = 0.0;
        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float d = dryMidBuffer.getSample (ch, i), w = midBuffer.getSample (ch, i);
                inSum += (double) d * d;
                wetSum += (double) w * w;
            }
        const float denom = (float) (n * numCh);
        const float msCoef = (float) (1.0 - std::exp (-(double) n / (sampleRateHz * 0.25)));
        inMeanSquare += ((float) inSum / denom - inMeanSquare) * msCoef;
        wetMeanSquare += ((float) wetSum / denom - wetMeanSquare) * msCoef;
        float newGain = autoGain;
        if (inMeanSquare > 1.0e-8f && wetMeanSquare > 1.0e-12f)
        {
            const float target = juce::jlimit (0.25f, 40.0f, std::sqrt (inMeanSquare / wetMeanSquare));
            newGain = autoGain + (target - autoGain) * (float) (1.0 - std::exp (-(double) n / (sampleRateHz * 0.15)));
        }

        // 5) Strength, Clear (Anschläge trocken durchlassen), Mix, Output
        for (int i = 0; i < n; ++i)
        {
            float x = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                x += std::abs (dryMidBuffer.getSample (ch, i));
            x /= (float) numCh;
            envFast += (x - envFast) * (x > envFast ? fastAtt : fastRel);
            envSlow += (x - envSlow) * (x > envSlow ? slowAtt : slowRel);
            const float raw = envFast > 1.0e-4f ? juce::jlimit (0.0f, 1.0f, (envFast / (envSlow + 1.0e-6f) - 1.3f) / 1.5f) : 0.0f;
            transient = raw > transient ? raw : transient + (raw - transient) * transientRel;

            const float s = strength * (1.0f - clear * transient);
            const float g = autoGain + (newGain - autoGain) * ((float) i / (float) n);
            const float m = mixSmoothed.getNextValue();
            const float out = outputSmoothed.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float dryMid = dryMidBuffer.getSample (ch, i);
                const float outer = outerBuffer.getSample (ch, i);
                const float processed = outer + dryMid * (1.0f - s) + midBuffer.getSample (ch, i) * g * s;
                const float dry = outer + dryMid;
                chunk.setSample (ch, i, softSafety ((dry * (1.0f - m) + processed * m) * out));
            }
        }
        autoGain = newGain;
    }
}

bool PrizmAudioProcessor::applyLearnResult (KeyAnalyzer::Result& out)
{
    if (! learnDone.exchange (false))
        return false;

    out.key = learnedKey.load();
    out.minor = learnedMinor.load();
    out.a4 = learnedA4.load();
    out.confidence = learnedConfidence.load();

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };
    set (ParamIDs::key, (float) out.key);
    set (ParamIDs::scale, out.minor ? 1.0f : 0.0f); // 0 = Major, 1 = Minor
    set (ParamIDs::tune, juce::jlimit (415.0f, 466.0f, out.a4));
    return true;
}

void PrizmAudioProcessor::applyMapPreset (MapPreset preset)
{
    const int key = keyParam->getIndex();
    std::array<int, 12> idx {}; // 0 = Keep
    auto mapRel = [&] (int from, int to) { idx[(size_t) ((key + from) % 12)] = (key + to) % 12 + 1; };
    if (preset == MapPreset::majorToMinor)
    {
        mapRel (4, 3);   // große Terz -> kleine Terz
        mapRel (9, 8);   // große Sexte -> kleine Sexte
        mapRel (11, 10); // große Septime -> kleine Septime
    }
    else if (preset == MapPreset::minorToMajor)
    {
        mapRel (3, 4);
        mapRel (8, 9);
        mapRel (10, 11);
    }
    for (int pc = 0; pc < 12; ++pc)
    {
        auto* param = mapParams[(size_t) pc];
        param->beginChangeGesture();
        *param = idx[(size_t) pc];
        param->endChangeGesture();
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
