#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
struct Wav { int sr = 44100; std::vector<std::vector<float>> ch; };

static bool readWav(const char* path, Wav& w) {
    FILE* f = fopen(path, "rb"); if (!f) return false;
    char id[4]; uint32_t sz; fread(id, 1, 4, f); fread(&sz, 4, 1, f); fread(id, 1, 4, f);
    int fmt = 1, chans = 2, bits = 16;
    while (fread(id, 1, 4, f) == 4) {
        fread(&sz, 4, 1, f);
        if (!memcmp(id, "fmt ", 4)) {
            uint16_t a, c; uint32_t s, br; uint16_t ba, b;
            fread(&a, 2, 1, f); fread(&c, 2, 1, f); fread(&s, 4, 1, f); fread(&br, 4, 1, f); fread(&ba, 2, 1, f); fread(&b, 2, 1, f);
            fmt = a; chans = c; w.sr = (int)s; bits = b; fseek(f, sz - 16, SEEK_CUR);
            if (fmt == 0xFFFE) fmt = bits == 32 ? 3 : 1;
        } else if (!memcmp(id, "data", 4)) {
            long frames = sz / (chans * bits / 8);
            w.ch.assign(chans, std::vector<float>(frames));
            for (long i = 0; i < frames; ++i) for (int c = 0; c < chans; ++c) {
                if (fmt == 3) { float v; fread(&v, 4, 1, f); w.ch[c][i] = v; }
                else if (bits == 16) { int16_t v; fread(&v, 2, 1, f); w.ch[c][i] = v / 32768.f; }
                else if (bits == 24) { uint8_t b[3]; fread(b, 1, 3, f); int32_t v = (b[0] << 8 | b[1] << 16 | b[2] << 24) >> 8; w.ch[c][i] = v / 8388608.f; }
            }
            break;
        } else fseek(f, sz, SEEK_CUR);
    }
    fclose(f); return !w.ch.empty();
}

static void writeWav(const char* path, const Wav& w) {
    FILE* f = fopen(path, "wb");
    uint16_t C = (uint16_t)w.ch.size(); uint32_t n = (uint32_t)w.ch[0].size();
    uint32_t dataSz = n * C * 4, sz = 36 + dataSz, sr = w.sr, br = sr * C * 4; uint16_t fmt = 3, ba = C * 4, bits = 32; uint32_t fs = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fs, 4, 1, f);
    fwrite(&fmt, 2, 1, f); fwrite(&C, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) for (int c = 0; c < C; ++c) fwrite(&w.ch[c][i], 4, 1, f);
    fclose(f);
}

