"""Compile the actual shared honk DSP into a native, asset-free WAV preview.
This verifies source determinism/format, not subjective realism or game audio.
"""
import argparse, hashlib, json, subprocess, tempfile, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);a=p.parse_args()
out=a.directory.resolve();assert not out.exists();out.mkdir(parents=True)
header=ROOT/'Source/SeniorSendoff/HouseCalderWildlifeGooseHonkDsp.h'
code=r'''
#include "HouseCalderWildlifeGooseHonkDsp.h"
#include <algorithm>
#include <fstream>
#include <vector>
int main(int argc,char** argv){
 if(argc!=2)return 1;
 using namespace HouseCalderWildlifeGooseHonkDsp;
 FVoice voice;std::vector<double> values(int(Length*Rate));double peak=.000001;
 for(int i=0;i<int(values.size());++i){values[i]=voice.Sample(i);peak=std::max(peak,std::abs(values[i]));}
 std::ofstream out(argv[1],std::ios::binary);
 for(double v:values){int16_t s=int16_t(v/peak*.72*32760.);out.put(char(s&255));out.put(char((uint16_t(s)>>8)&255));}
}
'''
with tempfile.TemporaryDirectory(prefix='calder-honk-') as tmp:
 tmp=Path(tmp);(tmp/'preview.cpp').write_text(code)
 subprocess.run(['xcrun','clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-I',str(header.parent),str(tmp/'preview.cpp'),'-o',str(tmp/'preview')],check=True)
 subprocess.run([str(tmp/'preview'),str(tmp/'honk.pcm')],check=True)
 pcm=(tmp/'honk.pcm').read_bytes()
with wave.open(str(out/'GooseHonk.wav'),'wb') as w:
 w.setparams((1,2,24000,0,'NONE','not compressed'));w.writeframes(pcm)
old=ROOT/'Saved/CalderWork/runs/codex_goose_audio_arrival_v2/Saved/CalderWildlifeSound/GooseHonk.wav'
with wave.open(str(old),'rb') as w:before=w.readframes(w.getnframes());assert w.getparams()[:3]==(1,2,24000)
with wave.open(str(out/'BeforeAfter.wav'),'wb') as w:
 w.setparams((1,2,24000,0,'NONE','not compressed'));w.writeframes(before+b'\0'*int(24000*.7)*2+pcm)
report={'scope':'shared production DSP offline preview only','sample_rate':24000,'samples':len(pcm)//2,'duration':len(pcm)/48000,'pcm_sha256':hashlib.sha256(pcm).hexdigest(),'header_sha256':hashlib.sha256(header.read_bytes()).hexdigest(),'old_pcm_sha256':hashlib.sha256(before).hexdigest(),'changed':pcm!=before,'external_assets':False,'subjective_listening_accepted':False,'native_game_validation':False}
(out/'preview.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
