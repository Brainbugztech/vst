// Brainfuzz – Pegel-Abgleich der Werkspresets
// Rendert jedes Preset mit synthetischem Testmaterial (Kick, Hats, Saw-Bass, Akkorde) und
// berechnet den Output, mit dem das Preset so laut ist wie das Original.
// Schreibt ../Source/dsp/PresetLevels.h. Mit "--wav <ordner>" zusaetzlich WAV-Dateien zum Reinhoeren.
//
// g++ -O2 -std=c++17 -I../Source level_match.cpp -o level_match && ./level_match

#include "dsp/Presets.h"
#include <cstdio>
#include <cstdint>
#include <random>
#include <fstream>
#include <map>

using namespace bfz;
static const double fs = 48000.0;

// ------------------------------------------------------------------ Testmaterial (145 BPM)
static void makeTestSignal (std::vector<float>& L, std::vector<float>& R, double seconds)
{
    const int n = (int) (seconds * fs);
    L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f);
    const double beat = 60.0 / 145.0;
    std::mt19937 rng (42);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    double bassPh = 0, padPh[3] { 0, 0, 0 };
    float hpState = 0, lpBass = 0;
    const double chords[4][3] { { 220.0, 261.6, 329.6 }, { 196.0, 246.9, 293.7 }, { 174.6, 220.0, 261.6 }, { 196.0, 246.9, 311.1 } };
    const double bassNotes[4] { 55.0, 49.0, 43.65, 49.0 };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const double pb = std::fmod (t, beat) / beat;           // Position im Beat
        const int beatIdx = (int) (t / beat);
        const int bar = (beatIdx / 4) % 4;
        // Kick
        const double kt = pb * beat;
        const double kEnv = std::exp (-kt * 9.0);
        const double kFreq = 48.0 + 120.0 * std::exp (-kt * 35.0);
        static double kPh = 0; if (pb * beat < 1.0 / fs) kPh = 0;
        kPh += kFreq / fs;
        const float kick = (float) (0.8 * kEnv * std::sin (6.2831853 * kPh));
        // Offbeat-Bass (Psy-Rolling): 3 Sechzehntel nach der Kick
        const double sixteenth = std::fmod (t, beat / 4.0) / (beat / 4.0);
        const int six = (int) (pb * 4.0);
        const double bEnv = six == 0 ? 0.0 : std::exp (-sixteenth * 3.5);
        bassPh += bassNotes[bar] / fs; if (bassPh >= 1) bassPh -= 1;
        float saw = (float) (2.0 * bassPh - 1.0);
        lpBass += (saw - lpBass) * 0.08f;
        const float bass = (float) (0.45 * bEnv) * lpBass;
        // Hats (Offbeat-Achtel)
        const double eighth = std::fmod (t + beat / 2.0, beat) / beat;
        const float nz = u (rng);
        const float hp = nz - hpState; hpState = nz;
        const float hat = (float) (0.22 * std::exp (-eighth * beat * 60.0)) * hp;
        // Pad
        float pad = 0;
        for (int k = 0; k < 3; ++k) { padPh[k] += chords[bar][k] / fs; if (padPh[k] >= 1) padPh[k] -= 1; pad += (float) (2.0 * padPh[k] - 1.0); }
        pad *= 0.045f;
        const float pan = (float) (0.5 + 0.3 * std::sin (t * 0.7));
        L[(size_t) i] = kick + bass + hat * 0.8f + pad * (1.0f - pan) * 1.4f;
        R[(size_t) i] = kick + bass + hat * 1.0f + pad * pan * 1.4f;
    }
}

// Lautheit: RMS nach Hochpass 80 Hz und leichter Hoehen-Anhebung (grob wie K-Gewichtung)
static double loudness (const std::vector<float>& L, const std::vector<float>& R, size_t from)
{
    auto hp = bilinear (toHighpass (butterworth (2)), 80.0, fs);
    SosRunner<2> f; f.f = hp;
    double e = 0;
    for (size_t i = 0; i < L.size(); ++i)
    {
        const double a = f.process (0, L[i]), b = f.process (1, R[i]);
        if (i >= from) e += a * a + b * b;
    }
    return 10.0 * std::log10 (e / (double) (L.size() - from) + 1e-20);
}

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t n = (uint32_t) L.size(), rate = (uint32_t) fs, bytes = n * 4;
    auto w32 = [&] (uint32_t x) { f.write ((const char*) &x, 4); };
    auto w16 = [&] (uint16_t x) { f.write ((const char*) &x, 2); };
    f.write ("RIFF", 4); w32 (36 + bytes); f.write ("WAVEfmt ", 8); w32 (16); w16 (1); w16 (2); w32 (rate); w32 (rate * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (bytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        const auto s = [] (float x) { return (int16_t) std::lround (std::clamp (x, -1.0f, 1.0f) * 32767.0f); };
        const int16_t a = s (L[i]), b = s (R[i]);
        f.write ((const char*) &a, 2); f.write ((const char*) &b, 2);
    }
}

int main (int argc, char** argv)
{
    std::string wavDir;
    for (int i = 1; i < argc; ++i) if (std::string (argv[i]) == "--wav" && i + 1 < argc) wavDir = argv[++i];

    std::vector<float> inL, inR;
    makeTestSignal (inL, inR, 10.0);
    const size_t from = (size_t) (1.5 * fs);
    const double refLoud = loudness (inL, inR, from);
    if (! wavDir.empty()) writeWav (wavDir + "/00_Original.wav", inL, inR);

    const auto presets = factoryPresetsRaw();
    std::vector<std::pair<std::string, float>> result;
    int idx = 0;
    for (const auto& pr : presets)
    {
        auto vals = defaultValues();
        for (const auto& [id, x] : pr.set)
        {
            const int k = paramIndex (id);
            if (k < 0) { fprintf (stderr, "Unbekannter Parameter %s in %s\n", id.c_str(), pr.name.c_str()); return 1; }
            vals[(size_t) k] = x;
        }
        // Mehrere Runden: messen, ausgleichen, nachmessen (der Limiter macht es nichtlinear)
        float outDb = 0.0f;
        double loud = 0;
        std::vector<float> L, R;
        for (int pass = 0; pass < 7; ++pass)
        {
            vals[pOut] = outDb;
            BrainFuzzEngine e; e.prepare (fs, 512);
            L = inL; R = inR;
            const auto ep = toEngineParams (vals.data());
            for (size_t s = 0; s < L.size(); s += 512)
            {
                const int n = (int) std::min<size_t> (512, L.size() - s);
                float* ch[2] { L.data() + s, R.data() + s };
                e.process (ch, n, ep);
            }
            loud = loudness (L, R, from);
            outDb = std::clamp (outDb + (float) (refLoud - loud), -24.0f, 24.0f);
        }
        outDb = std::round (outDb * 10.0f) / 10.0f;
        printf ("%2d %-22s %-16s Output %+5.1f dB   (Rest %+.2f dB)\n", idx, pr.name.c_str(), pr.group.c_str(), outDb, loud - refLoud);
        result.push_back ({ pr.name, outDb });
        if (! wavDir.empty())
        {
            char fn[256]; snprintf (fn, sizeof fn, "%s/%02d_%s.wav", wavDir.c_str(), idx + 1, pr.name.c_str());
            std::string s = fn; for (auto& c : s) if (c == ' ') c = '_';
            writeWav (s, L, R);
        }
        ++idx;
    }

    std::ofstream h ("../Source/dsp/PresetLevels.h");
    h << "#pragma once\n\n// Automatisch erzeugt von tools/level_match.cpp – nicht von Hand bearbeiten.\n"
         "// Output-Pegel pro Werkspreset, damit alle Presets so laut sind wie das Original.\n\n"
         "#include <string>\n\nnamespace bfz\n{\n\ninline float presetLevelDb (const std::string& name)\n{\n";
    for (const auto& [n, d] : result)
    {
        char line[200]; snprintf (line, sizeof line, "    if (name == \"%s\") return %.1ff;\n", n.c_str(), d);
        h << line;
    }
    h << "    return 0.0f;\n}\n\n} // namespace bfz\n";
    printf ("PresetLevels.h geschrieben (%d Presets)\n", (int) result.size());
    return 0;
}
