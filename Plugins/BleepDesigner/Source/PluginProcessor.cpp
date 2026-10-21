#include "PluginProcessor.h"
#include "PluginEditor.h"

// --------------------------------------------------------------- BleepVoice

void BleepVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    core.prepare (getSampleRate());
    core.start (juce::MidiMessage::getMidiNoteInHertz (midiNote), velocity, params);
}

void BleepVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        core.release();
    }
    else
    {
        core.kill();
        clearCurrentNote();
    }
}

void BleepVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    core.update (params);
    const int numCh = out.getNumChannels();
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = core.next (params) * 0.25f;
        for (int ch = 0; ch < numCh; ++ch)
            out.addSample (ch, startSample + i, s);

        if (! core.isActive())
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
    genParams = readGenParams();
    for (int i = 0; i < kNumVoices; ++i)
    {
        liveSynth.addVoice (new BleepVoice (genParams));
        slotSynth.addVoice (new bleep::SamplerVoice (audioSlot));
    }
    liveSynth.addSound (new BleepSound());
    slotSynth.addSound (new bleep::SlotSound());
    startTimer (500);
}

// ------------------------------------------------------------------- Slots

void BleepProcessor::setSlot (bleep::SlotBuffer::Ptr newSlot)
{
    {
        const juce::ScopedLock pl (poolLock);
        if (newSlot != nullptr)
            pool.add (newSlot);
    }
    const juce::SpinLock::ScopedLockType sl (slotLock);
    slot = newSlot;
}

bleep::SlotBuffer::Ptr BleepProcessor::getSlot() const
{
    const juce::SpinLock::ScopedLockType sl (slotLock);
    return slot;
}

// Gibt Puffer frei, die nur noch der Pool haelt (laeuft auf dem Message-Thread)
void BleepProcessor::purgeSlots()
{
    const juce::ScopedLock pl (poolLock);
    for (int i = pool.size(); --i >= 0;)
        if (pool.getObjectPointerUnchecked (i)->getReferenceCount() == 1)
            pool.remove (i);
}

void BleepProcessor::captureSlot()
{
    auto s = juce::ReferenceCountedObjectPtr<bleep::SlotBuffer> (new bleep::SlotBuffer());
    s->sampleRate = currentSR.load();
    s->rootNote = juce::roundToInt (raw (ParamIDs::rootNote));
    s->audio = bleep::renderBleep (readGenParams(), s->sampleRate, s->rootNote, raw (ParamIDs::slotLen));

    // Spitzen ueber 0 dBFS vermeiden
    const float peak = s->audio.getMagnitude (0, 0, s->audio.getNumSamples());
    if (peak > 1.0f)
        s->audio.applyGain (1.0f / peak);

    setSlot (s);

    if (auto* src = apvts.getParameter (ParamIDs::source))
    {
        src->beginChangeGesture();
        src->setValueNotifyingHost (1.0f);
        src->endChangeGesture();
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout BleepProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto signedInt = [] (int v, int) { return v > 0 ? "+" + String (v) : String (v); };
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int)
    {
        return String (roundToInt (v)) + " %";
    });
    auto msAttr = AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int)
    {
        return v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms";
    });
    auto st = AudioParameterFloatAttributes().withLabel ("st").withStringFromValueFunction ([] (float v, int)
    {
        return (v > 0.0f ? "+" : "") + String (v, std::abs (v) < 10.0f ? 1 : 0);
    });
    auto db = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
    {
        return String (v, 1) + " dB";
    });

    auto msRange = [] (float max, float centre)
    {
        NormalisableRange<float> r (0.0f, max, 0.1f);
        r.setSkewForCentre (centre);
        return r;
    };

    // Square
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::sqOct, 1 }, "Square Oktave", -4, 2, -1,
                                                     AudioParameterIntAttributes().withStringFromValueFunction (signedInt)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::sqLevel, 1 }, "Square Pegel",
                                                       NormalisableRange<float> (-24.0f, 0.0f, 0.1f), 0.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::sqPw, 1 }, "Pulsbreite",
                                                       NormalisableRange<float> (5.0f, 95.0f, 0.1f), 50.0f, pct));

    // Triangle-Modulator
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::modOct, 1 }, "Tri-Mod Oktave", -4, 0, -3,
                                                     AudioParameterIntAttributes().withStringFromValueFunction (signedInt)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::modPd, 1 }, "PD-Amount",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::pitchLink, 1 }, "Pitch-Link", true));

    // Pitch-Envelope
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::penvAmt, 1 }, "Pitch-Env Amount",
                                                       NormalisableRange<float> (-48.0f, 48.0f, 0.1f), 24.0f, st));
    {
        NormalisableRange<float> r (1.0f, 500.0f, 0.1f);
        r.setSkewForCentre (60.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::penvDecay, 1 }, "Pitch-Env Decay", r, 60.0f, msAttr));
    }

    // LFO
    {
        NormalisableRange<float> r (0.1f, 500.0f, 0.01f);
        r.setSkewForCentre (6.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lfoRate, 1 }, "LFO Rate", r, 4.0f,
            AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
            {
                return String (v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz";
            })));
    }
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lfoPitch, 1 }, "LFO Pitch",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, st));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lfoPd, 1 }, "LFO PD",
                                                       NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, pct));

    // Amp-Envelope
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::ampA, 1 }, "Attack", msRange (2000.0f, 50.0f), 1.0f, msAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::ampD, 1 }, "Decay", msRange (2000.0f, 200.0f), 180.0f, msAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::ampS, 1 }, "Sustain",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::ampR, 1 }, "Release", msRange (2000.0f, 100.0f), 40.0f, msAttr));

    // Ausgang
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -3.0f, db));

    // Slot
    {
        NormalisableRange<float> r (50.0f, 1000.0f, 1.0f);
        r.setSkewForCentre (300.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::slotLen, 1 }, "Slot-Laenge", r, 300.0f, msAttr));
    }
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::rootNote, 1 }, "Root-Note", 24, 96, 48,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
        {
            return MidiMessage::getMidiNoteName (v, true, true, 3);
        })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::source, 1 }, "Quelle",
                                                        StringArray { "Live", "Slot" }, 0));

    // Arp
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::arpOn, 1 }, "Arp", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::arpRate, 1 }, "Arp Rate",
        StringArray { "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T", "1/32 T" }, 2));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::arpSteps, 1 }, "Steps", 1, 16, 16));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::arpSwing, 1 }, "Swing",
                                                       NormalisableRange<float> (0.0f, 75.0f, 0.1f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::arpGate, 1 }, "Gate",
                                                       NormalisableRange<float> (10.0f, 100.0f, 0.1f), 80.0f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::arpMode, 1 }, "Arp-Modus",
        StringArray { "Up", "Down", "Up/Down", "Random", "As Played" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::arpScale, 1 }, "Tonleiter",
        StringArray { "Chroma", "Minor", "Major", "Phrygisch" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::arpRange, 1 }, "Oktav-Range", 0, 3, 1,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " Okt"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::arpRestart, 1 }, "Restart", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::density, 1 }, "Dichte",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::autoRoll, 1 }, "Auto-Roll",
        StringArray { "Aus", "1x", "2x", "4x", "8x" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::randMode, 1 }, "Zufall",
        StringArray { "Seed", "Free" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::seed, 1 }, "Seed", 0, 9999, 4271));
    return layout;
}

bleep::GenParams BleepProcessor::readGenParams() const
{
    bleep::GenParams p;
    p.sqOct     = juce::roundToInt (raw (ParamIDs::sqOct));
    p.sqGain    = juce::Decibels::decibelsToGain (raw (ParamIDs::sqLevel), -24.0f);
    p.pw        = raw (ParamIDs::sqPw) * 0.01f;
    p.modOct    = juce::roundToInt (raw (ParamIDs::modOct));
    p.pd        = raw (ParamIDs::modPd) * 0.01f;
    p.pitchLink = raw (ParamIDs::pitchLink) > 0.5f;
    p.penvAmt   = raw (ParamIDs::penvAmt);
    p.penvDecay = raw (ParamIDs::penvDecay);
    p.lfoRate   = raw (ParamIDs::lfoRate);
    p.lfoPitch  = raw (ParamIDs::lfoPitch);
    p.lfoPd     = raw (ParamIDs::lfoPd) * 0.01f;
    p.attack    = raw (ParamIDs::ampA);
    p.decay     = raw (ParamIDs::ampD);
    p.sustain   = raw (ParamIDs::ampS) * 0.01f;
    p.release   = raw (ParamIDs::ampR);
    return p;
}

bleep::ArpSettings BleepProcessor::readArpSettings() const
{
    static constexpr double rateBeats[] = { 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 };
    static constexpr int autoEvery[] = { 0, 1, 2, 4, 8 };
    bleep::ArpSettings a;
    a.on        = raw (ParamIDs::arpOn) > 0.5f;
    a.stepBeats = rateBeats[juce::jlimit (0, 7, juce::roundToInt (raw (ParamIDs::arpRate)))];
    a.numSteps  = juce::roundToInt (raw (ParamIDs::arpSteps));
    a.swing     = raw (ParamIDs::arpSwing) * 0.01f;
    a.gate      = raw (ParamIDs::arpGate) * 0.01f;
    a.mode      = juce::roundToInt (raw (ParamIDs::arpMode));
    a.scale     = juce::roundToInt (raw (ParamIDs::arpScale));
    a.range     = juce::roundToInt (raw (ParamIDs::arpRange));
    a.restart   = raw (ParamIDs::arpRestart) > 0.5f;
    a.density   = raw (ParamIDs::density) * 0.01f;
    a.autoEvery = autoEvery[juce::jlimit (0, 4, juce::roundToInt (raw (ParamIDs::autoRoll)))];
    a.seedMode  = raw (ParamIDs::randMode) < 0.5f;
    a.seed      = juce::roundToInt (raw (ParamIDs::seed));
    return a;
}

void BleepProcessor::rollSteps()
{
    bleep::SplitMix rng ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    arp.roll (readArpSettings(), rng);
}

void BleepProcessor::prepareToPlay (double sampleRate, int)
{
    arp.prepare (sampleRate);
    arpMidi.ensureSize (4096);
    liveSynth.setCurrentPlaybackSampleRate (sampleRate);
    slotSynth.setCurrentPlaybackSampleRate (sampleRate);
    currentSR.store (sampleRate);
    outGain.reset (sampleRate, 0.02);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw (ParamIDs::output)));
    dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 10.0 / sampleRate);
    env = 0.0f;
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
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

    // Takt von Ableton
    bleep::Transport tr;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) tr.bpm = juce::jlimit (20.0, 999.0, *b);
            tr.playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) tr.ppq = *q;
            else tr.playing = false;
        }
    }

    // Arp erzeugt die Noten (ist er aus, gehen die MIDI-Noten direkt durch)
    arp.process (midi, arpMidi, n, readArpSettings(), tr);

    for (const auto meta : arpMidi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            lastNote.store (msg.getNoteNumber(), std::memory_order_relaxed);
            noteCounter.fetch_add (1, std::memory_order_relaxed);
        }
    }

    genParams = readGenParams();

    // Neuen Slot uebernehmen, ohne den Audio-Thread zu blockieren
    {
        const juce::SpinLock::ScopedTryLockType tl (slotLock);
        if (tl.isLocked())
            audioSlot = slot;
    }

    // Quelle umgeschaltet: haengende Noten der alten Quelle ausklingen lassen
    const int source = juce::roundToInt (raw (ParamIDs::source));
    if (source != lastSource)
    {
        (lastSource == 0 ? liveSynth : slotSynth).allNotesOff (0, true);
        lastSource = source;
    }

    // Die aktive Quelle bekommt das MIDI, die andere klingt nur noch aus
    liveSynth.renderNextBlock (buffer, source == 0 ? arpMidi : emptyMidi, 0, n);
    slotSynth.renderNextBlock (buffer, source == 1 ? arpMidi : emptyMidi, 0, n);

    // DC-Blocker (Pulsbreite != 50 % erzeugt Gleichanteil), Output, Pegel fuer die Anzeige
    outGain.setTargetValue (juce::Decibels::decibelsToGain (raw (ParamIDs::output)));
    const int numCh = std::min (2, buffer.getNumChannels());
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            const float x = d[i];
            const float y = x - dcX[ch] + dcR * dcY[ch];
            dcX[ch] = x;
            dcY[ch] = y;
            d[i] = juce::jlimit (-2.0f, 2.0f, y * g);
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
    auto state = apvts.copyState();

    // Steps
    juce::StringArray st;
    for (int i = 0; i < bleep::Arpeggiator::kMaxSteps; ++i)
        st.add (juce::String (arp.steps[(size_t) i].load()));
    state.setProperty ("steps", st.joinIntoString (","), nullptr);

    // Slot-Audio mitspeichern, damit das Projekt beim Oeffnen exakt gleich klingt
    if (auto s = getSlot())
    {
        juce::MemoryBlock mb (s->audio.getReadPointer (0), (size_t) s->audio.getNumSamples() * sizeof (float));
        state.setProperty ("slot1", mb.toBase64Encoding(), nullptr);
        state.setProperty ("slot1sr", s->sampleRate, nullptr);
        state.setProperty ("slot1root", s->rootNote, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BleepProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    bleep::SlotBuffer::Ptr restored;
    if (state.hasProperty ("slot1"))
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (state["slot1"].toString()) && mb.getSize() >= 4 * sizeof (float))
        {
            restored = new bleep::SlotBuffer();
            const int n = (int) (mb.getSize() / sizeof (float));
            restored->audio.setSize (1, n);
            std::memcpy (restored->audio.getWritePointer (0), mb.getData(), (size_t) n * sizeof (float));
            restored->sampleRate = (double) state.getProperty ("slot1sr", 48000.0);
            restored->rootNote = (int) state.getProperty ("slot1root", 48);
        }
        state.removeProperty ("slot1", nullptr);
        state.removeProperty ("slot1sr", nullptr);
        state.removeProperty ("slot1root", nullptr);
    }

    if (state.hasProperty ("steps"))
    {
        juce::StringArray st;
        st.addTokens (state["steps"].toString(), ",", "");
        for (int i = 0; i < juce::jmin (st.size(), bleep::Arpeggiator::kMaxSteps); ++i)
            arp.steps[(size_t) i].store (bleep::packStep (bleep::unpackStep (st[i].getIntValue())));
        state.removeProperty ("steps", nullptr);
    }

    apvts.replaceState (state);
    setSlot (restored);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepProcessor();
}
