#include "../Engine/WormholeEngine.h"
#include "wavio.h"
int main(int c,char**v){Wav w; readWav(v[1],w); printf("%.2f\n", wormhole::detectTempo(w.ch,w.sr));}
