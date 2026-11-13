#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Beats pro LFO-Zyklus für die Divisions 1/1 … 1/32
    constexpr std::array<double, 6> divisionBeats { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };

    double wrap01 (double x) { return x - std::floor (x); }

    // 0 Sine, 1 Triangle, 2 Saw, 3 Square; Ergebnis -1..1, Phase 0..1
    float waveform (int shape, double p)
    {
        switch (shape)
        {
            case 1:  return (float) (1.0 - 4.0 * std::abs (p - 0.5));
            case 2:  return (float) (2.0 * p - 1.0);
            case 3:  return p < 0.5 ? 1.0f : -1.0f;
            default: return (float) std::sin (2.0 * juce::MathConstants<double>::pi * p);
        }
    }

    // Felder: Name, Osc, Drive-Typ, Filter, LFO-Form, Division, Cutoff, Depth, Resonanz,
    //         Drive, Stereo, Attack, Decay, Sustain, Release
    // Osc: 0 Sine, 1 Triangle, 2 Saw, 3 Square · Drive: 0 Soft, 1 Transformer
    // Filter: 0 LP, 1 BP, 2 HP · Division: 0 1/1 … 5 1/32
    const std::array<BugWobbleProcessor::Preset, BugWobbleProcessor::numPresets> presets {{
        { "Transformer Growl Saw",  2, 1, 0, 0, 3, 180.0f, 4.5f, 0.6f,  0.70f, 0.25f, 0.005f, 0.20f, 0.90f, 0.15f },
        { "Square Sub Hammer",      3, 1, 0, 3, 2, 120.0f, 5.0f, 0.4f,  0.80f, 0.10f, 0.001f, 0.15f, 1.00f, 0.05f },
        { "Tri Warm Wub",           1, 0, 0, 0, 3, 220.0f, 3.0f, 0.5f,  0.40f, 0.15f, 0.010f, 0.30f, 0.80f, 0.25f },
        { "Sine Sub Pulse",         0, 0, 0, 0, 2,  80.0f, 2.0f, 0.2f,  0.20f, 0.00f, 0.010f, 0.30f, 1.00f, 0.20f },
        { "Reese Grind",            2, 1, 0, 0, 4, 150.0f, 4.0f, 0.7f,  0.85f, 0.20f, 0.020f, 0.40f, 0.90f, 0.30f },
        { "Bandpass Honk",          2, 0, 1, 2, 3, 400.0f, 3.5f, 0.8f,  0.50f, 0.10f, 0.003f, 0.10f, 0.70f, 0.10f },
        { "Highpass Chatter",       2, 0, 2, 0, 4, 600.0f, 2.5f, 0.5f,  0.30f, 0.00f, 0.001f, 0.05f, 0.60f, 0.05f },
        { "Dirty Dub Saw",          2, 1, 0, 1, 3, 200.0f, 4.0f, 0.6f,  0.90f, 0.35f, 0.005f, 0.25f, 0.80f, 0.15f },
        { "Half-Time Heavy",        3, 1, 0, 0, 1, 140.0f, 5.0f, 0.55f, 0.75f, 0.50f, 0.001f, 0.20f, 1.00f, 0.10f },
        { "Slow Tank Wobble",       2, 0, 0, 0, 1,  90.0f, 3.0f, 0.5f,  0.60f, 0.10f, 0.500f, 0.80f, 0.90f, 1.00f },
        { "Filter Sweeper",         2, 0, 0, 0, 0, 300.0f, 5.5f, 0.8f,  0.20f, 0.00f, 0.010f, 0.50f, 0.70f, 0.50f },
        { "Heavy Square Growl",     3, 1, 0, 2, 3, 160.0f, 4.5f, 0.7f,  0.65f, 0.60f, 0.002f, 0.20f, 0.85f, 0.12f },
        { "Mid Reese Bite",         2, 1, 1, 0, 4, 500.0f, 3.0f, 0.5f,  0.60f, 0.45f, 0.005f, 0.20f, 0.85f, 0.20f },
        { "Lo-Fi Transformer",      1, 1, 0, 3, 3, 110.0f, 3.5f, 0.4f,  0.50f, 0.80f, 0.010f, 0.30f, 0.70f, 0.25f },
        { "Wide Stereo Wob",        2, 0, 0, 0, 3, 200.0f, 4.0f, 0.5f,  0.10f, 0.50f, 0.005f, 0.20f, 0.90f, 0.20f },
        { "Crushed Saw Bass",       2, 1, 0, 0, 3, 130.0f, 4.0f, 0.6f,  0.95f, 0.00f, 0.001f, 0.10f, 0.90f, 0.05f },
        { "Deep Pulse Wub",         3, 0, 0, 0, 2, 100.0f, 2.5f, 0.3f,  0.50f, 0.00f, 0.001f, 0.10f, 1.00f, 0.10f },
        { "Fat Sync Monster",       2, 1, 0, 1, 3, 240.0f, 4.5f, 0.6f,  0.70f, 0.50f, 0.002f, 0.15f, 0.90f, 0.20f },
        { "Glass Bandpass",         1, 0, 1, 2, 4, 800.0f, 2.0f, 0.7f,  0.30f, 0.00f, 0.050f, 0.40f, 0.60f, 0.50f },
        { "Wobble Pad",             1, 0, 0, 0, 1, 260.0f, 2.5f, 0.4f,  0.20f, 0.10f, 0.800f, 1.00f, 0.80f, 1.50f }
    }};
}

BugWobbleProcessor::BugWobbleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BugWobbleProcessor::createLayout()
{
    using namespace juce;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Modus
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                          StringArray { "Effect", "Synth" }, 1));

    // Klangerzeuger (Synth-Modus)
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "osc", 1 }, "Osc",
                          StringArray { "Sine", "Triangle", "Saw", "Square" }, 2));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack (s)",
                          NormalisableRange<float> (0.001f, 2.0f, 0.0f, 0.3f), 0.005f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 }, "Decay (s)",
                          NormalisableRange<float> (0.001f, 3.0f, 0.0f, 0.3f), 0.2f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sustain", 1 }, "Sustain",
                          NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release (s)",
                          NormalisableRange<float> (0.001f, 3.0f, 0.0f, 0.3f), 0.15f));

    // LFO / Wobble
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rate", 1 }, "Rate (Hz)",
                          NormalisableRange<float> (0.05f, 20.0f, 0.0f, 0.3f), 2.0f));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Division",
                          StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 3));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Tempo Sync", true));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "LFO Shape",
                          StringArray { "Sine", "Triangle", "Saw", "Square" }, 0));

    // Filter
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "type", 1 }, "Filter",
                          StringArray { "Lowpass", "Bandpass", "Highpass" }, 0));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 }, "Cutoff (Hz)",
                          NormalisableRange<float> (40.0f, 8000.0f, 0.0f, 0.3f), 300.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "depth", 1 }, "Depth (oct)",
                          NormalisableRange<float> (0.0f, 6.0f, 0.01f), 3.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "resonance", 1 }, "Resonance",
                          NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    // Sättigung
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive",
                          NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "driveType", 1 }, "Drive Type",
                          StringArray { "Soft", "Transformer" }, 1));

    // Ausgang
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "stereo", 1 }, "Stereo",
                          NormalisableRange<float> (0.0f, 0.5f, 0.001f), 0.25f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                          NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output (dB)",
                          NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

bool BugWobbleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // Instrument-Hosts dürfen den Eingang abschalten
    const auto& in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == out;
}

void BugWobbleProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    lfoPhase = 0.0;
    oscPhase = 0.0;
    envLevel = 0.0f;
    envStage = EnvStage::idle;
    heldNotes.clear();
    currentNote = -1;
    channels = {};
}

// ---------- Preset ----------

void BugWobbleProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, numPresets))
        return;

    currentPreset = index;
    const auto& p = presets[(size_t) index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set ("mode",      1.0f);
    set ("osc",       (float) p.osc);
    set ("driveType", (float) p.driveType);
    set ("type",      (float) p.filterType);
    set ("shape",     (float) p.shape);
    set ("division",  (float) p.division);
    set ("sync",      1.0f);
    set ("cutoff",    p.cutoff);
    set ("depth",     p.depth);
    set ("resonance", p.resonance);
    set ("drive",     p.drive);
    set ("stereo",    p.stereo);
    set ("attack",    p.attack);
    set ("decay",     p.decay);
    set ("sustain",   p.sustain);
    set ("release",   p.release);
}

juce::StringArray BugWobbleProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : presets)
        names.add (p.name);
    return names;
}

// ---------- MIDI / Stimme ----------

void BugWobbleProcessor::setNoteFrequency (int note)
{
    currentNote = note;
    noteFreq = 440.0f * std::pow (2.0f, ((float) note - 69.0f) / 12.0f);
}

void BugWobbleProcessor::noteOn (int note, float vel)
{
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    heldNotes.push_back (note);

    setNoteFrequency (note);
    velocity = vel;
    envStage = EnvStage::attack; // Retrigger ab aktuellem Pegel, ohne Klick
}

void BugWobbleProcessor::noteOff (int note)
{
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());

    if (heldNotes.empty())
    {
        if (envStage != EnvStage::idle)
            envStage = EnvStage::release;
    }
    else if (note == currentNote)
    {
        // Legato: zur zuletzt noch gehaltenen Note zurück, ohne Neustart
        setNoteFrequency (heldNotes.back());
    }
}

void BugWobbleProcessor::allNotesOff()
{
    heldNotes.clear();
    if (envStage != EnvStage::idle)
        envStage = EnvStage::release;
}

float BugWobbleProcessor::renderVoice()
{
    switch (envStage)
    {
        case EnvStage::attack:
            envLevel += attackInc;
            if (envLevel >= 1.0f) { envLevel = 1.0f; envStage = EnvStage::decay; }
            break;

        case EnvStage::decay:
            envLevel -= decayInc;
            if (envLevel <= sustainLevel) { envLevel = sustainLevel; envStage = EnvStage::sustain; }
            break;

        case EnvStage::sustain:
            envLevel = sustainLevel;
            break;

        case EnvStage::release:
            envLevel -= releaseInc;
            if (envLevel <= 0.0f) { envLevel = 0.0f; envStage = EnvStage::idle; }
            break;

        case EnvStage::idle:
            envLevel = 0.0f;
            break;
    }

    const float sample = waveform (oscShape, oscPhase) * envLevel * velocity * 0.5f;
    oscPhase = wrap01 (oscPhase + (double) noteFreq / sampleRate);
    return sample;
}

// ---------- Audio ----------

void BugWobbleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    if (numChannels == 0 || numSamples == 0)
        return;

    // Parameter einmal pro Block lesen
    const bool  synthMode  = apvts.getRawParameterValue ("mode")->load() > 0.5f;
    const int   divIndex   = juce::jlimit (0, 5, (int) apvts.getRawParameterValue ("division")->load());
    const float rateHz     = apvts.getRawParameterValue ("rate")->load();
    const bool  sync       = apvts.getRawParameterValue ("sync")->load() > 0.5f;
    const int   lfoShape   = (int) apvts.getRawParameterValue ("shape")->load();
    const int   filterType = (int) apvts.getRawParameterValue ("type")->load();
    const float cutoffLow  = apvts.getRawParameterValue ("cutoff")->load();
    const float depthOct   = apvts.getRawParameterValue ("depth")->load();
    const float resonance  = apvts.getRawParameterValue ("resonance")->load();
    const float drive      = apvts.getRawParameterValue ("drive")->load();
    const int   driveType  = (int) apvts.getRawParameterValue ("driveType")->load();
    const double stereoOffset = apvts.getRawParameterValue ("stereo")->load();
    const float mix        = apvts.getRawParameterValue ("mix")->load();
    const float outGain    = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());

    // Synth-Parameter
    oscShape     = (int) apvts.getRawParameterValue ("osc")->load();
    sustainLevel = apvts.getRawParameterValue ("sustain")->load();
    const float attack  = juce::jmax (0.001f, apvts.getRawParameterValue ("attack")->load());
    const float decay   = juce::jmax (0.001f, apvts.getRawParameterValue ("decay")->load());
    const float release = juce::jmax (0.001f, apvts.getRawParameterValue ("release")->load());
    attackInc  = 1.0f / (attack * (float) sampleRate);
    decayInc   = (1.0f - sustainLevel) / (decay * (float) sampleRate);
    releaseInc = 1.0f / (release * (float) sampleRate);

    // LFO-Frequenz: Host-Tempo mit Division oder freier Wert in Hz
    double lfoFreq = rateHz;
    if (sync)
    {
        double bpm = 120.0;
        if (auto* playHead = getPlayHead())
            if (auto position = playHead->getPosition())
                if (auto hostBpm = position->getBpm())
                    bpm = *hostBpm;

        lfoFreq = (bpm / 60.0) / divisionBeats[(size_t) divIndex];
    }
    const double phaseInc = lfoFreq / sampleRate;

    // Resonanz 0..1 → Q 0.5..12.5; SVF-Dämpfung k = 1/Q
    const float k = (float) (1.0 / (0.5 + 12.0 * resonance));

    // Drive: bei 0 Durchreichen, bei 1 volle Sättigung
    const float driveShape = 1.0f + 4.0f * drive;
    const float driveNorm  = std::tanh (driveShape);
    const float driveBias  = std::tanh (driveShape * 0.15f); // Transformer: asymmetrischer Bias

    const float maxCutoff = (float) juce::jmin (20000.0, sampleRate * 0.45);

    float* data[2] = { buffer.getWritePointer (0),
                       numChannels > 1 ? buffer.getWritePointer (1) : nullptr };

    auto midiIt = midi.begin();
    const auto midiEnd = midi.end();

    for (int i = 0; i < numSamples; ++i)
    {
        // MIDI-Events an ihrer Sample-Position verarbeiten
        while (midiIt != midiEnd)
        {
            const auto meta = *midiIt;
            if (meta.samplePosition > i)
                break;

            const auto msg = meta.getMessage();
            if (msg.isNoteOn())            noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())      noteOff (msg.getNoteNumber());
            else if (msg.isAllNotesOff())  allNotesOff();

            ++midiIt;
        }

        const float voiceSample = synthMode ? renderVoice() : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& s = channels[(size_t) ch];

            // Rechter Kanal bekommt einen Phasenversatz für Breite
            const double phase = ch == 0 ? lfoPhase : wrap01 (lfoPhase + stereoOffset);

            // LFO auf 0..1, Cutoff wandert von cutoffLow bis cutoffLow * 2^depth
            const float lfoUni = 0.5f + 0.5f * waveform (lfoShape, phase);
            const float cutoff = juce::jlimit (20.0f, maxCutoff,
                                               cutoffLow * std::pow (2.0f, depthOct * lfoUni));

            // TPT-SVF-Koeffizienten (Simper)
            const float g  = (float) std::tan (juce::MathConstants<double>::pi * cutoff / sampleRate);
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;

            const float dry = synthMode ? voiceSample : data[ch][i];

            // Sättigung vor dem Filter, Blend mit dem Original
            float input = dry;
            if (drive > 0.0f)
            {
                const float shaped = driveType == 1
                    ? (std::tanh (driveShape * (dry + 0.15f)) - driveBias) / driveNorm
                    : std::tanh (driveShape * dry) / driveNorm;

                input = dry + drive * (shaped - dry);

                // Transformer-Modus: Gleichanteil durch den asymmetrischen Bias entfernen
                if (driveType == 1)
                {
                    const float dcOut = input - s.dcX1 + 0.995f * s.dcY1;
                    s.dcX1 = input;
                    s.dcY1 = dcOut;
                    input = dcOut;
                }
            }

            const float v3 = input - s.ic2;
            const float v1 = a1 * s.ic1 + a2 * v3;
            const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
            s.ic1 = 2.0f * v1 - s.ic1;
            s.ic2 = 2.0f * v2 - s.ic2;

            float wet = v2;                                       // Lowpass
            if (filterType == 1)      wet = v1;                   // Bandpass
            else if (filterType == 2) wet = input - k * v1 - v2;  // Highpass

            data[ch][i] = (dry * (1.0f - mix) + wet * mix) * outGain;
        }

        lfoPhase = wrap01 (lfoPhase + phaseInc);
    }
}

juce::AudioProcessorEditor* BugWobbleProcessor::createEditor()
{
    return new BugWobbleEditor (*this);
}

void BugWobbleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BugWobbleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Einstiegspunkt für das Plugin-Format
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BugWobbleProcessor();
}
