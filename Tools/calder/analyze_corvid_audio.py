"""Match actual in-game NPS crow/raven PCM and recorded submix calls.
Source identity, mixer audibility and owner listening are distinct evidence.
"""
import argparse,hashlib,json,math,re,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_wildlife_audio import read,analyze
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('run',type=Path);a=p.parse_args();run=a.run
folder=run/'Saved/CalderWildlifeSound';log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
manifest=json.loads((ROOT/'SourceArt/CalderLandscape/phase3/audio/nps_corvids/license_and_processing.json').read_text())
rate,mix=read(folder/'WildlifeMix.wav');mono=mix.mean(axis=1).astype(np.float64)
record=re.search(r'CALDER_WILDLIFE_AUDIO_RECORD_START nativeWall=([0-9.]+)',log);assert record
start=float(record[1]);old=ROOT/'Saved/CalderWork/runs/codex_owner_bird_audio_v1/Saved/CalderWildlifeSound'
originals={path.stem:hashlib.sha256(path.read_bytes()).hexdigest()==hashlib.sha256((old/path.name).read_bytes()).hexdigest() for path in folder.glob('*.wav') if path.stem not in ['CrowCaw','RavenCroak','WildlifeMix'] and not path.stem.startswith('Wildlife_')}
assert len(originals)==12
source={};proof=[]
def correlate(x,t):
 x=x.astype(np.float64);t=t.astype(np.float64);n=len(t)
 if len(x)<n:return 0.,0.
 count=1<<(len(x)+n-2).bit_length();cross=np.fft.irfft(np.fft.rfft(x,count)*np.conj(np.fft.rfft(t,count)),count)[:len(x)-n+1]
 s=np.concatenate(([0.],np.cumsum(x*x)));e=np.maximum(0,s[n:]-s[:-n]);denom=np.sqrt(e*np.dot(t,t));score=np.divide(cross,denom,out=np.zeros_like(cross),where=denom>1e-12)
 best=0.;where=0
 for i in np.argsort(score)[-min(64,len(score)):]:
  seg=x[i:i+n];d=math.sqrt(np.dot(seg,seg)*np.dot(t,t));v=np.dot(seg,t)/d if d>1e-12 else 0.
  if v>best:best=float(v);where=int(i)
 return best,where
for kind,name in [(12,'CrowCaw'),(13,'RavenCroak')]:
 sr,pcm=read(folder/(name+'.wav'));_,master=read(ROOT/'SourceArt/CalderLandscape/phase3/audio/nps_corvids'/(name+'.wav'))
 source[name]={'exact_production_samples':sr==24000 and np.array_equal(pcm,master),'seconds':len(pcm)/sr}
 for k,offset,wall in re.findall(r'CALDER_WILDLIFE_CALL kind=(\d+) .*?offsetSeconds=([0-9.]+) nativeWall=([0-9.]+)',log):
  if int(k)!=kind:continue
  at=float(wall)-start;t=pcm[int(float(offset)*sr):,0]
  if at<0 or at>len(mono)/rate:continue
  t=np.interp(np.arange(round(len(t)*rate/sr))*sr/rate,np.arange(len(t)),t)
  low=max(0,round((at-.5)*rate));high=min(len(mono),round((at+1.)*rate)+len(t))
  # The real renderer applies distance/occlusion low-pass. Retain both direct
  # and a conservative1.8kHz filtered reference, with no pitch/time changes.
  scores=[]
  for hz in [None,1800.]:
   ref=t
   if hz is not None:
    alpha=1.-math.exp(-2.*math.pi*hz/rate);ref=np.empty(len(t));v=0.
    for i,sample in enumerate(t):v+=alpha*(sample-v);ref[i]=v
   score,index=correlate(mono[low:high],ref);scores.append((score,index,hz))
  score,index,hz=max(scores,key=lambda x:x[0]);proof.append({'name':name,'expected_seconds':at,'matched_seconds':(low+index)/rate,'normalized_correlation':score,'reference_lpf_hz':hz,'detected':score>=.10})
qa=analyze(folder);stats=re.findall(r'CALDER_WILDLIFE_SOUND voices=(\d+) peakVoices=(\d+) peakActiveSources=(\d+)',log)
peaks={'voices':max(int(x[1]) for x in stats),'sources':max(int(x[2]) for x in stats)}
result={'passed':all(originals.values()) and all(x['exact_production_samples'] for x in source.values()) and {x['name'] for x in proof if x['detected']}=={'CrowCaw','RavenCroak'} and qa['passed'] and peaks['voices']<=7 and peaks['sources']<=24,'scope':'Actual PCM identity and native wildlife-only mixer detections; owner accepted recording direction and requested longer excerpts. Performance and subjective distance listening separate.','normal_clock_unpinned':not re.search(r'CalderSceneTime=|CalderWildlifeClockOffset=',log),'original_twelve_unchanged':originals,'source':source,'mix_seconds':len(mono)/rate,'peaks':peaks,'correlations':proof,'whole_phase3_complete':False}
out=folder/'corvid_audio_analysis.json';assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 1)
