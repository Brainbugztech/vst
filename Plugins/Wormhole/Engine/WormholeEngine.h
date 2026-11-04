// =============================================================================
//  Wormhole Engine  –  Brain Bugz
//  High-quality time-stretch / pitch-shift core (phase vocoder, own design)
//
//  Techniques:
//   - Instantaneous frequency from a second analysis frame exactly one hop
//     earlier in the input (independent of the stretch ratio, very accurate)
//   - Pitch shift by spectral frequency mapping (no resampling stage)
//   - Phases referenced to the frame centre, identity phase locking around peaks
//   - Shared mid-channel phase evolution -> stereo image stays intact
//   - Transient detection, phase reset at attacks, time-locking (attacks keep
//     original speed), pre-echo masking with adaptive overlap normalisation
//   - Stationarity-gated decorrelation of noisy regions (less metallic noise)
//   - Cepstral formant envelope for formant-preserving pitch shift
//   - Dual resolution: 4096 window below 300 Hz, 2048 above (at 44.1/48 kHz)
//
//  Header-only, no dependencies. C++17.
// =============================================================================
#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cstdint>
#include "pffft/pffft.h"

namespace wormhole {

using cfloat = std::complex<float>;
static constexpr double kPi  = 3.14159265358979323846;
static constexpr double kTau = 6.28318530717958647692;

inline float princarg(float p) {
    p = std::fmod(p + (float)kPi, (float)kTau);
    if (p < 0) p += (float)kTau;
    return p - (float)kPi;
}
inline cfloat cmul(cfloat a, cfloat b) {
    return { a.real() * b.real() - a.imag() * b.imag(), a.real() * b.imag() + a.imag() * b.real() };
}

// -----------------------------------------------------------------------------
//  Fast math
// -----------------------------------------------------------------------------
inline float fastAtan2(float y, float x) {
    const float ax = std::fabs(x), ay = std::fabs(y);
    const float mx = std::max(ax, ay), mn = std::min(ax, ay);
    if (mx < 1e-30f) return 0.f;
    const float a = mn / mx, s2 = a * a;
    float r = ((((-0.0117212f * s2 + 0.05265332f) * s2 - 0.11643287f) * s2 + 0.19354346f) * s2 - 0.33262347f) * s2 * a + 0.99997726f * a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    return y < 0 ? -r : r;
}
inline float fastAbs(cfloat v) { return std::sqrt(v.real() * v.real() + v.imag() * v.imag()); }
inline float fastArg(cfloat v) { return fastAtan2(v.imag(), v.real()); }

// -----------------------------------------------------------------------------
//  Real FFT (PFFFT, SIMD). Spectrum layout: complex bins 0..N/2.
// -----------------------------------------------------------------------------
class RealFFT {
public:
    RealFFT() = default;
    RealFFT(const RealFFT&) = delete; RealFFT& operator=(const RealFFT&) = delete;
    ~RealFFT() { release(); }
    void init(int n) {
        release(); N = n;
        setup = pffft_new_setup(N, PFFFT_REAL);
        buf  = (float*)pffft_aligned_malloc(sizeof(float) * N);
        tmp  = (float*)pffft_aligned_malloc(sizeof(float) * N);
        work = (float*)pffft_aligned_malloc(sizeof(float) * N);
    }
    // time (N real) -> bins (N/2+1 complex), unscaled
    void forward(const float* time, cfloat* bins) {
        std::memcpy(buf, time, sizeof(float) * N);
        pffft_transform_ordered(setup, buf, tmp, work, PFFFT_FORWARD);
        bins[0] = cfloat(tmp[0], 0.f); bins[N / 2] = cfloat(tmp[1], 0.f);
        for (int k = 1; k < N / 2; ++k) bins[k] = cfloat(tmp[2 * k], tmp[2 * k + 1]);
    }
    // bins (N/2+1) -> time (N real), unscaled (result * N)
    void inverse(const cfloat* bins, float* time) {
        tmp[0] = bins[0].real(); tmp[1] = bins[N / 2].real();
        for (int k = 1; k < N / 2; ++k) { tmp[2 * k] = bins[k].real(); tmp[2 * k + 1] = bins[k].imag(); }
        pffft_transform_ordered(setup, tmp, buf, work, PFFFT_BACKWARD);
        std::memcpy(time, buf, sizeof(float) * N);
    }
    float* input() { return buf; }
private:
    void release() {
        if (setup) pffft_destroy_setup(setup);
        if (buf) pffft_aligned_free(buf);
        if (tmp) pffft_aligned_free(tmp);
        if (work) pffft_aligned_free(work);
        setup = nullptr; buf = tmp = work = nullptr;
    }
    int N = 0;
    PFFFT_Setup* setup = nullptr;
    float *buf = nullptr, *tmp = nullptr, *work = nullptr;
};

// -----------------------------------------------------------------------------
//  Parameters
// -----------------------------------------------------------------------------
struct Params {
    double timeRatio        = 1.0;  // output length / input length (2.0 = half speed)
    double pitchSemitones   = 0.0;
    bool   formantPreserve  = true; // keep vocal character when pitching
    double formantSemitones = 0.0;  // extra formant shift (character)
    bool   transientLock    = true;
    double textureSeconds   = 0.0;  // Extrem-Modus: Fensterlänge in s (0 = automatisch)
};

// Ab diesem Längenfaktor (Tempo unter 25 %) arbeitet der Extrem-Algorithmus
static constexpr double kExtremeRatio = 4.0;
static constexpr double kMaxRatio     = 1000.0; // 0,1 %

inline double autoTextureSeconds(double ratio) {
    return std::clamp(0.08 * std::pow(ratio, 0.45), 0.12, 0.5);
}

// -----------------------------------------------------------------------------
//  One spectral band processor (single FFT size)
// -----------------------------------------------------------------------------
class BandProcessor {
public:
    void prepare(int fftSize, int channels, double sampleRate) {
        N = fftSize; H = N / 8; C = channels; sr = sampleRate; B = N / 2 + 1;
        fft.init(N);
        win.resize(N);
        for (int i = 0; i < N; ++i) win[i] = 0.5f - 0.5f * (float)std::cos(kTau * i / N);
        std::vector<double> acc(H, 0.0);
        for (int i = 0; i < N; ++i) acc[i % H] += (double)win[i] * win[i];
        olaNorm = (float)(1.0 / acc[0]);
        X.assign(C, std::vector<cfloat>(B)); Y.assign(C, std::vector<cfloat>(B));
        tbuf.assign(N, 0.f); spec.assign(B, {});
        Xm.assign(B, {}); Ym.assign(B, {}); srcM.assign(B, {});
        prevPhase.assign(B, 0.f); outPhase.assign(B, 0.f); srcPhase.assign(B, 0.f);
        omega.assign(B, 0.f); magMid.assign(B, 0.f); magOut.assign(B, 0.f);
        logEnv.assign(B, 0.f); peakOf.assign(B, -1); isPeak.assign(B, 0);
        rot.assign(B, {}); gain.assign(B, 1.f);
        peaks.clear(); peaks.reserve(B);
        setBandWeight({});
        first = true;
    }
    int fftSize() const { return N; }
    int hop() const { return H; }
    void reset() { std::fill(prevPhase.begin(), prevPhase.end(), 0.f); first = true; }

    // per output bin weight (crossover). Empty = full band.
    void setBandWeight(const std::vector<float>& w) {
        bandWeight = w;
        jMin = 0; jMax = B - 1;
        if (!w.empty()) {
            while (jMin < B - 1 && w[jMin] < 1e-5f) ++jMin;
            while (jMax > 0 && w[jMax] < 1e-5f) --jMax;
        }
    }
    void setPitch(double p) { P = (float)p; }

    bool needFullSpectrum = false; // formant envelope needs all bins
    float stationarity = 1.f;
    float noiseAmount  = 0.f; // 0..1, decorrelation of noisy regions

    // inX/inY: per channel N raw samples; Y frame is H samples earlier in input.
    // Returns transient measure (0 steady .. ~1 attack from silence).
    float analyse(const std::vector<const float*>& inX, const std::vector<const float*>& inY) {
        // bins needed (source side), with margin for peak search
        kNeed = std::min(B - 1, (int)std::ceil((jMax + 4) / std::min(1.f, P)) + 4);
        if (P > 1.f) kNeed = std::min(B - 1, (int)std::ceil((jMax + 4) / P) + 4);
        kStart = std::max(0, (int)std::floor((jMin - 4) / P) - 4);
        if (needFullSpectrum) { kStart = 0; kNeed = B - 1; }
        for (int c = 0; c < C; ++c) {
            for (int i = 0; i < N; ++i) tbuf[i] = inX[c][i] * win[i];
            fft.forward(tbuf.data(), spec.data());
            for (int k = kStart; k <= kNeed; ++k) X[c][k] = (k & 1) ? -spec[k] : spec[k]; // centre-referenced
            for (int i = 0; i < N; ++i) tbuf[i] = inY[c][i] * win[i];
            fft.forward(tbuf.data(), spec.data());
            for (int k = kStart; k <= kNeed; ++k) Y[c][k] = (k & 1) ? -spec[k] : spec[k];
        }
        float rise = 0, total = 1e-9f, ex = 0, ey = 0;
        const bool hasW = !bandWeight.empty();
        for (int k = kStart; k <= kNeed; ++k) {
            cfloat a(0, 0), b(0, 0); float sumMag = 0;
            for (int c = 0; c < C; ++c) { a += X[c][k]; b += Y[c][k]; sumMag += fastAbs(X[c][k]); }
            // out-of-phase stereo guard: fall back to the louder channel
            if (C > 1 && fastAbs(a) < 0.25f * sumMag) {
                int best = 0; float bm = -1;
                for (int c = 0; c < C; ++c) { float m = fastAbs(X[c][k]); if (m > bm) { bm = m; best = c; } }
                a = X[best][k] * (float)C; b = Y[best][k] * (float)C;
            }
            Xm[k] = a; Ym[k] = b;
            const float wb = (float)(kTau * k / N);
            const float dp = princarg(fastArg(a) - fastArg(b) - wb * H);
            omega[k] = wb + dp / H;
            const float m = fastAbs(a), my = fastAbs(b);
            magMid[k] = m;
            const float w = hasW ? weightAtSource(k) : 1.f;
            if (m > my) rise += (m - my) * w;
            total += m * w; ex += m * m * w; ey += my * my * w;
        }
        const float dDb = std::abs(10.f * std::log10((ex + 1e-12f) / (ey + 1e-12f)));
        stationarity = std::clamp(1.f - (dDb - 0.5f) / 1.5f, 0.f, 1.f);
        return rise / total;
    }

    // Synthesis into out[c][0..N) (windowed, normalised, to be overlap-added at hop H).
    void synthesise(bool formantPreserve, double formantFactor, bool resetPhase, std::vector<float*>& out) {
        const bool useEnv = formantPreserve || std::abs(formantFactor - 1.0) > 1e-6;
        if (useEnv) computeEnvelope();
        const int ja = std::max(0, jMin - 3), jb = std::min(B - 1, jMax + 3);
        const float sMax = (float)(N / 2 - 1);

        // 1) mapped mid spectrum + magnitudes
        float maxMag = 1e-12f;
        for (int j = ja; j <= jb; ++j) {
            const float s = j / P;
            if (s > sMax || s < kStart || s + 1 > kNeed) { magOut[j] = 0; srcM[j] = {}; continue; }
            const int k0 = (int)s; const float f = s - k0;
            srcM[j] = Xm[k0] * (1 - f) + Xm[k0 + 1] * f;
            magOut[j] = magMid[k0] * (1 - f) + magMid[k0 + 1] * f;
            srcPhase[j] = fastArg(srcM[j]);
            maxMag = std::max(maxMag, magOut[j]);
        }
        // 2) peaks: local maxima over +-2 bins above floor
        const float floorMag = maxMag * 1e-5f;
        peaks.clear();
        for (int j = ja; j <= jb; ++j) {
            const float m = magOut[j];
            bool pk = m > floorMag;
            if (pk) for (int d = -2; d <= 2; ++d) {
                if (!d) continue;
                const int jj = j + d;
                if (jj < ja || jj > jb) continue;
                if (magOut[jj] > m || (magOut[jj] == m && d < 0)) { pk = false; break; }
            }
            isPeak[j] = pk;
            if (pk) peaks.push_back(j);
        }
        // 3) regions: each bin belongs to the peak on its side of the minimum between peaks
        if (peaks.empty()) { for (int j = ja; j <= jb; ++j) peakOf[j] = -1; }
        else {
            int start = ja;
            for (size_t pi = 0; pi < peaks.size(); ++pi) {
                int end;
                if (pi + 1 < peaks.size()) {
                    int a = peaks[pi], b = peaks[pi + 1], mn = a;
                    for (int t = a + 1; t <= b; ++t) if (magOut[t] < magOut[mn]) mn = t;
                    end = mn;
                } else end = jb;
                for (int j = start; j <= end; ++j) peakOf[j] = peaks[pi];
                start = end + 1;
            }
        }
        // 4) mid phases
        const bool doReset = resetPhase || first;
        if (doReset) {
            for (int j = ja; j <= jb; ++j) outPhase[j] = srcPhase[j];
        } else {
            for (int p : peaks) {
                const int k = std::min((int)(p / P + 0.5f), kNeed);
                outPhase[p] = princarg(prevPhase[p] + omega[k] * P * H);
            }
            for (int j = ja; j <= jb; ++j) {
                const int p = peakOf[j];
                if (p < 0) { const int k = std::min((int)(j / P + 0.5f), kNeed); outPhase[j] = princarg(prevPhase[j] + omega[k] * P * H); continue; }
                if (p == j) continue;
                outPhase[j] = princarg(outPhase[p] + srcPhase[j] - srcPhase[p]);
            }
            // noisy regions: decorrelate (sinusoids share one inst. frequency across their lobe)
            if (noiseAmount > 0.f && stationarity > 0.f) {
                const float binW = (float)(kTau / N);
                int j = ja;
                while (j <= jb) {
                    const int p = peakOf[j];
                    int j1 = j; while (j1 <= jb && peakOf[j1] == p) ++j1;
                    if (p >= 0) {
                        const int kp = std::min((int)(p / P + 0.5f), kNeed);
                        float num = 0, den = 1e-12f;
                        for (int t = j; t < j1; ++t) {
                            const int k = std::min((int)(t / P + 0.5f), kNeed);
                            num += std::min(1.f, std::abs(omega[k] - omega[kp]) / binW) * magOut[t];
                            den += magOut[t];
                        }
                        const float amt = std::clamp((num / den - 0.15f) / 0.5f, 0.f, 1.f);
                        if (amt > 0) {
                            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                            const float u = (rng & 0xFFFFFF) / (float)0xFFFFFF * 2.f - 1.f;
                            const float delta = u * (float)kPi * amt * noiseAmount * stationarity;
                            for (int t = j; t < j1; ++t) outPhase[t] = princarg(outPhase[t] + delta);
                        }
                    }
                    j = j1;
                }
            }
        }
        first = false;
        // 5) per-bin rotation (output phase relative to source mid phase) and gain
        const bool hasW = !bandWeight.empty();
        for (int j = ja; j <= jb; ++j) {
            const float m = fastAbs(srcM[j]);
            if (m < 1e-20f) { rot[j] = {}; continue; }
            const float d = outPhase[j] - srcPhase[j];
            float g = hasW ? bandWeight[j] : 1.f;
            if (useEnv) {
                const float s = j / P; const int k0 = (int)s; const float f = s - k0;
                const float st = (float)(j / formantFactor);
                float et;
                if (st > sMax) et = logEnv[N / 2];
                else { const int t0 = (int)st; const float tf = st - t0; et = logEnv[t0] * (1 - tf) + logEnv[t0 + 1] * tf; }
                const float es = logEnv[k0] * (1 - f) + logEnv[k0 + 1] * f;
                g *= std::exp(std::clamp(et - es, -4.f, 3.f));
            }
            // per channel spectrum = X_c(s) * conj(srcM)/|srcM| * e^{i outPhase} ... folded into one rotation
            rot[j] = std::polar(g, d);
        }
        // 6) per-channel inverse FFT
        const float scale = olaNorm / (float)N;
        for (int c = 0; c < C; ++c) {
            std::fill(spec.begin(), spec.end(), cfloat(0, 0));
            for (int j = ja; j <= jb; ++j) {
                if (rot[j] == cfloat(0, 0)) continue;
                const float s = j / P; const int k0 = (int)s; const float f = s - k0;
                cfloat a = cmul(X[c][k0] * (1 - f) + X[c][k0 + 1] * f, rot[j]);
                spec[j] = (j & 1) ? -a : a;
            }
            fft.inverse(spec.data(), tbuf.data());
            float* o = out[c];
            for (int i = 0; i < N; ++i) o[i] = tbuf[i] * win[i] * scale;
        }
        for (int j = ja; j <= jb; ++j) prevPhase[j] = outPhase[j];
    }

private:
    float weightAtSource(int k) const {
        const float j = k * P; const int j0 = (int)j;
        if (j0 >= B - 1) return 0.f;
        return bandWeight[j0];
    }
    void computeEnvelope() {
        // real cepstrum of log magnitude, liftered (~1.1 ms) -> smooth spectral envelope
        for (int k = 0; k < B; ++k) {
            const float l = std::log((k >= kStart && k <= kNeed ? magMid[k] : 0.f) + 1e-7f);
            tbuf[k] = l;
            if (k > 0 && k < N / 2) tbuf[N - k] = l;
        }
        fft.forward(tbuf.data(), spec.data());       // symmetric input -> real cepstrum (x N)
        const int lifter = std::clamp((int)(0.0011 * sr), 8, N / 4);
        for (int q = 0; q < B; ++q) {
            const float w = q < lifter ? 1.f : (q < lifter + 4 ? 0.5f + 0.5f * (float)std::cos(kPi * (q - lifter) / 4.0) : 0.f);
            spec[q] = cfloat(spec[q].real() * w / (float)N, 0.f);
        }
        fft.inverse(spec.data(), tbuf.data());
        for (int k = 0; k < B; ++k) logEnv[k] = tbuf[k];
    }

    int N = 4096, H = 512, C = 2, B = 2049; double sr = 44100;
    float P = 1.f;
    int jMin = 0, jMax = 0, kNeed = 0, kStart = 0;
    RealFFT fft;
    std::vector<float> win, bandWeight, tbuf;
    std::vector<cfloat> spec;
    float olaNorm = 1;
    std::vector<std::vector<cfloat>> X, Y;
    std::vector<cfloat> Xm, Ym, srcM, rot;
    std::vector<float> prevPhase, outPhase, srcPhase, omega, magMid, magOut, logEnv, gain;
    std::vector<int> peakOf, peaks; std::vector<char> isPeak;
    bool first = true;
    uint32_t rng = 0x9E3779B9u;
};

inline std::vector<float> crossoverWeights(int N, double sr, double fc, bool low) {
    std::vector<float> w(N / 2 + 1);
    const double f1 = fc / std::sqrt(2.0), f2 = fc * std::sqrt(2.0);
    for (int k = 0; k <= N / 2; ++k) {
        const double f = k * sr / N; double v;
        if (f <= f1) v = 1; else if (f >= f2) v = 0;
        else v = 0.5 + 0.5 * std::cos(kPi * std::log(f / f1) / std::log(f2 / f1));
        w[k] = (float)(low ? v : 1.0 - v);
    }
    return w;
}

// -----------------------------------------------------------------------------
//  StretchStream: liefert das Ergebnis stückweise (für Vorhören und Rendern).
//
//  Normalbereich (Tempo >= 25 %): Phase-Vocoder in zwei Auflösungen, mit
//  Attack-Lock (Phasen-Reset, Zeit-Verriegelung, Vorecho-Maskierung).
//  Extrembereich (Tempo < 25 %, bis 0,1 %): Spektral-Textur mit sehr langen
//  Fenstern und zufälligen Phasen (Prinzip Paulstretch), aber stereo-kohärent:
//  alle Kanäle teilen die Zufallsphase, die Phasenunterschiede zwischen den
//  Kanälen bleiben erhalten, das Stereobild steht.
// -----------------------------------------------------------------------------
class StretchStream {
public:
    // Eingang muss so lange leben wie der Stream. Erkennt die Attacks (einmalig pro Eingang).
    void setInput(const std::vector<std::vector<float>>* in, double sampleRate,
                  const std::vector<long>* knownTransients = nullptr) {
        input = in; sr = sampleRate;
        C = in ? (int)in->size() : 0;
        inLen = C ? (long)(*in)[0].size() : 0;
        scale = sr > 60000 ? 2 : 1;
        transients.clear();
        if (knownTransients) transients = *knownTransients;
        else if (C && inLen) detectTransients(2048 * scale);
    }
    const std::vector<long>& getTransients() const { return transients; }

    // Startet die Ausgabe ab Ausgabe-Sample startOut mit neuen Parametern
    void start(const Params& params, long startOut = 0) {
        p = params;
        p.timeRatio = std::clamp(p.timeRatio, 1.0 / 4.0, kMaxRatio);
        outLen = (long)std::llround(inLen * p.timeRatio);
        pos = std::clamp(startOut, 0L, std::max(0L, outLen));
        pitch = std::pow(2.0, p.pitchSemitones / 12.0);
        fform = std::pow(2.0, p.formantSemitones / 12.0);
        extreme = p.timeRatio > kExtremeRatio;
        if (!C || !inLen) return;
        if (extreme) startExtreme();
        else startPV();
    }

    bool isExtreme() const { return extreme; }
    long outputLength() const { return outLen; }
    long position() const { return pos; }
    int channels() const { return C; }
    double textureUsed() const { return xN > 0 ? xN / sr : 0.0; }
    static constexpr int maxPull = 16384;

    // Erzeugt bis zu n (<= maxPull) Samples je Kanal. Rückgabe: erzeugte Samples (0 = Ende).
    int pull(float* const* out, int n) {
        n = std::min<long>(std::min(n, maxPull), outLen - pos);
        if (n <= 0 || !C) return 0;
        const long target = pos + n;
        if (extreme) {
            while (xNext - xN / 2 < target) runExtremeFrame();
            for (int i = 0; i < n; ++i) {
                const size_t r = (size_t)((pos + i) & (R - 1));
                for (int c = 0; c < C; ++c) { out[c][i] = xAcc[c][r] * xScale; xAcc[c][r] = 0.f; }
            }
        } else {
            for (auto& b : bands) while (b.next - b.N / 2 < target) runPVFrame(b);
            for (int i = 0; i < n; ++i) {
                const size_t r = (size_t)((pos + i) & (R - 1));
                for (int c = 0; c < C; ++c) out[c][i] = 0.f;
                for (auto& b : bands) {
                    const float wn = b.weight[r] / b.w2sum;
                    const float g = wn > 0.999f ? 1.f : 1.f / std::max(wn, 0.35f);
                    for (int c = 0; c < C; ++c) { out[c][i] += b.acc[c][r] * g; b.acc[c][r] = 0.f; }
                    b.weight[r] = 0.f;
                }
            }
        }
        pos += n;
        return n;
    }

private:
    static constexpr long R = 1L << 18; // Ring für die Überlappung (muss > maxPull + 2*N sein)

    // ---------------- Phase-Vocoder ----------------
    struct PVBand {
        BandProcessor bp; int N = 0, H = 0; long next = 0; double lastReset = -1e18;
        std::vector<std::vector<float>> bx, by, bo, acc; std::vector<const float*> px, py; std::vector<float*> po;
        std::vector<float> mask, w2, weight; float w2sum = 1;
    };
    PVBand bands[2];
    std::vector<double> mapOut, mapIn; std::vector<char> trAt;

    void startPV() {
        const int Ns[2] = { 4096 * scale, 2048 * scale };
        const float noiseAmt = (float)std::min(1.0, 1.5 * std::abs(std::log2(p.timeRatio)) + 1.5 * std::abs(std::log2(pitch)));
        for (int bi = 0; bi < 2; ++bi) {
            PVBand& b = bands[bi];
            const int N = Ns[bi];
            if (b.N != N || (int)b.acc.size() != C) {
                b.N = N; b.H = N / 8;
                b.bp.prepare(N, C, sr);
                b.bp.setBandWeight(crossoverWeights(N, sr, 300.0, bi == 0));
                b.bx.assign(C, std::vector<float>(N)); b.by.assign(C, std::vector<float>(N)); b.bo.assign(C, std::vector<float>(N));
                b.px.resize(C); b.py.resize(C); b.po.resize(C);
                for (int c = 0; c < C; ++c) { b.px[c] = b.bx[c].data(); b.py[c] = b.by[c].data(); b.po[c] = b.bo[c].data(); }
                b.acc.assign(C, std::vector<float>((size_t)R, 0.f));
                b.weight.assign((size_t)R, 0.f);
                b.mask.assign(N, 1.f); b.w2.resize(N);
                for (int i = 0; i < N; ++i) { const float w = 0.5f - 0.5f * (float)std::cos(kTau * i / N); b.w2[i] = w * w; }
                std::vector<double> a(b.H, 0.0); for (int i = 0; i < N; ++i) a[i % b.H] += b.w2[i];
                b.w2sum = (float)a[0];
            } else {
                for (auto& ch : b.acc) std::fill(ch.begin(), ch.end(), 0.f);
                std::fill(b.weight.begin(), b.weight.end(), 0.f);
            }
            b.bp.reset(); b.bp.setPitch(pitch); b.bp.noiseAmount = noiseAmt;
            b.bp.needFullSpectrum = p.formantPreserve || std::abs(p.formantSemitones) > 1e-6;
            b.next = pos - N;          // gleiches Raster wie beim Start bei 0
            b.lastReset = -1e18;
        }
        buildTimeMap(2048 * scale / 16, 4096 * scale);
    }

    double inTimeFor(double tOut) const {
        if (tOut <= mapOut.front()) return mapIn.front() + (tOut - mapOut.front()) / p.timeRatio;
        if (tOut >= mapOut.back())  return mapIn.back() + (tOut - mapOut.back()) / p.timeRatio;
        auto it = std::upper_bound(mapOut.begin(), mapOut.end(), tOut);
        const size_t i = (size_t)(it - mapOut.begin()) - 1;
        const double f = (tOut - mapOut[i]) / (mapOut[i + 1] - mapOut[i]);
        return mapIn[i] + f * (mapIn[i + 1] - mapIn[i]);
    }
    bool transientNear(double tOut, double radius) const {
        auto it = std::lower_bound(mapOut.begin(), mapOut.end(), tOut - radius);
        for (; it != mapOut.end() && *it <= tOut + radius; ++it)
            if (trAt[(size_t)(it - mapOut.begin())]) return true;
        return false;
    }

    void runPVFrame(PVBand& b) {
        const int N = b.N, H = b.H;
        const long tOut = b.next; b.next += H;
        const double tIn = inTimeFor((double)tOut);
        const long cIn = (long)std::llround(tIn);
        const long fade = std::max(16L, (long)(0.0015 * sr));
        bool masked = false;
        if (p.transientLock) {
            auto it = std::upper_bound(transients.begin(), transients.end(), cIn + H / 2);
            if (it != transients.end() && *it < cIn + N / 2) {
                const long T = *it; masked = true;
                for (int i = 0; i < N; ++i) {
                    const long a = cIn - N / 2 + i;
                    b.mask[i] = a >= T ? 0.f : (a > T - fade ? 0.5f + 0.5f * (float)std::cos(kPi * (a - (T - fade)) / (double)fade) : 1.f);
                }
            }
        }
        for (int c = 0; c < C; ++c) {
            const float* src = (*input)[c].data();
            for (int i = 0; i < N; ++i) {
                const long a = cIn - N / 2 + i, bb = a - H;
                const float m = masked ? b.mask[i] : 1.f;
                const float mb = masked ? (i >= H ? b.mask[i - H] : 1.f) : 1.f;
                b.bx[c][i] = (a >= 0 && a < inLen) ? src[a] * m : 0.f;
                b.by[c][i] = (bb >= 0 && bb < inLen) ? src[bb] * mb : 0.f;
            }
        }
        b.bp.analyse(b.px, b.py);
        bool reset = false;
        if (p.transientLock && transientNear((double)tOut, H * 0.5) && tOut - b.lastReset > N / 4) { reset = true; b.lastReset = (double)tOut; }
        b.bp.synthesise(p.formantPreserve, fform, reset, b.po);
        const long o0 = tOut - N / 2;
        for (int i = 0; i < N; ++i) {
            const long o = o0 + i;
            if (o < pos || o >= outLen) continue;
            const size_t r = (size_t)(o & (R - 1));
            for (int c = 0; c < C; ++c) b.acc[c][r] += b.bo[c][i];
            b.weight[r] += b.w2[i] * (masked ? b.mask[i] : 1.f);
        }
    }

    // ---------------- Extrem (Spektral-Textur, hybrid) ----------------
    // Klare Teiltöne: phasenrichtig fortgeführt (keine Schwebung, stehende Töne).
    // Rauschen/Textur: zufällige Phasen (weich, ohne metallischen Klang).
    RealFFT xfft;
    int xN = 0, xH = 0, xB = 0, xD = 512; long xNext = 0; float xScale = 1.f, xCoherentGain = 1.f;
    std::vector<float> xWin, xTime, xLogEnv, xMag, xOmega, xPrevPhase, xOutPhase, xSrcPhase, xMagOut, xPhRand;
    std::vector<std::vector<cfloat>> xSpec; std::vector<cfloat> xMid, xMidPrev, xOutSpec, xRot;
    std::vector<std::vector<float>> xAcc;
    std::vector<int> xPeakOf, xPeaks; std::vector<char> xTonal;
    bool xFirst = true;
    uint32_t xRng = 0x2545F491u;

    static int goodFFTSize(double want) {
        // pffft: N = 2^a * 3^b * 5^c, Vielfaches von 32 -> nächstgelegene Größe nach oben
        int best = 1 << 18;
        for (int base : { 32, 96, 160 })
            for (int n = base; n <= (1 << 18); n <<= 1)
                if (n >= want) { best = std::min(best, n); break; }
        return best;
    }

    void startExtreme() {
        const double tex = p.textureSeconds > 0.0 ? std::clamp(p.textureSeconds, 0.04, 2.0) : autoTextureSeconds(p.timeRatio);
        const int N = std::max(1024, goodFFTSize(tex * sr));
        if (N != xN || (int)xAcc.size() != C) {
            xN = N; xH = N / 4; xB = N / 2 + 1;
            xfft.init(N);
            xWin.resize(N);
            for (int i = 0; i < N; ++i) xWin[i] = 0.5f - 0.5f * (float)std::cos(kTau * i / N);
            xTime.assign(N, 0.f); outTime.assign(N, 0.f);
            for (auto* v : { &xLogEnv, &xMag, &xOmega, &xPrevPhase, &xOutPhase, &xSrcPhase, &xMagOut, &xPhRand }) v->assign(xB, 0.f);
            xSpec.assign(C, std::vector<cfloat>(xB)); xMid.assign(xB, {}); xMidPrev.assign(xB, {}); xOutSpec.assign(xB, {}); xRot.assign(xB, {});
            xPeakOf.assign(xB, -1); xPeaks.reserve(xB); xTonal.assign(xB, 0);
            xAcc.assign(C, std::vector<float>((size_t)R, 0.f));
            // Normierung: zufällige Phasen addieren sich nach Leistung, kohärente nach Amplitude
            double ma = 0; for (float w : xWin) ma += (double)w * w; ma /= N;
            std::vector<double> a(xH, 0.0); for (int i = 0; i < N; ++i) a[i % xH] += (double)xWin[i] * xWin[i];
            xScale = (float)(1.0 / std::sqrt(ma * a[0]));
            xCoherentGain = (float)(1.0 / (a[0] * xScale));
        } else for (auto& ch : xAcc) std::fill(ch.begin(), ch.end(), 0.f);
        xD = std::max(64, (int)(0.012 * sr));   // Abstand der zweiten Analyse für die Momentanfrequenz
        xNext = pos - xN;
        xFirst = true;
        xRng = 0x2545F491u ^ (uint32_t)(pos * 2654435761u);
    }

    float xRand() { xRng ^= xRng << 13; xRng ^= xRng >> 17; xRng ^= xRng << 5; return (xRng & 0xFFFFFF) / (float)0xFFFFFF; }

    void runExtremeFrame() {
        const int N = xN, B = xB;
        const long tOut = xNext; xNext += xH;
        const double tIn = (double)tOut / p.timeRatio;
        long cIn = (long)std::llround(tIn);
        // Fenster am Dateirand nach innen schieben, statt halb ins Leere zu greifen
        if (inLen > N + xD) cIn = std::clamp(cIn, (long)(N / 2 + xD), inLen - N / 2);
        const auto& in = *input;
        // Analyse je Kanal und Mitte; zweite Analyse der Mitte xD Samples früher (Momentanfrequenz)
        for (int c = 0; c < C; ++c) {
            const float* src = in[c].data();
            for (int i = 0; i < N; ++i) { const long a = cIn - N / 2 + i; xTime[i] = (a >= 0 && a < inLen) ? src[a] * xWin[i] : 0.f; }
            xfft.forward(xTime.data(), xSpec[c].data());
        }
        for (int i = 0; i < N; ++i) {
            const long a = cIn - xD - N / 2 + i; float v = 0.f;
            if (a >= 0 && a < inLen) for (int c = 0; c < C; ++c) v += in[c][a];
            xTime[i] = v * xWin[i];
        }
        xfft.forward(xTime.data(), xMidPrev.data());
        for (int k = 0; k < B; ++k) {
            cfloat m(0, 0); float sum = 0;
            for (int c = 0; c < C; ++c) { m += xSpec[c][k]; sum += fastAbs(xSpec[c][k]); }
            xMid[k] = m;
            xMag[k] = std::max(fastAbs(m), 0.5f * sum);
            const float wb = (float)(kTau * k / N);
            xOmega[k] = wb + princarg(fastArg(m) - fastArg(xMidPrev[k]) - wb * xD) / xD;
        }
        // sehr kurze Dateien: fehlende Fensteranteile ausgleichen
        float cover = 1.f;
        if (inLen <= N + xD) {
            double a = 0, t = 0;
            for (int i = 0; i < N; ++i) { const long q = cIn - N / 2 + i; const double w2 = (double)xWin[i] * xWin[i]; t += w2; if (q >= 0 && q < inLen) a += w2; }
            cover = (float)std::sqrt(t / std::max(a, t * 0.02));
        }
        const bool useEnv = p.formantPreserve || std::abs(fform - 1.0) > 1e-6;
        if (useEnv) envelope();
        const float P = (float)pitch, sMax = (float)(N / 2 - 1);

        // gemappte Mitte, Spitzen und Regionen
        float maxMag = 1e-12f;
        for (int j = 0; j < B; ++j) {
            const float s = j / P;
            if (s > sMax) { xMagOut[j] = 0; xSrcPhase[j] = 0; continue; }
            const int k0 = (int)s; const float f = s - k0;
            xMagOut[j] = xMag[k0] * (1 - f) + xMag[k0 + 1] * f;
            xSrcPhase[j] = fastArg(xMid[k0] * (1 - f) + xMid[k0 + 1] * f);
            maxMag = std::max(maxMag, xMagOut[j]);
        }
        xPeaks.clear();
        const float floorMag = maxMag * 1e-5f;
        for (int j = 2; j < B - 2; ++j) {
            const float m = xMagOut[j];
            if (m > floorMag && m >= xMagOut[j - 1] && m > xMagOut[j + 1] && m >= xMagOut[j - 2] && m > xMagOut[j + 2]) xPeaks.push_back(j);
        }
        std::fill(xPeakOf.begin(), xPeakOf.end(), -1);
        if (!xPeaks.empty()) {
            int startJ = 0;
            for (size_t pi = 0; pi < xPeaks.size(); ++pi) {
                int endJ = B - 1;
                if (pi + 1 < xPeaks.size()) { int a = xPeaks[pi], b2 = xPeaks[pi + 1], mn = a; for (int t = a + 1; t <= b2; ++t) if (xMagOut[t] < xMagOut[mn]) mn = t; endJ = mn; }
                for (int j = startJ; j <= endJ; ++j) xPeakOf[j] = xPeaks[pi];
                startJ = endJ + 1;
            }
        }
        // Tonalität je Region: Momentanfrequenz über die Hauptkeule konstant -> Sinus
        const float binW = (float)(kTau / N);
        for (int pk : xPeaks) {
            const int kp = std::min((int)(pk / P + 0.5f), B - 1);
            float dev = 0, wsum = 1e-12f;
            for (int d = -2; d <= 2; ++d) {
                const int j = pk + d; if (j < 0 || j >= B) continue;
                const int k = std::min((int)(j / P + 0.5f), B - 1);
                dev += std::min(2.f, std::abs(xOmega[k] - xOmega[kp]) / binW) * xMagOut[j]; wsum += xMagOut[j];
            }
            // Spitze muss sich auch deutlich über ihre Nachbarschaft erheben
            float nb = 0; int nn = 0;
            for (int d = 6; d <= 14; ++d) { if (pk - d >= 0) { nb += xMagOut[pk - d]; ++nn; } if (pk + d < B) { nb += xMagOut[pk + d]; ++nn; } }
            nb /= std::max(1, nn);
            xTonal[pk] = (dev / wsum < 0.25f && xMagOut[pk] > 4.0f * nb) ? 1 : 0;
        }
        // Phasen: tonal = fortgeführt mit Momentanfrequenz, sonst zufällig; Nachbarbins an die Spitze gekoppelt
        for (int pk : xPeaks) {
            if (xTonal[pk] && !xFirst) {
                const int k = std::min((int)(pk / P + 0.5f), B - 1);
                xOutPhase[pk] = princarg(xPrevPhase[pk] + xOmega[k] * P * xH);
            } else xOutPhase[pk] = xRand() * (float)kTau;
        }
        for (int j = 0; j < B; ++j) {
            const int pk = xPeakOf[j];
            if (pk < 0) { xOutPhase[j] = xRand() * (float)kTau; continue; }
            if (pk == j) continue;
            if (xTonal[pk]) xOutPhase[j] = princarg(xOutPhase[pk] + xSrcPhase[j] - xSrcPhase[pk]);
            else xOutPhase[j] = xRand() * (float)kTau;
        }
        xFirst = false;
        // Rotation je Bin (gemeinsam für alle Kanäle) inkl. Formant und Normierung
        for (int j = 0; j < B; ++j) {
            const float s = j / P;
            if (s > sMax || j == 0) { xRot[j] = {}; continue; }
            const int k0 = (int)s; const float f = s - k0;
            float g = 1.f;
            if (useEnv) {
                const float st = (float)(j / fform);
                float et;
                if (st > sMax) et = xLogEnv[N / 2];
                else { const int t0 = (int)st; const float tf = st - t0; et = xLogEnv[t0] * (1 - tf) + xLogEnv[t0 + 1] * tf; }
                const float es = xLogEnv[k0] * (1 - f) + xLogEnv[k0 + 1] * f;
                g = std::exp(std::clamp(et - es, -4.f, 3.f));
            }
            const int pk = xPeakOf[j];
            if (pk >= 0 && xTonal[pk]) g *= xCoherentGain;
            g *= cover;
            xRot[j] = std::polar(g, xOutPhase[j] - xSrcPhase[j]);
        }
        for (int j = 0; j < B; ++j) xPrevPhase[j] = xOutPhase[j];
        // Synthese je Kanal: Kanal-Spektrum * Rotation (Kanalversatz bleibt -> Stereobild steht)
        for (int c = 0; c < C; ++c) {
            for (int j = 0; j < B; ++j) {
                if (xRot[j] == cfloat(0, 0)) { xOutSpec[j] = {}; continue; }
                const float s = j / P; const int k0 = (int)s; const float f = s - k0;
                const cfloat xv = xSpec[c][k0] * (1 - f) + xSpec[c][k0 + 1] * f;
                xOutSpec[j] = cmul(xv, xRot[j]);
            }
            xOutSpec[0] = {}; xOutSpec[B - 1] = cfloat(xOutSpec[B - 1].real(), 0.f);
            xfft.inverse(xOutSpec.data(), outTime.data());
            const float g = 1.f / (float)N;
            const long o0 = tOut - N / 2;
            for (int i = 0; i < N; ++i) {
                const long o = o0 + i;
                if (o < pos || o >= outLen) continue;
                xAcc[c][(size_t)(o & (R - 1))] += outTime[i] * xWin[i] * g;
            }
        }
    }
    std::vector<float> outTime;

    void envelope() {
        const int N = xN, B = xB;
        envTime.resize(N); envSpec.resize(B);
        for (int k = 0; k < B; ++k) {
            const float l = std::log(xMag[k] + 1e-7f);
            envTime[k] = l; if (k > 0 && k < N / 2) envTime[N - k] = l;
        }
        xfft.forward(envTime.data(), envSpec.data());
        const int lifter = std::clamp((int)(0.0011 * sr), 8, N / 4);
        for (int q = 0; q < B; ++q) {
            const float w = q < lifter ? 1.f : (q < lifter + 4 ? 0.5f + 0.5f * (float)std::cos(kPi * (q - lifter) / 4.0) : 0.f);
            envSpec[q] = cfloat(envSpec[q].real() * w / (float)N, 0.f);
        }
        xfft.inverse(envSpec.data(), envTime.data());
        for (int k = 0; k < B; ++k) xLogEnv[k] = envTime[k];
    }
    std::vector<float> envTime; std::vector<cfloat> envSpec;

    // ---------------- Attacks und Zeitkarte ----------------
    void detectTransients(int N) {
        BandProcessor det; det.prepare(N, C, sr); det.setPitch(1.0);
        std::vector<float> w(N / 2 + 1);
        for (int k = 0; k <= N / 2; ++k) w[k] = (float)std::min(1.0, (k * sr / N) / 2000.0);
        det.setBandWeight(w);
        const int H = det.hop();
        std::vector<float> flux;
        std::vector<std::vector<float>> bx(C, std::vector<float>(N)), by(C, std::vector<float>(N));
        std::vector<const float*> px(C), py(C);
        for (int c = 0; c < C; ++c) { px[c] = bx[c].data(); py[c] = by[c].data(); }
        const auto& in = *input;
        for (long c0 = 0; c0 < inLen + N; c0 += H) {
            for (int c = 0; c < C; ++c) for (int i = 0; i < N; ++i) {
                const long a = c0 - N / 2 + i, b = a - H;
                bx[c][i] = (a >= 0 && a < inLen) ? in[c][a] : 0.f;
                by[c][i] = (b >= 0 && b < inLen) ? in[c][b] : 0.f;
            }
            flux.push_back(det.analyse(px, py));
        }
        const int win = 12; long lastT = -100;
        const long F = (long)flux.size();
        for (long i = 0; i < F; ++i) {
            double m = 0; int n = 0;
            for (long j = std::max(0L, i - win); j <= std::min(F - 1, i + win); ++j) { m += flux[j]; ++n; }
            m /= std::max(1, n);
            const bool localMax = (i == 0 || flux[i] >= flux[i - 1]) && (i + 1 >= F || flux[i] > flux[i + 1]);
            if (!(localMax && flux[i] > 1.8 * m + 0.02 && flux[i] > 0.25 && i - lastT > 6)) continue;
            lastT = i;
            const long c0 = i * H, blk = 64;
            long best = c0; double bestRise = -1e9, prevE = -1;
            for (long a = c0 - N / 2; a + blk <= c0 + N / 2; a += blk / 2) {
                double e = 1e-12;
                for (long n2 = a; n2 < a + blk; ++n2) {
                    if (n2 < 1 || n2 >= inLen) continue;
                    for (int c = 0; c < C; ++c) { const double d = in[c][n2] - in[c][n2 - 1]; e += d * d; }
                }
                if (prevE > 0) { const double r = std::log(e / prevE); if (r > bestRise) { bestRise = r; best = a; } }
                prevE = e;
            }
            if (transients.empty() || best - transients.back() > (long)(0.03 * sr)) transients.push_back(std::max(0L, best));
        }
    }

    void buildTimeMap(int Hs, int Nmax) {
        mapOut.clear(); mapIn.clear(); trAt.clear();
        const double ratio = p.timeRatio;
        const bool stretching = std::abs(ratio - 1.0) > 1e-6;
        const double lockLen = std::max(2.0 * Hs, 0.012 * sr);
        double tOut = -2.0 * Nmax, tIn = tOut / ratio, lockLeft = 0;
        size_t next = 0;
        while (next < transients.size() && transients[next] < tIn) ++next;
        mapOut.push_back(tOut); mapIn.push_back(tIn); trAt.push_back(0);
        const double end = outLen + 2.0 * Nmax;
        while (tOut < end) {
            if (lockLeft > 0) {
                const double st = std::min((double)Hs, lockLeft);
                tOut += st; tIn += st; lockLeft -= st;
                mapOut.push_back(tOut); mapIn.push_back(tIn); trAt.push_back(0);
                while (next < transients.size() && transients[next] <= tIn) ++next;
                continue;
            }
            const double target = tOut / ratio;
            double stepIn = Hs / ratio;
            if (tOut > 0) stepIn += std::clamp((target - tIn) * 0.15, -0.5 * Hs / ratio, 0.5 * Hs / ratio);
            if (p.transientLock && next < transients.size() && transients[next] > tIn && transients[next] <= tIn + stepIn) {
                const double T = (double)transients[next++];
                const double frac = (T - tIn) / stepIn;
                tOut += frac * Hs; tIn = T;
                mapOut.push_back(tOut); mapIn.push_back(tIn); trAt.push_back(1);
                if (stretching) lockLeft = lockLen;
                continue;
            }
            tOut += Hs; tIn += stepIn;
            mapOut.push_back(tOut); mapIn.push_back(tIn); trAt.push_back(0);
        }
    }

    const std::vector<std::vector<float>>* input = nullptr;
    double sr = 44100; int C = 0, scale = 1; long inLen = 0, outLen = 0, pos = 0;
    Params p; double pitch = 1, fform = 1; bool extreme = false;
    std::vector<long> transients;
};

// -----------------------------------------------------------------------------
//  Offline-Renderer auf Basis des Streams (komplettes Ergebnis im Speicher).
// -----------------------------------------------------------------------------
class OfflineStretcher {
public:
    std::vector<long> transients;
    std::vector<std::vector<float>> process(const std::vector<std::vector<float>>& input,
                                            double sampleRate, const Params& p,
                                            std::function<bool(float)> progress = {}) {
        StretchStream st; st.setInput(&input, sampleRate); st.start(p, 0);
        transients = st.getTransients();
        const int C = st.channels();
        const long L = st.outputLength();
        std::vector<std::vector<float>> out(C, std::vector<float>(std::max(0L, L), 0.f));
        std::vector<float*> ptr(C);
        long done = 0;
        while (done < L) {
            for (int c = 0; c < C; ++c) ptr[c] = out[c].data() + done;
            const int got = st.pull(ptr.data(), StretchStream::maxPull);
            if (got <= 0) break;
            done += got;
            if (progress && !progress((float)done / (float)L)) break;
        }
        return out;
    }
};

// -----------------------------------------------------------------------------
//  Realtime pitch shifter (Live mode, time ratio 1). Latency = getLatency().
//  Uses the same transient handling as the offline renderer: attacks are
//  detected on the newest samples, masked out of earlier frames, and the
//  overlap-add is renormalised before output (possible because of latency).
// -----------------------------------------------------------------------------
class RealtimeShifter {
public:
    void prepare(double sampleRate, int channels) {
        C = channels; sr = sampleRate;
        const int scale = sampleRate > 60000 ? 2 : 1;
        const int Nlo = 4096 * scale, Nhi = 2048 * scale;
        lo.prepare(Nlo, C, sr); hi.prepare(Nhi, C, sr);
        lo.setBandWeight(crossoverWeights(Nlo, sr, 300.0, true));
        hi.setBandWeight(crossoverWeights(Nhi, sr, 300.0, false));
        bands[0].bp = &lo; bands[1].bp = &hi;
        latency = Nlo;
        histLen = 1; while (histLen < Nlo * 2 + Nlo / 4) histLen <<= 1; // power of two ring
        for (auto& b : bands) {
            const int N = b.bp->fftSize(), H = b.bp->hop();
            b.bx.assign(C, std::vector<float>(N)); b.by.assign(C, std::vector<float>(N)); b.bo.assign(C, std::vector<float>(N));
            b.px.resize(C); b.py.resize(C); b.po.resize(C);
            for (int c = 0; c < C; ++c) { b.px[c] = b.bx[c].data(); b.py[c] = b.by[c].data(); b.po[c] = b.bo[c].data(); }
            b.ola.assign(C, std::vector<float>(histLen, 0.f));
            b.wsum.assign(histLen, 0.f);
            b.w2.resize(N); b.mask.assign(N, 1.f);
            for (int i = 0; i < N; ++i) { const float w = 0.5f - 0.5f * (float)std::cos(kTau * i / N); b.w2[i] = w * w; }
            std::vector<double> acc(H, 0.0); for (int i = 0; i < N; ++i) acc[i % H] += b.w2[i];
            b.w2norm = (float)(1.0 / acc[0]);
        }
        hist.assign(C, std::vector<float>(histLen, 0.f));
        fade = std::max(16, (int)(0.0015 * sr));
        transients.clear(); transients.reserve(maxTransients + 1);
        reset();
    }
    int getLatency() const { return latency; }
    void setParams(double semitones, bool formant, double formantSemis) {
        pitch = std::pow(2.0, semitones / 12.0); formantOn = formant; fform = std::pow(2.0, formantSemis / 12.0);
        lo.setPitch(pitch); hi.setPitch(pitch);
        const float na = (float)std::min(1.0, 1.5 * std::abs(std::log2(pitch)));
        lo.noiseAmount = na; hi.noiseAmount = na;
        const bool full = formant || std::abs(formantSemis) > 1e-6;
        lo.needFullSpectrum = full; hi.needFullSpectrum = full;
    }
    void reset() {
        for (auto& h : hist) std::fill(h.begin(), h.end(), 0.f);
        for (auto& b : bands) {
            for (auto& o : b.ola) std::fill(o.begin(), o.end(), 0.f);
            // pre-fill weights as if steady state, so start-up is not boosted
            std::fill(b.wsum.begin(), b.wsum.end(), 1.f / b.w2norm);
            b.counter = 0; b.bp->reset(); b.lastReset = -(1LL << 40);
        }
        absPos = 0; transients.clear();
        detBlockPos = 0; detE = 0; detAvg = 0; lastT = -(1LL << 40);
    }
    // in-place
    void process(float* const* io, int numSamples) {
        for (int n = 0; n < numSamples; ++n) {
            const int wp = (int)(absPos & (histLen - 1));
            float xd = 0;
            for (int c = 0; c < C; ++c) {
                const float x = io[c][n];
                const int pp = (wp - 1) & (histLen - 1);
                const float d = x - hist[c][pp]; xd += d * d;
                hist[c][wp] = x;
                float y = 0;
                for (auto& b : bands) {
                    const float g = b.wsum[wp] * b.w2norm;
                    const float gain = g > 0.999f ? 1.f : 1.f / std::max(g, 0.35f);
                    y += b.ola[c][wp] * gain; b.ola[c][wp] = 0.f;
                }
                io[c][n] = y;
            }
            for (auto& b : bands) b.wsum[wp] = 0.f;
            detect(xd);
            for (auto& b : bands)
                if (++b.counter >= b.bp->hop()) { b.counter = 0; runFrame(b); }
            ++absPos;
        }
    }

private:
    struct Band {
        BandProcessor* bp = nullptr;
        std::vector<std::vector<float>> bx, by, bo, ola; std::vector<const float*> px, py; std::vector<float*> po;
        std::vector<float> wsum, w2, mask; float w2norm = 1;
        int counter = 0; long long lastReset = 0;
    };
    // time-domain attack detector on high-passed energy, 64-sample blocks
    void detect(float e) {
        detE += e;
        if (++detBlockPos < 64) return;
        const float be = detE / 64.f;
        detBlockPos = 0; detE = 0;
        const long long blockStart = absPos - 63;
        if (be > 8.f * detAvg + 1e-7f && be > 1e-6f && blockStart - lastT > (long long)(0.03 * sr)) {
            if (transients.size() >= maxTransients) transients.erase(transients.begin());
            transients.push_back(blockStart); lastT = blockStart; // Kapazität reserviert: keine Allokation
        }
        detAvg = 0.85f * detAvg + 0.15f * be;
        while (!transients.empty() && transients.front() < absPos - 4LL * latency) transients.erase(transients.begin());
    }
    void runFrame(Band& b) {
        const int N = b.bp->fftSize(), H = b.bp->hop();
        const long long frameStart = absPos - N + 1, centre = frameStart + N / 2;
        // masking of attacks ahead of the centre
        bool masked = false;
        for (long long T : transients) {
            if (T > centre + H / 2 && T <= absPos) {
                masked = true;
                for (int i = 0; i < N; ++i) {
                    const long long a = frameStart + i;
                    b.mask[i] = a >= T ? 0.f : (a > T - fade ? 0.5f + 0.5f * (float)std::cos(kPi * (double)(a - (T - fade)) / fade) : 1.f);
                }
                break;
            }
        }
        const int base = (int)(frameStart & (histLen - 1));
        for (int c = 0; c < C; ++c) {
            const float* h = hist[c].data();
            for (int i = 0; i < N; ++i) {
                const int a = (base + i) & (histLen - 1);
                const float m = masked ? b.mask[i] : 1.f;
                const float mb = masked ? (i >= H ? b.mask[i - H] : 1.f) : 1.f;
                b.bx[c][i] = h[a] * m; b.by[c][i] = h[(a - H) & (histLen - 1)] * mb;
            }
        }
        b.bp->analyse(b.px, b.py);
        bool reset = false;
        for (long long T : transients)
            if (T > centre - H / 2 && T <= centre + H / 2 && centre - b.lastReset > N / 4) { reset = true; b.lastReset = centre; break; }
        b.bp->synthesise(formantOn, fform, reset, b.po);
        const int ob = (int)((frameStart + latency) & (histLen - 1));
        for (int i = 0; i < N; ++i) {
            const int o = (ob + i) & (histLen - 1);
            for (int c = 0; c < C; ++c) b.ola[c][o] += b.bo[c][i];
            b.wsum[o] += b.w2[i] * (masked ? b.mask[i] : 1.f);
        }
    }
    int C = 2; double sr = 44100;
    BandProcessor lo, hi; Band bands[2];
    int latency = 4096, histLen = 0, fade = 64;
    long long absPos = 0, lastT = 0;
    static constexpr size_t maxTransients = 64;
    std::vector<long long> transients;
    int detBlockPos = 0; float detE = 0, detAvg = 0;
    std::vector<std::vector<float>> hist;
    double pitch = 1, fform = 1; bool formantOn = true;
};

// -----------------------------------------------------------------------------
//  Tempo-Erkennung (BPM) über Onset-Hüllkurve + Autokorrelation
//  Liefert 0, wenn nichts Sinnvolles gefunden wurde.
// -----------------------------------------------------------------------------
inline double detectTempo(const std::vector<std::vector<float>>& input, double sr) {
    const int C = (int)input.size();
    if (!C) return 0;
    const long n = (long)input[0].size();
    const int hop = 512, win = 1024;
    if (n < sr * 2.5) return 0;  // zu kurz für eine sichere Schätzung
    std::vector<float> env;
    float prev = 0;
    for (long a = 0; a + win <= n; a += hop) {
        double e = 1e-10;
        for (long i = a + 1; i < a + win; ++i)
            for (int c = 0; c < C; ++c) { const double d = input[c][i] - 0.97 * input[c][i - 1]; e += d * d; }
        const float l = (float)std::log(e);
        env.push_back(std::max(0.f, l - prev));
        prev = l;
    }
    // Mittelwert abziehen
    double m = 0; for (float v : env) m += v; m /= std::max<size_t>(1, env.size());
    for (float& v : env) v = (float)std::max(0.0, v - m);
    const double fr = sr / hop;
    double bestScore = 0, bestBpm = 0;
    for (double bpm = 60.0; bpm <= 200.0; bpm += 0.25) {
        const double lag = 60.0 / bpm * fr;
        double sc = 0;
        for (int mult = 1; mult <= 4; ++mult) {
            const double L = lag * mult; const long Li = (long)L; const double f = L - Li;
            double acc = 0;
            for (long i = Li + 1; i < (long)env.size(); ++i) acc += env[i] * (env[i - Li] * (1 - f) + env[i - Li - 1] * f);
            sc += acc / mult;
        }
        // leichte Vorliebe für den üblichen Bereich 85..160 BPM
        const double w = std::exp(-0.5 * std::pow(std::log2(bpm / 120.0) / 0.6, 2.0));
        sc *= 0.6 + 0.4 * w;
        if (sc > bestScore) { bestScore = sc; bestBpm = bpm; }
    }
    // auf ganze BPM einrasten, wenn sehr nah dran
    if (std::abs(bestBpm - std::round(bestBpm)) <= 0.35) bestBpm = std::round(bestBpm);
    return bestBpm;
}

} // namespace wormhole
