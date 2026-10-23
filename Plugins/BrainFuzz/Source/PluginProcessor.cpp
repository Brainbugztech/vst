#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/PresetLevels.h"

using namespace bfz;

BrainfuzzProcessor::BrainfuzzProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "BrainbugzBrainfuzz", createLayout())
{
    const auto& specs = paramSpecs();
    for (int i = 0; i < kNumParams; ++i)
    {
        params[(size_t) i] = apvts.getParameter (specs[(size_t) i].id);
        raw[(size_t) i] = apvts.getRawParameterValue (specs[(size_t) i].id);
        jassert (params[(size_t) i] != nullptr && raw[(size_t) i] != nullptr);
    }
    factory = factoryPresetsRaw();
    refreshUserPresets();
    loadPreset (0);
    undo.clearUndoHistory();
}

// ----------------------------------------------------------------- Parameter

juce::String BrainfuzzProcessor::formatValue (const PSpec& s, float v)
{
    using juce::String;
    if (s.type == PType::boolean) return v > 0.5f ? "On" : "Off";
    if (s.type == PType::choice) return String (s.choices[(size_t) juce::jlimit (0, (int) s.choices.size() - 1, juce::roundToInt (v))]);
    const auto& u = s.unit;
    if (u == "Hz") return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (juce::roundToInt (v)) + " Hz";
    if (u == "dB") return (v > 0.04f ? "+" : "") + String (v, 1) + " dB";
    if (u == "%")  return String (juce::roundToInt (v)) + " %";
    if (u == "s")  return v < 1.0f ? String (juce::roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s";
    if (u == "ms") return String (v, v < 10.0f ? 1 : 0) + " ms";
    if (u == "bit") return String (v, 1) + " bit";
    if (u == "x")  return String (v, 1) + "x";
    return String (v, 2);
}

juce::AudioProcessorValueTreeState::ParameterLayout BrainfuzzProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : paramSpecs())
    {
        const ParameterID pid { s.id, 1 };
        if (s.type == PType::boolean)
            layout.add (std::make_unique<AudioParameterBool> (pid, s.name, s.def > 0.5f));
        else if (s.type == PType::choice)
        {
            StringArray ch;
            for (const auto& c : s.choices) ch.add (c);
            layout.add (std::make_unique<AudioParameterChoice> (pid, s.name, ch, (int) s.def));
        }
        else
        {
            NormalisableRange<float> r (s.min, s.max);
            if (s.step > 0.0f) r.interval = s.step;
            if (s.centre > s.min && s.centre < s.max) r.setSkewForCentre (s.centre);
            const PSpec spec = s;
            auto attr = AudioParameterFloatAttributes()
                            .withLabel (s.unit)
                            .withStringFromValueFunction ([spec] (float v, int) { return formatValue (spec, v); })
                            .withValueFromStringFunction ([spec] (const String& text)
                            {
                                auto t = text.trim().toLowerCase();
                                double mul = 1.0;
                                if (spec.unit == "Hz" && (t.containsChar ('k'))) mul = 1000.0;
                                if (spec.unit == "s" && t.endsWith ("ms")) mul = 0.001;
                                const double x = t.retainCharacters ("0123456789.,-+").replaceCharacter (',', '.').getDoubleValue() * mul;
                                return (float) jlimit ((double) spec.min, (double) spec.max, x);
                            });
            layout.add (std::make_unique<AudioParameterFloat> (pid, s.name, r, s.def, attr));
        }
    }
    return layout;
}

void BrainfuzzProcessor::setValue (int index, float plain)
{
    if (auto* p = params[(size_t) index])
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

// ----------------------------------------------------------------- Audio

void BrainfuzzProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = std::max (32, std::min (samplesPerBlock, 2048));
    engine.prepare (sampleRate, maxBlock);
    setLatencySamples (engine.getLatencySamples());
    monoTmp.assign ((size_t) maxBlock, 0.0f);
    scopeTmpL.assign ((size_t) maxBlock, 0.0f);
    scopeTmpR.assign ((size_t) maxBlock, 0.0f);
    sampleRateViz.store (sampleRate);
}

bool BrainfuzzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void BrainfuzzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 2 || n == 0) return;
    if (numIn == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    for (int i = 0; i < kNumParams; ++i) values[(size_t) i] = raw[(size_t) i]->load (std::memory_order_relaxed);
    const auto ep = toEngineParams (values.data());

    const float sr = (float) getSampleRate();
    const float kRel = std::exp (-1.0f / (0.25f * sr));

    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = std::min (maxBlock, n - start);
        float* ch[2] { buffer.getWritePointer (0) + start, buffer.getWritePointer (1) + start };

        // Eingangspegel
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < len; ++i)
            {
                const float a = std::abs (ch[c][i]);
                inEnv[c] = a > inEnv[c] ? a : inEnv[c] * kRel;
            }

        engine.process (ch, len, ep);

        // Anzeige-Daten
        const auto& inL = engine.inputSignal (0);
        const auto& inR = engine.inputSignal (1);
        for (int i = 0; i < len; ++i) monoTmp[(size_t) i] = 0.5f * (inL[(size_t) i] + inR[(size_t) i]);
        inSpec.push (monoTmp.data(), len);
        for (int b = 0; b < 4; ++b)
        {
            const auto& L = engine.bandOutput (b, 0);
            const auto& R = engine.bandOutput (b, 1);
            float e = bandEnv[(size_t) b];
            for (int i = 0; i < len; ++i)
            {
                const float m = 0.5f * (L[(size_t) i] + R[(size_t) i]);
                monoTmp[(size_t) i] = m;
                const float a = std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
                e = a > e ? a : e * kRel;
            }
            bandEnv[(size_t) b] = e;
            bandLevel[(size_t) b].store (e);
            bandSpec[(size_t) b].push (monoTmp.data(), len);
            scopeL[(size_t) b].push (L.data(), len, 2);
            scopeR[(size_t) b].push (R.data(), len, 2);
        }
        for (int i = 0; i < len; ++i) monoTmp[(size_t) i] = 0.5f * (ch[0][i] + ch[1][i]);
        outSpec.push (monoTmp.data(), len);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < len; ++i)
            {
                const float a = std::abs (ch[c][i]);
                outEnv[c] = a > outEnv[c] ? a : outEnv[c] * kRel;
            }
    }
    for (int c = 0; c < 2; ++c) { inPeak[c].store (inEnv[c]); outPeak[c].store (outEnv[c]); }
    limiterGain.store (engine.getLimiterGain());
}

juce::AudioProcessorEditor* BrainfuzzProcessor::createEditor()
{
    return new BrainfuzzEditor (*this);
}

// ----------------------------------------------------------------- State

void BrainfuzzProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("presetIndex", currentPreset, nullptr);
    state.setProperty ("cleanPrint", cleanPrint, nullptr);
    juce::String lk, tabs;
    for (auto l : locks) lk << (l ? "1" : "0");
    for (auto t : fxTab) tabs << juce::String (t);
    state.setProperty ("locks", lk, nullptr);
    state.setProperty ("fxTabs", tabs, nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    state.setProperty ("ioLink", ioLink, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BrainfuzzProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    currentPreset = (int) state.getProperty ("presetIndex", -1);
    const auto print = state.getProperty ("cleanPrint", "").toString();
    const auto lk = state.getProperty ("locks", "0000").toString();
    const auto tabs = state.getProperty ("fxTabs", "0000").toString();
    for (int i = 0; i < kNumLocks; ++i) locks[(size_t) i] = lk[i] == '1';
    for (int i = 0; i < 4; ++i) fxTab[(size_t) i] = juce::jlimit (0, 2, tabs.substring (i, i + 1).getIntValue());
    uiScale = juce::jlimit (0.6f, 2.0f, (float) state.getProperty ("uiScale", 1.0f));
    ioLink = (bool) state.getProperty ("ioLink", false);
    for (auto* key : { "presetName", "presetIndex", "cleanPrint", "locks", "fxTabs", "uiScale", "ioLink" })
        state.removeProperty (key, nullptr);
    apvts.replaceState (state);
    cleanPrint = print.isNotEmpty() ? print : fingerprint();
    undo.clearUndoHistory();
}

// ----------------------------------------------------------------- Presets

juce::File BrainfuzzProcessor::getPresetFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Brain Bugz").getChildFile ("Brainfuzz").getChildFile ("Presets");
}

void BrainfuzzProcessor::refreshUserPresets()
{
    userPresets = getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.bfz");
    std::sort (userPresets.begin(), userPresets.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });
    presetList.clear();
    for (const auto& f : factory) presetList.push_back ({ juce::String (f.name), juce::String (f.group), false });
    for (const auto& f : userPresets) presetList.push_back ({ f.getFileNameWithoutExtension(), "Eigene", true });
}

bool BrainfuzzProcessor::isLocked (int paramIndex) const
{
    const int g = paramSpecs()[(size_t) paramIndex].group;
    return (g == gInput && locks[lockInput]) || (g == gOutput && locks[lockOutput])
        || (g == gFx && locks[lockFx]) || (g == gTight && locks[lockTight]);
}

void BrainfuzzProcessor::loadPreset (int index)
{
    const int numFactory = getNumFactoryPresets();
    if (index < 0 || index >= (int) presetList.size()) return;

    std::vector<float> target = defaultValues();
    juce::String name;
    if (index < numFactory)
    {
        const auto& fp = factory[(size_t) index];
        for (const auto& [id, x] : fp.set)
        {
            const int k = paramIndex (id);
            if (k >= 0) target[(size_t) k] = x;
        }
        target[pOut] = presetLevelDb (fp.name);
        name = fp.name;
    }
    else
    {
        const auto file = userPresets[index - numFactory];
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr) return;
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType())) return;
        for (int i = 0; i < kNumParams; ++i)
        {
            auto child = state.getChildWithProperty ("id", juce::String (paramSpecs()[(size_t) i].id));
            if (child.isValid()) target[(size_t) i] = (float) child.getProperty ("value");
        }
        name = file.getFileNameWithoutExtension();
    }

    undo.beginNewTransaction ("Preset " + name);
    for (int i = 0; i < kNumParams; ++i)
    {
        if (isLocked (i) || i == pBypass) continue;
        auto* p = params[(size_t) i];
        const float nv = p->convertTo0to1 (target[(size_t) i]);
        if (std::abs (nv - p->getValue()) > 1e-7f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (nv);
            p->endChangeGesture();
        }
    }
    currentPreset = index;
    currentPresetName = name;
    markClean();
}

bool BrainfuzzProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getPresetFolder();
    if (! folder.createDirectory()) return false;
    const auto file = folder.getChildFile (clean + ".bfz");
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr || ! xml->writeTo (file)) return false;
    refreshUserPresets();
    currentPreset = getNumFactoryPresets() + userPresets.indexOf (file);
    currentPresetName = clean;
    markClean();
    return true;
}

bool BrainfuzzProcessor::deleteUserPreset (int index)
{
    const int u = index - getNumFactoryPresets();
    if (u < 0 || u >= userPresets.size()) return false;
    const bool ok = userPresets[u].moveToTrash() || userPresets[u].deleteFile();
    refreshUserPresets();
    if (ok && index == currentPreset) currentPreset = -1;
    return ok;
}

juce::String BrainfuzzProcessor::fingerprint() const
{
    juce::String s;
    for (int i = 0; i < kNumParams; ++i)
        if (i != pBypass)
            s <<juce::String (params[(size_t) i]->getValue(), 4) << ";";
    return s;
}

bool BrainfuzzProcessor::isPresetModified() const
{
    return fingerprint() != cleanPrint;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrainfuzzProcessor();
}
