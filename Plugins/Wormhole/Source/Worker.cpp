#include "Worker.h"

namespace wh
{
// ===========================================================================
//  Worker
// ===========================================================================
Worker::Worker() : juce::Thread ("Wormhole Worker")
{
    formats.registerBasicFormats();
    startThread (juce::Thread::Priority::normal);
}

Worker::~Worker()
{
    cancelFlag = true;
    stopThread (10000);
    // Plugin wird geschlossen: einen nie benutzten letzten Render wegräumen
    deleteUnexported (getResult());
}

void Worker::loadFile (const juce::File& f)
{
    { const juce::ScopedLock sl (jobLock); pendingJob = Job::load; pendingFile = f; }
    cancelFlag = true;
    notify();
}

void Worker::loadCaptured (juce::AudioBuffer<float>&& buffer, double sampleRate)
{
    { const juce::ScopedLock sl (jobLock); pendingJob = Job::captured; pendingCapture = std::move (buffer); pendingCaptureRate = sampleRate; }
    cancelFlag = true;
    notify();
}

void Worker::render (const RenderSettings& s)
{
    { const juce::ScopedLock sl (jobLock); pendingJob = Job::render; pendingSettings = s; }
    cancelFlag = true;
    notify();
}

juce::String Worker::getMessage() const { const juce::ScopedLock sl (dataLock); return message; }
std::shared_ptr<const Clip> Worker::getClip() const { const juce::ScopedLock sl (dataLock); return clip; }
std::shared_ptr<const Result> Worker::getResult() const { const juce::ScopedLock sl (dataLock); return result; }

void Worker::markExported (const juce::File& f)
{
    const juce::ScopedLock sl (dataLock);
    exported.insert (f.getFullPathName());
}

void Worker::deleteUnexported (const std::shared_ptr<const Result>& r)
{
    if (r == nullptr) return;
    bool keep;
    { const juce::ScopedLock sl (dataLock); keep = exported.count (r->file.getFullPathName()) > 0; }
    if (! keep && r->file.existsAsFile() && r->file.isAChildOf (outputFolder()))
        r->file.deleteFile();
}

juce::File Worker::outputFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Brain Bugz").getChildFile ("Wormhole");
}

std::vector<float> Worker::makePeaks (const std::vector<std::vector<float>>& ch, int bins)
{
    std::vector<float> p ((size_t) bins, 0.0f);
    if (ch.empty() || ch[0].empty()) return p;
    const size_t n = ch[0].size();
    float mx = 1e-9f;
    for (int b = 0; b < bins; ++b)
    {
        const size_t a = n * (size_t) b / (size_t) bins, e = std::max (a + 1, n * (size_t) (b + 1) / (size_t) bins);
        float m = 0.0f;
        for (auto& c : ch)
            for (size_t i = a; i < e && i < n; ++i) m = std::max (m, std::abs (c[i]));
        p[(size_t) b] = m; mx = std::max (mx, m);
    }
    for (auto& v : p) v /= mx;
    return p;
}

void Worker::fail (const juce::String& msg)
{
    { const juce::ScopedLock sl (dataLock); message = msg; }
    status = (int) Status::error;
    ++generation;
}

void Worker::run()
{
    while (! threadShouldExit())
    {
        Job job;
        { const juce::ScopedLock sl (jobLock); job = pendingJob; pendingJob = Job::none; }
        if (job == Job::none) { wait (200); continue; }
        cancelFlag = false;
        progress = 0.0f;

        if (job == Job::load)          { juce::File f; { const juce::ScopedLock sl (jobLock); f = pendingFile; } doLoad (f); }
        else if (job == Job::captured) doCaptured();
        else if (job == Job::render)   { RenderSettings s; { const juce::ScopedLock sl (jobLock); s = pendingSettings; } doRender (s); }
    }
}

void Worker::finishClip (std::shared_ptr<Clip> c)
{
    c->detectedBpm = wormhole::detectTempo (c->ch, c->sampleRate);
    c->peaks = makePeaks (c->ch, 900);
    {
        wormhole::StretchStream st;
        st.setInput (&c->ch, c->sampleRate);
        c->transients = st.getTransients();
    }
    std::shared_ptr<const Result> old;
    { const juce::ScopedLock sl (dataLock); old = result; clip = c; result.reset(); message = {}; }
    deleteUnexported (old);
    status = (int) Status::idle;
    ++generation;
}

void Worker::doLoad (const juce::File& f)
{
    status = (int) Status::loading;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr) { fail ("Datei kann nicht gelesen werden"); return; }

    const double sr = reader->sampleRate;
    const auto maxLen = (juce::int64) (sr * 60.0 * 20.0); // 20 Minuten
    const int len = (int) std::min (reader->lengthInSamples, maxLen);
    const int numCh = (int) juce::jlimit (1u, 2u, reader->numChannels);
    if (len <= 0 || sr <= 0) { fail ("Leere Datei"); return; }

    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);

    auto c = std::make_shared<Clip>();
    c->sampleRate = sr;
    c->name = f.getFileNameWithoutExtension();
    c->file = f;
    c->ch.resize ((size_t) numCh);
    for (int i = 0; i < numCh; ++i)
        c->ch[(size_t) i].assign (buf.getReadPointer (i), buf.getReadPointer (i) + len);
    finishClip (c);
}

void Worker::doCaptured()
{
    status = (int) Status::loading;
    juce::AudioBuffer<float> buf; double sr;
    { const juce::ScopedLock sl (jobLock); buf = std::move (pendingCapture); sr = pendingCaptureRate; }
    if (buf.getNumSamples() < 64) { fail ("Aufnahme ist leer"); return; }

    auto c = std::make_shared<Clip>();
    c->sampleRate = sr;
    c->name = "Aufnahme " + juce::Time::getCurrentTime().formatted ("%H-%M-%S");
    c->ch.resize ((size_t) juce::jlimit (1, 2, buf.getNumChannels()));
    for (size_t i = 0; i < c->ch.size(); ++i)
        c->ch[i].assign (buf.getReadPointer ((int) i), buf.getReadPointer ((int) i) + buf.getNumSamples());

    // Aufnahme als Datei sichern, damit das Projekt sie später wiederfindet
    auto folder = outputFolder(); folder.createDirectory();
    auto f = folder.getNonexistentChildFile (c->name, ".wav");
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (new juce::FileOutputStream (f));
        if (static_cast<juce::FileOutputStream*> (os.get())->openedOk())
        {
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr)
                                                 .withNumChannels ((int) c->ch.size()).withBitsPerSample (24));
            if (w != nullptr) { w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples()); c->file = f; }
        }
    }
    markExported (f);   // Quelle, nie automatisch löschen
    finishClip (c);
}

static juce::String fmtNum (double v, int dec) { return juce::String (v, dec).replace (".", ","); }

void Worker::doRender (const RenderSettings& s)
{
    auto c = getClip();
    if (c == nullptr) { status = (int) Status::idle; return; }

    const auto p = s.toParams();
    const double outSeconds = c->lengthSeconds() * p.timeRatio;
    if (outSeconds > maxRenderSeconds)
    {
        const int mins = juce::roundToInt (outSeconds / 60.0);
        fail ("Ergebnis w\xc3\xa4re " + juce::String (mins / 60) + " h " + juce::String (mins % 60) + " min lang, maximal 2 h. Tempo h\xc3\xb6her oder k\xc3\xbcrzeren Clip.");
        return;
    }
    status = (int) Status::rendering;

    wormhole::StretchStream st;
    st.setInput (&c->ch, c->sampleRate, &c->transients);
    st.start (p, 0);
    const int C = st.channels();
    const long L = st.outputLength();

    const double tempoPct = 100.0 / p.timeRatio;
    juce::String tag = " (T" + fmtNum (tempoPct, tempoPct < 1.0 ? 2 : 1) + " P" + (s.pitchSemitones >= 0 ? "+" : "") + fmtNum (s.pitchSemitones, 2) + ")";
    auto folder = outputFolder(); folder.createDirectory();
    auto f = folder.getNonexistentChildFile (c->name + tag, ".wav");

    std::unique_ptr<juce::AudioFormatWriter> w;
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (new juce::FileOutputStream (f));
        if (static_cast<juce::FileOutputStream*> (os.get())->openedOk())
            w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (c->sampleRate).withNumChannels (C)
                                           .withBitsPerSample (32)
                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    }
    if (w == nullptr) { fail ("Konnte nicht speichern: " + f.getFullPathName()); return; }

    // stückweise rechnen und schreiben, Hüllkurve nebenbei sammeln
    const int bins = 900;
    std::vector<float> peaks ((size_t) bins, 0.0f);
    juce::AudioBuffer<float> chunk (C, wormhole::StretchStream::maxPull);
    long done = 0;
    bool ok = true;
    while (done < L)
    {
        if (cancelFlag.load() || threadShouldExit()) { ok = false; break; }
        const int got = st.pull (chunk.getArrayOfWritePointers(), wormhole::StretchStream::maxPull);
        if (got <= 0) break;
        for (int i = 0; i < got; ++i)
        {
            const size_t b = (size_t) juce::jlimit<long> (0, bins - 1, (done + i) * (long) bins / juce::jmax (1L, L));
            for (int ch = 0; ch < C; ++ch) peaks[b] = std::max (peaks[b], std::abs (chunk.getSample (ch, i)));
        }
        if (! w->writeFromAudioSampleBuffer (chunk, 0, got)) { ok = false; break; }
        done += got;
        progress = (float) done / (float) juce::jmax (1L, L);
    }
    w.reset();
    if (! ok)
    {
        f.deleteFile();
        if (cancelFlag.load() || threadShouldExit()) { status = (int) Status::idle; return; }
        fail ("Schreiben fehlgeschlagen (Speicherplatz?)");
        return;
    }
    float mx = 1e-9f; for (float v : peaks) mx = std::max (mx, v);
    for (float& v : peaks) v /= mx;

    auto r = std::make_shared<Result>();
    r->file = f;
    r->settings = s;
    r->lengthSeconds = L / c->sampleRate;
    r->peaks = std::move (peaks);
    if (! s.isExtreme())
    {
        const double inLen = (double) c->ch[0].size();
        for (long t : st.getTransients()) r->transients.push_back ((float) juce::jlimit (0.0, 1.0, t / inLen));
    }

    std::shared_ptr<const Result> old;
    { const juce::ScopedLock sl (dataLock); old = result; result = r; message = {}; }
    deleteUnexported (old);   // nie benutzte Vorgänger-Version wegräumen
    progress = 1.0f;
    status = (int) Status::idle;
    ++renderCount;
    ++generation;
}

// ===========================================================================
//  Previewer
// ===========================================================================
Previewer::Previewer() : juce::Thread ("Wormhole Preview")
{
    fifoBuf.setSize (2, fifo.getTotalSize());
    staging.assign (2, std::vector<float> ((size_t) wormhole::StretchStream::maxPull * 2, 0.0f));
    stagingPtr.resize (2);
    chunk.setSize (2, 4096);
    startThread (juce::Thread::Priority::high);
}

Previewer::~Previewer() { stopThread (5000); }

void Previewer::prepare (double rate, int numChannels)
{
    hostRate = rate;
    hostChannels = juce::jlimit (1, 2, numChannels);
}

void Previewer::play (std::shared_ptr<const Clip> c, const RenderSettings& s, double startFraction)
{
    { const juce::ScopedLock sl (lock); pendingClip = std::move (c); pendingSettings = s; pendingFraction = startFraction; }
    playing = true;
    restartRequest = true;
    notify();
}

void Previewer::update (const RenderSettings& s)
{
    if (! playing.load()) return;
    { const juce::ScopedLock sl (lock); pendingSettings = s; pendingFraction = posFraction.load(); }
    restartRequest = true;
    notify();
}

void Previewer::stop()
{
    playing = false;
    notify();
}

void Previewer::restartStream (double fraction)
{
    std::shared_ptr<const Clip> c; RenderSettings s;
    { const juce::ScopedLock sl (lock); c = pendingClip; s = pendingSettings; }
    // altes Material ausblenden lassen, dann FIFO leeren
    if (written.load() > 0)
    {
        flushRequest = true;
        for (int i = 0; i < 40 && flushRequest.load(); ++i) wait (5);
        flushRequest = false;
    }
    fifo.reset();
    written = 0; consumed = 0;

    if (c != clip) { clip = c; if (clip != nullptr) stream.setInput (&clip->ch, clip->sampleRate, &clip->transients); }
    settings = s;
    if (clip == nullptr) { playing = false; return; }
    stream.start (s.toParams(), 0);
    const long L = juce::jmax (1L, stream.outputLength());
    streamStartOut = juce::jlimit (0L, L - 1, (long) (fraction * L));
    stream.start (s.toParams(), streamStartOut);
    outLength = L;
    clipRate = clip->sampleRate;
    fifoBaseOut = streamStartOut;
    stagingFill = 0;
    for (auto& ip : interp) ip.reset();
}

int Previewer::produce (int numHost)
{
    // erzeugt numHost Samples in Host-Rate nach chunk (Stream läuft in Schleife)
    const int C = clip != nullptr ? (int) clip->ch.size() : 1;
    const double ratio = clipRate.load() / hostRate;  // Eingangs-Samples pro Ausgabe-Sample
    const bool resample = std::abs (ratio - 1.0) > 1e-9;
    const int need = resample ? (int) std::ceil (numHost * ratio) + 8 : numHost;
    while (stagingFill < need)
    {
        for (int c = 0; c < C; ++c) stagingPtr[(size_t) c] = staging[(size_t) c].data() + stagingFill;
        const int want = juce::jmin (need - stagingFill, wormhole::StretchStream::maxPull);
        int got = stream.pull (stagingPtr.data(), want);
        if (got <= 0) { stream.start (settings.toParams(), 0); got = stream.pull (stagingPtr.data(), want); } // Schleife
        if (got <= 0) return 0;
        if (C == 1) std::copy (staging[0].begin() + stagingFill, staging[0].begin() + stagingFill + got, staging[1].begin() + stagingFill);
        stagingFill += got;
    }
    int used = numHost;
    for (int c = 0; c < 2; ++c)
    {
        if (resample) used = interp[c].process (ratio, staging[(size_t) c].data(), chunk.getWritePointer (c), numHost);
        else std::copy (staging[(size_t) c].begin(), staging[(size_t) c].begin() + numHost, chunk.getWritePointer (c));
    }
    for (int c = 0; c < 2; ++c)
        std::copy (staging[(size_t) c].begin() + used, staging[(size_t) c].begin() + stagingFill, staging[(size_t) c].begin());
    stagingFill -= used;
    return numHost;
}

void Previewer::run()
{
    while (! threadShouldExit())
    {
        if (restartRequest.exchange (false))
        {
            double fr; { const juce::ScopedLock sl (lock); fr = pendingFraction; }
            restartStream (fr);
        }
        if (! playing.load())
        {
            if (written.load() > 0)
            {
                // Audio-Thread blendet aus, danach leeren
                flushRequest = true;
                for (int i = 0; i < 40 && flushRequest.load(); ++i) wait (5);
                flushRequest = false;
                fifo.reset(); written = 0; consumed = 0;
            }
            wait (50);
            continue;
        }
        // FIFO bis ca. 0,5 s Vorlauf füllen
        const int target = juce::jmin (fifo.getTotalSize() - 4096, (int) (hostRate * 0.5));
        if (fifo.getNumReady() >= target) { wait (5); continue; }
        const int n = juce::jmin (chunk.getNumSamples(), fifo.getFreeSpace());
        if (n <= 0) { wait (5); continue; }
        if (produce (n) <= 0) { playing = false; continue; }
        int s1, n1, s2, n2;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        for (int c = 0; c < 2; ++c)
        {
            if (n1 > 0) fifoBuf.copyFrom (c, s1, chunk, c, 0, n1);
            if (n2 > 0) fifoBuf.copyFrom (c, s2, chunk, c, n1, n2);
        }
        fifo.finishedWrite (n1 + n2);
        written += n1 + n2;
    }
}

bool Previewer::readAudio (juce::AudioBuffer<float>& buffer, int numChannels)
{
    const int n = buffer.getNumSamples();
    const bool active = playing.load() && ! flushRequest.load();
    if (! active && fadeGain <= 0.0f && ! flushRequest.load()) return false;

    const int ch = juce::jmin (numChannels, buffer.getNumChannels());
    int s1, n1, s2, n2;
    fifo.prepareToRead (n, s1, n1, s2, n2);
    const int avail = n1 + n2;
    const float step = 1.0f / 256.0f;
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float target = active ? 1.0f : 0.0f;
        fadeGain = target > fadeGain ? juce::jmin (target, fadeGain + step) : juce::jmax (target, fadeGain - step);
        float l = 0.0f, r = 0.0f;
        if (i < avail)
        {
            const int idx = i < n1 ? s1 + i : s2 + (i - n1);
            l = fifoBuf.getSample (0, idx); r = fifoBuf.getSample (1, idx);
        }
        l *= fadeGain; r *= fadeGain;
        if (ch == 1) buffer.setSample (0, i, 0.5f * (l + r));
        else { buffer.setSample (0, i, l); buffer.setSample (1, i, r); }
        sum += (double) l * l + (double) r * r;
    }
    fifo.finishedRead (avail);
    consumed += avail;
    if (! active && fadeGain <= 0.0f && flushRequest.load())
    {
        // Rest verwerfen und Preview-Thread freigeben
        int a1, b1, a2, b2; fifo.prepareToRead (fifo.getNumReady(), a1, b1, a2, b2); fifo.finishedRead (b1 + b2);
        flushRequest = false;
    }
    const float rms = (float) std::sqrt (sum / juce::jmax (1, 2 * n));
    level = level.load() * 0.7f + rms * 0.3f;
    const long L = juce::jmax (1L, outLength.load());
    const double played = (double) fifoBaseOut.load() + (double) consumed.load() * clipRate.load() / hostRate;
    posFraction = std::fmod (played, (double) L) / (double) L;
    return true;
}
} // namespace wh
