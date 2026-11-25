#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace orch;

juce::AudioProcessorValueTreeState::ParameterLayout OrchestraProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto sec = [] (float v, int) { return v < 1.0f ? String (juce::roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; };
    auto add = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                    std::function<String (float, int)> fmt = nullptr)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                     AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Engine",
                 StringArray { "Strings", "Cellos", "Atmos Pads", "Drums" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "artic", 1 }, "Articulation",
                 StringArray { "Sustain", "Spiccato", "Pizzicato", "Tremolo" }, 0));

    NormalisableRange<float> att (0.0f, 4.0f); att.setSkewForCentre (0.4f);
    NormalisableRange<float> rel (0.02f, 8.0f); rel.setSkewForCentre (0.8f);
    add ("attack",  "Attack",  att, 0.08f, sec);
    add ("release", "Release", rel, 0.6f, sec);
    add ("tone",    "Tone",    { -1.0f, 1.0f }, 0.15f, [] (float v, int) { return String (juce::roundToInt (v * 100.0f)); });
    add ("tune",    "Tune",    { -12.0f, 12.0f, 0.01f }, 0.0f, [] (float v, int) { return String (v, 2) + " st"; });
    add ("reverb",  "Reverb",  { 0.0f, 1.0f }, 0.35f, pct);
    add ("space",   "Space",   { 0.0f, 1.0f }, 0.7f, pct);
    add ("width",   "Width",   { 0.0f, 2.0f }, 1.2f, pct);
    add ("volume",  "Volume",  { -36.0f, 6.0f, 0.1f }, -3.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    add ("ensemble","Ensemble",{ 0.0f, 1.0f }, 0.45f, pct);
    add ("humanize","Humanize",{ 0.0f, 1.0f }, 0.25f, pct);
    add ("air",     "Air",     { 0.0f, 1.0f }, 0.35f, pct);
    add ("shimmer", "Shimmer", { 0.0f, 1.0f }, 0.3f, pct);
    add ("motion",  "Motion",  { 0.0f, 1.0f }, 0.45f, pct);
    add ("punch",   "Punch",   { 0.0f, 1.0f }, 0.45f, pct);
    add ("decay",   "Decay",   { 0.0f, 1.0f }, 1.0f, pct);
    return { p.begin(), p.end() };
}

OrchestraProcessor::OrchestraProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzOrchestra", createLayout()),
      engine (*bank)
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pMode = raw ("mode"); pArtic = raw ("artic"); pAttack = raw ("attack"); pRelease = raw ("release");
    pTone = raw ("tone"); pTune = raw ("tune"); pReverb = raw ("reverb"); pSpace = raw ("space");
    pWidth = raw ("width"); pVolume = raw ("volume"); pEnsemble = raw ("ensemble"); pHumanize = raw ("humanize");
    pAir = raw ("air"); pShimmer = raw ("shimmer"); pMotion = raw ("motion"); pPunch = raw ("punch"); pDecay = raw ("decay");
    timerCallback();
    startTimerHz (15);
}

OrchestraProcessor::~OrchestraProcessor()
{
    stopTimer();
    bank->removeUser (this);
}

// Message-Thread: der Bank melden, welche Samples gebraucht werden
void OrchestraProcessor::timerCallback()
{
    auto w = Engine::groupsFor ((int) pMode->load(), (int) pArtic->load());
    if (w != wanted) { wanted = w; bank->setWanted (this, wanted); }
    loading = ! bank->isLoaded (wanted);
}

Settings OrchestraProcessor::readSettings() const
{
    Settings s;
    s.mode = (int) pMode->load(); s.artic = (int) pArtic->load();
    s.attack = pAttack->load(); s.release = pRelease->load();
    s.tone = pTone->load(); s.tune = pTune->load();
    s.reverb = pReverb->load(); s.space = pSpace->load(); s.width = pWidth->load(); s.volumeDb = pVolume->load();
    s.ensemble = pEnsemble->load(); s.humanize = pHumanize->load();
    s.air = pAir->load(); s.shimmer = pShimmer->load(); s.motion = pMotion->load();
    s.punch = pPunch->load(); s.decay = pDecay->load();
    return s;
}

void OrchestraProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

bool OrchestraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void OrchestraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    engine.setSettings (readSettings());
    const int pad = padNote.exchange (-1);
    if (pad >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, pad, 0.85f), 0);

    // in Teilbloecken bis zum naechsten MIDI-Event rendern (sampelgenau)
    const int total = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, total, meta.samplePosition);
        while (pos < t) { const int n = std::min (t - pos, 512); engine.render (buffer, pos, n); pos += n; }
        engine.handleMidi (meta.getMessage());
    }
    while (pos < total) { const int n = std::min (total - pos, 512); engine.render (buffer, pos, n); pos += n; }

    if (buffer.getNumChannels() == 1) buffer.applyGain (0.7071f);
}

int OrchestraProcessor::getNumPrograms() { return (int) std::size (factoryPresets); }

const juce::String OrchestraProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? factoryPresets[index].name : "";
}

void OrchestraProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentPreset = index;
    const auto& pr = factoryPresets[index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    set ("mode", (float) pr.mode); set ("artic", (float) pr.artic);
    set ("attack", pr.attack); set ("release", pr.release); set ("tone", pr.tone);
    set ("reverb", pr.reverb); set ("space", pr.space); set ("width", pr.width);
    if (pr.mode <= M_CELLOS) { set ("ensemble", pr.ensemble); set ("humanize", pr.humanize); }
    if (pr.mode == M_ATMOS) { set ("air", pr.air); set ("shimmer", pr.shimmer); set ("motion", pr.motion); }
    if (pr.mode == M_DRUMS) { set ("punch", pr.punch); set ("decay", pr.decay); }
    set ("tune", 0.0f);
    ++presetChanged;
}

void OrchestraProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void OrchestraProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = state.getProperty ("preset", 0);
            apvts.replaceState (state);
            ++presetChanged;
        }
}

juce::AudioProcessorEditor* OrchestraProcessor::createEditor() { return new OrchestraEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrchestraProcessor(); }
