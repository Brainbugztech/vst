#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace orch;

// ---------------------------------------------------------------------------------------------
const std::vector<Preset>& orch::factoryPresets()
{
    static const std::vector<Preset> list =
    {
        // ---------------- Strings
        { "Epic Strings",          "STRINGS", { { "basses", 0.5f } } },
        { "Soft Strings",          "STRINGS", { { "attack", 0.5f }, { "release", 1.2f }, { "tone", -0.35f }, { "reverb", 0.4f }, { "space", 0.75f } } },
        { "Legato Lines",          "STRINGS", { { "legato", 1 }, { "glide", 0.15f }, { "attack", 0.15f }, { "release", 0.8f } } },
        { "Marcato Power",         "STRINGS", { { "artic", A_MARC }, { "basses", 0.6f }, { "tone", 0.25f }, { "drive", 0.2f } } },
        { "Spiccato Runs",         "STRINGS", { { "artic", A_SPIC }, { "humanize", 0.35f }, { "reverb", 0.22f }, { "space", 0.5f } } },
        { "Pizzicato Hall",        "STRINGS", { { "artic", A_PIZZ } } },
        { "Tremolo Tension",       "STRINGS", { { "artic", A_TREM }, { "tone", 0.1f }, { "reverb", 0.4f }, { "space", 0.8f }, { "width", 1.3f } } },
        { "Swell Rise",            "STRINGS", { { "artic", A_SWELL }, { "attack", 0.8f }, { "release", 1.5f }, { "reverb", 0.45f } } },
        { "Ostinato 16ths",        "STRINGS", { { "rhyMode", R_STRIKE }, { "rhyPattern", 0 }, { "rhyRate", 2 }, { "basses", 0.4f }, { "reverb", 0.25f } } },
        { "Gallop Action",         "STRINGS", { { "rhyMode", R_STRIKE }, { "rhyPattern", 1 }, { "rhyRate", 2 }, { "basses", 0.6f }, { "drive", 0.15f } } },
        { "Trance Gate Strings",   "STRINGS", { { "rhyMode", R_GATE }, { "rhyPattern", 7 }, { "rhyRate", 2 }, { "reverb", 0.4f } } },
        { "Heartbeat Pizz",        "STRINGS", { { "artic", A_PIZZ }, { "rhyMode", R_STRIKE }, { "rhyPattern", 5 }, { "rhyRate", 1 } } },
        // ---------------- Cellos
        { "Cello Section",         "CELLOS",  { { "mode", M_CELLOS } } },
        { "Dark Cellos & Basses",  "CELLOS",  { { "mode", M_CELLOS }, { "basses", 0.8f }, { "tone", -0.5f }, { "attack", 0.35f }, { "release", 1.0f } } },
        { "Cello Legato",          "CELLOS",  { { "mode", M_CELLOS }, { "legato", 1 }, { "glide", 0.15f }, { "basses", 0.3f } } },
        { "Cello Ostinato",        "CELLOS",  { { "mode", M_CELLOS }, { "rhyMode", R_STRIKE }, { "rhyPattern", 3 }, { "rhyRate", 2 }, { "basses", 0.5f } } },
        { "Battle Drive",          "CELLOS",  { { "mode", M_CELLOS }, { "artic", A_MARC }, { "rhyMode", R_STRIKE }, { "rhyPattern", 4 }, { "rhyRate", 2 }, { "basses", 0.7f }, { "drive", 0.25f } } },
        { "Cello Marcato",         "CELLOS",  { { "mode", M_CELLOS }, { "artic", A_MARC }, { "basses", 0.5f } } },
        { "Cello Pizz Walk",       "CELLOS",  { { "mode", M_CELLOS }, { "artic", A_PIZZ }, { "basses", 0.3f } } },
        { "Cello Tremolo Storm",   "CELLOS",  { { "mode", M_CELLOS }, { "artic", A_TREM }, { "basses", 0.5f }, { "reverb", 0.4f } } },
        { "Cello Swells",          "CELLOS",  { { "mode", M_CELLOS }, { "artic", A_SWELL }, { "attack", 0.6f }, { "basses", 0.4f } } },
        // ---------------- Pads
        { "Dark Matter",           "PADS",    { { "mode", M_ATMOS }, { "padA", 8 }, { "padB", 0 }, { "morph", 0.3f }, { "evolve", 0.4f }, { "air", 0.45f }, { "shimmer", 0.15f },
                                                { "motion", 0.55f }, { "tone", -0.45f }, { "attack", 1.2f }, { "release", 3.0f }, { "reverb", 0.6f }, { "space", 0.9f }, { "width", 1.5f } } },
        { "Shimmer Sky",           "PADS",    { { "mode", M_ATMOS }, { "padA", 0 }, { "padB", 5 }, { "morph", 0.4f }, { "evolve", 0.5f }, { "shimmer", 0.6f }, { "air", 0.3f },
                                                { "tone", 0.25f }, { "reverb", 0.65f }, { "space", 0.95f }, { "width", 1.6f }, { "attack", 0.8f }, { "release", 2.5f } } },
        { "Horn Clouds",           "PADS",    { { "mode", M_ATMOS }, { "padA", 2 }, { "padB", 0 }, { "morph", 0.3f }, { "evolve", 0.35f }, { "reverb", 0.55f }, { "release", 2.0f } } },
        { "Brass Wall",            "PADS",    { { "mode", M_ATMOS }, { "padA", 3 }, { "padB", 2 }, { "morph", 0.5f }, { "evolve", 0.2f }, { "drive", 0.2f }, { "tone", 0.1f }, { "air", 0.1f } } },
        { "Golden Trumpets",       "PADS",    { { "mode", M_ATMOS }, { "padA", 4 }, { "padB", 2 }, { "morph", 0.3f }, { "shimmer", 0.15f }, { "air", 0.1f } } },
        { "Woodwind Dream",        "PADS",    { { "mode", M_ATMOS }, { "padA", 5 }, { "padB", 6 }, { "morph", 0.5f }, { "evolve", 0.6f }, { "shimmer", 0.3f } } },
        { "Cathedral Organ",       "PADS",    { { "mode", M_ATMOS }, { "padA", 7 }, { "padB", 0 }, { "morph", 0.2f }, { "evolve", 0.2f }, { "reverb", 0.7f }, { "space", 1.0f } } },
        { "Deep Space",            "PADS",    { { "mode", M_ATMOS }, { "padA", 8 }, { "padB", 9 }, { "morph", 0.5f }, { "evolve", 0.7f }, { "air", 0.5f }, { "tone", -0.3f }, { "motion", 0.7f } } },
        { "Pure Synth Pad",        "PADS",    { { "mode", M_ATMOS }, { "padA", 9 }, { "padB", 9 }, { "air", 0.4f }, { "shimmer", 0.2f } } },
        { "Pulsing Pad",           "PADS",    { { "mode", M_ATMOS }, { "padA", 0 }, { "padB", 2 }, { "rhyMode", R_GATE }, { "rhyPattern", 7 }, { "rhyRate", 2 } } },
        // ---------------- Drums
        { "Epic Drums",            "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_EPIC } } },
        { "Battle Taikos",         "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_EPIC }, { "stack", 0.7f }, { "punch", 0.7f }, { "sub", 0.5f }, { "decay", 0.7f }, { "reverb", 0.3f } } },
        { "Tight Drums",           "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_EPIC }, { "punch", 0.7f }, { "decay", 0.45f }, { "reverb", 0.18f }, { "space", 0.45f } } },
        { "Huge Hall Drums",       "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_EPIC }, { "reverb", 0.6f }, { "space", 1.0f }, { "width", 1.3f } } },
        { "World Percussion",      "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_PERC }, { "reverb", 0.25f }, { "space", 0.5f }, { "punch", 0.3f }, { "sub", 0.0f } } },
        { "Metal & FX",            "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_METAL }, { "reverb", 0.45f } } },
        { "Chromatic Timpani",     "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_TIMPANI }, { "reverb", 0.4f }, { "sub", 0.2f } } },
        { "Trailer Hits",          "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_TRAILER }, { "sub", 0.5f }, { "punch", 0.5f }, { "reverb", 0.45f } } },
        { "Trailer Monster",       "DRUMS",   { { "mode", M_DRUMS }, { "kit", K_TRAILER }, { "sub", 0.9f }, { "stack", 0.5f }, { "punch", 0.7f }, { "drive", 0.3f } } },
    };
    return list;
}

// ---------------------------------------------------------------------------------------------
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
    auto choice = [&] (const char* id, const char* name, StringArray items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    choice ("mode", "Engine", { "Strings", "Cellos", "Atmos Pads", "Drums" }, 0);
    StringArray arts; for (auto* a : articNames) arts.add (String (a).substring (0, 1) + String (a).substring (1).toLowerCase());
    choice ("artic", "Articulation", arts, 0);

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
    add ("drive",   "Drive",   { 0.0f, 1.0f }, 0.0f, pct);

    add ("ensemble","Ensemble",{ 0.0f, 1.0f }, 0.45f, pct);
    add ("humanize","Humanize",{ 0.0f, 1.0f }, 0.25f, pct);
    add ("basses",  "Basses",  { 0.0f, 1.0f }, 0.0f, pct);
    NormalisableRange<float> gl (0.01f, 1.0f); gl.setSkewForCentre (0.15f);
    add ("glide",   "Glide",   gl, 0.12f, sec);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "legato", 1 }, "Legato", false));

    StringArray srcs; for (auto& s : padSources) srcs.add (s.name);
    choice ("padA", "Pad Source A", srcs, 0);
    choice ("padB", "Pad Source B", srcs, 2);
    add ("morph",   "Morph",   { 0.0f, 1.0f }, 0.3f, pct);
    add ("evolve",  "Evolve",  { 0.0f, 1.0f }, 0.3f, pct);
    add ("air",     "Air",     { 0.0f, 1.0f }, 0.3f, pct);
    add ("shimmer", "Shimmer", { 0.0f, 1.0f }, 0.3f, pct);
    add ("motion",  "Motion",  { 0.0f, 1.0f }, 0.45f, pct);

    StringArray kits; for (auto* k : kitNames) kits.add (k);
    choice ("kit", "Drum Kit", kits, 0);
    add ("punch",   "Punch",   { 0.0f, 1.0f }, 0.45f, pct);
    add ("decay",   "Decay",   { 0.0f, 1.0f }, 1.0f, pct);
    add ("sub",     "Sub",     { 0.0f, 1.0f }, 0.3f, pct);
    add ("stack",   "Stack",   { 0.0f, 1.0f }, 0.0f, pct);

    choice ("rhyMode", "Rhythm Mode", { "Off", "Gate", "Strike" }, 0);
    StringArray pats; for (auto& pt : patterns) pats.add (pt.name);
    choice ("rhyPattern", "Rhythm Pattern", pats, 0);
    StringArray rates; for (auto* r : rateNames) rates.add (r);
    choice ("rhyRate", "Rhythm Rate", rates, 2);
    add ("swing",   "Swing",   { 0.0f, 1.0f }, 0.0f, pct);
    add ("rhyDepth","Rhythm Depth", { 0.0f, 1.0f }, 1.0f, pct);
    return { p.begin(), p.end() };
}

OrchestraProcessor::OrchestraProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrainbugzOrchestra", createLayout()),
      engine (*bank)
{
    currentPresetName = factoryPresets().front().name;
    timerCallback();
    startTimerHz (20);
}

OrchestraProcessor::~OrchestraProcessor()
{
    stopTimer();
    bank->removeUser (this);
}

void OrchestraProcessor::setParam (const char* id, float value)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

// Message-Thread: Samples anfordern, Keyswitch-Artikulation in den Parameter uebernehmen
void OrchestraProcessor::timerCallback()
{
    const int ks = engine.pendingArtic.exchange (-1);
    if (ks >= 0 && ks != (int) getParam ("artic")) setParam ("artic", (float) ks);

    auto w = Engine::groupsFor (readSettings());
    if (w != wanted) { wanted = w; bank->setWanted (this, wanted); }
    loading = ! bank->isLoaded (wanted);
}

Settings OrchestraProcessor::readSettings() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto i = [&f] (const char* id) { return (int) std::round (f (id)); };
    Settings s;
    s.mode = i ("mode"); s.artic = i ("artic");
    s.attack = f ("attack"); s.release = f ("release"); s.tone = f ("tone"); s.tune = f ("tune");
    s.reverb = f ("reverb"); s.space = f ("space"); s.width = f ("width"); s.volumeDb = f ("volume"); s.drive = f ("drive");
    s.ensemble = f ("ensemble"); s.humanize = f ("humanize"); s.basses = f ("basses"); s.glide = f ("glide"); s.legato = f ("legato") > 0.5f;
    s.padA = i ("padA"); s.padB = i ("padB"); s.morph = f ("morph"); s.evolve = f ("evolve");
    s.air = f ("air"); s.shimmer = f ("shimmer"); s.motion = f ("motion");
    s.kit = i ("kit"); s.punch = f ("punch"); s.decay = f ("decay"); s.sub = f ("sub"); s.stack = f ("stack");
    s.rhyMode = i ("rhyMode"); s.rhyPattern = i ("rhyPattern"); s.rhyRate = i ("rhyRate"); s.swing = f ("swing"); s.rhyDepth = f ("rhyDepth");
    return s;
}

void OrchestraProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
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

    const auto s = readSettings();
    const bool articChanged = s.artic != lastArticParam;
    lastArticParam = s.artic;
    engine.setSettings (s, articChanged);

    // Songtempo und -position fuer die Rhythm-Engine
    Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
            t.playing = pos->getIsPlaying();
        }
    engine.setTransport (t);

    // Bildschirm-Tastatur + Drum-Pad-Klicks einspeisen
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    const int pad = padNote.exchange (-1);
    if (pad >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, pad, 0.85f), 0);

    const int total = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int tt = juce::jlimit (0, total, meta.samplePosition);
        if (tt > pos) { engine.render (buffer, pos, tt - pos); pos = tt; }
        engine.handleMidi (meta.getMessage());
    }
    if (pos < total) engine.render (buffer, pos, total - pos);

    if (buffer.getNumChannels() == 1) buffer.applyGain (0.7071f);
}

// ---------------------------------------------------------------------------------------------
int OrchestraProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String OrchestraProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? factoryPresets()[(size_t) index].name : "";
}

void OrchestraProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentPreset = index;
    const auto& pr = factoryPresets()[(size_t) index];
    // erst alles auf Standard, dann die Abweichungen
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "volume") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& v : pr.values) setParam (v.id, v.value);
    currentPresetName = pr.name;
    ++presetChanged;
}

juce::File OrchestraProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Brain Bugz").getChildFile ("Orchestra").getChildFile ("Presets");
}

juce::Array<juce::File> OrchestraProcessor::getUserPresets() const
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.bbpreset");
    files.sort();
    return files;
}

bool OrchestraProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    userPresetFolder().createDirectory();
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        if (xml->writeTo (userPresetFolder().getChildFile (clean + ".bbpreset")))
        {
            currentPresetName = clean; ++presetChanged;
            return true;
        }
    return false;
}

bool OrchestraProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            for (auto* prm : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    for (int i = 0; i < state.getNumChildren(); ++i)
                    {
                        auto c = state.getChild (i);
                        if (c.getProperty ("id").toString() == rp->getParameterID())
                            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) c.getProperty ("value")));
                    }
            currentPresetName = file.getFileNameWithoutExtension();
            ++presetChanged;
            return true;
        }
    return false;
}

void OrchestraProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void OrchestraProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = state.getProperty ("preset", 0);
            currentPresetName = state.getProperty ("presetName", factoryPresets().front().name).toString();
            apvts.replaceState (state);
            ++presetChanged;
        }
}

juce::AudioProcessorEditor* OrchestraProcessor::createEditor() { return new OrchestraEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrchestraProcessor(); }
