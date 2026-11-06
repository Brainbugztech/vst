#pragma once

// Gain Lock Engine: Lautheitsmessung (BS.1770 / RMS), Kopplung Quelle <-> Lock,
// Latenz-Erkennung (GCC-PHAT). Reines C++, ohne JUCE, damit es sich offline testen lässt.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace gl
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db)   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)    { return g > 1.0e-9f ? 20.0f * std::log10 (g) : -180.0f; }

//==============================================================================
// Biquad (Direct Form II transposed, double)
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline float process (float xf) noexcept
    {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

// K-Filter nach ITU-R BS.1770 (Koeffizienten-Herleitung wie libebur128, für jede Abtastrate)
struct KWeighting
{
    Biquad shelf, hp;

    void prepare (double fs)
    {
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;
            hp.a1 = 2.0 * (K * K - 1.0) / a0;
            hp.a2 = (1.0 - K / Q + K * K) / a0;
        }
        reset();
    }
    inline float process (float x) noexcept { return hp.process (shelf.process (x)); }
    void reset() noexcept { shelf.reset(); hp.reset(); }
};

//==============================================================================
// Misst die Lautheit in 100-ms-Blöcken, Fenster wählbar (bis 10 s).
// LUFS-Modus: K-Filter + Kanalsumme (BS.1770). RMS-Modus: ungefiltert, gleiche Skala.
class LoudnessMeter
{
public:
    static constexpr int kMaxBlocks = 100;   // 100 x 100 ms = 10 s
    static constexpr int kMaxChannels = 2;

    void prepare (double fs)
    {
        sampleRate = fs;
        blockLen = std::max (1, (int) std::lround (fs * 0.1));
        for (auto& k : kf) k.prepare (fs);
        reset();
    }

    void reset()
    {
        for (auto& k : kf) k.reset();
        acc = 0.0; accCount = 0; filled = 0; head = 0;
        std::fill (blocks.begin(), blocks.end(), 0.0);
    }

    void setWeighted (bool k) { weighted = k; }

    // Ein Sample (alle Kanäle). Gibt true zurück, wenn ein 100-ms-Block fertig ist.
    inline bool push (const float* ch, int numCh) noexcept
    {
        double sum = 0.0;
        for (int c = 0; c < std::min (numCh, kMaxChannels); ++c)
        {
            const float v = weighted ? kf[(size_t) c].process (ch[c]) : ch[c];
            sum += (double) v * v;
        }
        // Mono wie BS.1770 mit einem Kanal; RMS-Modus mittelt Kanäle, damit Mono/Stereo vergleichbar bleibt
        if (! weighted && numCh > 1) sum /= (double) std::min (numCh, kMaxChannels);
        acc += sum;
        if (++accCount >= blockLen)
        {
            blocks[(size_t) head] = acc / (double) accCount;
            head = (head + 1) % kMaxBlocks;
            filled = std::min (filled + 1, kMaxBlocks);
            acc = 0.0; accCount = 0;
            return true;
        }
        return false;
    }

    // Mittlere Leistung der letzten n Blöcke -> LUFS (bzw. dBFS-RMS + gleicher Offset)
    float loudness (int nBlocks) const noexcept
    {
        const int n = std::min (std::max (1, nBlocks), filled);
        if (n <= 0) return -180.0f;
        double s = 0.0;
        for (int i = 1; i <= n; ++i)
            s += blocks[(size_t) ((head - i + kMaxBlocks) % kMaxBlocks)];
        const double ms = s / n;
        if (ms <= 1.0e-18) return -180.0f;
        return (float) (-0.691 + 10.0 * std::log10 (ms));
    }

    int blocksFilled() const noexcept { return filled; }
    int samplesPerBlock() const noexcept { return blockLen; }

private:
    double sampleRate = 48000.0;
    int blockLen = 4800;
    bool weighted = true;
    std::array<KWeighting, kMaxChannels> kf;
    std::array<double, kMaxBlocks> blocks {};
    double acc = 0.0;
    int accCount = 0, filled = 0, head = 0;
};

//==============================================================================
// Ringpuffer, den die Quelle schreibt und Lock (Audio- und Analyse-Thread) liest.
// atomic<float> mit relaxed-Zugriff: auf x86/ARM so schnell wie normale floats, aber ohne Data Race.
struct SharedRing
{
    static constexpr int kBits = 19;                 // 524288 Samples: ~10,9 s @ 48 kHz
    static constexpr int64_t kSize = int64_t (1) << kBits;
    static constexpr int64_t kMask = kSize - 1;

    std::unique_ptr<std::atomic<float>[]> data[2];
    std::unique_ptr<std::atomic<float>[]> mono;     // L+R für die Latenz-Analyse

    SharedRing()
    {
        for (auto& d : data) d.reset (new std::atomic<float>[(size_t) kSize]);
        mono.reset (new std::atomic<float>[(size_t) kSize]);
        for (int64_t i = 0; i < kSize; ++i)
        {
            data[0][(size_t) i].store (0.0f, std::memory_order_relaxed);
            data[1][(size_t) i].store (0.0f, std::memory_order_relaxed);
            mono[(size_t) i].store (0.0f, std::memory_order_relaxed);
        }
    }
};

//==============================================================================
// Ein Abgriff in der Kette: 0 = Quelle (Anfang), 1..3 = Zwischenpunkte.
// Genau eine Instanz besitzt ihn und schreibt, Locks lesen.
struct Tap
{
    std::atomic<int> owner { 0 };                    // Instanz-ID (0 = frei)
    std::atomic<SharedRing*> ring { nullptr };       // einmal angelegt, lebt bis zum Entladen
    std::atomic<int64_t> writePos { 0 };             // Gesamtzahl geschriebener Samples
    std::atomic<uint32_t> heartbeat { 0 };           // +1 pro Block
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> numChannels { 2 };

    std::mutex nameLock;                             // nur Message-Thread
    char name[64] {};

    void setName (const char* n)
    {
        std::lock_guard<std::mutex> l (nameLock);
        std::strncpy (name, n != nullptr ? n : "", sizeof (name) - 1);
        name[sizeof (name) - 1] = 0;
    }
    void getName (char* out, size_t len)
    {
        std::lock_guard<std::mutex> l (nameLock);
        std::strncpy (out, name, len - 1);
        out[len - 1] = 0;
    }
};

// Eine Kopplungs-Gruppe: Quelle + bis zu drei Zwischenpunkte, beliebig viele Locks.
struct LinkSlot
{
    static constexpr int kNumTaps = 4;
    std::array<Tap, kNumTaps> taps;
    std::atomic<uint32_t> lockHeartbeat { 0 };       // +1 pro Lock-Block (Anzeige in Quelle/Punkt)
};

// Prozessweit geteilt: alle Gain-Lock-Instanzen im selben Host-Prozess sehen denselben Hub.
class LinkHub
{
public:
    static constexpr int kNumGroups = 8;

    static LinkHub& get()
    {
        static LinkHub hub;
        return hub;
    }

    LinkSlot& slot (int group) { return slots[(size_t) std::clamp (group, 0, kNumGroups - 1)]; }
    Tap& tap (int group, int t) { return slot (group).taps[(size_t) std::clamp (t, 0, LinkSlot::kNumTaps - 1)]; }

    // Nur außerhalb des Audio-Threads aufrufen (legt Speicher an).
    SharedRing* ensureRing (int group, int t)
    {
        auto& tp = tap (group, t);
        if (auto* r = tp.ring.load (std::memory_order_acquire)) return r;
        std::lock_guard<std::mutex> l (allocLock);
        if (auto* r = tp.ring.load (std::memory_order_acquire)) return r;
        rings.push_back (std::make_unique<SharedRing>());
        tp.ring.store (rings.back().get(), std::memory_order_release);
        return rings.back().get();
    }

    int newInstanceId() { return nextId.fetch_add (1) + 1; }

private:
    LinkHub() = default;
    std::array<LinkSlot, kNumGroups> slots;
    std::mutex allocLock;
    std::vector<std::unique_ptr<SharedRing>> rings;
    std::atomic<int> nextId { 0 };
};

//==============================================================================
// Kleine iterative Radix-2-FFT (nur für den Analyse-Thread).
class FFT
{
public:
    explicit FFT (int order) : n (1 << order)
    {
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
            tw[(size_t) i] = std::polar (1.0f, (float) (-2.0 * kPi * i / n));
    }

    int size() const { return n; }

    void perform (std::vector<std::complex<float>>& a, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (a[(size_t) i], a[(size_t) rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    auto w = tw[(size_t) (j * step)];
                    if (inverse) w = std::conj (w);
                    const auto u = a[(size_t) (i + j)];
                    const auto v = a[(size_t) (i + j + half)] * w;
                    a[(size_t) (i + j)] = u + v;
                    a[(size_t) (i + j + half)] = u - v;
                }
        }
        if (inverse)
        {
            const float s = 1.0f / (float) n;
            for (auto& x : a) x *= s;
        }
    }

private:
    int n;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};

//==============================================================================
// Findet den Versatz zwischen Quelle und dem Signal am Ende der Kette (GCC-PHAT).
// lockSeg[i] entspricht srcSeg[i + maxLag - L]  ->  gesucht: L in [0, maxLag]
class LatencyFinder
{
public:
    struct Result
    {
        bool valid = false;
        int lag = 0;            // Samples
        float confidence = 0;   // Peak / mittlerer Betrag
        bool inverted = false;  // Polarität in der Kette gedreht
    };

    LatencyFinder (int segLen, int maxLag) : N (segLen), maxLagSamples (maxLag)
    {
        int order = 1;
        while ((1 << order) < N + N + maxLag) ++order;
        fft = std::make_unique<FFT> (order);
        A.resize ((size_t) fft->size());
        B.resize ((size_t) fft->size());
    }

    int segmentLength() const { return N; }
    int maxLag() const { return maxLagSamples; }

    // src: N + maxLag Samples, lock: N Samples
    Result analyse (const float* src, const float* lock)
    {
        Result res;
        const int M = fft->size();

        double eS = 0, eL = 0;
        for (int i = 0; i < N + maxLagSamples; ++i) eS += (double) src[i] * src[i];
        for (int i = 0; i < N; ++i) eL += (double) lock[i] * lock[i];
        // zu leise -> keine Aussage (ca. -60 dBFS RMS)
        if (eS / (N + maxLagSamples) < 1.0e-6 || eL / N < 1.0e-6) return res;

        std::fill (A.begin(), A.end(), std::complex<float> {});
        std::fill (B.begin(), B.end(), std::complex<float> {});
        // Hann-Fenster nur am Rand (sanfter Ein-/Ausstieg), Rest unverändert
        const int edge = std::max (1, N / 16);
        for (int i = 0; i < N; ++i)
        {
            float w = 1.0f;
            if (i < edge) w = 0.5f - 0.5f * std::cos ((float) kPi * i / edge);
            else if (i >= N - edge) w = 0.5f - 0.5f * std::cos ((float) kPi * (N - 1 - i) / edge);
            B[(size_t) i] = lock[i] * w;
        }
        for (int i = 0; i < N + maxLagSamples; ++i) A[(size_t) i] = src[i];

        fft->perform (A, false);
        fft->perform (B, false);

        // c[m] = sum_i lock[i] * src[i + m]  ->  IFFT( conj(FFT(lock)) * FFT(src) ), PHAT-gewichtet
        for (int k = 0; k < M; ++k)
        {
            auto x = std::conj (B[(size_t) k]) * A[(size_t) k];
            const float mag = std::abs (x);
            A[(size_t) k] = mag > 1.0e-12f ? x / mag : std::complex<float> {};
        }
        fft->perform (A, true);

        int best = 0; float bestAbs = 0.0f, bestVal = 0.0f;
        double sumAbs = 0.0;
        for (int m = 0; m <= maxLagSamples; ++m)
        {
            const float v = A[(size_t) m].real();
            sumAbs += std::abs (v);
            if (std::abs (v) > bestAbs) { bestAbs = std::abs (v); bestVal = v; best = m; }
        }
        const float meanAbs = (float) (sumAbs / (maxLagSamples + 1));
        res.confidence = meanAbs > 0 ? bestAbs / meanAbs : 0.0f;
        res.lag = maxLagSamples - best;
        res.inverted = bestVal < 0.0f;
        res.valid = res.confidence > 8.0f;
        return res;
    }

private:
    int N, maxLagSamples;
    std::unique_ptr<FFT> fft;
    std::vector<std::complex<float>> A, B;
};

//==============================================================================
// Rechnet aus Quell- und Ketten-Lautheit die Korrektur und glättet sie.
struct GainComputer
{
    float currentDb = 0.0f;   // gerade angewendet
    float targetDb  = 0.0f;   // Ziel aus der Messung

    // Wird pro 100-ms-Block aufgerufen. Gibt true zurück, wenn neu gemessen wurde.
    bool update (float srcLufs, float postLufs, float rangeDb, float gateLufs)
    {
        // Pause, Stille oder Fade: Korrektur halten statt hochzuziehen
        if (srcLufs < gateLufs || postLufs < gateLufs - 10.0f) return false;
        targetDb = std::clamp (srcLufs - postLufs, -rangeDb, rangeDb);
        return true;
    }

    // Exponentielle Annäherung an das Ziel, timeConst in Sekunden
    float stepTowards (int numSamples, double fs, float timeConstSec)
    {
        const double a = std::exp (-(double) numSamples / (std::max (0.01, (double) timeConstSec) * fs));
        currentDb = (float) (targetDb + (currentDb - targetDb) * a);
        return currentDb;
    }
};
//==============================================================================
// Auswertung des Blind-Vorliebetests
struct BlindVerdict
{
    enum Kind { notEnough, reliable, trend, noPreference, inaudible };
    Kind kind = notEnough;
    int winner = -1;        // 0 = A, 1 = B
    double p = 1.0;         // zweiseitiger Binomial-p-Wert
};

// Zweiseitiger exakter Binomialtest gegen 50/50
inline double binomTwoSided (int k, int n)
{
    if (n <= 0) return 1.0;
    k = std::max (k, n - k);
    double tail = 0.0;
    for (int i = k; i <= n; ++i)
    {
        double c = 1.0;   // n über i
        for (int j = 1; j <= i; ++j) c = c * (n - i + j) / j;
        tail += c * std::pow (0.5, n);
    }
    return std::min (1.0, 2.0 * tail);
}

inline BlindVerdict judgeBlind (int prefA, int prefB, int none)
{
    BlindVerdict v;
    const int total = prefA + prefB + none;
    const int decided = prefA + prefB;
    if (total < 3) return v;
    // Überwiegend "kein Unterschied": praktisch nicht hörbar
    if (none * 2 > total) { v.kind = BlindVerdict::inaudible; return v; }
    v.p = binomTwoSided (std::max (prefA, prefB), decided);
    v.winner = prefA == prefB ? -1 : (prefA > prefB ? 0 : 1);
    if (v.winner < 0)       v.kind = BlindVerdict::noPreference;
    else if (v.p < 0.05)    v.kind = BlindVerdict::reliable;
    else if (v.p < 0.25)    v.kind = BlindVerdict::trend;
    else                    v.kind = BlindVerdict::noPreference;
    return v;
}
} // namespace gl
