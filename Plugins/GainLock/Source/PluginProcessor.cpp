#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Schnell / Mittel / Langsam: Messfenster (100-ms-Blöcke) und Regelzeit (s)
    constexpr int   kWindowBlocks[3] = { 4, 30, 100 };
    constexpr float kTimeConst[3]    = { 0.3f, 1.2f, 4.0f };
    constexpr float kGateLufs        = -60.0f;
    constexpr int64_t kRingMask      = gl::SharedRing::kMask;
    constexpr int64_t kRingSize      = gl::SharedRing::kSize;

    inline float fastDbToGain (float db) noexcept { return std::exp (db * 0.11512925465f); }

    inline void storePeak (std::atomic<float>& a, float v) noexcept
    {
        if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed);
    }
}

GainLockProcessor::GainLockProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "BrainbugzGainLock", createLayout()),
      instanceId (gl::LinkHub::get().newInstanceId())
{
    pRole   = apvts.getRawParameterValue (ParamIDs::role);
    pGroup  = apvts.getRawParameterValue (ParamIDs::group);
    pSpeed  = apvts.getRawParameterValue (ParamIDs::speed);
    pMetric = apvts.getRawParameterValue (ParamIDs::metric);
    pTrim   = apvts.getRawParameterValue (ParamIDs::trim);
    pRange  = apvts.getRawParameterValue (ParamIDs::range);
    pHold   = apvts.getRawParameterValue (ParamIDs::hold);
    pListen = apvts.getRawParameterValue (ParamIDs::listen);

    lockMono.reset (new std::atomic<float>[(size_t) kRingSize]);
    for (int64_t i = 0; i < kRingSize; ++i) lockMono[(size_t) i].store (0.0f, std::memory_order_relaxed);

    gl::LinkHub::get().ensureRing (getGroupIndex());
    startTimerHz (5);
}

GainLockProcessor::~GainLockProcessor()
{
    stopTimer();
    stopAnalysis();
    releaseSourceSlot();
}

juce::AudioProcessorValueTreeState::ParameterLayout GainLockProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto db = [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; };

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::role, 1 }, "Rolle", StringArray { "Quelle", "Lock" }, 1));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamIDs::group, 1 }, "Gruppe", 1, gl::LinkHub::kNumGroups, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::speed, 1 }, "Tempo", StringArray { "Schnell", "Mittel", "Langsam" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::metric, 1 }, "Messung", StringArray { "LUFS", "RMS" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::trim, 1 }, "Trim", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (db)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::range, 1 }, "Bereich", NormalisableRange<float> (3.0f, 36.0f, 0.5f), 18.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
        {
            return String::fromUTF8 ("\xc2\xb1") + String (v, 1) + " dB";
        })));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamIDs::hold, 1 }, "Hold", false));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamIDs::listen, 1 }, "A/B Original", false));

    return layout;
}

int GainLockProcessor::getGroupIndex() const
{
    return juce::jlimit (0, gl::LinkHub::kNumGroups - 1, (int) std::lround (pGroup->load()) - 1);
}

bool GainLockProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

//==============================================================================
void GainLockProcessor::prepareToPlay (double sampleRate, int)
{
    stopAnalysis();

    const bool rateChanged = std::abs (sampleRate - sr) > 0.5;
    sr = sampleRate;
    refMeter.prepare (sr);
    postMeter.prepare (sr);
    outMeter.prepare (sr);
    const bool weighted = pMetric->load() < 0.5f;
    refMeter.setWeighted (weighted);
    postMeter.setWeighted (weighted);
    outMeter.setWeighted (weighted);

    gl::LinkHub::get().ensureRing (getGroupIndex());

    {
        std::lock_guard<std::mutex> l (finderLock);
        const int N = sr > 60000.0 ? 65536 : 32768;
        const int maxLag = (int) std::min ((int64_t) std::lround (sr), kRingSize / 4);
        finder = std::make_unique<gl::LatencyFinder> (N, maxLag);
    }
    if (rateChanged)
    {
        latencySamples.store (0);
        status.latencyKnown.store (false);
        status.latencyMs.store (0.0f);
    }

    staleBlocks = 0;
    lastRole = lastGroup = lastRef = lastMetric = -1;
    abMix = pListen->load() > 0.5f ? 1.0f : 0.0f;
    trimSmoothed = pTrim->load();

    startAnalysis();
}

void GainLockProcessor::releaseResources()
{
    stopAnalysis();
}

void GainLockProcessor::startAnalysis()
{
    if (analysis == nullptr) analysis = std::make_unique<AnalysisThread> (*this);
    analysis->startThread (juce::Thread::Priority::low);
}

void GainLockProcessor::stopAnalysis()
{
    if (analysis != nullptr) analysis->stopThread (2000);
}

void GainLockProcessor::releaseSourceSlot()
{
    if (ownedGroup >= 0)
    {
        int expected = instanceId;
        gl::LinkHub::get().slot (ownedGroup).owner.compare_exchange_strong (expected, 0);
        ownedGroup = -1;
    }
}

void GainLockProcessor::resetMeasurement()
{
    refMeter.reset();
    postMeter.reset();
    outMeter.reset();
    staleBlocks = 0;
}

//==============================================================================
void GainLockProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    if (hasRestoredGain.exchange (false))
        gc.currentDb = gc.targetDb = restoredGain.load();

    const int role = pRole->load() > 0.5f ? roleLock : roleSource;
    const int group = getGroupIndex();
    const int metric = pMetric->load() > 0.5f ? 1 : 0;

    if (role != lastRole || group != lastGroup)
    {
        releaseSourceSlot();
        resetMeasurement();
        status.latencyKnown.store (false);
        lastRole = role;
        lastGroup = group;
        lastHeartbeat = gl::LinkHub::get().slot (group).heartbeat.load();
    }
    if (metric != lastMetric)
    {
        refMeter.setWeighted (metric == 0);
        postMeter.setWeighted (metric == 0);
        outMeter.setWeighted (metric == 0);
        resetMeasurement();
        lastMetric = metric;
    }
    status.role.store (role);

    auto io = getBusBuffer (buffer, false, 0);
    if (n <= 0 || io.getNumChannels() == 0) return;

    if (role == roleSource) processSource (io, n);
    else                    processLock (buffer, n);
}

void GainLockProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Eine umgangene Quelle teilt ihr Signal trotzdem weiter (sie verändert ja nichts).
    if (pRole->load() < 0.5f)
    {
        processBlock (buffer, midi);
        return;
    }
    // Lock umgangen: unverändert durchreichen, Sidechain-Kanäle stören nicht
    for (int ch = getMainBusNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

void GainLockProcessor::processSource (juce::AudioBuffer<float>& io, int n)
{
    auto& slot = gl::LinkHub::get().slot (lastGroup);

    int expected = 0;
    const bool own = slot.owner.load() == instanceId
                  || slot.owner.compare_exchange_strong (expected, instanceId);
    if (own) ownedGroup = lastGroup;
    status.groupTaken.store (! own);

    const int numCh = std::min (2, io.getNumChannels());
    const float* L = io.getReadPointer (0);
    const float* R = numCh > 1 ? io.getReadPointer (1) : L;

    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float s[2] = { L[i], R[i] };
        if (refMeter.push (s, numCh))
            status.refLufs.store (refMeter.loudness (kWindowBlocks[0]));
        peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
    }
    storePeak (status.refPeak, peak);

    auto* ring = slot.ring.load (std::memory_order_acquire);
    if (own && ring != nullptr)
    {
        const int64_t w = slot.writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const size_t idx = (size_t) ((w + i) & kRingMask);
            ring->data[0][idx].store (L[i], std::memory_order_relaxed);
            ring->data[1][idx].store (R[i], std::memory_order_relaxed);
            ring->mono[idx].store (0.5f * (L[i] + R[i]), std::memory_order_relaxed);
        }
        slot.sampleRate.store (sr);
        slot.numChannels.store (numCh);
        slot.sourceLufs.store (status.refLufs.load());
        slot.writePos.store (w + n, std::memory_order_release);
        slot.heartbeat.fetch_add (1, std::memory_order_release);
    }

    // Hört ein Lock auf diese Gruppe? (Herzschlag ändert sich)
    const uint32_t lhb = slot.lockHeartbeat.load();
    if (lhb != lastHeartbeat) { lastHeartbeat = lhb; staleBlocks = 0; }
    else ++staleBlocks;
    status.lockSeen.store (own && staleBlocks < std::max (8, (int) (0.5 * sr / std::max (1, n))));
    status.refKind.store (refNone);
}

void GainLockProcessor::processLock (juce::AudioBuffer<float>& buffer, int n)
{
    auto io = getBusBuffer (buffer, false, 0);
    auto& slot = gl::LinkHub::get().slot (lastGroup);
    auto* ring = slot.ring.load (std::memory_order_acquire);
    const int owner = slot.owner.load();

    // Quelle aktiv? (Herzschlag muss sich bewegen)
    const uint32_t hb = slot.heartbeat.load (std::memory_order_acquire);
    if (hb != lastHeartbeat) { lastHeartbeat = hb; staleBlocks = 0; }
    else ++staleBlocks;
    const bool stale = staleBlocks > std::max (8, (int) (0.5 * sr / std::max (1, n)));
    const bool rateOk = std::abs (slot.sampleRate.load() - sr) < 1.0;
    const bool srcOk = owner != 0 && owner != instanceId && ring != nullptr && ! stale && rateOk
                       && slot.writePos.load (std::memory_order_acquire) >= n;

    // Sidechain als Ersatz-Referenz
    juce::AudioBuffer<float> sc;
    bool scOk = false;
    if (! srcOk && getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
        {
            sc = getBusBuffer (buffer, true, 1);
            scOk = sc.getNumChannels() > 0;
        }

    const int ref = srcOk ? refSource : (scOk ? refSidechain : refNone);
    if (ref != lastRef)
    {
        refMeter.reset();
        if (ref != refSource) status.latencyKnown.store (false);
        lastRef = ref;
    }
    status.refKind.store (ref);
    status.sourceStale.store (owner != 0 && stale);
    status.rateMismatch.store (owner != 0 && ! rateOk);
    status.groupTaken.store (false);
    status.lockSeen.store (false);

    const int64_t lockStart = lockWritePos.load (std::memory_order_relaxed);
    int64_t Ws = 0, D = 0;
    const int lat = status.latencyKnown.load() ? latencySamples.load() : 0;
    if (srcOk)
    {
        Ws = slot.writePos.load (std::memory_order_acquire);
        D = Ws - n - lockStart;
        offsetD.store (D);
        offsetValid.store (true);
        slot.lockHeartbeat.fetch_add (1);
    }
    else
    {
        offsetValid.store (false);
    }

    const int numCh = std::min (2, io.getNumChannels());
    float* o0 = io.getWritePointer (0);
    float* o1 = numCh > 1 ? io.getWritePointer (1) : o0;
    const int scCh = scOk ? std::min (2, sc.getNumChannels()) : 0;
    const float* s0 = scOk ? sc.getReadPointer (0) : nullptr;
    const float* s1 = scCh > 1 ? sc.getReadPointer (1) : s0;

    const int speed = juce::jlimit (0, 2, (int) std::lround (pSpeed->load()));
    const int win = kWindowBlocks[speed];
    const float range = pRange->load();
    const bool hold = pHold->load() > 0.5f;
    const float abTarget = (pListen->load() > 0.5f && ref != refNone) ? 1.0f : 0.0f;
    const float trimTarget = pTrim->load();

    const float coef = (float) std::exp (-1.0 / (kTimeConst[speed] * sr));
    const float abCoef = (float) std::exp (-1.0 / (0.03 * sr));
    const float trimCoef = (float) std::exp (-1.0 / (0.02 * sr));
    const int minBlocks = std::min (win, 4);
    const int refChannels = numCh;   // gleiche Kanalzahl wie die Kette, damit LUFS vergleichbar sind

    // Bereich verkleinert: Korrektur sofort mitbegrenzen
    gc.targetDb = juce::jlimit (-range, range, gc.targetDb);

    float refPeak = 0.0f, outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float in0 = o0[i];
        const float in1 = numCh > 1 ? o1[i] : in0;
        lockMono[(size_t) ((lockStart + i) & kRingMask)].store (numCh > 1 ? 0.5f * (in0 + in1) : in0,
                                                                std::memory_order_relaxed);

        float r0 = 0.0f, r1 = 0.0f;
        if (ref == refSource)
        {
            const int64_t idx = lockStart + i + D - lat;
            if (idx >= 0 && idx < Ws && idx > Ws - kRingSize + 8)
            {
                r0 = ring->data[0][(size_t) (idx & kRingMask)].load (std::memory_order_relaxed);
                r1 = ring->data[1][(size_t) (idx & kRingMask)].load (std::memory_order_relaxed);
            }
        }
        else if (ref == refSidechain)
        {
            r0 = s0[i];
            r1 = s1[i];
        }
        if (numCh == 1) r0 = r1 = 0.5f * (r0 + r1);

        const float post[2] = { in0, in1 };
        const bool blockDone = postMeter.push (post, numCh);
        if (ref != refNone)
        {
            const float rr[2] = { r0, r1 };
            refMeter.push (rr, refChannels);
            refPeak = std::max (refPeak, std::max (std::abs (r0), std::abs (r1)));
        }

        if (blockDone)
        {
            const float postL = postMeter.loudness (win);
            status.postLufs.store (postL);
            if (ref != refNone)
            {
                const float refL = refMeter.loudness (win);
                status.refLufs.store (refL);
                if (! hold && refMeter.blocksFilled() >= minBlocks && postMeter.blocksFilled() >= minBlocks)
                    gc.update (refL, postL, range, kGateLufs);
            }
            else
            {
                status.refLufs.store (-180.0f);
            }
        }

        gc.currentDb = gc.targetDb + (gc.currentDb - gc.targetDb) * coef;
        trimSmoothed = trimTarget + (trimSmoothed - trimTarget) * trimCoef;
        abMix = abTarget + (abMix - abTarget) * abCoef;

        const float g = fastDbToGain (gc.currentDb + trimSmoothed);
        const float a = abMix, b = 1.0f - abMix;
        const float y0 = in0 * g * b + r0 * a;
        const float y1 = in1 * g * b + r1 * a;

        const float outS[2] = { y0, y1 };
        if (outMeter.push (outS, numCh))
            status.outLufs.store (outMeter.loudness (win));

        o0[i] = y0;
        if (numCh > 1) o1[i] = y1;
        outPeak = std::max (outPeak, std::max (std::abs (y0), std::abs (y1)));
    }

    lockWritePos.store (lockStart + n, std::memory_order_release);

    // Kanäle, die nicht zum Hauptausgang gehören (Sidechain), nicht ausgeben
    for (int ch = getMainBusNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    status.gainDb.store (gc.currentDb + trimSmoothed);
    status.targetDb.store (gc.targetDb + trimTarget);
    storePeak (status.refPeak, refPeak);
    storePeak (status.outPeak, outPeak);
}

//==============================================================================
void GainLockProcessor::AnalysisThread::run()
{
    std::vector<float> src, lock;
    int candidate = -1, stable = 0;

    while (! threadShouldExit())
    {
        wait (400);
        if (threadShouldExit()) break;

        auto& p = owner;
        if (p.status.role.load() != roleLock || p.status.refKind.load() != refSource || ! p.offsetValid.load())
        {
            candidate = -1; stable = 0;
            continue;
        }

        std::lock_guard<std::mutex> l (p.finderLock);
        if (p.finder == nullptr) continue;

        auto& slot = gl::LinkHub::get().slot (p.getGroupIndex());
        auto* ring = slot.ring.load (std::memory_order_acquire);
        if (ring == nullptr) continue;

        const int N = p.finder->segmentLength();
        const int maxLag = p.finder->maxLag();

        int64_t pos = p.lockWritePos.load (std::memory_order_acquire);
        int64_t D = p.offsetD.load();
        if (p.lockWritePos.load (std::memory_order_acquire) != pos) continue;   // gerade geschrieben, nächstes Mal

        if (pos < N + 1) continue;
        const int64_t Ws = slot.writePos.load (std::memory_order_acquire);
        const int64_t sStart = pos - N + D - maxLag;
        const int64_t sEnd = pos + D;
        if (sStart < 0 || sStart < Ws - kRingSize + (int64_t) (p.sr * 2.0) || sEnd > Ws) continue;

        src.resize ((size_t) (N + maxLag));
        lock.resize ((size_t) N);
        for (int i = 0; i < N + maxLag; ++i)
            src[(size_t) i] = ring->mono[(size_t) ((sStart + i) & kRingMask)].load (std::memory_order_relaxed);
        for (int i = 0; i < N; ++i)
            lock[(size_t) i] = p.lockMono[(size_t) ((pos - N + i) & kRingMask)].load (std::memory_order_relaxed);

        const auto r = p.finder->analyse (src.data(), lock.data());
        if (! r.valid) continue;

        if (std::abs (r.lag - candidate) <= 1) ++stable;
        else { candidate = r.lag; stable = 1; }

        // zweimal hintereinander gleich, oder einmal sehr eindeutig
        if (stable >= 2 || (! p.status.latencyKnown.load() && r.confidence > 40.0f))
        {
            p.latencySamples.store (r.lag);
            p.status.latencyMs.store ((float) (r.lag * 1000.0 / p.sr));
            p.status.polarityFlip.store (r.inverted);
            p.status.latencyKnown.store (true);
        }
    }
}

//==============================================================================
void GainLockProcessor::timerCallback()
{
    const int group = getGroupIndex();
    auto& hub = gl::LinkHub::get();
    hub.ensureRing (group);

    if (pRole->load() < 0.5f && hub.slot (group).owner.load() == instanceId)
    {
        juce::String name;
        {
            const juce::SpinLock::ScopedLockType l (nameLock);
            name = trackName;
        }
        hub.slot (group).setTrackName (name.toRawUTF8());
    }
}

void GainLockProcessor::updateTrackProperties (const TrackProperties& properties)
{
    const juce::SpinLock::ScopedLockType l (nameLock);
    trackName = properties.name.value_or (juce::String()).substring (0, 60);
}

juce::String GainLockProcessor::getLinkedTrackName()
{
    char buf[64];
    gl::LinkHub::get().slot (getGroupIndex()).getTrackName (buf, sizeof (buf));
    return juce::String::fromUTF8 (buf);
}

//==============================================================================
void GainLockProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    // Korrektur ohne Trim merken, damit es nach dem Laden ohne Sprung weitergeht
    state.setProperty ("lastGain", (double) (status.gainDb.load() - pTrim->load()), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GainLockProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const float g = (float) (double) tree.getProperty ("lastGain", 0.0);
            tree.removeProperty ("lastGain", nullptr);
            apvts.replaceState (tree);
            restoredGain.store (juce::jlimit (-36.0f, 36.0f, std::isfinite (g) ? g : 0.0f));
            hasRestoredGain.store (true);
        }
}

juce::AudioProcessorEditor* GainLockProcessor::createEditor()
{
    return new GainLockEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GainLockProcessor();
}
