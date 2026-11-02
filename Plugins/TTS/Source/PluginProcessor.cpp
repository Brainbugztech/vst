#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>

using namespace ttsparams;

// ================================================================ Echtzeit-Kette

void TtsChain::prepare (double rate, int maxBlock)
{
    hostRate = rate > 0 ? rate : 48000.0;
    grains.prepare (innerRate);
    fx.prepare (hostRate, juce::jmax (1, maxBlock));
    rsL.prepare (innerRate, hostRate);
    rsR.prepare (innerRate, hostRate);
    const int fifoSize = (int) std::ceil (1024.0 * innerRate / juce::jmin (hostRate, (double) innerRate)) + 512;
    fifoL.assign ((size_t) fifoSize, 0.0f);
    fifoR.assign ((size_t) fifoSize, 0.0f);
    mono.assign (64, 0.0f);
    gL.assign (64, 0.0f);
    gR.assign (64, 0.0f);
    clearFifo();
    volume.reset (hostRate, 0.03);
    grainMix.reset (innerRate, 0.03);
    dryGain.reset (innerRate, 0.05);
    dryGain.setCurrentAndTargetValue (1.0f);
}

void TtsChain::clearFifo()
{
    // Vorlauf für den Resampler
    std::fill (fifoL.begin(), fifoL.end(), 0.0f);
    std::fill (fifoR.begin(), fifoR.end(), 0.0f);
    fifoCount = tts::dsp::SincResampler::half;
    rsL.reset();
    rsR.reset();
}

void TtsChain::start (float semis, float velGain)
{
    noteSemis = semis;
    noteGain = velGain;
    voice.start();
}

void TtsChain::stop()
{
    voice.stop();
    grains.reset();
    clearFifo();
}

void TtsChain::renderInternal (int n, const Settings& s)
{
    const double pitch = std::pow (2.0, juce::jlimit (-36.0, 36.0, (double) (s.pitchSemis + noteSemis)) / 12.0);
    const bool frozen = s.grainOn && s.grain.freeze;
    grainMix.setTargetValue (s.grainOn ? s.grainMix : 0.0f);
    dryGain.setTargetValue (frozen ? 0.0f : 1.0f);  // bei Freeze nur die Körner hören
    const double speed = frozen ? 0.0 : (double) s.speed;
    const auto& src = voice.getSource();
    while (n > 0)
    {
        const int c = juce::jmin (n, 32);
        if (fifoCount + c > (int) fifoL.size()) break;
        const double head = voice.getPosition();
        const bool spawn = voice.isActive() && head < voice.getLength();
        voice.render (mono.data(), c, speed, pitch, (double) s.formant);
        if (std::abs (noteGain - 1.0f) > 1.0e-6f) for (int i = 0; i < c; ++i) mono[(size_t) i] *= noteGain;
        float* oL = fifoL.data() + fifoCount;
        float* oR = fifoR.data() + fifoCount;
        if (s.grainOn || grainMix.isSmoothing() || grains.isBusy())
        {
            grains.process (src != nullptr ? &src->raw : nullptr, head, speed, pitch, spawn && s.grainOn,
                            gL.data(), gR.data(), c, s.grain);
            for (int i = 0; i < c; ++i)
            {
                const float m = grainMix.getNextValue();
                const float d = mono[(size_t) i] * (1.0f - m) * dryGain.getNextValue();
                oL[i] = d + gL[(size_t) i] * m * noteGain;
                oR[i] = d + gR[(size_t) i] * m * noteGain;
            }
        }
        else
        {
            for (int i = 0; i < c; ++i) oL[i] = oR[i] = mono[(size_t) i] * dryGain.getNextValue();
            grainMix.skip (c);
        }
        fifoCount += c;
        n -= c;
    }
}

void TtsChain::process (float* L, float* R, int num, const Settings& s)
{
    // in Stücken, damit der Puffer nie überläuft
    int done = 0;
    while (done < num)
    {
        const int n = juce::jmin (num - done, 1024);
        const int need = rsL.inputNeeded (n);
        if (fifoCount < need) renderInternal (need - fifoCount, s);
        if (fifoCount < need) { std::fill (L + done, L + done + n, 0.0f); std::fill (R + done, R + done + n, 0.0f); done += n; continue; }
        rsR.process (fifoR.data(), R + done, n);
        const int drop = juce::jlimit (0, fifoCount, rsL.process (fifoL.data(), L + done, n));
        std::memmove (fifoL.data(), fifoL.data() + drop, sizeof (float) * (size_t) (fifoCount - drop));
        std::memmove (fifoR.data(), fifoR.data() + drop, sizeof (float) * (size_t) (fifoCount - drop));
        fifoCount -= drop;
        done += n;
    }

    volume.setTargetValue (juce::Decibels::decibelsToGain (s.volumeDb, -60.0f));
    for (int i = 0; i < num; ++i)
    {
        const float g = volume.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }
    fx.process (L, R, num, s.fx);
}

// ================================================================ Render-Thread

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

    void submit (const Job& job)
    {
        {
            const juce::ScopedLock sl (lock);
            next = std::make_unique<Job> (job);
        }
        cancel = true;
        wake.signal();
    }

    std::atomic<bool> cancel { false };

    void run() override
    {
        while (! threadShouldExit())
        {
            wake.wait (200);
            std::unique_ptr<Job> job;
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
    std::unique_ptr<Job> next;
    juce::WaitableEvent wake;
};

// ================================================================ Processor

TtsProcessor::TtsProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TTS", createLayout())
{
    renderThread = std::make_unique<RenderThread> (*this);
    refreshUserPresets();
    snapshotPreset();
    startTimer (60);
}

// ---------------------------------------------------------------- Presets

juce::File TtsProcessor::getPresetFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Brain Bugz").getChildFile ("Brainbugz TTS").getChildFile ("Presets");
}

void TtsProcessor::refreshUserPresets()
{
    userPresets = getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.bbtts");
    std::sort (userPresets.begin(), userPresets.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });
    presetList.clear();
    for (auto& f : ttspresets::factory()) presetList.push_back ({ f.name, f.group, false });
    for (auto& f : userPresets) presetList.push_back ({ f.getFileNameWithoutExtension(), "Eigene", true });
}

void TtsProcessor::snapshotPreset()
{
    presetSnapshot.clear();
    for (auto* p : getParameters()) presetSnapshot.push_back (p->getValue());
}

bool TtsProcessor::isPresetModified() const
{
    const auto& ps = getParameters();
    if (presetSnapshot.size() != (size_t) ps.size()) return false;
    for (int i = 0; i < ps.size(); ++i)
        if (std::abs (ps[i]->getValue() - presetSnapshot[(size_t) i]) > 1e-4f) return true;
    return false;
}

void TtsProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presetList.size()) return;
    std::map<juce::String, float> target; // normierte Werte
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) target[r->getParameterID()] = r->getDefaultValue();
    juce::String name;
    if (index < getNumFactoryPresets())
    {
        const auto& fp = ttspresets::factory()[(size_t) index];
        for (auto& [id, v] : fp.set)
            if (auto* r = apvts.getParameter (id)) target[id] = r->convertTo0to1 (v);
        name = fp.name;
    }
    else
    {
        const auto file = userPresets[index - getNumFactoryPresets()];
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr) return;
        auto st = juce::ValueTree::fromXml (*xml);
        if (! st.hasType (apvts.state.getType())) return;
        for (auto& [id, v] : target)
        {
            auto child = st.getChildWithProperty ("id", id);
            if (child.isValid())
                if (auto* r = apvts.getParameter (id)) v = r->convertTo0to1 ((float) child.getProperty ("value"));
        }
        name = file.getFileNameWithoutExtension();
    }
    for (auto& [id, v] : target)
        if (auto* r = apvts.getParameter (id))
            if (std::abs (r->getValue() - v) > 1e-7f)
            {
                r->beginChangeGesture();
                r->setValueNotifyingHost (v);
                r->endChangeGesture();
            }
    currentPreset = index;
    currentPresetName = name;
    snapshotPreset();
}

bool TtsProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getPresetFolder();
    if (! folder.createDirectory()) return false;
    const auto file = folder.getChildFile (clean + ".bbtts");
    auto st = apvts.copyState();
    auto xml = st.createXml();
    if (xml == nullptr || ! xml->writeTo (file)) return false;
    refreshUserPresets();
    currentPreset = getNumFactoryPresets() + userPresets.indexOf (file);
    currentPresetName = clean;
    snapshotPreset();
    return true;
}

bool TtsProcessor::deleteUserPreset (int index)
{
    const int u = index - getNumFactoryPresets();
    if (u < 0 || u >= userPresets.size()) return false;
    const bool ok = userPresets[u].moveToTrash() || userPresets[u].deleteFile();
    refreshUserPresets();
    if (ok && index == currentPreset) { currentPreset = -1; currentPresetName = "-"; }
    return ok;
}

TtsProcessor::~TtsProcessor()
{
    stopTimer();
    exportCancel = true;
    if (exportThread != nullptr) exportThread->stopThread (10000);
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
    chain.prepare (hostRate, samplesPerBlock);
    appliedVersion = -1; // Stimme neu übergeben
}

TtsChain::Settings TtsProcessor::readSettings (double bpm) const
{
    TtsChain::Settings s;
    s.pitchSemis = param (pitch);
    s.speed = param (speed);
    s.volumeDb = param (volume);
    s.formant = param (formant);

    s.grainOn = param (grOn) > 0.5f;
    s.grainMix = param (grMix) / 100.0f;
    s.grain.sizeMs = param (grSize);
    s.grain.flux = param (grFlux) / 100.0f;
    s.grain.density = param (grDensity);
    s.grain.sprayMs = param (grSpray);
    s.grain.pitchSpread = param (grPitch);
    s.grain.width = param (grWidth) / 100.0f;
    s.grain.reverse = param (grReverse) / 100.0f;
    s.grain.freeze = param (grFreeze) > 0.5f;
    s.grain.octave = param (grOctave) / 100.0f;
    s.grain.drift = param (grDrift) / 100.0f;
    s.grain.lfoRate = param (grLfoRate);
    s.grain.lfoDepth = param (grLfoDepth) / 100.0f;

    auto& f = s.fx;
    f.eqOn = param (eqOn) > 0.5f;
    for (int b = 0; b < 3; ++b)
    {
        f.band[b].on = param (eqId (b, "On")) > 0.5f;
        f.band[b].type = (tts::dsp::EqType) juce::roundToInt (param (eqId (b, "Type")));
        f.band[b].freq = param (eqId (b, "Freq"));
        f.band[b].gainDb = param (eqId (b, "Gain"));
        f.band[b].q = param (eqId (b, "Q"));
    }
    f.dlOn = param (dlOn) > 0.5f;
    const double b = (bpm > 20.0 && bpm < 400.0) ? bpm : 120.0;
    f.dlTimeMs = param (dlSync) > 0.5f ? (float) (delayDivBeats (juce::roundToInt (param (dlDiv))) * 60000.0 / b)
                                       : param (dlTime);
    f.dlFeedback = param (dlFeedback) / 100.0f;
    f.dlPing = param (dlPing) > 0.5f;
    f.dlLow = param (dlLow);
    f.dlHigh = param (dlHigh);
    f.dlMix = param (dlMix) / 100.0f;
    f.rvOn = param (rvOn) > 0.5f;
    f.rvSize = param (rvSize) / 100.0f;
    f.rvDamp = param (rvDamp) / 100.0f;
    f.rvPreMs = param (rvPre);
    f.rvWidth = param (rvWidth) / 100.0f;
    f.rvLow = param (rvLow);
    f.rvMix = param (rvMix) / 100.0f;
    return s;
}

// ---------------------------------------------------------------- Generate

void TtsProcessor::generate()
{
    generatedOnce = true;
    if (! holder->isReady()) { pendingRender = true; return; }
    Job job;
    {
        const juce::ScopedLock sl (stateLock);
        job.req.text = text;
        job.req.voiceA = voice;
        job.req.voiceB = blendVoice;
    }
    job.req.blend = blend.load();
    job.req.british = british.load();
    job.req.speed = 1.0f; // Tempo macht die Echtzeit-Engine
    if (job.req.text.trim().isEmpty()) return;
    rendering = true;
    renderProgress = 0.0f;
    renderThread->submit (job);
}

void TtsProcessor::runJob (const Job& job)
{
    rendering = true;
    {
        const juce::ScopedLock sl (stateLock);
        renderError.clear();
    }
    auto& cancel = renderThread->cancel;
    juce::String err;
    auto raw = holder->getEngine().synthesize (job.req, [this] (float f) { renderProgress = f * 0.85f; }, cancel, err);
    if (cancel.load()) { rendering = false; return; }
    if (raw.empty())
    {
        const juce::ScopedLock sl (stateLock);
        renderError = err;
        rendering = false;
        return;
    }
    // Pegel angleichen
    float peak = 0.0f;
    for (auto v : raw) peak = juce::jmax (peak, std::abs (v));
    if (peak > 1e-6f) for (auto& v : raw) v *= 0.8f / peak;
    auto analysis = tts::dsp::analyseVoice (raw, tts::KokoroEngine::sampleRate, &cancel);
    if (cancel.load() || analysis == nullptr) { rendering = false; return; }
    {
        const juce::SpinLock::ScopedLockType sl (sourceLock);
        if (current != nullptr) retired.push_back (current);
        current = analysis;
    }
    ++sourceVersion;
    renderProgress = 1.0f;
    rendering = false;
}

void TtsProcessor::timerCallback()
{
    if (pendingRender && holder->isReady())
    {
        pendingRender = false;
        generate();
    }
    // alte Stimmen erst freigeben, wenn der Audio-Thread umgeschaltet hat (oder gar nicht läuft)
    if (appliedVersion.load() == sourceVersion.load() || juce::Time::getMillisecondCounter() - lastProcessMs.load() > 1500)
    {
        std::vector<std::shared_ptr<const tts::dsp::VoiceAnalysis>> toFree;
        {
            const juce::SpinLock::ScopedLockType sl (sourceLock);
            toFree.swap (retired);
        }
    }
}

bool TtsProcessor::hasAudio() const
{
    const juce::SpinLock::ScopedLockType sl (sourceLock);
    return current != nullptr && current->length() > 0;
}

double TtsProcessor::getAudioSeconds() const
{
    const juce::SpinLock::ScopedLockType sl (sourceLock);
    if (current == nullptr) return 0.0;
    return (double) current->length() / tts::KokoroEngine::sampleRate / juce::jmax (0.001, (double) param (speed));
}

std::vector<float> TtsProcessor::getPeaks (int count) const
{
    std::shared_ptr<const tts::dsp::VoiceAnalysis> a;
    {
        const juce::SpinLock::ScopedLockType sl (sourceLock);
        a = current;
    }
    if (a == nullptr || a->peaks.empty() || count <= 0) return {};
    std::vector<float> out ((size_t) count, 0.0f);
    const size_t n = a->peaks.size();
    for (int i = 0; i < count; ++i)
    {
        const size_t s = n * (size_t) i / (size_t) count, e = juce::jmax (s + 1, n * (size_t) (i + 1) / (size_t) count);
        float m = 0.0f;
        for (size_t k = s; k < e && k < n; ++k) m = juce::jmax (m, a->peaks[k]);
        out[(size_t) i] = m;
    }
    return out;
}

// ---------------------------------------------------------------- Audio

void TtsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int num = buffer.getNumSamples();
    buffer.clear();
    lastProcessMs = juce::Time::getMillisecondCounter();
    if (work.getNumSamples() < num) work.setSize (2, num, false, false, true);

    // neue Stimme übernehmen
    if (appliedVersion.load() != sourceVersion.load())
    {
        const juce::SpinLock::ScopedTryLockType sl (sourceLock);
        if (sl.isLocked())
        {
            chain.setSource (current);
            appliedVersion = sourceVersion.load();
        }
    }

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    lastBpm = bpm;
    const auto s = readSettings (bpm);

    if (stopRequest.exchange (false)) chain.stop();
    if (startRequest.exchange (false) && chain.getSource() != nullptr) chain.start (0.0f, 1.0f);

    auto* L = work.getWritePointer (0);
    auto* R = work.getWritePointer (1);
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (pos, num, meta.samplePosition);
        if (at > pos) { chain.process (L + pos, R + pos, at - pos, s); pos = at; }
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && chain.getSource() != nullptr)
            chain.start ((float) (msg.getNoteNumber() - 60), 0.35f + 0.65f * msg.getFloatVelocity());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            chain.stop();
    }
    if (pos < num) chain.process (L + pos, R + pos, num - pos, s);

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

    voiceFlag = chain.voiceActive();
    playingFlag = chain.anyActive();
    playhead = chain.voiceActive() && chain.length() > 0 ? (float) (chain.position() / chain.length()) : 0.0f;
    float rms = 0.0f;
    for (int ch = 0; ch < juce::jmin (2, outCh); ++ch) rms = juce::jmax (rms, buffer.getRMSLevel (ch, 0, num));
    level = juce::jmax (rms * 2.2f, level.load() * 0.85f);
    chain.grainEngine().updateVis();
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

juce::String TtsProcessor::exportKey() const
{
    juce::String k (sourceVersion.load());
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) k << "|" << juce::String (r->getValue(), 5);
    return juce::String::toHexString (k.hashCode64());
}

namespace
{
struct LambdaThread : public juce::Thread
{
    explicit LambdaThread (std::function<void()> f) : juce::Thread ("BrainbugzTTS export"), fn (std::move (f)) {}
    void run() override { fn(); }
    std::function<void()> fn;
};
} // namespace

void TtsProcessor::startExport (const juce::File& file, const juce::String& key)
{
    if (exporting.load()) return;
    if (exportThread != nullptr) exportThread->stopThread (5000);
    exporting = true;
    exportFinished = false;
    exportCancel = false;
    exportProgress = 0.0f;
    exportFile = file;
    exportKeyStr = key;
    exportThread = std::make_unique<LambdaThread> ([this, file]
    {
        juce::String err;
        exportWav (file, err, &exportProgress, &exportCancel);
        {
            const juce::ScopedLock sl (stateLock);
            exportError = err;
        }
        exporting = false;
        exportFinished = true;
    });
    exportThread->startThread();
}

bool TtsProcessor::takeExportResult (juce::File& file, juce::String& key, juce::String& error)
{
    if (! exportFinished.exchange (false)) return false;
    const juce::ScopedLock sl (stateLock);
    file = exportFile;
    key = exportKeyStr;
    error = exportError;
    return true;
}

bool TtsProcessor::exportWav (const juce::File& file, juce::String& error, std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
    std::shared_ptr<const tts::dsp::VoiceAnalysis> src;
    {
        const juce::SpinLock::ScopedLockType sl (sourceLock);
        src = current;
    }
    if (src == nullptr || src->length() == 0) { error = "Nothing generated yet"; return false; }

    const double rate = hostRate > 0 ? hostRate : 48000.0;
    const int block = 2048;
    TtsChain offline;
    offline.prepare (rate, block);
    offline.setSource (src);
    offline.start (0.0f, 1.0f);
    const auto s = readSettings (lastBpm.load());

    const juce::int64 maxLen = (juce::int64) (rate * 60.0 * 10.0); // höchstens 10 Minuten
    const int tailMax = (int) (rate * 6.0);
    juce::AudioBuffer<float> out (2, (int) std::min (maxLen, (juce::int64) (rate * 20.0)));
    juce::int64 written = 0;
    int afterVoice = 0;
    std::vector<float> l ((size_t) block), r ((size_t) block);
    const double expected = juce::jmin ((double) maxLen, (double) src->length() / tts::KokoroEngine::sampleRate / juce::jmax (0.001, (double) s.speed) * rate);
    while (written < maxLen)
    {
        if (cancel != nullptr && cancel->load()) { error = "cancelled"; return false; }
        if (progress != nullptr) *progress = (float) juce::jlimit (0.0, 0.99, (double) written / juce::jmax (1.0, expected));
        offline.process (l.data(), r.data(), block, s);
        if (written + block > out.getNumSamples())
            out.setSize (2, (int) juce::jmin (maxLen, (juce::int64) out.getNumSamples() * 2), true, true);
        const int n = (int) std::min ((juce::int64) block, maxLen - written);
        out.copyFrom (0, (int) written, l.data(), n);
        out.copyFrom (1, (int) written, r.data(), n);
        written += n;
        // Freeze klingt ewig: im Export 10 s Textur, dann 1 s ausblenden
        if (s.grainOn && s.grain.freeze && written >= (juce::int64) (rate * 10.0))
        {
            const int fadeLen = (int) rate;
            if (written + fadeLen > out.getNumSamples()) out.setSize (2, (int) written + fadeLen, true, true);
            int done = 0;
            while (done < fadeLen)
            {
                offline.process (l.data(), r.data(), block, s);
                const int m = juce::jmin (block, fadeLen - done);
                for (int i = 0; i < m; ++i)
                {
                    const float g = 1.0f - (float) (done + i) / (float) fadeLen;
                    out.setSample (0, (int) written + done + i, l[(size_t) i] * g);
                    out.setSample (1, (int) written + done + i, r[(size_t) i] * g);
                }
                done += m;
            }
            written += fadeLen;
            break;
        }
        if (! offline.voiceActive())
        {
            afterVoice += block;
            // Freeze würde ewig klingen: dann nach 4 s Schluss
            if (afterVoice > tailMax) break;
            // Nachhall vorbei?
            float mx = 0.0f;
            for (int i = 0; i < n; ++i) mx = juce::jmax (mx, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]));
            if (afterVoice > (int) (rate * 0.3) && mx < 1.0e-4f && ! offline.anyActive()) break;
        }
    }
    // Stille am Ende kürzen
    int end = (int) written;
    while (end > 1 && std::abs (out.getSample (0, end - 1)) < 1.0e-4f && std::abs (out.getSample (1, end - 1)) < 1.0e-4f) --end;
    out.setSize (2, juce::jmax (1, end), true);
    const float peak = out.getMagnitude (0, out.getNumSamples());
    if (peak > 0.99f) out.applyGain (0.99f / peak);

    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (file.createOutputStream().release());
    if (os == nullptr) { error = "Cannot write the file"; return false; }
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) { error = "WAV error"; return false; }
    const bool ok = w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    if (progress != nullptr) *progress = 1.0f;
    return ok;
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
    st.setProperty ("tab", uiTab.load(), nullptr);
    st.setProperty ("presetName", currentPresetName, nullptr);
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
    uiTab = (int) st.getProperty ("tab", 0);
    currentPresetName = st.getProperty ("presetName", "Init").toString();
    currentPreset = -1;
    for (int i = 0; i < (int) presetList.size(); ++i)
        if (presetList[(size_t) i].name == currentPresetName) currentPreset = i;
    snapshotPreset();
    if ((bool) st.getProperty ("generated", false) && getText().trim().isNotEmpty())
    {
        generatedOnce = true;
        pendingRender = true;
    }
}

juce::AudioProcessorEditor* TtsProcessor::createEditor() { return new TtsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TtsProcessor(); }
