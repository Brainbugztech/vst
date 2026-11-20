#pragma once

// Wavetables: 8 eingebaute Tabellen + eigene (per Drag & Drop) - je 32 Frames x 2048 Samples,
// 10 bandbegrenzte Mip-Stufen (pro Oktave weniger Obertoene, damit hohe Toene nicht aliasen).

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include <cmath>
#include <vector>

namespace pluto
{
// Eine Wavetable: 32 Frames x 2048 Samples, je 10 bandbegrenzte Mip-Stufen
class WtTable
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numFrames = 32;
    static constexpr int numMips = 10;

    WtTable() { data.resize ((size_t) numFrames * numMips * tableSize); }

    // frames: beliebig viele Frames mit je tableSize Samples; werden auf 32 Frames verteilt
    void build (const std::vector<std::vector<float>>& src)
    {
        juce::dsp::FFT fft (11);
        std::vector<float> spec (tableSize * 2), buf (tableSize * 2);
        const int n = (int) src.size();
        for (int fr = 0; fr < numFrames; ++fr)
        {
            // Zwischen den Quell-Frames interpolieren
            const float fp = n > 1 ? (float) fr / (float) (numFrames - 1) * (float) (n - 1) : 0.0f;
            const int a = juce::jmin ((int) fp, juce::jmax (0, n - 1));
            const int b = juce::jmin (a + 1, n - 1);
            const float t = fp - (float) a;
            std::fill (spec.begin(), spec.end(), 0.0f);
            for (int i = 0; i < tableSize; ++i)
                spec[(size_t) i] = src[(size_t) a][(size_t) i] * (1.0f - t) + src[(size_t) b][(size_t) i] * t;
            buildFrame (fft, spec, buf, fr);
        }
    }

    float read (float pos, double phase, double inc) const noexcept
    {
        const double ai = std::abs (inc);
        int mip = 0;
        if (ai > 0.0) mip = juce::jlimit (0, numMips - 1, (int) std::ceil (std::log2 (ai * (double) tableSize)));
        const float fp = juce::jlimit (0.0f, 1.0f, pos) * (float) (numFrames - 1);
        const int f0 = juce::jmin ((int) fp, numFrames - 2);
        const float ff = fp - (float) f0;
        const double x = phase * tableSize;
        const int i0 = (int) x & (tableSize - 1);
        const int i1 = (i0 + 1) & (tableSize - 1);
        const float fx = (float) (x - std::floor (x));
        const float* fa = frame (f0, mip);
        const float* fb = frame (f0 + 1, mip);
        const float va = fa[i0] + (fa[i1] - fa[i0]) * fx;
        const float vb = fb[i0] + (fb[i1] - fb[i0]) * fx;
        return va + (vb - va) * ff;
    }

    // Rohdaten der obersten Mip-Stufe (fuers Speichern im Projekt)
    void getFrames (std::vector<float>& out) const
    {
        out.resize ((size_t) numFrames * tableSize);
        for (int fr = 0; fr < numFrames; ++fr)
            std::copy (frame (fr, 0), frame (fr, 0) + tableSize, out.begin() + (ptrdiff_t) fr * tableSize);
    }

private:
    std::vector<float> data;
    float* frame (int fr, int m) { return data.data() + ((size_t) fr * numMips + (size_t) m) * tableSize; }
    const float* frame (int fr, int m) const { return data.data() + ((size_t) fr * numMips + (size_t) m) * tableSize; }

    void buildFrame (juce::dsp::FFT& fft, std::vector<float>& spec, std::vector<float>& buf, int fr)
    {
        fft.performRealOnlyForwardTransform (spec.data(), true);
        spec[0] = spec[1] = 0.0f; // DC weg
        float gain = 1.0f;
        for (int m = 0; m < numMips; ++m)
        {
            const int maxH = (tableSize / 2) >> m;
            std::copy (spec.begin(), spec.end(), buf.begin());
            for (int h = maxH; h <= tableSize / 2; ++h)
                buf[(size_t) h * 2] = buf[(size_t) h * 2 + 1] = 0.0f;
            fft.performRealOnlyInverseTransform (buf.data());
            float* dst = frame (fr, m);
            if (m == 0)
            {
                float peak = 1.0e-6f;
                for (int i = 0; i < tableSize; ++i) peak = std::max (peak, std::abs (buf[(size_t) i]));
                gain = 1.0f / peak;
            }
            for (int i = 0; i < tableSize; ++i) dst[i] = buf[(size_t) i] * gain;
        }
    }
};

// Eingebaute Tabellen (werden beim Start einmal berechnet)
class WavetableBank
{
public:
    static constexpr int tableSize = WtTable::tableSize;
    static constexpr int numTables = 8;

    WavetableBank()
    {
        std::vector<std::vector<float>> frames ((size_t) WtTable::numFrames, std::vector<float> (tableSize));
        for (int t = 0; t < numTables; ++t)
        {
            for (int fr = 0; fr < WtTable::numFrames; ++fr)
            {
                const float pos = (float) fr / (float) (WtTable::numFrames - 1);
                for (int i = 0; i < tableSize; ++i)
                    frames[(size_t) fr][(size_t) i] = generate (t, pos, (float) i / (float) tableSize);
            }
            tables[(size_t) t].build (frames);
        }
    }

    float read (int table, float pos, double phase, double inc) const noexcept
    {
        return tables[(size_t) juce::jlimit (0, numTables - 1, table)].read (pos, phase, inc);
    }
    float readDisplay (int table, float pos, float phase) const noexcept { return read (table, pos, phase, 0.0); }

private:
    std::array<WtTable, numTables> tables;

    static float sinc2pi (float x) { return std::sin (juce::MathConstants<float>::twoPi * x); }

    static float naive (int shape, float x)
    {
        switch (shape)
        {
            case 0: return sinc2pi (x);
            case 1: return x < 0.25f ? 4.0f * x : (x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f);
            case 2: return 2.0f * x - 1.0f;
            default: return x < 0.5f ? 1.0f : -1.0f;
        }
    }

    static float generate (int table, float t, float x)
    {
        using juce::MathConstants;
        switch (table)
        {
            case 0: // Basic Morph: Sin -> Tri -> Saw -> Square
            {
                const float p = t * 3.0f;
                const int a = juce::jmin ((int) p, 2);
                const float f = p - (float) a;
                return naive (a, x) * (1.0f - f) + naive (a + 1, x) * f;
            }
            case 1: // PWM Sweep
            {
                const float w = 0.5f - t * 0.46f;
                return x < w ? 1.0f : -1.0f;
            }
            case 2: // Harmonic Rise: Obertoene kommen nacheinander dazu
            {
                const float n = 1.0f + t * 63.0f;
                float s = 0.0f;
                for (int h = 1; h <= 64; ++h)
                {
                    const float g = juce::jlimit (0.0f, 1.0f, n - (float) (h - 1));
                    if (g <= 0.0f) break;
                    s += g * sinc2pi ((float) h * x) / (float) h;
                }
                return s;
            }
            case 3: // Vowel Morph A-E-I-O-U
            {
                static const float F[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
                const float p = t * 4.0f;
                const int a = juce::jmin ((int) p, 3);
                const float f = p - (float) a;
                float fm[3];
                for (int k = 0; k < 3; ++k) fm[k] = F[a][k] * (1.0f - f) + F[a + 1][k] * f;
                const float f0 = 110.0f;
                float s = 0.0f;
                for (int h = 1; h <= 120; ++h)
                {
                    const float hz = f0 * (float) h;
                    float amp = 0.0f;
                    for (int k = 0; k < 3; ++k)
                    {
                        const float bw = 80.0f + 40.0f * (float) k;
                        const float d = (hz - fm[k]) / bw;
                        amp += std::exp (-0.5f * d * d) * (k == 0 ? 1.0f : (k == 1 ? 0.6f : 0.3f));
                    }
                    s += (amp + 0.02f / (float) h) * sinc2pi ((float) h * x);
                }
                return s;
            }
            case 4: // Sync Sweep: hart synchronisierter Saw
            {
                const float r = 1.0f + t * 7.0f;
                const float y = x * r;
                return 2.0f * (y - std::floor (y)) - 1.0f;
            }
            case 5: // FM Sweep
            {
                const float idx = t * 8.0f;
                return std::sin (MathConstants<float>::twoPi * x + idx * std::sin (MathConstants<float>::twoPi * x * 2.0f));
            }
            case 6: // Hollow Comb: Saw-Spektrum mit wanderndem Kamm
            {
                float s = 0.0f;
                for (int h = 1; h <= 96; ++h)
                {
                    const float c = 0.5f + 0.5f * std::cos ((float) h * MathConstants<float>::pi * (0.15f + t * 0.6f));
                    s += c * sinc2pi ((float) h * x) / (float) h;
                }
                return s;
            }
            default: // Digital Grit: gestufter Sinus mit Sample-and-Hold
            {
                const float levels = 24.0f - t * 21.0f;
                const float steps = 64.0f - t * 56.0f;
                const float xs = std::floor (x * steps) / steps;
                const float v = sinc2pi (xs) * 0.7f + sinc2pi (xs * 3.0f) * 0.3f * t;
                return std::round (v * levels) / levels;
            }
        }
    }
};

// ---------------- Eigene Wavetables laden ----------------
// Unterstuetzt: Serum-Wavetables (Frames a 2048, auch mit "clm "-Angabe), andere Frame-Groessen
// per clm-Chunk, und Single-Cycle-Dateien (die ganze Datei = ein Frame).
inline std::unique_ptr<WtTable> loadWavetableFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
    if (r == nullptr) { error = "Datei nicht lesbar (WAV/AIFF/FLAC erwartet)"; return nullptr; }
    const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) 2048 * 512);
    if (len < 16) { error = "Datei zu kurz"; return nullptr; }

    juce::AudioBuffer<float> b (1, len);
    r->read (&b, 0, len, 0, true, false);
    const float* x = b.getReadPointer (0);

    // Frame-Groesse: clm-Chunk ("<!>2048 ...") aus Serum/WaveEdit lesen
    int frameSize = 0;
    {
        juce::MemoryBlock mb;
        if (file.loadFileAsData (mb))
        {
            const auto* d = static_cast<const char*> (mb.getData());
            for (size_t i = 0; i + 12 < mb.getSize() && i < 1000000; ++i)
                if (d[i] == 'c' && d[i + 1] == 'l' && d[i + 2] == 'm' && d[i + 3] == ' ')
                {
                    juce::String txt (d + i + 8, juce::jmin<size_t> (40, mb.getSize() - i - 8));
                    if (txt.startsWith ("<!>")) frameSize = txt.substring (3).getIntValue();
                    break;
                }
        }
    }
    if (frameSize < 16 || frameSize > len)
    {
        if (len % 2048 == 0) frameSize = 2048;
        else if (len <= 4096) frameSize = len;   // Single-Cycle
        else frameSize = 2048;
    }

    const int numSrc = juce::jlimit (1, 256, len / frameSize);
    std::vector<std::vector<float>> frames ((size_t) numSrc, std::vector<float> (WtTable::tableSize));
    for (int f = 0; f < numSrc; ++f)
        for (int i = 0; i < WtTable::tableSize; ++i)
        {
            const double p = (double) i * frameSize / WtTable::tableSize;
            const int i0 = (int) p;
            const int i1 = (i0 + 1) % frameSize;
            const float t = (float) (p - i0);
            const float* fr = x + (size_t) f * (size_t) frameSize;
            frames[(size_t) f][(size_t) i] = fr[i0] + (fr[i1] - fr[i0]) * t;
        }

    auto table = std::make_unique<WtTable>();
    table->build (frames);
    return table;
}

inline std::unique_ptr<WtTable> wavetableFromFrames (const std::vector<float>& flat)
{
    const int n = (int) flat.size() / WtTable::tableSize;
    if (n < 1) return nullptr;
    std::vector<std::vector<float>> frames ((size_t) n);
    for (int f = 0; f < n; ++f)
        frames[(size_t) f].assign (flat.begin() + (ptrdiff_t) f * WtTable::tableSize, flat.begin() + (ptrdiff_t) (f + 1) * WtTable::tableSize);
    auto t = std::make_unique<WtTable>();
    t->build (frames);
    return t;
}
} // namespace pluto
