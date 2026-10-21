// CLI renderer: renders every factory preset to a 24-bit WAV + an "all presets" demo file.
// usage: render <outdir> [bpm]
#include "../Source/Presets.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace snarehead;

static void writeWav24(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::perror(path.c_str()); return; }
    uint32_t n = (uint32_t)L.size(), dataBytes = n * 2 * 3;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32((uint32_t)sr); w32((uint32_t)sr * 6); w16(6); w16(24);
    std::fwrite("data", 1, 4, f); w32(dataBytes);
    for (uint32_t i = 0; i < n; ++i)
        for (float s : {L[i], R[i]}) {
            int32_t v = (int32_t)std::lround(std::clamp(s, -1.f, 1.f) * 8388607.f);
            uint8_t b[3] = {(uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff), (uint8_t)((v >> 16) & 0xff)};
            std::fwrite(b, 1, 3, f);
        }
    std::fclose(f);
}

static void renderHits(SnareEngine& e, const std::vector<float>& hitTimes, float len, int sr,
                       std::vector<float>& L, std::vector<float>& R) {
    int n = (int)(len * sr);
    L.assign((size_t)n, 0); R.assign((size_t)n, 0);
    std::vector<int> hits; for (float t : hitTimes) hits.push_back((int)(t * sr));
    int pos = 0; const int block = 64;
    while (pos < n) {
        int nb = std::min(block, n - pos);
        for (int h : hits) if (h >= pos && h < pos + nb) {
            // split so the hit lands exactly
            int pre = h - pos;
            if (pre > 0) { e.process(&L[(size_t)pos], &R[(size_t)pos], pre); pos += pre; nb -= pre; }
            e.noteOn(60, 0.9f);
        }
        if (nb > 0) { e.process(&L[(size_t)pos], &R[(size_t)pos], nb); pos += nb; }
    }
}

int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : ".";
    float bpm = argc > 2 ? (float)std::atof(argv[2]) : 145.f;
    const int sr = 44100;
    auto presets = factoryPresets();
    std::vector<float> allL, allR;
    int idx = 1;
    for (auto& pr : presets) {
        SnareEngine e; e.prepare(sr); e.setTempo(bpm); e.setParams(pr.p, true);
        std::vector<float> L, R;
        renderHits(e, {0.f}, 2.5f, sr, L, R);
        // trim trailing silence (-80 dB)
        size_t end = L.size();
        while (end > 1 && std::fabs(L[end - 1]) < 1e-4f && std::fabs(R[end - 1]) < 1e-4f) --end;
        end = std::min(L.size(), end + (size_t)(0.01f * sr));
        L.resize(end); R.resize(end);
        float pk = 0, rms = 0; size_t rn = std::min(end, (size_t)(0.2f * sr));
        bool bad = false;
        for (size_t i = 0; i < end; ++i) { pk = std::max({pk, std::fabs(L[i]), std::fabs(R[i])}); if (!std::isfinite(L[i]) || !std::isfinite(R[i])) bad = true; }
        for (size_t i = 0; i < rn; ++i) rms += 0.5f * (L[i] * L[i] + R[i] * R[i]);
        rms = std::sqrt(rms / (float)rn);
        char name[256]; std::snprintf(name, sizeof name, "%s/%02d %s.wav", out.c_str(), idx, pr.name.c_str());
        writeWav24(name, L, R, sr);
        std::printf("%02d %-20s len %.2fs  peak %6.1f dB  rms200 %6.1f dB %s\n", idx, pr.name.c_str(),
                    (double)end / sr, 20 * std::log10(pk + 1e-9), 20 * std::log10(rms + 1e-9), bad ? "NaN!" : "");
        // groove demo: 2 bars, snare on 2 and 4
        SnareEngine g; g.prepare(sr); g.setTempo(bpm); g.setParams(pr.p, true);
        float beat = 60.f / bpm;
        std::vector<float> GL, GR;
        renderHits(g, {beat, 3 * beat, 5 * beat, 7 * beat}, 8 * beat, sr, GL, GR);
        allL.insert(allL.end(), GL.begin(), GL.end()); allR.insert(allR.end(), GR.begin(), GR.end());
        ++idx;
    }
    writeWav24(out + "/00 SnareHead - All Presets (2 bars each).wav", allL, allR, sr);
    return 0;
}
