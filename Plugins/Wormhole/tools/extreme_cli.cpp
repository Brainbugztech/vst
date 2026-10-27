#include "../Engine/WormholeEngine.h"
#include "wavio.h"
#include <chrono>
// xtest in.wav out.wav ratio semis formant maxSeconds [startSec]
int main(int argc,char**argv){
  Wav in; readWav(argv[1],in);
  wormhole::Params p; p.timeRatio=atof(argv[3]); p.pitchSemitones=atof(argv[4]); p.formantPreserve=atoi(argv[5]);
  double maxS=atof(argv[6]); double startS = argc>7?atof(argv[7]):0;
  wormhole::StretchStream st; st.setInput(&in.ch,in.sr); st.start(p,(long)(startS*in.sr));
  long want=std::min<long>((long)(maxS*in.sr), st.outputLength()-st.position());
  Wav out; out.sr=in.sr; int C=in.ch.size(); out.ch.assign(C,std::vector<float>(want));
  auto t0=std::chrono::steady_clock::now(); long done=0; std::vector<float*> pp(C);
  while(done<want){ for(int c=0;c<C;c++) pp[c]=out.ch[c].data()+done; int g=st.pull(pp.data(),(int)std::min<long>(4096,want-done)); if(g<=0)break; done+=g; }
  double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
  printf("extreme=%d texture=%.3fs  %.1fs Ausgabe in %.2fs (%.1fx Echtzeit), Gesamtlaenge %.1fs\n", st.isExtreme(), st.textureUsed(), done/(double)in.sr, sec, done/(double)in.sr/sec, st.outputLength()/(double)in.sr);
  writeWav(argv[2],out);
}
