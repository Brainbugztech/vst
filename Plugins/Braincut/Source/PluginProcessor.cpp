#include "PluginProcessor.h"
#include "PluginEditor.h"

BraincutProcessor::BraincutProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzBraincut", createLayout())
{
    apvts.state.setProperty ("preset", 0, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout BraincutProcessor::createLayout()
{
    using namespace juce;
    using P = ParameterID;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        return String (roundToInt (v * 100.0f)) + " %";
    });
    auto floatP = [&] (const char* id, const char* name, float lo, float hi, float def, AudioParameterFloatAttributes attr)
    {
        layout.add (std::make_unique<AudioParameterFloat> (P { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.001f), def, attr));
    };
    auto boolP = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (P { id, 1 }, name, def));
    };

    // Slicer
    layout.add (std::make_unique<AudioParameterChoice> (P { ParamIDs::mode, 1 }, "Slicer Mode", StringArray { "CUT", "WARP" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (P { ParamIDs::loop, 1 }, "Loop Length", StringArray { "1 Bar", "2 Bars", "4 Bars" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (P { ParamIDs::phMin, 1 }, "Min Phrase", 1, 8, 1));
    layout.add (std::make_unique<AudioParameterInt> (P { ParamIDs::phMax, 1 }, "Max Phrase", 1, 8, 4));
    layout.add (std::make_unique<AudioParameterInt> (P { ParamIDs::repMin, 1 }, "Min Rep", 0, 16, 0));
    layout.add (std::make_unique<AudioParameterInt> (P { ParamIDs::repMax, 1 }, "Max Rep", 0, 16, 3));
    layout.add (std::make_unique<AudioParameterChoice> (P { ParamIDs::subdiv, 1 }, "Subdivision", StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));
    floatP (ParamIDs::repeat, "Repeat", 0.0f, 1.0f, 0.4f, percent);
    layout.add (std::make_unique<AudioParameterInt> (P { ParamIDs::seed, 1 }, "Seed", 0, 9999, 1));
    boolP (ParamIDs::lock, "Lock", false);

    // Stretch
    boolP (ParamIDs::strOn, "Stretch On", true);
    floatP (ParamIDs::strAmt, "Stretch Amount", 0.0f, 1.0f, 0.5f, percent);
    floatP (ParamIDs::strMix, "Stretch Mix", 0.0f, 1.0f, 1.0f, percent);

    // Repeat
    boolP (ParamIDs::repOn, "Repeater On", true);
    floatP (ParamIDs::repChance, "Repeater Chance", 0.0f, 1.0f, 0.3f, percent);
    layout.add (std::make_unique<AudioParameterChoice> (P { ParamIDs::repRate, 1 }, "Repeater Rate",
                                                       StringArray { "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 2));
    floatP (ParamIDs::repGate, "Repeater Gate", 0.1f, 1.0f, 0.7f, percent);
    floatP (ParamIDs::repDecay, "Repeater Decay", 0.0f, 1.0f, 0.3f, percent);

    // Glitch
    boolP (ParamIDs::gliOn, "Glitch On", true);
    floatP (ParamIDs::gliAmt, "Glitch Amount", 0.0f, 1.0f, 0.3f, percent);
    floatP (ParamIDs::gliMix, "Glitch Mix", 0.0f, 1.0f, 1.0f, percent);
    boolP (ParamIDs::gBit, "Glitch Bit", true);
    boolP (ParamIDs::gRev, "Glitch Rev", true);
    boolP (ParamIDs::gStop, "Glitch Stop", true);
    boolP (ParamIDs::gFrz, "Glitch Freeze", true);
    boolP (ParamIDs::gPit, "Glitch Pitch", true);

    // Ausgang
    floatP (ParamIDs::mix, "Mix", 0.0f, 1.0f, 1.0f, percent);
    floatP (ParamIDs::out, "Output", -12.0f, 12.0f, 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
            {
                return String (v, 1) + " dB";
            }));

    return layout;
}

bc::EngineParams BraincutProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto b = [&] (const char* id) { return f (id) > 0.5f; };
    auto i = [&] (const char* id) { return (int) std::lround (f (id)); };

    bc::EngineParams p;
    p.slicer.mode       = i (ParamIDs::mode);
    p.slicer.loopIdx    = i (ParamIDs::loop);
    p.slicer.phraseMin  = i (ParamIDs::phMin);
    p.slicer.phraseMax  = i (ParamIDs::phMax);
    p.slicer.repMin     = i (ParamIDs::repMin);
    p.slicer.repMax     = i (ParamIDs::repMax);
    p.slicer.subdivIdx  = i (ParamIDs::subdiv);
    p.slicer.repeatProb = f (ParamIDs::repeat);
    p.slicer.seed       = i (ParamIDs::seed);
    p.slicer.lock       = b (ParamIDs::lock);

    p.stretch.on     = b (ParamIDs::strOn);
    p.stretch.amount = f (ParamIDs::strAmt);
    p.stretch.mix    = f (ParamIDs::strMix);

    p.repeat.on     = b (ParamIDs::repOn);
    p.repeat.chance = f (ParamIDs::repChance);
    p.repeat.rateIdx = i (ParamIDs::repRate);
    p.repeat.gate   = f (ParamIDs::repGate);
    p.repeat.decay  = f (ParamIDs::repDecay);

    p.glitch.on     = b (ParamIDs::gliOn);
    p.glitch.amount = f (ParamIDs::gliAmt);
    p.glitch.mix    = f (ParamIDs::gliMix);
    p.glitch.bit    = b (ParamIDs::gBit);
    p.glitch.rev    = b (ParamIDs::gRev);
    p.glitch.stop   = b (ParamIDs::gStop);
    p.glitch.freeze = b (ParamIDs::gFrz);
    p.glitch.pitch  = b (ParamIDs::gPit);

    p.mix   = f (ParamIDs::mix);
    p.outDb = f (ParamIDs::out);
    return p;
}

void BraincutProcessor::setParam (const char* id, float plainValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}

void BraincutProcessor::rollSeed()
{
    setParam (ParamIDs::seed, (float) juce::Random::getSystemRandom().nextInt (10000));
}

const juce::String BraincutProcessor::getProgramName (int index)
{
    if (index < 0 || index >= bcpresets::kNumPresets)
        return {};
    return bcpresets::kPresets[index].name;
}

int BraincutProcessor::getPresetIndex() const
{
    return juce::jlimit (0, bcpresets::kNumPresets - 1, (int) apvts.state.getProperty ("preset", 0));
}

void BraincutProcessor::applyPreset (int index)
{
    if (index < 0 || index >= bcpresets::kNumPresets)
        return;
    const auto& s = bcpresets::kPresets[index];

    setParam (ParamIDs::mode, (float) s.mode);
    setParam (ParamIDs::loop, (float) s.loop);
    setParam (ParamIDs::phMin, (float) s.phMin);
    setParam (ParamIDs::phMax, (float) s.phMax);
    setParam (ParamIDs::repMin, (float) s.repMin);
    setParam (ParamIDs::repMax, (float) s.repMax);
    setParam (ParamIDs::subdiv, (float) s.subdiv);
    setParam (ParamIDs::repeat, s.repeat);
    setParam (ParamIDs::seed, (float) s.seed);
    setParam (ParamIDs::lock, s.lock ? 1.0f : 0.0f);

    setParam (ParamIDs::strOn, s.strOn ? 1.0f : 0.0f);
    setParam (ParamIDs::strAmt, s.strAmt);
    setParam (ParamIDs::strMix, s.strMix);

    setParam (ParamIDs::repOn, s.repOn ? 1.0f : 0.0f);
    setParam (ParamIDs::repChance, s.repChance);
    setParam (ParamIDs::repRate, (float) s.repRate);
    setParam (ParamIDs::repGate, s.repGate);
    setParam (ParamIDs::repDecay, s.repDecay);

    setParam (ParamIDs::gliOn, s.gliOn ? 1.0f : 0.0f);
    setParam (ParamIDs::gliAmt, s.gliAmt);
    setParam (ParamIDs::gliMix, s.gliMix);
    setParam (ParamIDs::gBit, s.bit ? 1.0f : 0.0f);
    setParam (ParamIDs::gRev, s.rev ? 1.0f : 0.0f);
    setParam (ParamIDs::gStop, s.stop ? 1.0f : 0.0f);
    setParam (ParamIDs::gFrz, s.frz ? 1.0f : 0.0f);
    setParam (ParamIDs::gPit, s.pit ? 1.0f : 0.0f);

    setParam (ParamIDs::mix, s.mix);
    setParam (ParamIDs::out, s.outDb);

    apvts.state.setProperty ("preset", index, nullptr);
}

void BraincutProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
    freeBeats = 0.0;
}

bool BraincutProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void BraincutProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    // Takt vom Host
    bc::Transport tr;
    bool havePpq = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) tr.bpm = *b;
            tr.playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { tr.ppq = *q; havePpq = true; }
            if (auto ts = pos->getTimeSignature())
                if (ts->numerator > 0 && ts->denominator > 0)
                    tr.beatsPerBar = (double) ts->numerator * 4.0 / (double) ts->denominator;
        }
    }
    if (! havePpq)
    {
        // Host liefert keine Taktposition: selbst mitzählen
        tr.ppq = freeBeats;
        freeBeats += (double) n * tr.bpm / 60.0 / sr;
    }
    else
        freeBeats = tr.ppq + (double) n * tr.bpm / 60.0 / sr;

    vizPlaying.store (tr.playing, std::memory_order_relaxed);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, tr, readParams());
}

juce::AudioProcessorEditor* BraincutProcessor::createEditor()
{
    return new BraincutEditor (*this);
}

void BraincutProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BraincutProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BraincutProcessor();
}
