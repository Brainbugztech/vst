#pragma once

// Eine Kokoro-Engine für alle Plugin-Instanzen (spart RAM) + Download der Modelldateien beim ersten Start

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "KokoroEngine.h"
#include "TTSAssets.h"

namespace tts
{
class EngineHolder
{
public:
    enum class State { missing, downloading, loading, ready, failed };

    struct FileInfo { const char* name; const char* url; juce::int64 size; };
    static const FileInfo* files (int& count)
    {
        static const FileInfo f[] = {
            { "kokoro-v1.0.onnx", "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.1/kokoro-v1.0.onnx", 325505369 },
            { "voices-v1.0.bin",  "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.1/voices-v1.0.bin",  28214398 } };
        count = 2;
        return f;
    }
    static juce::int64 totalDownloadSize()
    {
        int n = 0; auto* f = files (n);
        juce::int64 t = 0;
        for (int i = 0; i < n; ++i) t += f[i].size;
        return t;
    }

    static juce::File dataDir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Brainbugz").getChildFile ("TTS");
    }

    static juce::File runtimeLibrary()
    {
        const auto here = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
       #if JUCE_WINDOWS
        return here.getChildFile ("bbtts_onnxruntime.dll");
       #elif JUCE_MAC
        return here.getChildFile ("libonnxruntime.dylib");
       #else
        return here.getChildFile ("libonnxruntime.so");
       #endif
    }

    EngineHolder()
    {
        engine.setLexicons (gunzipResource (TTSAssets::lexicon_us_txt_gz, TTSAssets::lexicon_us_txt_gzSize),
                            gunzipResource (TTSAssets::lexicon_gb_txt_gz, TTSAssets::lexicon_gb_txt_gzSize));
        if (filesPresent()) startLoading();
        else state = State::missing;
    }

    ~EngineHolder()
    {
        cancelDownload = true;
        if (worker != nullptr) worker->stopThread (8000);
    }

    KokoroEngine& getEngine() { return engine; }
    State getState() const { return state.load(); }
    juce::String getError() const { const juce::ScopedLock sl (lock); return error; }
    juce::int64 getDownloaded() const { return downloaded.load(); }
    bool isReady() const { return state.load() == State::ready; }

    bool filesPresent() const
    {
        int n = 0; auto* f = files (n);
        for (int i = 0; i < n; ++i)
        {
            auto file = dataDir().getChildFile (f[i].name);
            if (! file.existsAsFile() || file.getSize() != f[i].size) return false;
        }
        return true;
    }

    void startDownload()
    {
        if (state == State::downloading || state == State::loading || state == State::ready) return;
        cancelDownload = false;
        downloaded = 0;
        setError ({});
        state = State::downloading;
        runWorker ([this] { downloadAll(); });
    }

    void cancel() { cancelDownload = true; }

    void retry()
    {
        if (filesPresent()) startLoading();
        else startDownload();
    }

private:
    static std::string gunzipResource (const char* data, int size)
    {
        juce::MemoryInputStream mem (data, (size_t) size, false);
        juce::GZIPDecompressorInputStream gz (&mem, false, juce::GZIPDecompressorInputStream::gzipFormat);
        return gz.readEntireStreamAsString().toStdString();
    }

    void setError (const juce::String& e) { const juce::ScopedLock sl (lock); error = e; }

    struct Worker : public juce::Thread
    {
        explicit Worker (std::function<void()> f) : juce::Thread ("BrainbugzTTS worker"), fn (std::move (f)) {}
        void run() override { fn(); }
        std::function<void()> fn;
    };

    void runWorker (std::function<void()> fn)
    {
        if (worker != nullptr) worker->stopThread (8000);
        worker = std::make_unique<Worker> (std::move (fn));
        worker->startThread();
    }

    void startLoading()
    {
        state = State::loading;
        runWorker ([this]
        {
            juce::String err;
            const auto dir = dataDir();
            if (engine.load (runtimeLibrary(), dir.getChildFile ("kokoro-v1.0.onnx"), dir.getChildFile ("voices-v1.0.bin"), err))
                state = State::ready;
            else
            {
                setError (err);
                state = State::failed;
            }
        });
    }

    void downloadAll()
    {
        const auto dir = dataDir();
        if (! dir.createDirectory())
        {
            setError ("Could not create folder: " + dir.getFullPathName());
            state = State::failed;
            return;
        }
        int n = 0; auto* f = files (n);
        juce::int64 done = 0;
        for (int i = 0; i < n; ++i)
        {
            auto target = dir.getChildFile (f[i].name);
            if (target.existsAsFile() && target.getSize() == f[i].size) { done += f[i].size; downloaded = done; continue; }
            auto part = target.withFileExtension (target.getFileExtension().substring (1) + ".part");
            part.deleteFile();
            if (! downloadOne (f[i], part, done))
            {
                part.deleteFile();
                if (cancelDownload) { setError ("Download cancelled"); state = State::missing; }
                else state = State::failed;
                return;
            }
            target.deleteFile();
            if (! part.moveFileTo (target))
            {
                setError ("Could not save file: " + target.getFullPathName());
                state = State::failed;
                return;
            }
            done += f[i].size;
        }
        // Download fertig -> laden (im selben Thread)
        state = State::loading;
        juce::String err;
        if (engine.load (runtimeLibrary(), dir.getChildFile ("kokoro-v1.0.onnx"), dir.getChildFile ("voices-v1.0.bin"), err))
            state = State::ready;
        else
        {
            setError (err);
            state = State::failed;
        }
    }

    bool downloadOne (const FileInfo& info, const juce::File& part, juce::int64 base)
    {
        auto stream = juce::URL (info.url).createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs (20000)
                .withNumRedirectsToFollow (10)
                .withExtraHeaders ("User-Agent: BrainbugzTTS"));
        if (stream == nullptr)
        {
            setError ("No connection to GitHub. Are you online?");
            return false;
        }
        juce::FileOutputStream out (part);
        if (! out.openedOk())
        {
            setError ("Cannot write: " + part.getFullPathName());
            return false;
        }
        juce::HeapBlock<char> buf (1 << 16);
        juce::int64 got = 0;
        while (! stream->isExhausted())
        {
            if (cancelDownload || juce::Thread::currentThreadShouldExit()) return false;
            const int r = stream->read (buf.get(), 1 << 16);
            if (r < 0) break;
            if (r == 0) { if (stream->isExhausted()) break; juce::Thread::sleep (5); continue; }
            if (! out.write (buf.get(), (size_t) r)) { setError ("Disk full?"); return false; }
            got += r;
            downloaded = base + got;
        }
        out.flush();
        if (got != info.size)
        {
            setError ("Download incomplete (" + juce::String (got / 1048576) + " of "
                      + juce::String (info.size / 1048576) + " MB). Please try again.");
            return false;
        }
        return true;
    }

    KokoroEngine engine;
    std::atomic<State> state { State::missing };
    std::atomic<juce::int64> downloaded { 0 };
    std::atomic<bool> cancelDownload { false };
    juce::CriticalSection lock;
    juce::String error;
    std::unique_ptr<Worker> worker;
};
} // namespace tts
