// Test CLI: cli in.wav out.wav timeRatio semitones [formant 0/1] [realtime 0/1]
#include "../Engine/WormholeEngine.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <chrono>

#include "wavio.h"
int main(int argc, char** argv) {
    if (argc < 5) { printf("usage: cli in out ratio semis [formant] [realtime]\n"); return 1; }
    Wav in; if (!readWav(argv[1], in)) { printf("read fail\n"); return 1; }
    wormhole::Params p; p.timeRatio = atof(argv[3]); p.pitchSemitones = atof(argv[4]);
    p.formantPreserve = argc > 5 ? atoi(argv[5]) != 0 : false;
    if (getenv("WH_NOTRANS")) p.transientLock = false;
    bool rt = argc > 6 && atoi(argv[6]) != 0;
    auto t0 = std::chrono::steady_clock::now();
    Wav out; out.sr = in.sr;
    if (!rt) {
        wormhole::OfflineStretcher s;
        out.ch = s.process(in.ch, in.sr, p);
    } else {
        wormhole::RealtimeShifter r; int C = (int)in.ch.size();
        r.prepare(in.sr, C); r.setParams(p.pitchSemitones, p.formantPreserve, 0);
        int L = r.getLatency(); long n = (long)in.ch[0].size();
        out.ch.assign(C, std::vector<float>(n + L, 0.f));
        for (int c = 0; c < C; ++c) std::copy(in.ch[c].begin(), in.ch[c].end(), out.ch[c].begin());
        std::vector<float*> ptr(C);
        for (long pos = 0; pos < n + L; pos += 512) {
            int blk = (int)std::min<long>(512, n + L - pos);
            for (int c = 0; c < C; ++c) ptr[c] = out.ch[c].data() + pos;
            r.process(ptr.data(), blk);
        }
        for (int c = 0; c < C; ++c) out.ch[c].erase(out.ch[c].begin(), out.ch[c].begin() + L);
    }
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    double dur = in.ch[0].size() / (double)in.sr;
    printf("processed %.2fs audio in %.2fs (%.1fx realtime)\n", dur, sec, dur / sec);
    writeWav(argv[2], out);
    return 0;
}
