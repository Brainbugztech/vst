#include "Worker.h"

namespace wh
{
Worker::Worker() : juce::Thread ("Wormhole Worker")
{
    formats.registerBasicFormats();
    startThread (juce::Thread::Priority::normal);
}

Worker::~Worker()
{
    cancelFlag = true;
    stopThread (10000);
}

void Worker::loadFile (const juce::File& f)
{
    {
        const juce::ScopedLock sl (jobLock);
        pendingJob = Job::load; pendingFile = f;
    }
    cancelFlag = true;   // laufenden Render abbrechen
    notify();
}

void Worker::loadCaptured (juce::AudioBuffer<float>&& buffer, double sampleRate)
{
    {
        const juce::ScopedLock sl (jobLock);
        pendingJob = Job::captured; pendingCapture = std::move (buffer); pendingCaptureRate = sampleRate;
    }
    cancelFlag = true;
    notify();
}

void Worker::render (const RenderSettings& s)
{
    {
        const juce::ScopedLock sl (jobLock);
        pendingJob = Job::render; pendingSettings = s;
    }
    cancelFlag = true;
    notify();
}

juce::String Worker::getMessage() const { const juce::ScopedLock sl (dataLock); return message; }
std::shared_ptr<const Clip> Worker::getClip() const { const juce::ScopedLock sl (dataLock); return clip; }
std::shared_ptr<const Result> Worker::getResult() const { const juce::ScopedLock sl (dataLock); return result; }

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
        {
            const juce::ScopedLock sl (jobLock);
            job = pendingJob; pendingJob = Job::none;
        }
        if (job == Job::none) { wait (200); continue; }
        cancelFlag = false;
        progress = 0.0f;

        if (job == Job::load)          { juce::File f; { const juce::ScopedLock sl (jobLock); f = pendingFile; } doLoad (f); }
        else if (job == Job::captured) doCaptured();
        else if (job == Job::render)   { RenderSettings s; { const juce::ScopedLock sl (jobLock); s = pendingSettings; } doRender (s); }
    }
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
    c->detectedBpm = wormhole::detectTempo (c->ch, sr);
    c->peaks = makePeaks (c->ch, 900);

    { const juce::ScopedLock sl (dataLock); clip = c; result.reset(); message = {}; }
    ++clipGeneration;
    status = (int) Status::idle;
    ++generation;
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
    c->detectedBpm = wormhole::detectTempo (c->ch, sr);
    c->peaks = makePeaks (c->ch, 900);

    // Aufnahme auch als Datei sichern, damit das Projekt sie später wiederfindet
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

    { const juce::ScopedLock sl (dataLock); clip = c; result.reset(); message = {}; }
    ++clipGeneration;
    status = (int) Status::idle;
    ++generation;
}

void Worker::doRender (const RenderSettings& s)
{
    auto c = getClip();
    if (c == nullptr) { status = (int) Status::idle; return; }
    status = (int) Status::rendering;

    wormhole::Params p;
    p.timeRatio = juce::jlimit (0.25, 4.0, s.timeRatio);
    p.pitchSemitones = s.pitchSemitones;
    p.formantPreserve = s.formantLock;
    p.formantSemitones = s.formantSemitones;
    p.transientLock = s.attackLock;

    wormhole::OfflineStretcher st;
    auto out = st.process (c->ch, c->sampleRate, p, [this] (float pr)
    {
        progress = pr;
        return ! cancelFlag.load() && ! threadShouldExit();
    });
    if (cancelFlag.load() || threadShouldExit()) { status = (int) Status::idle; return; }

    // Clipping-Schutz: nur wenn nötig sanft absenken
    float peak = 0.0f;
    for (auto& ch : out) for (float v : ch) peak = std::max (peak, std::abs (v));
    if (peak > 0.999f) for (auto& ch : out) for (float& v : ch) v *= 0.999f / peak;

    const int len = out.empty() ? 0 : (int) out[0].size();
    juce::AudioBuffer<float> buf ((int) out.size(), len);
    for (int i = 0; i < (int) out.size(); ++i) buf.copyFrom (i, 0, out[(size_t) i].data(), len);

    auto fmt = [] (double v, int dec) { return juce::String (v, dec).replace (".", ","); };
    juce::String tag = " (T" + fmt (100.0 / p.timeRatio, 1) + " P" + (s.pitchSemitones >= 0 ? "+" : "") + fmt (s.pitchSemitones, 2) + ")";
    auto folder = outputFolder(); folder.createDirectory();
    auto f = folder.getNonexistentChildFile (c->name + tag, ".wav");
    bool ok = false;
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (new juce::FileOutputStream (f));
        if (static_cast<juce::FileOutputStream*> (os.get())->openedOk())
        {
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (c->sampleRate)
                                                 .withNumChannels (buf.getNumChannels()).withBitsPerSample (24));
            if (w != nullptr) ok = w->writeFromAudioSampleBuffer (buf, 0, len);
        }
    }
    if (! ok) { fail ("Konnte nicht speichern: " + f.getFullPathName()); return; }

    auto r = std::make_shared<Result>();
    r->file = f;
    r->settings = s;
    r->lengthSeconds = len / c->sampleRate;
    r->peaks = makePeaks (out, 900);
    r->sourceGeneration = clipGeneration.load();
    const double inLen = (double) c->ch[0].size();
    for (long t : st.transients) r->transients.push_back ((float) juce::jlimit (0.0, 1.0, t / inLen));

    { const juce::ScopedLock sl (dataLock); result = r; message = {}; }
    progress = 1.0f;
    status = (int) Status::idle;
    ++generation;
}
} // namespace wh
