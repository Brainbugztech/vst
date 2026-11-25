#pragma once

// Gemeinsame Sample-Bank fuer alle Orchestra-Instanzen.
// Samples liegen als OGG im Plugin (BinaryData) und werden pro Gruppe erst bei Bedarf
// im Hintergrund dekodiert (int16, spart Speicher). Ungenutzte Gruppen werden nach
// einer Weile wieder freigegeben.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "SampleManifest.h"
#include "OrchestraSamples.h"
#include <atomic>
#include <set>
#include <vector>
#include <map>
#include <array>

namespace orch
{

struct Sample
{
    std::vector<int16_t> l, r;
    int length = 0, loopStart = -1, loopEnd = -1;
    int root = 60, layer = 0, rr = 0;
    bool loops() const noexcept { return loopStart >= 0 && loopEnd > loopStart; }
};

struct GroupData
{
    std::vector<Sample> samples;
    int numLayers = 1;
    int minRoot = 127, maxRoot = 0;

    // naechster Grundton zu einer Note, dann Layer und Round-Robin
    const Sample* pick (int note, float vel01, int rrCounter) const noexcept
    {
        int bestRoot = -1, bestDist = 1000;
        for (auto& s : samples)
        {
            // bei gleichem Abstand lieber den tieferen Grundton (hochpitchen klingt natuerlicher)
            const int d = std::abs (s.root - note) * 2 + (s.root > note ? 1 : 0);
            if (d < bestDist) { bestDist = d; bestRoot = s.root; }
        }
        if (bestRoot < 0) return nullptr;
        const int layer = juce::jlimit (0, numLayers - 1, (int) (vel01 * (float) numLayers));
        const Sample* cands[16]; int n = 0;
        int bestLayerDist = 1000;
        for (auto& s : samples)
            if (s.root == bestRoot)
                bestLayerDist = std::min (bestLayerDist, std::abs (s.layer - layer));
        for (auto& s : samples)
            if (s.root == bestRoot && std::abs (s.layer - layer) == bestLayerDist && n < 16)
                cands[n++] = &s;
        return n > 0 ? cands[rrCounter % n] : nullptr;
    }
};

class SampleBank : private juce::Thread, private juce::Timer
{
public:
    SampleBank() : juce::Thread ("Orchestra Sample Loader")
    {
        formats.registerBasicFormats();
        for (auto& g : groups) g.store (nullptr);
        startThread (juce::Thread::Priority::low);
        startTimer (2000);
    }

    ~SampleBank() override
    {
        stopTimer();
        signalThreadShouldExit();
        notify();
        stopThread (4000);
        for (auto& g : groups) delete g.load();
        for (auto& gy : graveyard) delete gy.first;
    }

    // Audio-Thread: fertig geladene Gruppe oder nullptr
    const GroupData* get (int group) const noexcept
    {
        return juce::isPositiveAndBelow (group, (int) G_NUM) ? groups[(size_t) group].load (std::memory_order_acquire) : nullptr;
    }

    // Eine Instanz meldet, welche Gruppen sie gerade braucht (Message-Thread oder Audio-Thread-frei)
    void setWanted (void* owner, const std::set<int>& wanted)
    {
        {
            const juce::ScopedLock sl (lock);
            users[owner] = wanted;
            for (auto g : wanted) lastUsed[(size_t) g] = juce::Time::getMillisecondCounter();
        }
        notify();
    }

    void removeUser (void* owner)
    {
        const juce::ScopedLock sl (lock);
        users.erase (owner);
    }

    bool isLoaded (const std::set<int>& wanted) const
    {
        for (auto g : wanted)
            if (get (g) == nullptr) return false;
        return true;
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            std::set<int> todo;
            {
                const juce::ScopedLock sl (lock);
                for (auto& u : users)
                    for (auto g : u.second)
                        if (get (g) == nullptr) todo.insert (g);
            }
            if (todo.empty()) { wait (500); continue; }
            for (auto g : todo)
            {
                if (threadShouldExit()) return;
                groups[(size_t) g].store (loadGroup (g), std::memory_order_release);
            }
        }
    }

    GroupData* loadGroup (int group)
    {
        auto* gd = new GroupData();
        for (auto& def : sampleDefs)
        {
            if (def.group != group) continue;
            int size = 0;
            auto* data = OrchSamples::getNamedResource (def.res, size);
            if (data == nullptr) continue;
            auto stream = std::make_unique<juce::MemoryInputStream> (data, (size_t) size, false);
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (std::move (stream)));
            if (reader == nullptr) continue;
            const int len = (int) reader->lengthInSamples;
            juce::AudioBuffer<float> buf (2, len);
            reader->read (&buf, 0, len, 0, true, true);
            Sample s;
            s.length = len;
            s.loopStart = def.loopStart; s.loopEnd = std::min (def.loopEnd, len);
            s.root = def.root; s.layer = def.layer; s.rr = def.rr;
            s.l.resize ((size_t) len + 4); s.r.resize ((size_t) len + 4);
            auto* L = buf.getReadPointer (0);
            auto* R = buf.getReadPointer (reader->numChannels > 1 ? 1 : 0);
            for (int i = 0; i < len; ++i)
            {
                s.l[(size_t) i] = (int16_t) juce::jlimit (-32767.0f, 32767.0f, L[i] * 32767.0f);
                s.r[(size_t) i] = (int16_t) juce::jlimit (-32767.0f, 32767.0f, R[i] * 32767.0f);
            }
            // Loop-Samples: Polster hinter dem Loop-Ende mit dem Loop-Anfang fuellen (saubere Interpolation)
            if (s.loops())
                for (int k = 0; k < 4; ++k)
                {
                    s.l[(size_t) (s.loopEnd + k)] = s.l[(size_t) (s.loopStart + k)];
                    s.r[(size_t) (s.loopEnd + k)] = s.r[(size_t) (s.loopStart + k)];
                }
            gd->numLayers = std::max (gd->numLayers, def.layer + 1);
            gd->minRoot = std::min (gd->minRoot, def.root);
            gd->maxRoot = std::max (gd->maxRoot, def.root);
            gd->samples.push_back (std::move (s));
        }
        return gd;
    }

    // Gruppen, die seit 20 s keine Instanz mehr braucht, freigeben.
    // Instanzen stoppen ihre Stimmen sofort beim Umschalten, daher ist das sicher.
    void timerCallback() override
    {
        const juce::ScopedLock sl (lock);
        std::set<int> inUse;
        for (auto& u : users) inUse.insert (u.second.begin(), u.second.end());
        const auto now = juce::Time::getMillisecondCounter();
        for (int g = 0; g < (int) G_NUM; ++g)
        {
            if (inUse.count (g)) { lastUsed[(size_t) g] = now; continue; }
            if (groups[(size_t) g].load() != nullptr && now - lastUsed[(size_t) g] > 20000)
                graveyard.push_back ({ groups[(size_t) g].exchange (nullptr), now });
        }
        // erst nach weiteren 5 s wirklich loeschen: Stimmen pruefen jeden Block, ob ihre Gruppe noch gilt
        for (auto it = graveyard.begin(); it != graveyard.end();)
        {
            if (now - it->second > 5000) { delete it->first; it = graveyard.erase (it); }
            else ++it;
        }
    }

    juce::AudioFormatManager formats;
    std::array<std::atomic<GroupData*>, (size_t) G_NUM> groups;
    std::array<juce::uint32, (size_t) G_NUM> lastUsed {};
    std::map<void*, std::set<int>> users;
    std::vector<std::pair<GroupData*, juce::uint32>> graveyard;
    juce::CriticalSection lock;
};

} // namespace orch
