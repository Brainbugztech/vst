#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Fast / Medium / Slow: Messfenster (100-ms-Blöcke) und Regelzeit (s)
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

    // Monitor-Quelle -> Abgriff (Original = Source, P1..P3 = Points), sonst -1
    inline int tapOfMonitor (int mon)
    {
        switch (mon)
        {
            case GainLockProcessor::monOriginal: return 0;
            case GainLockProcessor::monP1: return 1;
            case GainLockProcessor::monP2: return 2;
            case GainLockProcessor::monP3: return 3;
            default: return -1;
        }
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
    pRole    = apvts.getRawParameterValue (ParamIDs::role);
    pGroup   = apvts.getRawParameterValue (ParamIDs::group);
    pPoint   = apvts.getRawParameterValue (ParamIDs::point);
    pSpeed   = apvts.getRawParameterValue (ParamIDs::speed);
    pMetric  = apvts.getRawParameterValue (ParamIDs::metric);
    pTrim    = apvts.getRawParameterValue (ParamIDs::trim);
    pRange   = apvts.getRawParameterValue (ParamIDs::range);
    pHold    = apvts.getRawParameterValue (ParamIDs::hold);
    pMonitor = apvts.getRawParameterValue (ParamIDs::monitor);

    lockMono.reset (new std::atomic<float>[(size_t) kRingSize]);
    for (int64_t i = 0; i < kRingSize; ++i) lockMono[(size_t) i].store (0.0f, std::memory_order_relaxed);

    for (int t = 0; t < kNumTaps; ++t)
    {
        status.tapLinked[(size_t) t].store (false);
        status.tapLufs[(size_t) t].store (-180.0f);
        status.tapLatencyMs[(size_t) t].store (0.0f);
        status.tapLatencyKnown[(size_t) t].store (false);
        status.tapPolarity[(size_t) t].store (false);
        status.tapGainDb[(size_t) t].store (0.0f);
    }
    monMix[monFinal] = 1.0f;
    blindRng.setSeedRandomly();

    timerCallback();   // Ringe für die aktuelle Gruppe anlegen
    startTimerHz (5);
}

GainLockProcessor::~GainLockProcessor()
{
    stopTimer();
    stopAnalysis();
    releaseOwnedTap();
}

juce::AudioProcessorValueTreeState::ParameterLayout GainLockProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto db = [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; };

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::role, 1 }, "Role", StringArray { "Source", "Point", "Lock" }, 2));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamIDs::group, 1 }, "Group", 1, gl::LinkHub::kNumGroups, 1));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamIDs::point, 1 }, "Point Number", 1, kNumTaps - 1, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::speed, 1 }, "Speed", StringArray { "Fast", "Medium", "Slow" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::metric, 1 }, "Metering", StringArray { "LUFS", "RMS" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::trim, 1 }, "Trim", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (db)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::range, 1 }, "Range", NormalisableRange<float> (3.0f, 36.0f, 0.5f), 18.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
        {
            return String::fromUTF8 ("\xc2\xb1") + String (v, 1) + " dB";
        })));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamIDs::hold, 1 }, "Hold", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamIDs::monitor, 1 }, "Monitor",
        StringArray { "Final", "Original", "Point 1", "Point 2", "Point 3", "Delta" }, 0));

    return layout;
}

int GainLockProcessor::getGroupIndex() const
{
    return juce::jlimit (0, gl::LinkHub::kNumGroups - 1, (int) std::lround (pGroup->load()) - 1);
}

int GainLockProcessor::getRole() const
{
    return juce::jlimit (0, 2, (int) std::lround (pRole->load()));
}

bool GainLockProcessor::isMonitorAvailable (int mon) const
{
    if (mon == monFinal) return true;
    if (mon == monDelta) return status.tapLinked[0].load();
    const int t = tapOfMonitor (mon);
    return t >= 0 && status.tapLinked[(size_t) t].load();
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
    const bool weighted = pMetric->load() < 0.5f;
    for (auto* m : { &postMeter, &outMeter }) { m->prepare (sr); m->setWeighted (weighted); }
    for (auto& t : taps) { t.meter.prepare (sr); t.meter.setWeighted (weighted); }

    timerCallback();

    {
        std::lock_guard<std::mutex> l (finderLock);
        const int N = sr > 60000.0 ? 65536 : 32768;
        const int maxLag = (int) std::min ((int64_t) std::lround (sr), kRingSize / 4);
        finder = std::make_unique<gl::LatencyFinder> (N, maxLag);
    }
    if (rateChanged)
        for (int t = 0; t < kNumTaps; ++t)
        {
            taps[(size_t) t].latency.store (0);
            status.tapLatencyKnown[(size_t) t].store (false);
            status.tapLatencyMs[(size_t) t].store (0.0f);
        }

    lastRole = lastGroup = lastTapIdx = lastMetric = lastRef = -1;
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

void GainLockProcessor::releaseOwnedTap()
{
    if (ownedGroup >= 0 && ownedTap >= 0)
    {
        int expected = instanceId;
        gl::LinkHub::get().tap (ownedGroup, ownedTap).owner.compare_exchange_strong (expected, 0);
    }
    ownedGroup = ownedTap = -1;
}

void GainLockProcessor::resetMeasurement()
{
    postMeter.reset();
    outMeter.reset();
    for (auto& t : taps) { t.meter.reset(); t.stale = 0; t.wasOk = false; }
    lockStale = 0;
}

//==============================================================================
void GainLockProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    if (hasRestoredGain.exchange (false))
        gc.currentDb = gc.targetDb = restoredGain.load();

    const int role = getRole();
    const int group = getGroupIndex();
    const int tapIdx = role == roleSource ? 0 : (role == rolePoint ? juce::jlimit (1, kNumTaps - 1, (int) std::lround (pPoint->load())) : -1);
    const int metric = pMetric->load() > 0.5f ? 1 : 0;

    if (role != lastRole || group != lastGroup || tapIdx != lastTapIdx)
    {
        releaseOwnedTap();
        resetMeasurement();
        for (int t = 0; t < kNumTaps; ++t)
        {
            status.tapLatencyKnown[(size_t) t].store (false);
            status.tapLinked[(size_t) t].store (false);
        }
        lastRole = role; lastGroup = group; lastTapIdx = tapIdx;
    }
    if (metric != lastMetric)
    {
        for (auto* m : { &postMeter, &outMeter }) m->setWeighted (metric == 0);
        for (auto& t : taps) t.meter.setWeighted (metric == 0);
        resetMeasurement();
        lastMetric = metric;
    }
    status.role.store (role);

    auto io = getBusBuffer (buffer, false, 0);
    if (n <= 0 || io.getNumChannels() == 0) return;

    if (role == roleLock) processLock (buffer, n);
    else                  processTapRole (io, n, tapIdx);
}

void GainLockProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Source/Point umgangen: teilen trotzdem weiter (sie verändern ja nichts)
    if (getRole() != roleLock)
    {
        processBlock (buffer, midi);
        return;
    }
    for (int ch = getMainBusNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

void GainLockProcessor::processTapRole (juce::AudioBuffer<float>& io, int n, int tapIdx)
{
    auto& hub = gl::LinkHub::get();
    auto& tp = hub.tap (lastGroup, tapIdx);

    int expected = 0;
    const bool own = tp.owner.load() == instanceId || tp.owner.compare_exchange_strong (expected, instanceId);
    if (own) { ownedGroup = lastGroup; ownedTap = tapIdx; }
    status.slotTaken.store (! own);

    const int numCh = std::min (2, io.getNumChannels());
    const float* L = io.getReadPointer (0);
    const float* R = numCh > 1 ? io.getReadPointer (1) : L;

    auto& meter = taps[(size_t) tapIdx].meter;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float s[2] = { L[i], R[i] };
        if (meter.push (s, numCh))
            status.ownLufs.store (meter.loudness (kWindowBlocks[0]));
        peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
    }
    storePeak (status.refPeak, peak);

    auto* ring = tp.ring.load (std::memory_order_acquire);
    if (own && ring != nullptr)
    {
        const int64_t w = tp.writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const size_t idx = (size_t) ((w + i) & kRingMask);
            ring->data[0][idx].store (L[i], std::memory_order_relaxed);
            ring->data[1][idx].store (R[i], std::memory_order_relaxed);
            ring->mono[idx].store (0.5f * (L[i] + R[i]), std::memory_order_relaxed);
        }
        tp.sampleRate.store (sr);
        tp.numChannels.store (numCh);
        tp.writePos.store (w + n, std::memory_order_release);
        tp.heartbeat.fetch_add (1, std::memory_order_release);
    }

    // Hört ein Lock auf diese Gruppe?
    const uint32_t lhb = hub.slot (lastGroup).lockHeartbeat.load();
    if (lhb != lastLockHb) { lastLockHb = lhb; lockStale = 0; }
    else ++lockStale;
    status.lockSeen.store (own && lockStale < std::max (8, (int) (0.5 * sr / std::max (1, n))));
    status.refKind.store (refNone);
}

void GainLockProcessor::processLock (juce::AudioBuffer<float>& buffer, int n)
{
    auto io = getBusBuffer (buffer, false, 0);
    auto& hub = gl::LinkHub::get();
    auto& slot = hub.slot (lastGroup);
    const int staleLimit = std::max (8, (int) (0.5 * sr / std::max (1, n)));
    const int64_t lockStart = lockWritePos.load (std::memory_order_relaxed);

    // Welche Abgriffe sind gerade verwertbar?
    std::array<bool, kNumTaps> ok {}, fresh {};
    std::array<int64_t, kNumTaps> Ws {}, D {};
    std::array<int, kNumTaps> lat {};
    std::array<gl::SharedRing*, kNumTaps> rings {};
    bool anyStale = false, mismatch = false;
    for (int t = 0; t < kNumTaps; ++t)
    {
        auto& tp = slot.taps[(size_t) t];
        auto& st = taps[(size_t) t];
        const int owner = tp.owner.load();
        rings[(size_t) t] = tp.ring.load (std::memory_order_acquire);
        const uint32_t hb = tp.heartbeat.load (std::memory_order_acquire);
        if (hb != st.lastHb) { st.lastHb = hb; st.stale = 0; }
        else ++st.stale;
        const bool stale = st.stale > staleLimit;
        const bool rateOk = std::abs (tp.sampleRate.load() - sr) < 1.0;
        ok[(size_t) t] = owner != 0 && owner != instanceId && rings[(size_t) t] != nullptr && ! stale && rateOk
                         && tp.writePos.load (std::memory_order_acquire) >= n;
        if (t == 0) { anyStale = owner != 0 && stale; mismatch = owner != 0 && ! rateOk; }

        // frisch = der Abgriff hat seit unserem letzten Block neue Samples geschrieben.
        // Nur frische Daten gehen in die Messung, sonst würde der letzte Block mehrfach gelesen.
        fresh[(size_t) t] = ok[(size_t) t] && st.stale == 0;
        if (ok[(size_t) t] != st.wasOk)
        {
            st.meter.clearHistory();
            if (! ok[(size_t) t]) status.tapLatencyKnown[(size_t) t].store (false);
            st.wasOk = ok[(size_t) t];
        }
        if (ok[(size_t) t])
        {
            Ws[(size_t) t] = tp.writePos.load (std::memory_order_acquire);
            D[(size_t) t] = Ws[(size_t) t] - n - lockStart;
            st.offsetD.store (D[(size_t) t]);
            st.offsetValid.store (true);
            lat[(size_t) t] = status.tapLatencyKnown[(size_t) t].load() ? st.latency.load() : 0;
        }
        else
        {
            st.offsetValid.store (false);
        }
    }

    // Sidechain als Ersatz für die Source
    juce::AudioBuffer<float> sc;
    bool scOk = false;
    if (! ok[0] && getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
        {
            sc = getBusBuffer (buffer, true, 1);
            scOk = sc.getNumChannels() > 0;
        }

    const int ref = ok[0] ? refSource : (scOk ? refSidechain : refNone);
    // In diesem Block messen? Nur wenn die Referenz frisch ist (Sidechain ist es immer).
    // Sonst bleiben alle Vergleichsmesser stehen und damit untereinander synchron.
    const bool measure = ref == refSidechain || (ref == refSource && fresh[0]);
    for (int t = 1; t < kNumTaps; ++t)
    {
        auto& st = taps[(size_t) t];
        if (measure && fresh[(size_t) t] && ! st.wasFresh) st.meter.clearHistory();
        if (measure) st.wasFresh = fresh[(size_t) t];
    }
    if (ref != lastRef) { taps[0].meter.clearHistory(); lastRef = ref; }
    const bool refOn = ref != refNone;
    status.refKind.store (ref);
    status.sourceStale.store (anyStale);
    status.rateMismatch.store (mismatch);
    status.slotTaken.store (false);
    status.lockSeen.store (false);
    for (int t = 0; t < kNumTaps; ++t)
        status.tapLinked[(size_t) t].store (t == 0 ? refOn : ok[(size_t) t]);
    if (ok[0] || ok[1] || ok[2] || ok[3]) slot.lockHeartbeat.fetch_add (1);

    // Was soll zu hören sein? (Blindtest hat Vorrang; nicht verfügbare Quelle -> Final)
    int mon = blindSource.load();
    if (mon < 0) mon = juce::jlimit (0, kNumMon - 1, (int) std::lround (pMonitor->load()));
    {
        const int t = tapOfMonitor (mon);
        if ((t == 0 && ! refOn) || (t > 0 && ! ok[(size_t) t]) || (mon == monDelta && ! refOn)) mon = monFinal;
    }
    status.monitorActive.store (mon);

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
    const float trimTarget = pTrim->load();

    const float coef = (float) std::exp (-1.0 / (kTimeConst[speed] * sr));
    const float monCoef = (float) std::exp (-1.0 / (0.03 * sr));
    const float trimCoef = (float) std::exp (-1.0 / (0.02 * sr));
    const int minBlocks = std::min (win, 4);

    gc.targetDb = juce::jlimit (-range, range, gc.targetDb);
    for (int t = 1; t < kNumTaps; ++t)
        taps[(size_t) t].targetDb = juce::jlimit (-range, range, taps[(size_t) t].targetDb);

    float refPeak = 0.0f, outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float in0 = o0[i];
        const float in1 = numCh > 1 ? o1[i] : in0;
        lockMono[(size_t) ((lockStart + i) & kRingMask)].store (numCh > 1 ? 0.5f * (in0 + in1) : in0,
                                                                std::memory_order_relaxed);

        // Abgriffe zeitrichtig lesen
        float r[kNumTaps][2] = {};
        for (int t = 0; t < kNumTaps; ++t)
        {
            if (fresh[(size_t) t])
            {
                const int64_t idx = lockStart + i + D[(size_t) t] - lat[(size_t) t];
                if (idx >= 0 && idx < Ws[(size_t) t] && idx > Ws[(size_t) t] - kRingSize + 8)
                {
                    r[t][0] = rings[(size_t) t]->data[0][(size_t) (idx & kRingMask)].load (std::memory_order_relaxed);
                    r[t][1] = rings[(size_t) t]->data[1][(size_t) (idx & kRingMask)].load (std::memory_order_relaxed);
                }
            }
            else if (t == 0 && scOk)
            {
                r[0][0] = s0[i];
                r[0][1] = s1[i];
            }
            if (numCh == 1) r[t][0] = r[t][1] = 0.5f * (r[t][0] + r[t][1]);
        }

        const float post[2] = { in0, in1 };
        bool blockDone = false;
        if (measure)
        {
            blockDone = postMeter.push (post, numCh);
            for (int t = 0; t < kNumTaps; ++t)
                taps[(size_t) t].meter.push (r[t], numCh);   // alle zusammen, damit sie im selben Raster bleiben
        }
        if (refOn) refPeak = std::max (refPeak, std::max (std::abs (r[0][0]), std::abs (r[0][1])));

        if (blockDone)
        {
            // Vergleiche immer über exakt dieselbe Zeitspanne: gleiche Blockgrenzen, gleiche Länge.
            // Identisches Signal ergibt so exakt 0,0 dB.
            auto& refM = taps[0].meter;
            status.postLufs.store (postMeter.loudness (win));
            const int nRef = std::min ({ win, refM.blocksFilled(), postMeter.blocksFilled() });
            const float refL = refOn ? refM.loudness (nRef) : -180.0f;
            const float postL = postMeter.loudness (nRef);
            status.tapLufs[0].store (refOn ? refM.loudness (win) : -180.0f);
            if (refOn && ! hold && nRef >= minBlocks)
                gc.update (refL, postL, range, kGateLufs);
            // Zwischenpunkte ebenfalls auf Source-Lautheit bringen (gleiche Zeitspanne wie die Source)
            for (int t = 1; t < kNumTaps; ++t)
            {
                auto& st = taps[(size_t) t];
                status.tapLufs[(size_t) t].store (ok[(size_t) t] ? st.meter.loudness (win) : -180.0f);
                const int nT = std::min ({ win, refM.blocksFilled(), st.meter.blocksFilled() });
                if (! fresh[(size_t) t] || ! refOn || hold || nT < minBlocks) continue;
                const float l = st.meter.loudness (nT), rl = refM.loudness (nT);
                if (rl > kGateLufs && l > kGateLufs - 10.0f)
                    st.targetDb = juce::jlimit (-range, range, rl - l);
            }
        }

        gc.currentDb = gc.targetDb + (gc.currentDb - gc.targetDb) * coef;
        for (int t = 1; t < kNumTaps; ++t)
        {
            auto& st = taps[(size_t) t];
            st.gainDb = st.targetDb + (st.gainDb - st.targetDb) * coef;
        }
        trimSmoothed = trimTarget + (trimSmoothed - trimTarget) * trimCoef;

        // Monitor-Quellen
        const float g = fastDbToGain (gc.currentDb + trimSmoothed);
        float src[kNumMon][2];
        src[monFinal][0] = in0 * g;               src[monFinal][1] = in1 * g;
        src[monOriginal][0] = r[0][0];            src[monOriginal][1] = r[0][1];
        for (int t = 1; t < kNumTaps; ++t)
        {
            const float gt = fastDbToGain (taps[(size_t) t].gainDb);
            src[monP1 + t - 1][0] = r[t][0] * gt;
            src[monP1 + t - 1][1] = r[t][1] * gt;
        }
        src[monDelta][0] = src[monFinal][0] - r[0][0];
        src[monDelta][1] = src[monFinal][1] - r[0][1];

        float y0 = 0.0f, y1 = 0.0f;
        for (int m = 0; m < kNumMon; ++m)
        {
            const float target = m == mon ? 1.0f : 0.0f;
            monMix[(size_t) m] = target + (monMix[(size_t) m] - target) * monCoef;
            if (monMix[(size_t) m] > 1.0e-5f)
            {
                y0 += src[m][0] * monMix[(size_t) m];
                y1 += src[m][1] * monMix[(size_t) m];
            }
        }

        const float outS[2] = { y0, y1 };
        if (outMeter.push (outS, numCh))
            status.outLufs.store (outMeter.loudness (win));

        o0[i] = y0;
        if (numCh > 1) o1[i] = y1;
        outPeak = std::max (outPeak, std::max (std::abs (y0), std::abs (y1)));
    }

    lockWritePos.store (lockStart + n, std::memory_order_release);

    for (int ch = getMainBusNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    status.gainDb.store (gc.currentDb + trimSmoothed);
    status.targetDb.store (gc.targetDb + trimTarget);
    status.tapGainDb[0].store (0.0f);
    for (int t = 1; t < kNumTaps; ++t) status.tapGainDb[(size_t) t].store (taps[(size_t) t].gainDb);
    storePeak (status.refPeak, refPeak);
    storePeak (status.outPeak, outPeak);
}

//==============================================================================
void GainLockProcessor::AnalysisThread::run()
{
    std::vector<float> src, lock;
    std::array<int, kNumTaps> candidate, stable;
    candidate.fill (-1); stable.fill (0);

    while (! threadShouldExit())
    {
        wait (300);
        if (threadShouldExit()) break;

        auto& p = owner;
        if (p.status.role.load() != roleLock) { candidate.fill (-1); stable.fill (0); continue; }

        std::lock_guard<std::mutex> l (p.finderLock);
        if (p.finder == nullptr) continue;
        const int N = p.finder->segmentLength();
        const int maxLag = p.finder->maxLag();

        for (int t = 0; t < kNumTaps && ! threadShouldExit(); ++t)
        {
            auto& st = p.taps[(size_t) t];
            if (! st.offsetValid.load()) { candidate[(size_t) t] = -1; stable[(size_t) t] = 0; continue; }

            auto& tp = gl::LinkHub::get().tap (p.getGroupIndex(), t);
            auto* ring = tp.ring.load (std::memory_order_acquire);
            if (ring == nullptr) continue;

            const int64_t pos = p.lockWritePos.load (std::memory_order_acquire);
            const int64_t D = st.offsetD.load();
            if (p.lockWritePos.load (std::memory_order_acquire) != pos) continue;
            if (pos < N + 1) continue;

            const int64_t Ws = tp.writePos.load (std::memory_order_acquire);
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

            if (std::abs (r.lag - candidate[(size_t) t]) <= 1) ++stable[(size_t) t];
            else { candidate[(size_t) t] = r.lag; stable[(size_t) t] = 1; }

            if (stable[(size_t) t] >= 2 || (! p.status.tapLatencyKnown[(size_t) t].load() && r.confidence > 40.0f))
            {
                st.latency.store (r.lag);
                p.status.tapLatencyMs[(size_t) t].store ((float) (r.lag * 1000.0 / p.sr));
                p.status.tapPolarity[(size_t) t].store (r.inverted);
                p.status.tapLatencyKnown[(size_t) t].store (true);
            }
        }
    }
}

//==============================================================================
void GainLockProcessor::timerCallback()
{
    const int group = getGroupIndex();
    auto& hub = gl::LinkHub::get();
    for (int t = 0; t < kNumTaps; ++t) hub.ensureRing (group, t);

    if (ownedGroup == group && ownedTap >= 0 && hub.tap (group, ownedTap).owner.load() == instanceId)
    {
        juce::String name;
        {
            const juce::SpinLock::ScopedLockType l (nameLock);
            name = ownedTap == 0 ? trackName : (pointName.isNotEmpty() ? pointName : juce::String());
        }
        hub.tap (group, ownedTap).setName (name.toRawUTF8());
    }
}

void GainLockProcessor::updateTrackProperties (const TrackProperties& properties)
{
    const juce::SpinLock::ScopedLockType l (nameLock);
    trackName = properties.name.value_or (juce::String()).substring (0, 60);
}

void GainLockProcessor::setPointName (const juce::String& n)
{
    const juce::SpinLock::ScopedLockType l (nameLock);
    pointName = n.substring (0, 40);
}

juce::String GainLockProcessor::getPointName() const
{
    const juce::SpinLock::ScopedLockType l (nameLock);
    return pointName;
}

juce::String GainLockProcessor::getTapName (int tap)
{
    char buf[64];
    gl::LinkHub::get().tap (getGroupIndex(), tap).getName (buf, sizeof (buf));
    return juce::String::fromUTF8 (buf);
}

juce::String GainLockProcessor::monitorName (int mon)
{
    switch (mon)
    {
        case monFinal:    return "Final";
        case monOriginal: return "Original";
        case monDelta:    return "Delta";
        default: break;
    }
    const int t = tapOfMonitor (mon);
    const auto n = getTapName (t);
    return "P" + juce::String (t) + (n.isNotEmpty() ? " " + n : juce::String());
}

//==============================================================================
// Blindtest
void GainLockProcessor::blindStart (int a, int b, int rounds)
{
    const juce::ScopedLock l (blindLock);
    blind = {};
    blind.running = true;
    blind.a = a; blind.b = b;
    blind.rounds = juce::jlimit (3, 40, rounds);
    blindNextRound();
}

void GainLockProcessor::blindNextRound()
{
    blind.xIsA = blindRng.nextBool();
    blind.listenX = true;
    updateBlindAudio();
}

void GainLockProcessor::updateBlindAudio()
{
    if (! blind.running) { blindSource.store (-1); return; }
    const bool hearA = blind.listenX == blind.xIsA;
    blindSource.store (hearA ? blind.a : blind.b);
}

void GainLockProcessor::blindListen (bool x)
{
    const juce::ScopedLock l (blindLock);
    if (! blind.running) return;
    blind.listenX = x;
    updateBlindAudio();
}

void GainLockProcessor::blindAnswer (int choice)
{
    const juce::ScopedLock l (blindLock);
    if (! blind.running) return;
    if (choice == 2)                         ++blind.none;
    else if ((choice == 0) == blind.xIsA)    ++blind.prefA;
    else                                     ++blind.prefB;
    ++blind.round;

    if (blind.round >= blind.rounds)
    {
        blind.running = false;
        blind.finished = true;
        BlindResult res;
        res.nameA = monitorName (blind.a);
        res.nameB = monitorName (blind.b);
        res.when = juce::Time::getCurrentTime().formatted ("%d.%m. %H:%M");
        res.prefA = blind.prefA; res.prefB = blind.prefB; res.none = blind.none;
        blindHistory.insert (blindHistory.begin(), res);
        if (blindHistory.size() > 20) blindHistory.resize (20);
    }
    else
    {
        blindNextRound();
    }
    updateBlindAudio();
}

void GainLockProcessor::blindStop()
{
    const juce::ScopedLock l (blindLock);
    blind.running = false;
    blind.finished = false;
    updateBlindAudio();
}

void GainLockProcessor::blindDismiss()
{
    const juce::ScopedLock l (blindLock);
    blind.finished = false;
}

GainLockProcessor::BlindState GainLockProcessor::getBlindState() const
{
    const juce::ScopedLock l (blindLock);
    return blind;
}

std::vector<GainLockProcessor::BlindResult> GainLockProcessor::getBlindHistory() const
{
    const juce::ScopedLock l (blindLock);
    return blindHistory;
}

void GainLockProcessor::clearBlindHistory()
{
    const juce::ScopedLock l (blindLock);
    blindHistory.clear();
}

//==============================================================================
void GainLockProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    // Korrektur ohne Trim merken, damit es nach dem Laden ohne Sprung weitergeht
    state.setProperty ("lastGain", (double) (status.gainDb.load() - pTrim->load()), nullptr);
    state.setProperty ("tooltips", tooltipsOn.load(), nullptr);
    state.setProperty ("pointName", getPointName(), nullptr);

    juce::ValueTree hist ("BlindHistory");
    for (auto& r : getBlindHistory())
    {
        juce::ValueTree e ("Result");
        e.setProperty ("a", r.nameA, nullptr);
        e.setProperty ("b", r.nameB, nullptr);
        e.setProperty ("when", r.when, nullptr);
        e.setProperty ("prefA", r.prefA, nullptr);
        e.setProperty ("prefB", r.prefB, nullptr);
        e.setProperty ("none", r.none, nullptr);
        hist.appendChild (e, nullptr);
    }
    state.appendChild (hist, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GainLockProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    const float g = (float) (double) tree.getProperty ("lastGain", 0.0);
    tooltipsOn.store ((bool) tree.getProperty ("tooltips", true));
    setPointName (tree.getProperty ("pointName", "").toString());

    {
        const juce::ScopedLock l (blindLock);
        blindHistory.clear();
        for (auto e : tree.getChildWithName ("BlindHistory"))
        {
            BlindResult r;
            r.nameA = e.getProperty ("a").toString();
            r.nameB = e.getProperty ("b").toString();
            r.when = e.getProperty ("when").toString();
            r.prefA = (int) e.getProperty ("prefA", 0);
            r.prefB = (int) e.getProperty ("prefB", 0);
            r.none = (int) e.getProperty ("none", 0);
            blindHistory.push_back (r);
        }
    }

    for (auto* key : { "lastGain", "tooltips", "pointName" }) tree.removeProperty (key, nullptr);
    tree.removeChild (tree.getChildWithName ("BlindHistory"), nullptr);
    apvts.replaceState (tree);

    restoredGain.store (juce::jlimit (-36.0f, 36.0f, std::isfinite (g) ? g : 0.0f));
    hasRestoredGain.store (true);
}

juce::AudioProcessorEditor* GainLockProcessor::createEditor()
{
    return new GainLockEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GainLockProcessor();
}
