#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace pluto;

PlutoProcessor::PlutoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Plutonium239", createLayout()),
      engine (*bank)
{
    auto raw = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    for (int o = 0; o < numOsc; ++o)
    {
        auto& x = op[o];
        x.wave = raw (oid (o, "wave")); x.wt = raw (oid (o, "wt")); x.oct = raw (oid (o, "oct")); x.semi = raw (oid (o, "semi"));
        x.cent = raw (oid (o, "cent")); x.pwm = raw (oid (o, "pwm")); x.wtpos = raw (oid (o, "wtpos"));
        x.pdmode = raw (oid (o, "pdmode")); x.pdamt = raw (oid (o, "pdamt")); x.fine = raw (oid (o, "fine")); x.coarse = raw (oid (o, "coarse"));
        x.a = raw (oid (o, "a")); x.d = raw (oid (o, "d")); x.s = raw (oid (o, "s")); x.r = raw (oid (o, "r"));
        x.lvl = raw (oid (o, "lvl")); x.key = raw (oid (o, "key")); x.sync = raw (oid (o, "sync"));
        if (! isCarrier (o))
        {
            x.dest = raw (oid (o, "dest")); x.fmtype = raw (oid (o, "fmtype")); x.fmamt = raw (oid (o, "fmamt"));
            x.fmfine = raw (oid (o, "fmfine")); x.fb = raw (oid (o, "fb"));
        }
        else
            x.fdest = raw (oid (o, "fdest"));
    }
    for (int f = 0; f < numFilt; ++f)
    {
        auto& x = fp[f];
        x.on = raw (fid (f, "on")); x.type = raw (fid (f, "type")); x.cut = raw (fid (f, "cut")); x.res = raw (fid (f, "res"));
        x.mix = raw (fid (f, "mix")); x.drive = raw (fid (f, "drive")); x.pan = raw (fid (f, "pan")); x.xpan = raw (fid (f, "xpan"));
        x.lvl = raw (fid (f, "lvl"));
    }
    for (int l = 0; l < numLfo; ++l)
    {
        auto& x = lp[l];
        x.shape = raw (lid (l, "shape")); x.rate = raw (lid (l, "rate")); x.sync = raw (lid (l, "sync")); x.div = raw (lid (l, "div"));
        x.retrig = raw (lid (l, "retrig")); x.tA = raw (lid (l, "tA")); x.aA = raw (lid (l, "aA")); x.tB = raw (lid (l, "tB")); x.aB = raw (lid (l, "aB"));
    }
    mTune = raw ("m_tune"); mOct = raw ("m_oct"); mSemi = raw ("m_semi"); mCent = raw ("m_cent"); mFine = raw ("m_fine");
    mCoarse = raw ("m_coarse"); mVol = raw ("m_vol"); mVoices = raw ("m_voices"); mGlide = raw ("m_glide"); mBend = raw ("m_bend"); mVel = raw ("m_vel");
}

bool PlutoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PlutoProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void PlutoProcessor::buildPatch (Patch& p) const
{
    auto I = [] (const std::atomic<float>* a) { return (int) std::lround (a->load()); };
    auto Fv = [] (const std::atomic<float>* a) { return a->load(); };

    for (int o = 0; o < numOsc; ++o)
    {
        const auto& x = op[o];
        auto& d = p.o[(size_t) o];
        d.wave = I (x.wave); d.wt = I (x.wt); d.keyMode = I (x.key);
        d.sync = otherOscIndexToOsc (o, I (x.sync));
        // Fine = Cent (100 = 1 Halbton), Coarse = stufenlose Tonhoehe in Halbtoenen
        d.pitch = (float) I (x.oct) * 12.0f + (float) I (x.semi) + (Fv (x.cent) + Fv (x.fine)) * 0.01f + Fv (x.coarse);
        d.fineHz = 0.0f; d.pwm = Fv (x.pwm); d.wtPos = Fv (x.wtpos); d.pdMode = I (x.pdmode); d.pdAmt = Fv (x.pdamt);
        d.a = Fv (x.a); d.d = Fv (x.d); d.s = Fv (x.s); d.r = Fv (x.r); d.lvl = Fv (x.lvl);
        if (! isCarrier (o))
        {
            d.dest = otherOscIndexToOsc (o, I (x.dest));
            d.fmType = I (x.fmtype);
            d.fm = juce::jlimit (0.0f, 1.0f, Fv (x.fmamt) + Fv (x.fmfine) * 0.1f);
            d.fb = Fv (x.fb);
            d.fdest = -1;
        }
        else
        {
            const int fd = I (x.fdest);
            d.fdest = fd >= 3 ? -1 : fd;
            d.dest = -1;
        }
    }
    for (int f = 0; f < numFilt; ++f)
    {
        const auto& x = fp[f];
        auto& d = p.f[(size_t) f];
        d.on = Fv (x.on) > 0.5f; d.type = I (x.type); d.cut = Fv (x.cut); d.res = Fv (x.res); d.mix = Fv (x.mix);
        d.drive = Fv (x.drive); d.pan = Fv (x.pan); d.xpan = Fv (x.xpan); d.lvl = Fv (x.lvl);
    }
    for (int l = 0; l < numLfo; ++l)
    {
        const auto& x = lp[l];
        auto& d = p.l[(size_t) l];
        d.shape = I (x.shape); d.rate = Fv (x.rate); d.sync = Fv (x.sync) > 0.5f; d.div = I (x.div); d.retrig = Fv (x.retrig) > 0.5f;
        d.tA = I (x.tA); d.aA = Fv (x.aA); d.tB = I (x.tB); d.aB = Fv (x.aB);
    }
    p.m.pitch = Fv (mTune) * 0.01f + (float) I (mOct) * 12.0f + (float) I (mSemi) + Fv (mCent) * 0.01f + Fv (mFine) + Fv (mCoarse);
    p.m.vol = Fv (mVol); p.m.voices = I (mVoices); p.m.glide = Fv (mGlide); p.m.bend = I (mBend); p.m.vel = Fv (mVel);
}

void PlutoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    Patch p;
    buildPatch (p);
    engine.setPatch (p);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            engine.setTransport (pos->getIsPlaying(), pos->getPpqPosition().orFallback (0.0), pos->getBpm().orFallback (120.0));

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    juce::HeapBlock<float> tmpR;
    if (R == nullptr) { tmpR.calloc ((size_t) buffer.getNumSamples()); R = tmpR.get(); }

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        if (at > pos) { engine.render (L + pos, R + pos, at - pos); pos = at; }
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isSustainPedalOn()) engine.setSustain (true);
        else if (m.isSustainPedalOff()) engine.setSustain (false);
        else if (m.isPitchWheel()) engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    if (pos < buffer.getNumSamples()) engine.render (L + pos, R + pos, buffer.getNumSamples() - pos);
}

void PlutoProcessor::setCurrentProgram (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentPreset = index;
    applyPreset (apvts, list[(size_t) index]);
    presetChanged.fetch_add (1);
}

const juce::String PlutoProcessor::getProgramName (int index)
{
    const auto& list = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) list.size()) ? list[(size_t) index].name : juce::String();
}

bool PlutoProcessor::loadUserWavetable (int osc, const juce::File& file, juce::String& error)
{
    auto t = loadWavetableFile (file, error);
    if (t == nullptr) return false;
    setUserTable (osc, std::move (t), file.getFileNameWithoutExtension());
    // Oszillator direkt auf WT + User stellen
    auto setP = [this] (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    setP (oid (osc, "wave"), 4.0f);
    setP (oid (osc, "wt"), (float) userTableIndex);
    return true;
}

void PlutoProcessor::setUserTable (int osc, std::unique_ptr<WtTable> table, const juce::String& name)
{
    if (! juce::isPositiveAndBelow (osc, numOsc)) return;
    engine.userTables[osc].store (table.get(), std::memory_order_release);
    if (userOwned[osc] != nullptr) retiredTables.push_back (std::move (userOwned[osc]));
    userOwned[osc] = std::move (table);
    userNames[osc] = userOwned[osc] != nullptr ? name : juce::String();
    userTablesChanged.fetch_add (1);
}

void PlutoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    // eigene Wavetables mit ins Projekt speichern (32 Frames x 2048 Samples)
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType ("UserWT")) state.removeChild (i, nullptr);
    for (int o = 0; o < numOsc; ++o)
        if (userOwned[o] != nullptr)
        {
            std::vector<float> frames;
            userOwned[o]->getFrames (frames);
            juce::MemoryBlock mb (frames.data(), frames.size() * sizeof (float));
            juce::ValueTree t ("UserWT");
            t.setProperty ("osc", o, nullptr);
            t.setProperty ("name", userNames[o], nullptr);
            t.setProperty ("data", mb.toBase64Encoding(), nullptr);
            state.appendChild (t, nullptr);
        }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PlutoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentPreset = vt.getProperty ("preset", 0);
            for (int o = 0; o < numOsc; ++o) setUserTable (o, nullptr, {});
            for (int i = vt.getNumChildren(); --i >= 0;)
            {
                auto t = vt.getChild (i);
                if (! t.hasType ("UserWT")) continue;
                juce::MemoryBlock mb;
                if (mb.fromBase64Encoding (t.getProperty ("data").toString()))
                {
                    std::vector<float> frames (mb.getSize() / sizeof (float));
                    std::memcpy (frames.data(), mb.getData(), frames.size() * sizeof (float));
                    setUserTable ((int) t.getProperty ("osc"), wavetableFromFrames (frames), t.getProperty ("name").toString());
                }
                vt.removeChild (i, nullptr);
            }
            apvts.replaceState (vt);
            presetChanged.fetch_add (1);
        }
}

juce::AudioProcessorEditor* PlutoProcessor::createEditor() { return new PlutoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlutoProcessor(); }
