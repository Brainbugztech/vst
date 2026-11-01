#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

// ---------------------------------------------------------------- Render-Thread

class TtsProcessor::RenderThread : public juce::Thread
{
public:
    explicit RenderThread (TtsProcessor& p) : juce::Thread ("BrainbugzTTS render"), owner (p) { startThread(); }
    ~RenderThread() override
    {
        cancel = true;
        signalThreadShouldExit();
        wake.signal();
        stopThread (10000);
    }

    void submit (const RenderJob& job)
    {
        {
            const juce::ScopedLock sl (lock);
            next = std::make_unique<RenderJob> (job);
        }
        cancel = true; // laufenden Job abbrechen, der neue ist wichtiger
        wake.signal();
    }

    std::atomic<bool> cancel { false };

    void run() override
    {
        while (! threadShouldExit())
        {
            wake.wait (200);
            std::unique_ptr<RenderJob> job;
            {
                const juce::ScopedLock sl (lock);
                job = std::move (next);
            }
            if (job == nullptr) continue;
            cancel = false;
            owner.runJob (*job);
        }
    }

private:
    TtsProcessor& owner;
    juce::CriticalSection lock;
    std::unique_ptr<RenderJob> next;
    juce::WaitableEvent wake;
};

// ---------------------------------------------------------------- Parameter

namespace
{
float speedFrom01 (float n) { return n < 0.5f ? std::pow (10.0f, -3.0f + 6.0f * n) : std::pow (4.0f, (n - 0.5f) * 2.0f); }
float speedTo01 (float v)
{
    v = juce::jlimit (0.001f, 4.0f, v);
    return v < 1.0f ? (std::log10 (v) + 3.0f) / 6.0f : 0.5f + std::log (v) / std::log (4.0f) * 0.5f;
}
juce::String speedText (float v)
{
    if (v < 0.1f) return juce::String (v, 3) + " x";
    return juce::String (v, 2) + " x";
}
juce::String signedText (float v, int decimals, const char* unit)
{
    if (std::abs (v) < 0.5f * std::pow (10.0f, (float) -decimals)) return juce::String (0.0f, decimals) + unit;
    return (v > 0 ? "+" : "") + juce::String (v, decimals) + unit;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout TtsProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto txt = [] (std::function<String (float)> f)
    { return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); }); };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pPitch, 1 }, "Pitch", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                                       txt ([] (float v) { return signedText (v, 2, " st"); })));
    NormalisableRange<float> speedRange (0.001f, 4.0f,
        [] (float, float, float n) { return speedFrom01 (n); },
        [] (float, float, float v) { return speedTo01 (v); },
        [] (float, float, float v) { return juce::jlimit (0.001f, 4.0f, v); });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pSpeed, 1 }, "Speed", speedRange, 1.0f, txt (speedText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pVolume, 1 }, "Volume", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                                                       txt ([] (float v) { return signedText (v, 1, " dB"); })));
    NormalisableRange<float> formantRange (0.5f, 2.0f,
        [] (float, float, float n) { return std::pow (2.0f, (n - 0.5f) * 2.0f); },
        [] (float, float, float v) { return 0.5f + std::log2 (juce::jlimit (0.5f, 2.0f, v)) * 0.5f; },
        [] (float, float, float v) { return juce::jlimit (0.5f, 2.0f, v); });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pFormant, 1 }, "Formant", formantRange, 1.0f,
                                                       txt ([] (float v) { return juce::String (v, 2) + " x"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pReverb, 1 }, "Reverb", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f,
                                                       txt ([] (float v) { return juce::String (juce::roundToInt (v)) + "%"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pDelay, 1 }, "Delay", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 15.0f,
                                                       txt ([] (float v) { return juce::String (juce::roundToInt (v)) + "%"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pEq, 1 }, "EQ", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                       txt ([] (float v) { return signedText (v, 1, " dB"); })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { pFxOn, 1 }, "Effects", true));
    return { p.begin(), p.end() };
}

// ---------------------------------------------------------------- Lebenszyklus

TtsProcessor::TtsProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TTS", createLayout())
{
    for (auto* id : { pPitch, pSpeed, pFormant })
        apvts.addParameterListener (id, this);
    renderThread = std::make_unique<RenderThread> (*this);
    startTimer (60);
}

TtsProcessor::~TtsProcessor()
{
    stopTimer();
    for (auto* id : { pPitch, pSpeed, pFormant })
        apvts.removeParameterListener (id, this);
    renderThread.reset();
}

bool TtsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void TtsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate > 0 ? sampleRate : 48000.0;
    work.setSize (2, juce::jmax (1, samplesPerBlock), false, true, true);
    fx.prepare (hostRate, samplesPerBlock);
    gainSmooth.reset (hostRate, 0.03);
    gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (pVolume)));
}

void TtsProcessor::setVoice (const juce::String& v) { const juce::ScopedLock sl (stateLock); voice = v; }
void TtsProcessor::setBritish (bool b) { british = b; }
void TtsProcessor::setBlendVoice (const juce::String& v) { const juce::ScopedLock sl (stateLock); blendVoice = v; }
void TtsProcessor::setBlend (float b) { blend = juce::jlimit (0.0f, 1.0f, b); }

// ---------------------------------------------------------------- Rendern

tts::fx::VoiceParams TtsProcessor::currentVoiceParams() const
{
    tts::fx::VoiceParams p;
    p.pitchSemis = param (pPitch);
    p.formant = param (pFormant);
    p.speed = param (pSpeed);
    return p;
}

tts::SynthRequest TtsProcessor::currentRequest() const
{
    tts::SynthRequest r;
    {
        const juce::ScopedLock sl (stateLock);
        r.text = text;
        r.voiceA = voice;
        r.voiceB = blendVoice;
    }
    r.blend = blend.load();
    r.british = british.load();
    r.speed = tts::fx::kokoroSpeedFor (param (pSpeed));
    return r;
}

void TtsProcessor::generate()
{
    generatedOnce = true;
    if (! holder->isReady())
    {
        pendingRender = true; // sobald die Engine bereit ist
        return;
    }
    startJob (true);
}

void TtsProcessor::startJob (bool resynth)
{
    RenderJob job;
    job.req = currentRequest();
    job.params = currentVoiceParams();
    job.needsSynth = resynth;
    if (job.req.text.trim().isEmpty()) return;
    rendering = true;
    renderProgress = 0.0f;
    renderThread->submit (job);
}

void TtsProcessor::runJob (const RenderJob& job)
{
    rendering = true;
    {
        const juce::ScopedLock sl (stateLock);
        renderError.clear();
    }
    auto& cancel = renderThread->cancel;
    const juce::String key = job.req.text + "|" + job.req.voiceA + "|" + job.req.voiceB + "|" + juce::String (job.req.blend, 3)
                           + "|" + (job.req.british ? "gb" : "us") + "|" + juce::String (job.req.speed, 3);
    const bool synthNeeded = rawCache.empty() || key != rawKey;
    juce::String err;
    if (synthNeeded)
    {
        auto raw = holder->getEngine().synthesize (job.req, [this] (float f) { renderProgress = f * 0.8f; }, cancel, err);
        if (cancel.load()) { rendering = false; return; }
        if (raw.empty())
        {
            const juce::ScopedLock sl (stateLock);
            renderError = err;
            rendering = false;
            return;
        }
        rawCache = std::move (raw);
        rawKey = key;
    }
    const float base = synthNeeded ? 0.8f : 0.0f;
    auto processed = tts::fx::processVoice (rawCache, tts::KokoroEngine::sampleRate, job.params,
                                            [this, base] (float f) { renderProgress = base + (1.0f - base) * f; }, &cancel);
    if (cancel.load() || processed.empty()) { rendering = false; return; }

    auto fresh = std::make_shared<Rendered>();
    fresh->audio = std::move (processed);
    std::shared_ptr<Rendered> old;
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        old = std::move (current);
        current = std::move (fresh);
        readPos = 0.0;
        playing = false; // neue Version: nicht mitten im alten Abspielen weiterspielen
    }
    old.reset(); // großer Puffer wird hier freigegeben, nicht im Audio-Thread
    ++audioVersion;
    renderProgress = 1.0f;
    rendering = false;
}

void TtsProcessor::parameterChanged (const juce::String&, float)
{
    paramsDirtyAt = juce::Time::getMillisecondCounter();
}

void TtsProcessor::timerCallback()
{
    // Engine wurde bereit und es wartet ein Auftrag (z. B. Projekt geladen)
    if (pendingRender && holder->isReady())
    {
        pendingRender = false;
        startJob (true);
    }
    // Pitch/Speed/Formant geändert: kurz warten, dann neu rechnen
    const auto dirty = paramsDirtyAt.load();
    if (dirty != 0 && juce::Time::getMillisecondCounter() - dirty > 450)
    {
        paramsDirtyAt = 0;
        if (generatedOnce && holder->isReady() && getText().trim().isNotEmpty())
            startJob (true);
    }
}

bool TtsProcessor::hasAudio() const
{
    const juce::SpinLock::ScopedLockType sl (swapLock);
    return current != nullptr && ! current->audio.empty();
}

double TtsProcessor::getAudioSeconds() const
{
    const juce::SpinLock::ScopedLockType sl (swapLock);
    return current != nullptr ? (double) current->audio.size() / tts::KokoroEngine::sampleRate : 0.0;
}

std::vector<float> TtsProcessor::getPeaks (int count) const
{
    std::vector<float> peaks ((size_t) juce::jmax (1, count), 0.0f);
    std::shared_ptr<Rendered> r;
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        r = current;
    }
    if (r == nullptr || r->audio.empty()) return {};
    const size_t n = r->audio.size();
    for (int i = 0; i < count; ++i)
    {
        const size_t a = n * (size_t) i / (size_t) count, b = juce::jmax (a + 1, n * (size_t) (i + 1) / (size_t) count);
        float m = 0.0f;
        const size_t step = juce::jmax<size_t> (1, (b - a) / 512);
        for (size_t k = a; k < b && k < n; k += step) m = juce::jmax (m, std::abs (r->audio[k]));
        peaks[(size_t) i] = m;
    }
    return peaks;
}

// ---------------------------------------------------------------- Wiedergabe

void TtsProcessor::play()
{
    if (! hasAudio()) return;
    noteRatio = 1.0;
    noteGain = 1.0f;
    startRequest = true;
}

void TtsProcessor::stop() { stopRequest = true; }

void TtsProcessor::applyFx (juce::AudioBuffer<float>& b, int start, int num, double bpm)
{
    if (param (pFxOn) > 0.5f)
        fx.process (b, start, num, param (pEq), param (pDelay) / 100.0f, param (pReverb) / 100.0f, bpm);
}

void TtsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int num = buffer.getNumSamples();
    buffer.clear();
    if (work.getNumSamples() < num) work.setSize (2, num, false, false, true);
    work.clear();

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    if (stopRequest.exchange (false)) playing = false;
    if (startRequest.exchange (false)) { readPos = 0.0; playing = true; }

    gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (param (pVolume)));

    const juce::SpinLock::ScopedTryLockType lock (swapLock);
    const std::vector<float>* audio = (lock.isLocked() && current != nullptr) ? &current->audio : nullptr;

    // MIDI: jede Note startet die Stimme neu, Tonhöhe relativ zu C3 (60)
    int cursor = 0;
    auto renderTo = [&] (int end)
    {
        if (audio == nullptr || ! playing.load() || end <= cursor) { cursor = end; return; }
        const auto& a = *audio;
        const double n = (double) a.size();
        const double step = (double) tts::KokoroEngine::sampleRate / hostRate * noteRatio;
        auto* L = work.getWritePointer (0);
        auto* R = work.getWritePointer (1);
        for (int i = cursor; i < end; ++i)
        {
            if (readPos >= n - 1) { playing = false; break; }
            const int i1 = (int) readPos;
            const float fr = (float) (readPos - i1);
            const float y0 = a[(size_t) juce::jmax (0, i1 - 1)], y1 = a[(size_t) i1];
            const float y2 = a[(size_t) juce::jmin ((int) n - 1, i1 + 1)], y3 = a[(size_t) juce::jmin ((int) n - 1, i1 + 2)];
            // kubisch (Catmull-Rom)
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            const float s = ((c3 * fr + c2) * fr + c1) * fr + y1;
            L[i] = R[i] = s * noteGain;
            readPos += step;
        }
        cursor = end;
    };
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        renderTo (juce::jlimit (0, num, meta.samplePosition));
        if (msg.isNoteOn() && audio != nullptr)
        {
            noteRatio = std::pow (2.0, (msg.getNoteNumber() - 60) / 12.0);
            noteGain = 0.35f + 0.65f * msg.getFloatVelocity();
            readPos = 0.0;
            playing = true;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            playing = false;
    }
    renderTo (num);

    // Lautstärke + Effekte
    for (int i = 0; i < num; ++i)
    {
        const float g = gainSmooth.getNextValue();
        work.getWritePointer (0)[i] *= g;
        work.getWritePointer (1)[i] *= g;
    }
    applyFx (work, 0, num, bpm);

    const int outCh = buffer.getNumChannels();
    if (outCh >= 2)
    {
        buffer.copyFrom (0, 0, work, 0, 0, num);
        buffer.copyFrom (1, 0, work, 1, 0, num);
    }
    else if (outCh == 1)
    {
        buffer.copyFrom (0, 0, work, 0, 0, num);
        buffer.addFrom (0, 0, work, 1, 0, num);
        buffer.applyGain (0.5f);
    }

    // Anzeige
    if (audio != nullptr && ! audio->empty())
        playhead = playing.load() ? (float) (readPos / (double) audio->size()) : 0.0f;
    float rms = 0.0f;
    for (int ch = 0; ch < juce::jmin (2, outCh); ++ch) rms = juce::jmax (rms, buffer.getRMSLevel (ch, 0, num));
    level = juce::jmax (rms * 2.2f, level.load() * 0.85f);
}

// ---------------------------------------------------------------- Export

juce::String TtsProcessor::suggestedFileName() const
{
    auto words = juce::StringArray::fromTokens (getText().retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 "), " ", "");
    words.removeEmptyStrings();
    juce::String s;
    for (int i = 0; i < juce::jmin (5, words.size()); ++i) s << (i ? " " : "") << words[i];
    if (s.isEmpty()) s = "voice";
    auto* v = findVoice (getVoice());
    return "Brainbugz TTS - " + s + " (" + (v != nullptr ? v->name : "voice") + ").wav";
}

bool TtsProcessor::exportWav (const juce::File& file, juce::String& error)
{
    std::shared_ptr<Rendered> r;
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        r = current;
    }
    if (r == nullptr || r->audio.empty()) { error = "Nothing generated yet"; return false; }

    const double outRate = hostRate > 0 ? hostRate : 48000.0;
    auto up = tts::fx::resample (r->audio, (double) tts::KokoroEngine::sampleRate / outRate);
    const bool fxOn = param (pFxOn) > 0.5f;
    const int tail = fxOn && (param (pReverb) > 0.5f || param (pDelay) > 0.5f) ? (int) (outRate * 3.5) : 0;
    const int total = (int) up.size() + tail;
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    const float gain = juce::Decibels::decibelsToGain (param (pVolume));
    for (int i = 0; i < (int) up.size(); ++i)
        out.setSample (0, i, up[(size_t) i] * gain), out.setSample (1, i, up[(size_t) i] * gain);
    if (fxOn)
    {
        tts::PlaybackFx offline;
        offline.prepare (outRate, 4096);
        double bpm = 120.0;
        for (int pos = 0; pos < total; pos += 4096)
            offline.process (out, pos, juce::jmin (4096, total - pos), param (pEq), param (pDelay) / 100.0f, param (pReverb) / 100.0f, bpm);
        // Ende des Nachhalls abschneiden, wenn er leise genug ist
        int end = total;
        while (end > (int) up.size() && std::abs (out.getSample (0, end - 1)) < 1.0e-4f && std::abs (out.getSample (1, end - 1)) < 1.0e-4f) --end;
        out.setSize (2, end, true);
    }
    // nicht übersteuern
    const float peak = out.getMagnitude (0, out.getNumSamples());
    if (peak > 0.99f) out.applyGain (0.99f / peak);

    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (file.createOutputStream().release());
    if (os == nullptr) { error = "Cannot write the file"; return false; }
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (outRate).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) { error = "WAV error"; return false; }
    return w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
}

// ---------------------------------------------------------------- Zustand

void TtsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("text", getText(), nullptr);
    st.setProperty ("voice", getVoice(), nullptr);
    st.setProperty ("british", british.load(), nullptr);
    st.setProperty ("blendVoice", getBlendVoice(), nullptr);
    st.setProperty ("blend", blend.load(), nullptr);
    st.setProperty ("generated", generatedOnce, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void TtsProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (! st.hasType (apvts.state.getType())) return;
    apvts.replaceState (st);
    {
        const juce::ScopedLock sl (stateLock);
        text = st.getProperty ("text").toString();
        voice = st.getProperty ("voice", "af_heart").toString();
        if (findVoice (voice) == nullptr) voice = "af_heart";
        blendVoice = st.getProperty ("blendVoice").toString();
    }
    british = (bool) st.getProperty ("british", false);
    blend = (float) st.getProperty ("blend", 0.0f);
    paramsDirtyAt = 0;
    // Stimme beim Laden des Projekts automatisch wieder erzeugen
    if ((bool) st.getProperty ("generated", false) && text.trim().isNotEmpty())
    {
        generatedOnce = true;
        pendingRender = true;
    }
}

juce::AudioProcessorEditor* TtsProcessor::createEditor() { return new TtsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TtsProcessor(); }
