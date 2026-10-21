#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

// Etappe 4: Arp / Step-Sequencer.
// Laeuft, solange eine MIDI-Note gehalten wird, im Takt von Ableton (oder mit eigener Uhr,
// wenn der Transport steht). Erzeugt die Noten, die danach Generator oder Slot spielen.

namespace bleep
{

struct Step
{
    int semis = 0;      // Abstand zum Grundton in Halbtoenen (0 .. 63), wird auf die Tonleiter gerundet
    int prob = 100;     // Wahrscheinlichkeit 0 .. 100
    int vel = 90;       // Velocity 1 .. 127, ab 100 = Akzent
    bool locked = false;
};

// Ein Step passt in ein int, damit UI und Audio-Thread ohne Locks darauf zugreifen koennen
inline int packStep (const Step& s)
{
    return (juce::jlimit (0, 63, s.semis)) | (juce::jlimit (0, 100, s.prob) << 6)
         | (juce::jlimit (1, 127, s.vel) << 13) | ((s.locked ? 1 : 0) << 20);
}

inline Step unpackStep (int v)
{
    Step s;
    s.semis  = v & 63;
    s.prob   = (v >> 6) & 127;
    s.vel    = (v >> 13) & 127;
    s.locked = ((v >> 20) & 1) != 0;
    return s;
}

// Kleiner, schneller Zufallsgenerator mit festem Seed (gleicher Seed = gleiche Folge)
struct SplitMix
{
    explicit SplitMix (uint64_t seed) : state (seed) {}

    uint64_t next()
    {
        uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    double uniform() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    int below (int n) { return n <= 1 ? 0 : (int) (uniform() * n) % n; }

    uint64_t state;
};

inline uint64_t mixKey (uint64_t a, uint64_t b, uint64_t c)
{
    SplitMix r (a * 0x100000001B3ull ^ (b + 0x51ED27ull) * 0xC2B2AE3D27D4EB4Full ^ c * 0x165667B19E3779F9ull);
    r.next();
    return r.next();
}

inline double hash01 (uint64_t a, uint64_t b, uint64_t c)
{
    return (double) (mixKey (a, b, c) >> 11) * (1.0 / 9007199254740992.0);
}

enum Scale { scaleChroma = 0, scaleMinor, scaleMajor, scalePhrygian };

inline bool inScale (int rel, int scale)
{
    static constexpr bool minor[12]    = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 };
    static constexpr bool major[12]    = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
    static constexpr bool phrygian[12] = { 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0 };
    const int r = ((rel % 12) + 12) % 12;
    switch (scale)
    {
        case scaleMinor:    return minor[r];
        case scaleMajor:    return major[r];
        case scalePhrygian: return phrygian[r];
        default:            return true;
    }
}

// Rundet auf den naechsten Ton der Tonleiter (bei Gleichstand nach unten)
inline int snapToScale (int rel, int scale)
{
    for (int d = 0; d < 12; ++d)
    {
        if (inScale (rel - d, scale)) return rel - d;
        if (inScale (rel + d, scale)) return rel + d;
    }
    return rel;
}

struct ArpSettings
{
    bool on = true;
    double stepBeats = 0.25;   // Laenge eines Steps in Vierteln
    int numSteps = 16;
    float swing = 0.0f;        // 0 .. 0.75
    float gate = 0.8f;         // 0.1 .. 1
    int mode = 0;              // Up, Down, Up/Down, Random, As Played
    int scale = scaleChroma;
    int range = 1;             // Oktaven 0 .. 3
    bool restart = true;
    float density = 0.7f;      // Anteil der Steps mit eigener Tonhoehe beim Roll
    int autoEvery = 0;         // 0 = aus, sonst alle N Durchlaeufe neu wuerfeln
    bool seedMode = true;      // true = reproduzierbar ueber Seed und Position
    int seed = 4271;
};

struct Transport
{
    bool playing = false;
    double ppq = 0.0;
    double bpm = 120.0;
};

class Arpeggiator
{
public:
    static constexpr int kMaxSteps = 16;

    Arpeggiator()
    {
        static constexpr int semis[kMaxSteps] = { 12, 0, 7, 0, 3, 12, 0, 10, 7, 0, 15, 3, 0, 7, 12, 5 };
        static constexpr int probs[kMaxSteps] = { 100, 60, 100, 30, 80, 100, 40, 90, 100, 50, 70, 100, 20, 80, 100, 60 };
        for (int i = 0; i < kMaxSteps; ++i)
            steps[(size_t) i].store (packStep ({ semis[i], probs[i], i % 4 == 0 ? 115 : 90, false }));
    }

    std::array<std::atomic<int>, kMaxSteps> steps;
    std::atomic<int> currentStep { -1 };
    std::atomic<int> accentCounter { 0 };

    Step getStep (int i) const       { return unpackStep (steps[(size_t) i].load (std::memory_order_relaxed)); }
    void setStep (int i, const Step& s) { steps[(size_t) i].store (packStep (s), std::memory_order_relaxed); }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset()
    {
        numHeld = 0;
        numPending = 0;
        kStart = 0;
        lastK = std::numeric_limits<int64_t>::min();
        freeBeat = 0.0;
        clock = 0;
        currentStep.store (-1);
    }

    // Wuerfelt die Tonhoehen aller nicht gesperrten Steps neu (innerhalb Tonleiter und Range)
    void roll (const ArpSettings& s, SplitMix& rng)
    {
        int tones[48];
        int numTones = 0;
        for (int t = 1; t <= 12 * s.range && numTones < 48; ++t)
            if (inScale (t, s.scale))
                tones[numTones++] = t;

        for (int i = 0; i < kMaxSteps; ++i)
        {
            auto st = getStep (i);
            if (st.locked)
                continue;
            const bool own = rng.uniform() < s.density;
            st.semis = (own && numTones > 0) ? tones[rng.below (numTones)] : 0;
            setStep (i, st);
        }
    }

    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int n, const ArpSettings& s, const Transport& t)
    {
        out.clear();

        if (! s.on)
        {
            if (wasOn)
            {
                releaseAll (out, 0);
                numHeld = 0;
            }
            wasOn = false;
            out.addEvents (in, 0, n, 0);
            return;
        }
        wasOn = true;

        beatsPerSample = t.bpm / 60.0 / sr;
        if (t.playing != lastPlaying)
        {
            // Wechsel zwischen Ableton-Takt und eigener Uhr: neu am Raster einhaken
            lastPlaying = t.playing;
            freeBeat = 0.0;
            if (numHeld > 0)
                restartAt (0, t, s);
        }

        // Sprung im Song (Loop, Zurueckspulen): Pattern am neuen Ort neu einhaken
        if (t.playing && numHeld > 0 && std::abs (t.ppq - nextBeat) > 0.01)
            restartAt (0, t, s);

        int pos = 0;
        for (const auto meta : in)
        {
            const int at = juce::jlimit (0, n, meta.samplePosition);
            generate (pos, at, out, s, t);
            pos = at;

            const auto msg = meta.getMessage();
            if (msg.isNoteOn())
                noteOn (msg.getNoteNumber(), at, out, s, t);
            else if (msg.isNoteOff())
                noteOff (msg.getNoteNumber(), at, out);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                numHeld = 0;
                releaseAll (out, at);
            }
            else
                out.addEvent (msg, at);
        }
        generate (pos, n, out, s, t);
        flushPending (out, n);

        clock += n;
        if (t.playing)
            nextBeat = t.ppq + n * beatsPerSample;
        else
            freeBeat += n * beatsPerSample;
    }

private:
    struct Pending { int note; int64_t offAt; };

    double beatAt (int sample, const Transport& t) const
    {
        return (t.playing ? t.ppq : freeBeat) + sample * beatsPerSample;
    }

    void restartAt (int sample, const Transport& t, const ArpSettings& s)
    {
        if (t.playing)
        {
            kStart = (int64_t) std::ceil (beatAt (sample, t) / s.stepBeats - 1.0e-9);
        }
        else
        {
            // Eigene Uhr: erster Step sofort beim Anschlag
            freeBeat = -sample * beatsPerSample;
            kStart = 0;
        }
        lastK = kStart - 1;
    }

    void noteOn (int note, int at, juce::MidiBuffer& out, const ArpSettings& s, const Transport& t)
    {
        const bool wasEmpty = numHeld == 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == note)
                return;
        if (numHeld < (int) held.size())
            held[(size_t) numHeld++] = note;
        if (wasEmpty || s.restart)
            restartAt (at, t, s);
        juce::ignoreUnused (out);
    }

    void noteOff (int note, int at, juce::MidiBuffer& out)
    {
        for (int i = 0; i < numHeld; ++i)
        {
            if (held[(size_t) i] == note)
            {
                for (int j = i; j < numHeld - 1; ++j)
                    held[(size_t) j] = held[(size_t) j + 1];
                --numHeld;
                break;
            }
        }
        if (numHeld == 0)
            releaseAll (out, at);
    }

    void releaseAll (juce::MidiBuffer& out, int at)
    {
        for (int i = 0; i < numPending; ++i)
            out.addEvent (juce::MidiMessage::noteOff (1, pending[(size_t) i].note), at);
        numPending = 0;
        currentStep.store (-1, std::memory_order_relaxed);
    }

    void generate (int a, int b, juce::MidiBuffer& out, const ArpSettings& s, const Transport& t)
    {
        if (numHeld == 0 || b <= a)
            return;

        const double ba = beatAt (a, t), bb = beatAt (b, t);
        const double swingOffset = juce::jlimit (0.0f, 0.75f, s.swing) * 0.5 * s.stepBeats;
        const int64_t kFrom = juce::jmax (kStart, (int64_t) std::floor (ba / s.stepBeats) - 1);
        const int64_t kTo = (int64_t) std::floor (bb / s.stepBeats) + 1;

        for (int64_t k = kFrom; k <= kTo; ++k)
        {
            if (k <= lastK)
                continue;
            const double tk = (double) k * s.stepBeats + (((k - kStart) & 1) != 0 ? swingOffset : 0.0);
            if (tk < ba - 1.0e-9 || tk >= bb)
                continue;
            const int at = juce::jlimit (a, b - 1, a + (int) std::floor ((tk - ba) / beatsPerSample + 1.0e-4));
            lastK = k;
            trigger (k - kStart, at, out, s);
        }
    }

    void trigger (int64_t j, int at, juce::MidiBuffer& out, const ArpSettings& s)
    {
        const int numSteps = juce::jlimit (1, kMaxSteps, s.numSteps);
        const int ps = (int) (j % numSteps);
        const int64_t cycle = j / numSteps;

        // Auto-Roll am Anfang jedes N-ten Durchlaufs
        if (ps == 0 && cycle > 0 && s.autoEvery > 0 && cycle % s.autoEvery == 0)
        {
            SplitMix rng (s.seedMode ? mixKey ((uint64_t) s.seed, (uint64_t) (cycle / s.autoEvery), 0xA070ull)
                                     : (uint64_t) juce::Random::getSystemRandom().nextInt64());
            roll (s, rng);
        }

        currentStep.store (ps, std::memory_order_relaxed);
        const auto st = getStep (ps);

        const double r = s.seedMode ? hash01 ((uint64_t) s.seed, (uint64_t) j, 0x9Aull)
                                    : juce::Random::getSystemRandom().nextDouble();
        if (r * 100.0 >= (double) st.prob)
            return;

        // Grundton aus den gehaltenen Noten waehlen
        int sorted[16];
        for (int i = 0; i < numHeld; ++i)
            sorted[i] = held[(size_t) i];
        std::sort (sorted, sorted + numHeld);
        const int cnt = numHeld;
        int base = sorted[0];
        switch (s.mode)
        {
            case 0: base = sorted[j % cnt]; break;
            case 1: base = sorted[cnt - 1 - (int) (j % cnt)]; break;
            case 2:
            {
                if (cnt > 1)
                {
                    const int period = 2 * cnt - 2;
                    const int p = (int) (j % period);
                    base = sorted[p < cnt ? p : period - p];
                }
                break;
            }
            case 3:
            {
                const double h = s.seedMode ? hash01 ((uint64_t) s.seed, (uint64_t) j, 0x77ull)
                                            : juce::Random::getSystemRandom().nextDouble();
                base = sorted[juce::jlimit (0, cnt - 1, (int) (h * cnt))];
                break;
            }
            default: base = held[(size_t) (j % cnt)]; break;
        }

        const int root = sorted[0];
        const int semis = juce::jmin (st.semis, 12 * s.range);
        const int note = juce::jlimit (0, 127, root + snapToScale (base - root + semis, s.scale));

        // Gleiche Note noch an? Erst beenden, sonst haengt sie
        for (int i = 0; i < numPending; ++i)
        {
            if (pending[(size_t) i].note == note)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, note), at);
                pending[(size_t) i] = pending[(size_t) --numPending];
                break;
            }
        }

        out.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, st.vel)), at);
        if (st.vel >= 100)
            accentCounter.fetch_add (1, std::memory_order_relaxed);

        const int64_t gateSamples = juce::jmax ((int64_t) 1, (int64_t) (juce::jlimit (0.1f, 1.0f, s.gate) * s.stepBeats / beatsPerSample) - 1);
        if (numPending < (int) pending.size())
            pending[(size_t) numPending++] = { note, clock + at + gateSamples };
    }

    void flushPending (juce::MidiBuffer& out, int n)
    {
        for (int i = numPending; --i >= 0;)
        {
            const auto& p = pending[(size_t) i];
            if (p.offAt < clock + n)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, p.note), (int) juce::jlimit ((int64_t) 0, (int64_t) n - 1, p.offAt - clock));
                pending[(size_t) i] = pending[(size_t) --numPending];
            }
        }
    }

    double sr = 48000.0, beatsPerSample = 120.0 / 60.0 / 48000.0, freeBeat = 0.0, nextBeat = 0.0;
    int64_t kStart = 0, lastK = std::numeric_limits<int64_t>::min(), clock = 0;
    std::array<int, 16> held {};
    int numHeld = 0;
    std::array<Pending, 64> pending {};
    int numPending = 0;
    bool lastPlaying = false, wasOn = true;
};

} // namespace bleep
