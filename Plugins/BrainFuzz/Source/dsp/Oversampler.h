#pragma once

// Brainfuzz – 4x-Oversampling mit zwei linearphasigen Halbband-FIR-Stufen (polyphas)
// Stufe 1: 47 Taps (Basis <-> 2x), Stufe 2: 19 Taps (2x <-> 4x)
// Die Gesamtlatenz ist auf ganze Basis-Samples aufgerundet (kLatency), damit
// der saubere Weg mit einer einfachen Verzoegerung exakt zeitgleich gehalten werden kann.

#include <array>
#include <vector>
#include <cmath>

namespace bfz
{

inline double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 40; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}

// Halbband-Koeffizienten (Kaiser-Fenster): h[c] = 0.5, gerade Abstaende zur Mitte = 0
inline std::vector<double> halfbandTaps (int n, double beta)
{
    std::vector<double> h ((size_t) n);
    const int c = (n - 1) / 2;
    for (int k = 0; k < n; ++k)
    {
        const int m = k - c;
        double v = (m == 0) ? 0.5 : std::sin (3.14159265358979323846 * m / 2.0) / (3.14159265358979323846 * m);
        const double r = 2.0 * k / (n - 1) - 1.0;
        v *= besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
        h[(size_t) k] = v;
    }
    // DC exakt 1
    double s = 0.0;
    for (auto x : h) s += x;
    for (auto& x : h) x /= s;
    return h;
}

// Ringpuffer mit doppelter Laenge: zusammenhaengendes Lesen ohne Modulo
template <int N>
struct History
{
    std::array<float, (size_t) (2 * N)> buf {};
    int pos = 0;
    void reset() { buf.fill (0.0f); pos = 0; }
    inline void push (float x)
    {
        pos = (pos == 0) ? N - 1 : pos - 1;
        buf[(size_t) pos] = x;
        buf[(size_t) pos + N] = x;
    }
    // [0] = neuestes, [i] = i Schritte zurueck
    inline const float* data() const { return buf.data() + pos; }
};

// Halbband-Stufe: Hochtasten (1 -> 2) und Heruntertasten (2 -> 1)
template <int Taps>
struct HalfbandStage
{
    static_assert ((Taps - 3) % 4 == 0, "Taps muss 4k+3 sein");
    static constexpr int kC = (Taps - 1) / 2;          // Mitte (ungerade)
    static constexpr int kSide = (Taps + 1) / 2;       // Anzahl Taps an geraden Positionen
    static constexpr int kUpCentreDelay = (kC - 1) / 2;
    static constexpr int kDownOddDelay = (kC + 1) / 2;

    std::array<float, (size_t) kSide> side {}; // h[2i]
    History<kSide> upHist, downEven;
    History<kDownOddDelay + 1> downOdd;

    void init (double beta)
    {
        auto h = halfbandTaps (Taps, beta);
        for (int i = 0; i < kSide; ++i) side[(size_t) i] = (float) h[(size_t) (2 * i)];
    }

    void reset() { upHist.reset(); downEven.reset(); downOdd.reset(); }

    // x -> (a, b): zwei Samples der doppelten Rate
    inline void up (float x, float& a, float& b)
    {
        upHist.push (x);
        const float* d = upHist.data();
        float s = 0.0f;
        for (int i = 0; i < kSide; ++i) s += side[(size_t) i] * d[i];
        a = 2.0f * s;
        b = d[kUpCentreDelay];
    }

    // (a, b) -> y
    inline float down (float a, float b)
    {
        downEven.push (a);
        const float* e = downEven.data();
        float s = 0.0f;
        for (int i = 0; i < kSide; ++i) s += side[(size_t) i] * e[i];
        const float odd = downOdd.data()[kDownOddDelay - 1];
        downOdd.push (b);
        return s + 0.5f * odd;
    }
};

class Oversampler4x
{
public:
    static constexpr int kTaps1 = 47, kTaps2 = 19;
    // Latenz: Stufe 1 = 2*c1 Samples bei 2x = c1 Basis, Stufe 2 = 2*c2 bei 4x = c2/2 Basis (+ Ausgleich)
    static constexpr int kC1 = (kTaps1 - 1) / 2, kC2 = (kTaps2 - 1) / 2;
    static constexpr int kExtra4x = (4 - (2 * kC2) % 4) % 4;   // Ausgleich bei 4x auf ganze Basis-Samples
    static constexpr int kLatency = kC1 + (2 * kC2 + kExtra4x) / 4;

    Oversampler4x()
    {
        s1.init (9.0);
        s2.init (7.0);
    }

    void reset() { s1.reset(); s2.reset(); extra.fill (0.0f); ePos = 0; }

    // f bekommt jedes der 4 Samples und gibt das bearbeitete zurueck
    template <typename Fn>
    inline float process (float x, Fn&& f)
    {
        float a, b;
        s1.up (x, a, b);
        float u[4];
        s2.up (a, u[0], u[1]);
        s2.up (b, u[2], u[3]);
        for (auto& v : u) v = f (v);
        // Ausgleichsverzoegerung bei 4x
        if constexpr (kExtra4x > 0)
            for (auto& v : u)
            {
                const float o = extra[(size_t) ePos];
                extra[(size_t) ePos] = v;
                ePos = (ePos + 1) % kExtra4x;
                v = o;
            }
        const float da = s2.down (u[0], u[1]);
        const float db = s2.down (u[2], u[3]);
        return s1.down (da, db);
    }

private:
    HalfbandStage<kTaps1> s1;
    HalfbandStage<kTaps2> s2; // der 2x-Strom (a, b, a', b' ...) laeuft nacheinander durch
    std::array<float, 4> extra {};
    int ePos = 0;
};

} // namespace bfz
