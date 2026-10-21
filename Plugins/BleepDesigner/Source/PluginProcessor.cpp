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
        slotSynth.addVoice (new bleep::SamplerVoice (audioSlots, audioLevels));
    }
    liveSynth.addSound (new BleepSound());
    slotSynth.addSound (new bleep::SlotSound());

    renderSlots();   // damit ein frisch geladenes Plugin sofort klingt
    refreshUserPresets();
    markClean();
    startTimer (250);
}

// ------------------------------------------------------------------- Slots

void BleepProcessor::setParamPlain (const juce::String& id, float value)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

void BleepProcessor::setSlot (int index, bleep::SlotBuffer::Ptr newSlot)
{
    {
        const juce::ScopedLock pl (poolLock);
        if (newSlot != nullptr)
            pool.add (newSlot);
    }
    const juce::SpinLock::ScopedLockType sl (slotLock);
    slots[(size_t) index] = newSlot;
}

bleep::SlotBuffer::Ptr BleepProcessor::getSlot (int index) const
{
    const juce::SpinLock::ScopedLockType sl (slotLock);
    return slots[(size_t) index];
}

// Gibt Puffer frei, die nur noch der Pool haelt (laeuft auf dem Message-Thread)
void BleepProcessor::purgeSlots()
{
    const juce::ScopedLock pl (poolLock);
    for (int i = pool.size(); --i >= 0;)
        if (pool.getObjectPointerUnchecked (i)->getReferenceCount() == 1)
            pool.remove (i);
}

void BleepProcessor::timerCallback()
{
    purgeSlots();

    // Seed von Hand geaendert (oder automatisiert): Slots passend neu rendern
    const int seedNow = juce::roundToInt (raw (ParamIDs::seed));
    if (seedNow != lastRenderedSeed)
        renderSlots();
}

void BleepProcessor::renderSlots()
{
    const int seedNow = juce::roundToInt (raw (ParamIDs::seed));
    lastRenderedSeed = seedNow;

    const auto base = readGenParams();
    const float amount = raw (ParamIDs::variation) * 0.01f;
    const double sr = currentSR.load();
    const int root = juce::roundToInt (raw (ParamIDs::rootNote));
    const double len = raw (ParamIDs::slotLen);

    for (int i = 0; i < kNumSlots; ++i)
    {
        if (raw (ParamIDs::slotLock (i)) > 0.5f && getSlot (i) != nullptr)
            continue;

        // Slot 1 bleibt nah an den Reglern, Slot 2-4 bekommen je einen Charakter; gleicher Seed = gleiche Bleeps
        int character = -1;
        const auto p = bleep::slotParams (base, amount, seedNow, i, &character);

        auto s = juce::ReferenceCountedObjectPtr<bleep::SlotBuffer> (new bleep::SlotBuffer());
        s->sampleRate = sr;
        s->rootNote = root;
        s->character = character < 0 ? juce::String() : juce::String (bleep::characterName (character));
        s->audio = bleep::renderBleep (p, sr, root, len);
        const float peak = s->audio.getMagnitude (0, 0, s->audio.getNumSamples());
        if (peak > 1.0f)
            s->audio.applyGain (1.0f / peak);
        setSlot (i, s);
    }
}

void BleepProcessor::generate()
{
    int next = juce::Random::getSystemRandom().nextInt (10000);
    if (next == juce::roundToInt (raw (ParamIDs::seed)))
        next = (next + 1) % 10000;
    setParamPlain (ParamIDs::seed, (float) next);
    renderSlots();
}

void BleepProcessor::auditionSlot (int index)
{
    pendingAudition.store (juce::jlimit (0, kNumSlots - 1, index));
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
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::shape, 1 }, "Wellenform",
        NormalisableRange<float> (0.0f, 3.0f, 0.01f), 2.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            static const char* n[] = { "Sin", "Tri", "Sqr", "Saw" };
            const int i = roundToInt (v);
            return std::abs (v - (float) i) < 0.05f ? String (n[jlimit (0, 3, i)]) : String (v, 2);
        })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::modFine, 1 }, "Mod Fine",
                                                       NormalisableRange<float> (0.0f, 12.0f, 0.01f), 0.0f, st));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::modOct, 1 }, "Mod Oktave", -4, 3, -3,
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
        NormalisableRange<float> r (50.0f, 2000.0f, 1.0f);
        r.setSkewForCentre (400.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::slotLen, 1 }, String::fromUTF8 ("Max. Slot-L\xc3\xa4" "nge"), r, 1200.0f, msAttr));
    }
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::rootNote, 1 }, "Root-Note", 24, 96, 48,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
        {
            return MidiMessage::getMidiNoteName (v, true, true, 3);
        })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::source, 1 }, "Quelle",
                                                        StringArray { "Live", "Slots" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::variation, 1 }, "Variation",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 75.0f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::slotMode, 1 }, "Slot-Modus",
                                                        StringArray { "Random", "No Repeat", "Gewichtet", "Cycle" }, 1));
    for (int i = 0; i < kNumSlots; ++i)
    {
        const String n (i + 1);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::slotLock (i), 1 }, "Slot " + n + " Lock", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::slotWeight (i), 1 }, "Slot " + n + " Gewicht",
                                                           NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::slotLevel (i), 1 }, "Slot " + n + " Pegel",
                                                           NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f, db));
    }

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

    auto hz = AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
    {
        return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz";
    });
    auto hzRange = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi, 0.1f);
        r.setSkewForCentre (centre);
        return r;
    };
    auto ratio = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + "x"; });

    // Comb
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::combOn, 1 }, "Comb an", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::combFreq, 1 }, "Comb Freq", hzRange (20.0f, 5000.0f, 500.0f), 880.0f, hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::combFb, 1 }, "Comb Feedback",
                                                       NormalisableRange<float> (-99.0f, 99.0f, 0.1f), -72.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::combDamp, 1 }, "Comb Damping",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::combKey, 1 }, "Comb Keytrack",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::combCount, 1 }, "Combs", 1, 3, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::combRatio2, 1 }, "Comb Ratio 2",
                                                       NormalisableRange<float> (1.0f, 4.0f, 0.01f), 1.5f, ratio));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::combRatio3, 1 }, "Comb Ratio 3",
                                                       NormalisableRange<float> (1.0f, 4.0f, 0.01f), 2.3f, ratio));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::combMix, 1 }, "Comb Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, pct));

    // Crush
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::crushOn, 1 }, "Crush an", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::crushBits, 1 }, "Bits",
        NormalisableRange<float> (1.0f, 16.0f, 0.01f), 8.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " bit"; })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::crushRate, 1 }, "Crush Rate", 1, 32, 4,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 1 ? String ("1/1") : "1/" + String (v); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::crushMix, 1 }, "Crush Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, pct));

    // Filter
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::filterOn, 1 }, "Filter an", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::filterType, 1 }, "Filter-Typ", StringArray { "LP", "BP", "HP" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::filterCut, 1 }, "Cutoff", hzRange (20.0f, 20000.0f, 1000.0f), 2400.0f, hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::filterRes, 1 }, "Resonanz",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::filterEnv, 1 }, "Filter-Env",
                                                       NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 40.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::filterDec, 1 }, "Filter-Decay", msRange (2000.0f, 150.0f), 90.0f, msAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::filterKey, 1 }, "Filter Keytrack",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct));

    // Delay
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::delayOn, 1 }, "Delay an", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::delayTime, 1 }, "Delay-Zeit",
        StringArray { "1/32", "1/16 T", "1/16", "1/16 .", "1/8 T", "1/8", "1/8 .", "1/4 T", "1/4", "1/4 .", "1/2" }, 6));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::delayFb, 1 }, "Delay Feedback",
                                                       NormalisableRange<float> (0.0f, 95.0f, 0.1f), 45.0f, pct));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::delayPP, 1 }, "Ping-Pong", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::delayLow, 1 }, "Delay Low-Cut", hzRange (20.0f, 2000.0f, 200.0f), 150.0f, hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::delayHigh, 1 }, "Delay High-Cut", hzRange (1000.0f, 20000.0f, 6000.0f), 6000.0f, hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::delayMix, 1 }, "Delay Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct));
    return layout;
}

bleep::GenParams BleepProcessor::readGenParams() const
{
    bleep::GenParams p;
    p.sqOct     = juce::roundToInt (raw (ParamIDs::sqOct));
    p.sqGain    = juce::Decibels::decibelsToGain (raw (ParamIDs::sqLevel), -24.0f);
    p.pw        = raw (ParamIDs::sqPw) * 0.01f;
    p.modOct    = juce::roundToInt (raw (ParamIDs::modOct));
    p.shape     = raw (ParamIDs::shape);
    p.modFine   = raw (ParamIDs::modFine);
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
    a.slotMode  = juce::roundToInt (raw (ParamIDs::slotMode));
    for (int i = 0; i < kNumSlots; ++i)
        a.weights[(size_t) i] = raw (ParamIDs::slotWeight (i)) * 0.01f;
    return a;
}

bleep::FxParams BleepProcessor::readFxParams() const
{
    static constexpr double delayBeats[] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
    bleep::FxParams f;
    f.combOn      = raw (ParamIDs::combOn) > 0.5f;
    f.combFreq    = raw (ParamIDs::combFreq);
    f.combFb      = raw (ParamIDs::combFb) * 0.01f;
    f.combDamp    = raw (ParamIDs::combDamp) * 0.01f;
    f.combKey     = raw (ParamIDs::combKey) * 0.01f;
    f.combCount   = juce::roundToInt (raw (ParamIDs::combCount));
    f.combRatio2  = raw (ParamIDs::combRatio2);
    f.combRatio3  = raw (ParamIDs::combRatio3);
    f.combMix     = raw (ParamIDs::combMix) * 0.01f;
    f.crushOn     = raw (ParamIDs::crushOn) > 0.5f;
    f.crushBits   = raw (ParamIDs::crushBits);
    f.crushRate   = juce::roundToInt (raw (ParamIDs::crushRate));
    f.crushMix    = raw (ParamIDs::crushMix) * 0.01f;
    f.filterOn    = raw (ParamIDs::filterOn) > 0.5f;
    f.filterType  = juce::roundToInt (raw (ParamIDs::filterType));
    f.filterCut   = raw (ParamIDs::filterCut);
    f.filterRes   = raw (ParamIDs::filterRes) * 0.01f;
    f.filterEnv   = raw (ParamIDs::filterEnv) * 0.01f;
    f.filterDecay = raw (ParamIDs::filterDec);
    f.filterKey   = raw (ParamIDs::filterKey) * 0.01f;
    f.delayOn     = raw (ParamIDs::delayOn) > 0.5f;
    f.delayBeats  = delayBeats[juce::jlimit (0, 10, juce::roundToInt (raw (ParamIDs::delayTime)))];
    f.delayFb     = raw (ParamIDs::delayFb) * 0.01f;
    f.delayPingPong = raw (ParamIDs::delayPP) > 0.5f;
    f.delayLowCut = raw (ParamIDs::delayLow);
    f.delayHighCut = raw (ParamIDs::delayHigh);
    f.delayMix    = raw (ParamIDs::delayMix) * 0.01f;
    return f;
}

void BleepProcessor::rollSteps()
{
    bleep::SplitMix rng ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    arp.roll (readArpSettings(), rng);
}

void BleepProcessor::prepareToPlay (double sampleRate, int)
{
    arp.prepare (sampleRate);
    fx.prepare (sampleRate);
    arpMidi.ensureSize (4096);
    slotMidi.ensureSize (4096);
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

    // Neue Slots uebernehmen, ohne den Audio-Thread zu blockieren
    {
        const juce::SpinLock::ScopedTryLockType tl (slotLock);
        if (tl.isLocked())
            audioSlots = slots;
    }
    int mask = 0;
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (audioSlots[(size_t) i] != nullptr)
            mask |= 1 << i;
        audioLevels[(size_t) i] = juce::Decibels::decibelsToGain (raw (ParamIDs::slotLevel (i)), -24.0f);
    }

    // Arp erzeugt die Noten, der MIDI-Kanal sagt welcher Slot spielt
    auto arpSettings = readArpSettings();
    arpSettings.slotMask = mask == 0 ? 0xF : mask;
    arp.process (midi, arpMidi, n, arpSettings, tr);

    const int source = juce::roundToInt (raw (ParamIDs::source));

    // Arp aus: gespielte Noten reihum auf die belegten Slots verteilen
    slotMidi.clear();
    for (const auto meta : arpMidi)
    {
        auto msg = meta.getMessage();
        if (! arpSettings.on && msg.isNoteOn())
        {
            for (int k = 0; k < kNumSlots; ++k)
            {
                directSlot = (directSlot + 1) % kNumSlots;
                if ((arpSettings.slotMask >> directSlot) & 1)
                    break;
            }
            msg.setChannel (directSlot + 1);
        }
        else if (! arpSettings.on && msg.isNoteOff())
        {
            continue;   // One-Shot, Note-Off wird nicht gebraucht
        }

        if (msg.isNoteOn())
        {
            lastNote.store (msg.getNoteNumber(), std::memory_order_relaxed);
            noteCounter.fetch_add (1, std::memory_order_relaxed);
            if (source == 1)
                slotHits[(size_t) juce::jlimit (0, kNumSlots - 1, msg.getChannel() - 1)].fetch_add (1, std::memory_order_relaxed);
            if (msg.getVelocity() >= 100)
            {
                accentSlot.store (juce::jlimit (0, kNumSlots - 1, msg.getChannel() - 1), std::memory_order_relaxed);
                accentHits.fetch_add (1, std::memory_order_relaxed);
            }
        }
        slotMidi.addEvent (msg, meta.samplePosition);
    }

    // Vorhoeren per Klick auf einen Slot
    int aud = pendingAudition.exchange (-1);
    if (aud >= 0 && audioSlots[(size_t) aud] == nullptr)
        aud = -1;
    if (aud >= 0)
    {
        if (source != 1)
            slotMidi.clear();   // im Live-Modus nur den Vorhoer-Ton an die Slots schicken
        slotMidi.addEvent (juce::MidiMessage::noteOn (aud + 1, audioSlots[(size_t) aud]->rootNote, (juce::uint8) 100), 0);
        slotHits[(size_t) aud].fetch_add (1, std::memory_order_relaxed);
    }

    genParams = readGenParams();

    // Quelle umgeschaltet: haengende Noten der alten Quelle ausklingen lassen
    if (source != lastSource)
    {
        (lastSource == 0 ? liveSynth : slotSynth).allNotesOff (0, true);
        lastSource = source;
    }

    // Die aktive Quelle bekommt das MIDI, die andere klingt nur noch aus
    liveSynth.renderNextBlock (buffer, source == 0 ? arpMidi : emptyMidi, 0, n);
    slotSynth.renderNextBlock (buffer, (source == 1 || aud >= 0) ? slotMidi : emptyMidi, 0, n);

    // Effekte: jeder Step startet die Filter-Huellkurve neu und setzt die Keytrack-Note
    int numTrig = 0;
    for (const auto meta : (source == 1 ? slotMidi : arpMidi))
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && numTrig < (int) fxTriggers.size())
            fxTriggers[(size_t) numTrig++] = { meta.samplePosition, msg.getNoteNumber() };
    }
    fx.process (buffer, readFxParams(), fxTriggers.data(), numTrig, tr.bpm);

    // DC-Blocker (Pulsbreite != 50 % erzeugt Gleichanteil), Output, Soft-Clip, Pegel fuer die Anzeige
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
            // Limiter: bis -0.9 dB linear, darueber weich bis maximal 0 dBFS
            const float o = y * g, ao = std::abs (o);
            d[i] = ao < 0.9f ? o : std::copysign (0.9f + 0.1f * std::tanh ((ao - 0.9f) / 0.1f), o);
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
    if (auto xml = createStateXml())
        copyXmlToBinary (*xml, destData);
}

void BleepProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        applyStateXml (*xml);
}

std::unique_ptr<juce::XmlElement> BleepProcessor::createStateXml()
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("presetIndex", currentPreset, nullptr);

    // Steps
    juce::StringArray st;
    for (int i = 0; i < bleep::Arpeggiator::kMaxSteps; ++i)
        st.add (juce::String (arp.steps[(size_t) i].load()));
    state.setProperty ("steps", st.joinIntoString (","), nullptr);

    // Slot-Audio mitspeichern, damit das Projekt beim Oeffnen exakt gleich klingt
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (auto sl = getSlot (i))
        {
            const juce::String key = "slot" + juce::String (i + 1);
            juce::MemoryBlock mb (sl->audio.getReadPointer (0), (size_t) sl->audio.getNumSamples() * sizeof (float));
            state.setProperty (key, mb.toBase64Encoding(), nullptr);
            state.setProperty (key + "sr", sl->sampleRate, nullptr);
            state.setProperty (key + "root", sl->rootNote, nullptr);
            state.setProperty (key + "char", sl->character, nullptr);
        }
    }

    return state.createXml();
}

void BleepProcessor::applyStateXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (xml);
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    currentPreset = (int) state.getProperty ("presetIndex", -1);
    state.removeProperty ("presetName", nullptr);
    state.removeProperty ("presetIndex", nullptr);
    bleep::SlotArray restored;
    bool anyRestored = false;
    for (int i = 0; i < kNumSlots; ++i)
    {
        const juce::Identifier key ("slot" + juce::String (i + 1));
        const juce::Identifier keySr (key.toString() + "sr"), keyRoot (key.toString() + "root"), keyChar (key.toString() + "char");
        if (! state.hasProperty (key))
            continue;
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (state[key].toString()) && mb.getSize() >= 4 * sizeof (float))
        {
            auto r = juce::ReferenceCountedObjectPtr<bleep::SlotBuffer> (new bleep::SlotBuffer());
            const int n = (int) (mb.getSize() / sizeof (float));
            r->audio.setSize (1, n);
            std::memcpy (r->audio.getWritePointer (0), mb.getData(), (size_t) n * sizeof (float));
            r->sampleRate = (double) state.getProperty (keySr, 48000.0);
            r->rootNote = (int) state.getProperty (keyRoot, 48);
            r->character = state.getProperty (keyChar, "").toString();
            restored[(size_t) i] = r;
            anyRestored = true;
        }
        state.removeProperty (key, nullptr);
        state.removeProperty (keySr, nullptr);
        state.removeProperty (keyRoot, nullptr);
        state.removeProperty (keyChar, nullptr);
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
    if (anyRestored)
    {
        for (int i = 0; i < kNumSlots; ++i)
            setSlot (i, restored[(size_t) i]);
        lastRenderedSeed = juce::roundToInt (raw (ParamIDs::seed));   // nicht neu rendern, gespeicherter Klang gilt
    }
    else
    {
        renderSlots();
    }
    markClean();
}

// ----------------------------------------------------------------- Presets

juce::File BleepProcessor::getPresetFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Brain Bugz").getChildFile ("Bleep Designer").getChildFile ("Presets");
}

void BleepProcessor::refreshUserPresets()
{
    userPresets = getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.bbleep");
    std::sort (userPresets.begin(), userPresets.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
    });
}

juce::StringArray BleepProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& fp : bleep::factoryPresets())
        names.add (fp.name);
    for (const auto& f : userPresets)
        names.add (f.getFileNameWithoutExtension());
    return names;
}

void BleepProcessor::loadPreset (int index)
{
    const int numFactory = getNumFactoryPresets();
    if (index < 0 || index >= numFactory + userPresets.size())
        return;

    if (index >= numFactory)
    {
        // Eigenes Preset: kompletter Zustand inklusive der 4 Bleeps
        const auto file = userPresets[index - numFactory];
        if (auto xml = juce::XmlDocument::parse (file))
        {
            applyStateXml (*xml);
            currentPreset = index;
            currentPresetName = file.getFileNameWithoutExtension();
            markClean();
        }
        return;
    }

    // Werkspreset: alles auf Standard, dann die abweichenden Werte
    const auto& fp = bleep::factoryPresets()[(size_t) index];
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, value] : fp.values)
        setParamPlain (id, value);
    setParamPlain (ParamIDs::seed, (float) fp.seed);

    // Steps aus dem Step-Seed: Tonhoehen per Roll, Akzente auf den Zaehlzeiten, ein paar wackelige Steps
    if (fp.stepSeed == 0)
    {
        arp.resetSteps();
    }
    else
    {
        auto settings = readArpSettings();
        bleep::SplitMix rng ((uint64_t) fp.stepSeed * 0x9E3779B97F4A7C15ull + 17);
        for (int i = 0; i < bleep::Arpeggiator::kMaxSteps; ++i)
            arp.setStep (i, { 0, 100, i % 4 == 0 ? 115 : 90, false });
        arp.roll (settings, rng);
        for (int i = 0; i < bleep::Arpeggiator::kMaxSteps; ++i)
        {
            auto st = arp.getStep (i);
            st.prob = i % 4 == 0 ? 100 : 40 + rng.below (61);
            if (i % 4 != 0 && rng.uniform() < 0.15)
                st.vel = 115;
            arp.setStep (i, st);
        }
    }

    renderSlots();
    currentPreset = index;
    currentPresetName = fp.name;
    markClean();
}

bool BleepProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;
    auto folder = getPresetFolder();
    if (! folder.createDirectory())
        return false;

    currentPresetName = clean;
    auto xml = createStateXml();
    const auto file = folder.getChildFile (clean + ".bbleep");
    if (xml == nullptr || ! xml->writeTo (file))
        return false;

    refreshUserPresets();
    currentPreset = getNumFactoryPresets() + userPresets.indexOf (file);
    markClean();
    return true;
}

bool BleepProcessor::deleteUserPreset (int index)
{
    const int u = index - getNumFactoryPresets();
    if (u < 0 || u >= userPresets.size())
        return false;
    const bool ok = userPresets[u].moveToTrash() || userPresets[u].deleteFile();
    refreshUserPresets();
    if (ok && index == currentPreset)
        currentPreset = -1;
    return ok;
}

juce::String BleepProcessor::fingerprint() const
{
    juce::String fp;
    for (auto* prm : getParameters())
        fp << juce::String (prm->getValue(), 4) << ";";
    for (int i = 0; i < bleep::Arpeggiator::kMaxSteps; ++i)
        fp << arp.steps[(size_t) i].load() << ",";
    return fp;
}

bool BleepProcessor::isPresetModified()
{
    return fingerprint() != cleanPrint;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepProcessor();
}
