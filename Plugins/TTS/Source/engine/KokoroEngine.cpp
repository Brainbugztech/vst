#include "KokoroEngine.h"
#include "KokoroVocab.h"
#include "Phonemizer.h"
#include "Utf.h"

#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#undef ORT_API_MANUAL_INIT

#include <cmath>
#include <unordered_map>

namespace tts
{
namespace
{
constexpr int maxTokens = 510;
constexpr int styleDim = 256;

// Die Runtime bleibt bis Prozessende geladen (mehrere Plugin-Instanzen teilen sie sich)
struct Runtime
{
    std::unique_ptr<juce::DynamicLibrary> lib;
    const OrtApi* api = nullptr;
    std::unique_ptr<Ort::Env> env;

    // absichtlich nie freigegeben: ORT beim Prozessende/Plugin-Entladen abzubauen führt zu Abstürzen
    static Runtime& get() { static Runtime* r = new Runtime(); return *r; }

    bool load (const juce::File& file, juce::String& error)
    {
        if (api != nullptr) return true;
        if (! file.existsAsFile()) { error = "ONNX Runtime missing: " + file.getFullPathName(); return false; }
        lib = std::make_unique<juce::DynamicLibrary>();
        if (! lib->open (file.getFullPathName()))
        {
           #if JUCE_WINDOWS
            error = "Could not load ONNX Runtime. Install the Microsoft Visual C++ Redistributable (x64) and restart.";
           #else
            error = "Could not load ONNX Runtime";
           #endif
            lib.reset();
            return false;
        }
        using GetBase = const OrtApiBase* (ORT_API_CALL*) ();
        auto getBase = (GetBase) lib->getFunction ("OrtGetApiBase");
        if (getBase == nullptr) { error = "ONNX Runtime is broken (OrtGetApiBase missing)"; return false; }
        api = getBase()->GetApi (ORT_API_VERSION);
        if (api == nullptr) { error = "ONNX Runtime has the wrong version"; return false; }
        Ort::InitApi (api);
        try { env = std::make_unique<Ort::Env> (ORT_LOGGING_LEVEL_WARNING, "BrainbugzTTS"); }
        catch (const std::exception& e) { error = e.what(); api = nullptr; return false; }
        return true;
    }
};

bool readNpy (juce::InputStream& in, std::vector<float>& out, std::vector<int64_t>& shape)
{
    juce::MemoryBlock mb;
    in.readIntoMemoryBlock (mb);
    auto* d = (const unsigned char*) mb.getData();
    const size_t n = mb.getSize();
    if (n < 12 || d[0] != 0x93 || std::memcmp (d + 1, "NUMPY", 5) != 0) return false;
    const int major = d[6];
    size_t headerLen = 0, off = 0;
    if (major == 1) { headerLen = (size_t) (d[8] | (d[9] << 8)); off = 10; }
    else { headerLen = (size_t) (d[8] | (d[9] << 8) | (d[10] << 16) | ((size_t) d[11] << 24)); off = 12; }
    if (off + headerLen > n) return false;
    const juce::String header (juce::CharPointer_UTF8 ((const char*) d + off), headerLen);
    if (! header.contains ("<f4")) return false;
    const auto shapeStr = header.fromFirstOccurrenceOf ("'shape': (", false, false).upToFirstOccurrenceOf (")", false, false);
    shape.clear();
    for (auto& t : juce::StringArray::fromTokens (shapeStr, ",", ""))
        if (t.trim().isNotEmpty()) shape.push_back (t.trim().getLargeIntValue());
    size_t count = 1;
    for (auto s : shape) count *= (size_t) s;
    const size_t dataOff = off + headerLen;
    if (dataOff + count * 4 > n) return false;
    out.resize (count);
    std::memcpy (out.data(), d + dataOff, count * 4);
    return true;
}

bool isStop (char32_t c) { return c == U'.' || c == U'!' || c == U'?' || c == U'…'; }
bool isClause (char32_t c) { return c == U',' || c == U';' || c == U':' || c == U'—'; }

void trimSilence (std::vector<float>& a, float thresh = 0.004f, int keep = 1200)
{
    if (a.empty()) return;
    int s = 0, e = (int) a.size() - 1;
    while (s < e && std::abs (a[(size_t) s]) < thresh) ++s;
    while (e > s && std::abs (a[(size_t) e]) < thresh) --e;
    s = std::max (0, s - keep);
    e = std::min ((int) a.size() - 1, e + keep);
    a = std::vector<float> (a.begin() + s, a.begin() + e + 1);
}
} // namespace

struct KokoroEngine::Impl
{
    std::unique_ptr<Ort::Session> session;
    std::string tokenInput = "input_ids";
    std::map<juce::String, std::vector<float>> voices; // name -> [510 * 256]
    std::unordered_map<char32_t, int64_t> vocab;
    std::string usLex, gbLex;
    std::unique_ptr<Phonemizer> us, gb;
    std::mutex phonMutex;

    Phonemizer* phonemizer (bool british)
    {
        std::lock_guard<std::mutex> lock (phonMutex);
        auto& p = british ? gb : us;
        if (p == nullptr)
        {
            auto& txt = british ? gbLex : usLex;
            if (txt.empty()) return nullptr;
            p = std::make_unique<Phonemizer> (txt, british);
            txt.clear();
            txt.shrink_to_fit();
        }
        return p.get();
    }

    std::vector<float> style (const juce::String& a, const juce::String& b, float blend, int numTokens)
    {
        const int row = juce::jlimit (0, maxTokens - 1, numTokens - 1);
        std::vector<float> s ((size_t) styleDim, 0.0f);
        auto va = voices.find (a);
        if (va == voices.end()) va = voices.begin();
        const float* pa = va->second.data() + (size_t) row * styleDim;
        auto vb = b.isNotEmpty() ? voices.find (b) : voices.end();
        const float wb = vb != voices.end() ? juce::jlimit (0.0f, 1.0f, blend) : 0.0f;
        const float* pb = vb != voices.end() ? vb->second.data() + (size_t) row * styleDim : pa;
        for (int i = 0; i < styleDim; ++i) s[(size_t) i] = pa[i] * (1.0f - wb) + pb[i] * wb;
        return s;
    }

    std::vector<float> run (const std::vector<int64_t>& tokens, const std::vector<float>& st, float speed)
    {
        auto mem = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> padded;
        padded.reserve (tokens.size() + 2);
        padded.push_back (0);
        padded.insert (padded.end(), tokens.begin(), tokens.end());
        padded.push_back (0);
        const int64_t tokShape[] = { 1, (int64_t) padded.size() };
        const int64_t styleShape[] = { 1, styleDim };
        const int64_t speedShape[] = { 1 };
        float sp = speed;
        std::vector<Ort::Value> inputs;
        inputs.push_back (Ort::Value::CreateTensor<int64_t> (mem, padded.data(), padded.size(), tokShape, 2));
        inputs.push_back (Ort::Value::CreateTensor<float> (mem, const_cast<float*> (st.data()), st.size(), styleShape, 2));
        inputs.push_back (Ort::Value::CreateTensor<float> (mem, &sp, 1, speedShape, 1));
        const char* inNames[] = { tokenInput.c_str(), "style", "speed" };
        Ort::AllocatorWithDefaultOptions alloc;
        auto outName = session->GetOutputNameAllocated (0, alloc);
        const char* outNames[] = { outName.get() };
        auto out = session->Run (Ort::RunOptions { nullptr }, inNames, inputs.data(), inputs.size(), outNames, 1);
        const auto info = out[0].GetTensorTypeAndShapeInfo();
        const size_t n = info.GetElementCount();
        const float* p = out[0].GetTensorData<float>();
        return std::vector<float> (p, p + n);
    }
};

KokoroEngine::KokoroEngine() : impl (std::make_unique<Impl>())
{
    for (auto& [c, id] : kokoroVocab) impl->vocab[c] = id;
}

KokoroEngine::~KokoroEngine() = default;

void KokoroEngine::setLexicons (std::string usText, std::string gbText)
{
    std::lock_guard<std::mutex> lock (impl->phonMutex);
    impl->usLex = std::move (usText);
    impl->gbLex = std::move (gbText);
}

bool KokoroEngine::load (const juce::File& runtimeLib, const juce::File& modelFile, const juce::File& voicesFile, juce::String& error)
{
    if (loaded) return true;
    if (! Runtime::get().load (runtimeLib, error)) return false;
    if (! modelFile.existsAsFile()) { error = "Voice model missing"; return false; }
    if (! voicesFile.existsAsFile()) { error = "Voice file missing"; return false; }

    // Stimmen (npz = zip aus .npy)
    {
        juce::ZipFile zip (voicesFile);
        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            auto* entry = zip.getEntry (i);
            if (entry == nullptr || ! entry->filename.endsWith (".npy")) continue;
            std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
            if (in == nullptr) continue;
            std::vector<float> data;
            std::vector<int64_t> shape;
            if (readNpy (*in, data, shape) && data.size() == (size_t) maxTokens * styleDim)
                impl->voices[entry->filename.upToLastOccurrenceOf (".npy", false, false)] = std::move (data);
        }
        if (impl->voices.empty()) { error = "Voice file is damaged - delete it and download again"; return false; }
    }

    try
    {
        Ort::SessionOptions opts;
        const int threads = juce::jlimit (1, 6, juce::SystemStats::getNumCpus() / 2);
        opts.SetIntraOpNumThreads (threads);
        opts.SetInterOpNumThreads (1);
        opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
       #if JUCE_WINDOWS
        impl->session = std::make_unique<Ort::Session> (*Runtime::get().env, modelFile.getFullPathName().toWideCharPointer(), opts);
       #else
        impl->session = std::make_unique<Ort::Session> (*Runtime::get().env, modelFile.getFullPathName().toRawUTF8(), opts);
       #endif
        Ort::AllocatorWithDefaultOptions alloc;
        impl->tokenInput = "input_ids";
        for (size_t i = 0; i < impl->session->GetInputCount(); ++i)
        {
            auto name = impl->session->GetInputNameAllocated (i, alloc);
            if (std::string (name.get()) == "tokens") impl->tokenInput = "tokens";
        }
    }
    catch (const std::exception& e)
    {
        error = juce::String ("Could not load the voice model: ") + e.what();
        impl->session.reset();
        return false;
    }
    loaded = true;
    return true;
}

juce::StringArray KokoroEngine::getVoiceNames() const
{
    juce::StringArray s;
    for (auto& v : impl->voices) s.add (v.first);
    return s;
}

std::string KokoroEngine::phonemize (const juce::String& text, bool british)
{
    auto* p = impl->phonemizer (british);
    return p != nullptr ? p->phonemize (text.toStdString()) : std::string();
}

std::vector<float> KokoroEngine::synthesize (const SynthRequest& req, std::function<void (float)> progress,
                                             const std::atomic<bool>& cancel, juce::String& error)
{
    std::vector<float> out;
    if (! loaded) { error = "Voice engine not loaded"; return out; }
    auto* ph = impl->phonemizer (req.british);
    if (ph == nullptr) { error = "Pronunciation lexicon missing"; return out; }

    const auto phon = toU32 (ph->phonemize (req.text.toStdString()));

    // In Sätze zerlegen, zu lange Sätze an Kommas bzw. Leerzeichen teilen
    struct Chunk { std::vector<int64_t> tokens; float pause; };
    std::vector<Chunk> chunks;
    {
        std::vector<std::pair<std::u32string, float>> sentences;
        std::u32string cur;
        for (size_t i = 0; i < phon.size(); ++i)
        {
            cur.push_back (phon[i]);
            const bool stop = isStop (phon[i]) && (i + 1 == phon.size() || phon[i + 1] == U' ' || phon[i + 1] == U'"' || phon[i + 1] == U'”');
            if (stop) { sentences.push_back ({ cur, 0.22f }); cur.clear(); }
        }
        if (! cur.empty()) sentences.push_back ({ cur, 0.0f });

        auto toTokens = [&] (const std::u32string& s)
        {
            std::vector<int64_t> t;
            for (auto c : s)
                if (auto it = impl->vocab.find (c); it != impl->vocab.end()) t.push_back (it->second);
            while (! t.empty() && t.front() == 16) t.erase (t.begin()); // führende Leerzeichen
            while (! t.empty() && t.back() == 16) t.pop_back();
            return t;
        };
        for (auto& [s, pause] : sentences)
        {
            auto toks = toTokens (s);
            if (toks.empty()) continue;
            if ((int) toks.size() <= maxTokens - 10) { chunks.push_back ({ toks, pause }); continue; }
            // zu lang: an Satzteilen trennen
            std::u32string part;
            std::vector<std::u32string> parts;
            for (auto c : s)
            {
                part.push_back (c);
                if (isClause (c) && part.size() > 40) { parts.push_back (part); part.clear(); }
            }
            if (! part.empty()) parts.push_back (part);
            std::u32string acc;
            for (auto& p : parts)
            {
                if (! acc.empty() && (int) toTokens (acc + p).size() > maxTokens - 10)
                {
                    chunks.push_back ({ toTokens (acc), 0.12f });
                    acc.clear();
                }
                acc += p;
            }
            if (! acc.empty())
            {
                auto t = toTokens (acc);
                // notfalls hart schneiden
                while ((int) t.size() > maxTokens - 10)
                {
                    std::vector<int64_t> head (t.begin(), t.begin() + (maxTokens - 10));
                    chunks.push_back ({ head, 0.05f });
                    t.erase (t.begin(), t.begin() + (maxTokens - 10));
                }
                if (! t.empty()) chunks.push_back ({ t, pause });
            }
        }
    }
    if (chunks.empty()) { error = "Nothing to say in this text"; return out; }

    try
    {
        for (size_t i = 0; i < chunks.size(); ++i)
        {
            if (cancel.load()) { out.clear(); error = "cancelled"; return out; }
            const auto& ch = chunks[i];
            auto st = impl->style (req.voiceA, req.voiceB, req.blend, (int) ch.tokens.size());
            auto audio = impl->run (ch.tokens, st, juce::jlimit (0.5f, 2.0f, req.speed));
            trimSilence (audio);
            out.insert (out.end(), audio.begin(), audio.end());
            if (i + 1 < chunks.size())
                out.insert (out.end(), (size_t) (ch.pause / juce::jmax (0.5f, req.speed) * sampleRate), 0.0f);
            if (progress) progress ((float) (i + 1) / (float) chunks.size());
        }
    }
    catch (const std::exception& e)
    {
        error = juce::String ("Speech error: ") + e.what();
        out.clear();
    }
    return out;
}
} // namespace tts
