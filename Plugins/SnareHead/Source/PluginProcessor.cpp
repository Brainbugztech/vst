#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace snarehead;

namespace
{
    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::String hz (float v, int)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String ((int) std::round (v)) + " Hz"; }
    juce::String pct (float v, int)  { return juce::String ((int) std::round (v * 100.0f)) + " %"; }
    juce::String ms (float v, int)   { return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String ((int) std::round (v)) + " ms"; }
    juce::String db (float v, int)   { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
}

// ----------------------------------------------------------------- Noten <-> Hz

juce::String noteNameForHz (float hz, bool withCents)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (hz <= 0.0f) return "-";
    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int n = (int) std::round (midi);
    const int cents = (int) std::round ((midi - (float) n) * 100.0f);
    juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    if (withCents && cents != 0) s << (cents > 0 ? " +" : " ") << cents << "ct";
    return s;
}

float parseFreqText (const juce::String& textIn, float lo, float hi)
{
    auto text = textIn.trim().replace (",", ".").toLowerCase().removeCharacters (" ");
    if (text.isEmpty()) return lo;
    const auto first = text[0];
    if (first >= 'a' && first <= 'g' && ! text.startsWith ("hz"))
    {
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 }; // a b c d e f g
        int semi = base[first - 'a'];
        int i = 1;
        if (i < text.length() && (text[i] == '#' || text[i] == 's')) { ++semi; ++i; }
        else if (i < text.length() && text[i] == 'b') { --semi; ++i; }
        const auto rest = text.substring (i);
        const bool hasOct = rest.containsAnyOf ("0123456789");
        const int oct = hasOct ? rest.retainCharacters ("-0123456789").getIntValue() : 5;
        float hz = 440.0f * std::pow (2.0f, (float) ((oct + 1) * 12 + semi - 69) / 12.0f);
        // in den Bereich oktavieren (z. B. "A1" -> A5)
        while (hz < lo * 0.999f) hz *= 2.0f;
        while (hz > hi * 1.001f) hz *= 0.5f;
        if (hz < lo * 0.999f) hz *= 2.0f; // passt nicht ganz -> lieber oberhalb
        return juce::jlimit (lo, hi, hz);
    }
    float v = text.retainCharacters ("0123456789.").getFloatValue();
    if (text.contains ("k")) v *= 1000.0f;
    return juce::jlimit (lo, hi, v);
}

SnareHeadProcessor::SnareHeadProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SnareHead", createLayout())
{
    factory = factoryPresets();
    refreshUserPresets();
    loadPreset (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout SnareHeadProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto f = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def,
                  std::function<juce::String (float, int)> txt)
    {
        ps.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name, r, def, A().withStringFromValueFunction (txt)));
    };
    const Params d;
    auto fq = [&] (const char* id, const char* name, float def)
    {
        ps.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name, skewed (700.0f, 1600.0f, 1058.0f), def,
                                           A().withStringFromValueFunction (hz)
                                              .withValueFromStringFunction ([] (const juce::String& t) { return parseFreqText (t, 700.0f, 1600.0f); })));
    };
    fq (ParamIDs::tune, "Tune", d.tune);
    f (ParamIDs::spread,    "Spread",      { 0.0f, 1.0f }, d.spread, pct);
    f (ParamIDs::metal,     "Metal",       { 0.0f, 1.0f }, d.metal, pct);
    f (ParamIDs::ring,      "Ring Level",  { 0.0f, 1.0f }, d.ringLevel, pct);
    f (ParamIDs::punch,     "Punch",       { 0.0f, 1.0f }, d.punch, pct);
    f (ParamIDs::noise,     "Noise Level", { 0.0f, 1.0f }, d.noiseLevel, pct);
    f (ParamIDs::noiseTone, "Noise Tone",  { 0.0f, 1.0f }, d.noiseTone, pct);
    f (ParamIDs::hold,      "Hold",        skewed (10.0f, 800.0f, 150.0f), d.hold, ms);
    f (ParamIDs::release,   "Release",     skewed (2.0f, 600.0f, 60.0f), d.release, ms);
    fq (ParamIDs::eqFreq, "Res Freq", d.eqFreq);
    f (ParamIDs::eqGain,    "Res Gain",    { 0.0f, 18.0f }, d.eqGain, db);
    f (ParamIDs::eqQ,       "Res Q",       skewed (0.5f, 12.0f, 3.0f), d.eqQ,
       [] (float v, int) { return juce::String (v, 1); });
    ps.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::eqLink, 1 }, "Res Link", d.eqLink));
    f (ParamIDs::noisy,     "Xtreme Noisy", { 0.0f, 1.0f }, d.noisy, pct);
    f (ParamIDs::verbSize,  "Reverb Size", { 0.0f, 1.0f }, d.verbSize, pct);
    f (ParamIDs::verbMix,   "Reverb Mix",  { 0.0f, 1.0f }, d.verbMix, pct);
    f (ParamIDs::verbGate,  "Gate Time",   skewed (30.0f, 2000.0f, 300.0f), d.verbGate,
       [] (float v, int) { return v >= 1999.0f ? juce::String ("OFF") : ms (v, 0); });
    {
        juce::StringArray divs;
        for (auto* s : kSyncNames) divs.add (s);
        ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::verbSync, 1 }, "Gate Sync", divs, d.verbSync));
    }
    f (ParamIDs::verbCut,   "Gate Cut",    { 0.0f, 1.0f }, d.verbCut, pct);
    f (ParamIDs::width,     "Width",       { 0.0f, 1.0f }, d.width, pct);
    f (ParamIDs::volume,    "Volume",      { -24.0f, 6.0f, 0.1f }, d.volume, db);
    ps.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::keyTrack, 1 }, "Key Track", d.keyTrack));
    return { ps.begin(), ps.end() };
}

Params SnareHeadProcessor::readParams() const
{
    Params p;
    p.tune       = raw (ParamIDs::tune);
    p.spread     = raw (ParamIDs::spread);
    p.metal      = raw (ParamIDs::metal);
    p.ringLevel  = raw (ParamIDs::ring);
    p.punch      = raw (ParamIDs::punch);
    p.noiseLevel = raw (ParamIDs::noise);
    p.noiseTone  = raw (ParamIDs::noiseTone);
    p.hold       = raw (ParamIDs::hold);
    p.release    = raw (ParamIDs::release);
    p.eqFreq     = raw (ParamIDs::eqFreq);
    p.eqGain     = raw (ParamIDs::eqGain);
    p.eqQ        = raw (ParamIDs::eqQ);
    p.eqLink     = raw (ParamIDs::eqLink) > 0.5f;
    p.noisy      = raw (ParamIDs::noisy);
    p.verbSize   = raw (ParamIDs::verbSize);
    p.verbMix    = raw (ParamIDs::verbMix);
    p.verbGate   = raw (ParamIDs::verbGate);
    p.verbSync   = (int) std::round (raw (ParamIDs::verbSync));
    p.verbCut    = raw (ParamIDs::verbCut);
    p.width      = raw (ParamIDs::width);
    p.volume     = raw (ParamIDs::volume);
    p.keyTrack   = raw (ParamIDs::keyTrack) > 0.5f;
    return p;
}

void SnareHeadProcessor::setParamPlain (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

// ----------------------------------------------------------------- Audio

void SnareHeadProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setParams (readParams(), true);
    scratch.setSize (1, std::max (samplesPerBlock, 512), false, true, false);
    levelEnv = 0.0f;
}

bool SnareHeadProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SnareHeadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numOut = getTotalNumOutputChannels();
    buffer.clear();
    if (n <= 0 || numOut <= 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
            {
                engine.setTempo (*bpm);
                hostBpm.store ((float) *bpm);
            }

    const Params p = readParams();
    engine.setParams (p);

    if (scratch.getNumSamples() < n)
        scratch.setSize (1, n, false, true, true);
    float* L = buffer.getWritePointer (0);
    float* R = numOut > 1 ? buffer.getWritePointer (1) : scratch.getWritePointer (0);

    auto trigger = [&] (int note, float vel)
    {
        engine.noteOn (note, vel);
        float f0 = p.tune;
        if (p.keyTrack) f0 *= std::pow (2.0f, (float) (note - 60) / 12.0f);
        lastNoteHz.store (f0);
        lastVelocity.store (vel);
        hitCounter.fetch_add (1);
    };

    int pos = 0;
    const float aud = pendingAudition.exchange (-1.0f);
    if (aud > 0.0f) trigger (60, aud);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn()) continue;
        const int t = juce::jlimit (0, n, meta.samplePosition);
        if (t > pos) { engine.process (L + pos, R + pos, t - pos); pos = t; }
        trigger (m.getNoteNumber(), m.getFloatVelocity());
    }
    if (pos < n) engine.process (L + pos, R + pos, n - pos);

    if (numOut == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);

    // Pegel fuer die Anzeige
    float peak = 0.0f;
    for (int c = 0; c < numOut; ++c) peak = std::max (peak, buffer.getMagnitude (c, 0, n));
    levelEnv = std::max (peak, levelEnv * std::pow (0.5f, (float) n / (0.08f * (float) getSampleRate())));
    vizLevel.store (levelEnv);
    midi.clear();
}

// ----------------------------------------------------------------- State

void SnareHeadProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("presetIndex", currentPreset, nullptr);
    state.setProperty ("cleanPrint", cleanPrint, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SnareHeadProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    currentPreset = (int) state.getProperty ("presetIndex", -1);
    const auto print = state.getProperty ("cleanPrint", "").toString();
    state.removeProperty ("presetName", nullptr);
    state.removeProperty ("presetIndex", nullptr);
    state.removeProperty ("cleanPrint", nullptr);
    apvts.replaceState (state);
    cleanPrint = print.isNotEmpty() ? print : fingerprint();
}

// ----------------------------------------------------------------- Presets

juce::File SnareHeadProcessor::getPresetFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Brain Bugz").getChildFile ("Snare Head").getChildFile ("Presets");
}

void SnareHeadProcessor::refreshUserPresets()
{
    userPresets = getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.snarehead");
    std::sort (userPresets.begin(), userPresets.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });
}

juce::StringArray SnareHeadProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& fp : factory) names.add (fp.name);
    for (const auto& f : userPresets) names.add (f.getFileNameWithoutExtension());
    return names;
}

void SnareHeadProcessor::loadPreset (int index)
{
    const int numFactory = getNumFactoryPresets();
    if (index < 0 || index >= numFactory + userPresets.size()) return;

    if (index >= numFactory)
    {
        const auto file = userPresets[index - numFactory];
        if (auto xml = juce::XmlDocument::parse (file))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            if (state.hasType (apvts.state.getType()))
            {
                for (auto* param : getParameters())
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                    {
                        auto child = state.getChildWithProperty ("id", rp->getParameterID());
                        if (child.isValid())
                            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
                    }
                currentPreset = index;
                currentPresetName = file.getFileNameWithoutExtension();
                markClean();
            }
        }
        return;
    }

    const Params& p = factory[(size_t) index].p;
    setParamPlain (ParamIDs::tune, p.tune);
    setParamPlain (ParamIDs::spread, p.spread);
    setParamPlain (ParamIDs::metal, p.metal);
    setParamPlain (ParamIDs::ring, p.ringLevel);
    setParamPlain (ParamIDs::punch, p.punch);
    setParamPlain (ParamIDs::noise, p.noiseLevel);
    setParamPlain (ParamIDs::noiseTone, p.noiseTone);
    setParamPlain (ParamIDs::hold, p.hold);
    setParamPlain (ParamIDs::release, p.release);
    setParamPlain (ParamIDs::eqFreq, p.eqFreq);
    setParamPlain (ParamIDs::eqGain, p.eqGain);
    setParamPlain (ParamIDs::eqQ, p.eqQ);
    setParamPlain (ParamIDs::eqLink, p.eqLink ? 1.0f : 0.0f);
    setParamPlain (ParamIDs::noisy, p.noisy);
    setParamPlain (ParamIDs::verbSize, p.verbSize);
    setParamPlain (ParamIDs::verbMix, p.verbMix);
    setParamPlain (ParamIDs::verbGate, p.verbGate);
    setParamPlain (ParamIDs::verbSync, (float) p.verbSync);
    setParamPlain (ParamIDs::verbCut, p.verbCut);
    setParamPlain (ParamIDs::width, p.width);
    setParamPlain (ParamIDs::volume, p.volume);
    setParamPlain (ParamIDs::keyTrack, p.keyTrack ? 1.0f : 0.0f);
    currentPreset = index;
    currentPresetName = factory[(size_t) index].name;
    markClean();
}

bool SnareHeadProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getPresetFolder();
    if (! folder.createDirectory()) return false;
    const auto file = folder.getChildFile (clean + ".snarehead");
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr || ! xml->writeTo (file)) return false;
    refreshUserPresets();
    currentPreset = getNumFactoryPresets() + userPresets.indexOf (file);
    currentPresetName = clean;
    markClean();
    return true;
}

bool SnareHeadProcessor::deleteUserPreset (int index)
{
    const int u = index - getNumFactoryPresets();
    if (u < 0 || u >= userPresets.size()) return false;
    const bool ok = userPresets[u].moveToTrash() || userPresets[u].deleteFile();
    refreshUserPresets();
    if (ok && index == currentPreset) currentPreset = -1;
    return ok;
}

juce::String SnareHeadProcessor::fingerprint() const
{
    juce::String s;
    for (auto* param : getParameters())
        s << juce::String (param->getValue(), 4) << ";";
    return s;
}

bool SnareHeadProcessor::isPresetModified() const
{
    return fingerprint() != cleanPrint;
}

juce::AudioProcessorEditor* SnareHeadProcessor::createEditor()
{
    return new SnareHeadEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SnareHeadProcessor();
}
