#include "PluginProcessor.h"
#include "PluginEditor.h"

WormholeProcessor::WormholeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WormholeState", createLayout())
{
    pMode = apvts.getRawParameterValue ("mode");
    pPitch = apvts.getRawParameterValue ("pitch");
    pFine = apvts.getRawParameterValue ("fine");
    pFormant = apvts.getRawParameterValue ("formant");
    pFormantLock = apvts.getRawParameterValue ("formantLock");
}

juce::AudioProcessorValueTreeState::ParameterLayout WormholeProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modus", StringArray { "Live", "Render" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pitch", 1 }, "Pitch",
        NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "fine", 1 }, "Fine",
        NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("ct")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "formant", 1 }, "Formant",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "formantLock", 1 }, "Formant Lock", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "attackLock", 1 }, "Attack Lock", true));
    // Tempo 0,1 % .. 400 %: untere 35 % des Reglers für den Extrembereich (0,1..25 %), Rest 25..400 %
    NormalisableRange<float> tempoRange (0.1f, 400.0f,
        [] (float, float, float x)
        {
            if (x < 0.35f) return 0.1f * std::pow (250.0f, x / 0.35f);
            return 25.0f * std::pow (16.0f, (x - 0.35f) / 0.65f);
        },
        [] (float, float, float v)
        {
            v = juce::jlimit (0.1f, 400.0f, v);
            if (v < 25.0f) return 0.35f * std::log (v / 0.1f) / std::log (250.0f);
            return 0.35f + 0.65f * std::log (v / 25.0f) / std::log (16.0f);
        },
        [] (float, float, float v)
        {
            v = juce::jlimit (0.1f, 400.0f, v);
            const float step = v < 1.0f ? 0.001f : v < 10.0f ? 0.01f : 0.1f;
            return std::round (v / step) * step;
        });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 2 }, "Tempo", tempoRange, 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));
    // Extrem-Modus: Fensterlänge der Textur, 0 = automatisch
    NormalisableRange<float> texRange (0.0f, 2.0f, 0.01f);
    texRange.setSkewForCentre (0.4f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "texture", 1 }, "Textur", texRange, 0.0f,
        AudioParameterFloatAttributes().withLabel ("s")
            .withStringFromValueFunction ([] (float v, int) { return v < 0.04f ? juce::String ("Auto") : juce::String (v, 2) + " s"; })));
    return { p.begin(), p.end() };
}

bool WormholeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void WormholeProcessor::prepareToPlay (double sampleRate, int)
{
    numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());
    shifter.prepare (sampleRate, numCh);
    latency = shifter.getLatency();
    setLatencySamples (latency);
    delayBuf.setSize (numCh, latency + 1);
    delayBuf.clear();
    delayPos = 0;
    lastSemis = 1e9f;
    captureRate = sampleRate;
    preview.prepare (sampleRate, numCh);
}

void WormholeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (numCh, buffer.getNumChannels());
    for (int i = ch; i < buffer.getNumChannels(); ++i) buffer.clear (i, 0, n);
    if (ch <= 0 || n <= 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) hostBpm = *bpm;

    // Aufnahme (vor der Bearbeitung, also das Original der Spur)
    if (capturing.load())
    {
        int pos = capturePos.load();
        const int todo = juce::jmin (n, captureBuf.getNumSamples() - pos);
        if (todo > 0)
        {
            for (int c = 0; c < captureBuf.getNumChannels(); ++c)
                captureBuf.copyFrom (c, pos, buffer, juce::jmin (c, ch - 1), 0, todo);
            capturePos = pos + todo;
        }
    }

    if (pMode->load() < 0.5f)
    {
        const float semis = pPitch->load() + pFine->load() * 0.01f;
        const float form = pFormant->load();
        const bool lock = pFormantLock->load() > 0.5f;
        if (semis != lastSemis || form != lastFormant || lock != lastLock)
        {
            shifter.setParams (semis, lock, form);
            lastSemis = semis; lastFormant = form; lastLock = lock;
        }
        if (ch == numCh)
            shifter.process (buffer.getArrayOfWritePointers(), n);   // nutzt die ersten numCh Kanäle
    }
    else
    {
        // Render-Modus: Vorschau ersetzt das Spursignal, solange sie läuft
        if (preview.readAudio (buffer, ch)) return;
        // sonst läuft die Spur trocken durch, mit derselben Latenz
        const int L = delayBuf.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < ch; ++c)
            {
                float* d = delayBuf.getWritePointer (c);
                float* io = buffer.getWritePointer (c);
                const float x = io[i];
                io[i] = d[delayPos];
                d[delayPos] = x;
            }
            if (++delayPos >= L) delayPos = 0;
        }
    }
}

// ---------------------------------------------------------------------------
wh::RenderSettings WormholeProcessor::currentRenderSettings() const
{
    wh::RenderSettings s;
    s.timeRatio = 100.0 / juce::jmax (1.0, (double) apvts.getRawParameterValue ("tempo")->load());
    s.pitchSemitones = apvts.getRawParameterValue ("pitch")->load() + apvts.getRawParameterValue ("fine")->load() * 0.01;
    s.formantSemitones = apvts.getRawParameterValue ("formant")->load();
    s.formantLock = apvts.getRawParameterValue ("formantLock")->load() > 0.5f;
    s.attackLock = apvts.getRawParameterValue ("attackLock")->load() > 0.5f;
    const float tex = apvts.getRawParameterValue ("texture")->load();
    s.textureSeconds = tex < 0.04f ? 0.0 : (double) tex;
    return s;
}

double WormholeProcessor::getTempoPercent() const { return apvts.getRawParameterValue ("tempo")->load(); }

void WormholeProcessor::setTempoPercent (double pct)
{
    if (auto* p = apvts.getParameter ("tempo"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0.1, 400.0, pct)));
        p->endChangeGesture();
    }
}

bool WormholeProcessor::startCapture()
{
    if (capturing.load()) return true;
    const int len = (int) (juce::jmax (8000.0, captureRate) * maxCaptureSeconds);
    captureBuf.setSize (numCh, len, false, true, false);
    captureBuf.clear();
    capturePos = 0;
    capturing = true;
    return true;
}

void WormholeProcessor::stopCapture()
{
    if (! capturing.load()) return;
    capturing = false;
    juce::Thread::sleep (30); // letzten Audio-Block abschließen lassen
    const int len = capturePos.load();
    juce::AudioBuffer<float> b (captureBuf.getNumChannels(), juce::jmax (1, len));
    b.clear();
    for (int c = 0; c < b.getNumChannels(); ++c) b.copyFrom (c, 0, captureBuf, c, 0, len);
    captureBuf.setSize (1, 1);
    worker.loadCaptured (std::move (b), captureRate);
}

// ---------------------------------------------------------------------------
void WormholeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    if (auto c = worker.getClip(); c != nullptr && c->file.existsAsFile())
        xml->setAttribute ("clipFile", c->file.getFullPathName());
    xml->setAttribute ("sourceBpm", sourceBpm.load());
    copyXmlToBinary (*xml, dest);
}

void WormholeProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    const juce::File f (xml->getStringAttribute ("clipFile"));
    sourceBpm = xml->getDoubleAttribute ("sourceBpm", 0.0);
    xml->removeAttribute ("clipFile");
    xml->removeAttribute ("sourceBpm");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    if (f.existsAsFile()) worker.loadFile (f);
}

juce::AudioProcessorEditor* WormholeProcessor::createEditor() { return new WormholeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WormholeProcessor(); }
