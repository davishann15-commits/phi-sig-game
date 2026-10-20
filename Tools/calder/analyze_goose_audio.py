"""Actual native PCM/mix evidence; run with bundled Python/NumPy.
Keeps source-signal, original-buffer identity, mixer audibility and listening
separate. A clean source WAV alone does not prove the voice reached the mix.
"""
import argparse,hashlib,json,math,re,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_wildlife_audio import read,analyze
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--mode',choices=['arrival','hiss','locations'],default='arrival');p.add_argument('--revision',default='');p.add_argument('--log',type=Path);a=p.parse_args();directory=a.directory;assert not a.revision or re.fullmatch('[a-z0-9_]+',a.revision)
base=ROOT/'Saved/CalderWork/runs/codex_deer_native_full_v3/Saved/CalderWildlifeSound'
originals={n:hashlib.sha256((directory/(n+'.wav')).read_bytes()).hexdigest()==hashlib.sha256((base/(n+'.wav')).read_bytes()).hexdigest() for n in ('DeerBlow','LeafCrunch','NightBed','Creek','LonePeeper')}
signals=analyze(directory,require_locations=a.mode=='locations',log_path=a.log)
rate,dry=read(directory/'GooseHonk.wav');assert rate==24000 and dry.shape[1]==1;dry=dry[:,0];energy=float(np.mean(dry*dry,dtype=np.float64))
def centroid(x):
 y=np.abs(np.fft.rfft(x))**2;return float(np.sum(np.fft.rfftfreq(len(x),1/rate)*y)/np.sum(y))
echoes=[]
for delay in range(2,6):
 hz,x=read(directory/f'GooseHonkEcho{delay}.wav');x=x[:,0];tail=x[delay*rate:];gain=20*math.log10(max(1e-12,float(np.sqrt(np.mean(tail*tail,dtype=np.float64)/energy))));valid=hz==rate and len(tail)==len(dry) and np.array_equal(x[:len(dry)],dry) and np.all(x[len(dry):delay*rate]==0) and -20<=gain<=-15 and centroid(tail)<centroid(dry)
 echoes.append({'delay_seconds':delay,'echo_rms_db_relative_to_direct':gain,'direct_centroid_hz':centroid(dry),'echo_centroid_hz':centroid(tail),'one_silent_gap_and_one_echo':bool(valid),'passed':bool(valid)})
log_path=a.log if a.log is not None else directory.parent/'Logs/SeniorSendoff.log';log=log_path.read_text(errors='replace');faces=[]
for x,y,z in re.findall(r'CALDER_GOOSE_HONK .*?ridge=X=([-0-9.]+) Y=([-0-9.]+) Z=([-0-9.]+)',log):
 x,y,z=map(float,(x,y,z));faces.append({'xyz':[x,y,z],'elevated_enclosing_ridge_region':z>=9600 and (y<=-65000 or y>=32000 or abs(x)>=42000)})
proof=[];mix_nonzero=False;duration=None;record=re.search(r'CALDER_GOOSE_AUDIO_RECORD_START nativeWall=([0-9.]+)',log)
def correlation(search,template):
 search=np.asarray(search,dtype=np.float64);template=np.asarray(template,dtype=np.float64)
 n=len(template)
 if len(search)<n:return 0.,0
 count=1<<(len(search)+n-2).bit_length();c=np.fft.irfft(np.fft.rfft(search,count)*np.conj(np.fft.rfft(template,count)),count)[:len(search)-n+1]
 s=np.concatenate(([0.],np.cumsum(search*search,dtype=np.float64)));energy=np.maximum(0.,s[n:]-s[:-n]);ref=float(np.dot(template,template));r=np.zeros(len(c));valid=energy>max(1e-25,float(np.max(energy))*1e-12)
 r[valid]=c[valid]/np.sqrt(energy[valid]*ref)
 # FFT roundoff divided by silent-window energy can exceed1. Recompute the
 # best candidates with direct dot products and actual window norms. This
 # also prevents cancellation in rolling energy from inventing an audible call.
 top=np.argsort(r)[-min(64,len(r)):];best=0.;index=0
 for at in top:
  segment=search[at:at+n];denom=math.sqrt(float(np.dot(segment,segment))*ref)
  if denom<=1e-15:continue
  score=float(np.dot(segment,template))/denom
  assert abs(score)<=1.00000001,score
  if score>best:best=score;index=int(at)
 return best,index
mix=directory/'WildlifeMix.wav'
if mix.exists():
 hz,pcm=read(mix);mono=pcm.mean(axis=1);duration=len(mono)/hz;mix_nonzero=bool(np.any(mono!=0))
 if hz!=rate:mono=np.interp(np.arange(round(len(mono)*rate/hz))*hz/rate,np.arange(len(mono)),mono)
 if record:
  began=float(record[1]);names={5:'GooseHiss',6:'GooseSplash',7:'GooseHonk',8:'GooseHonk',9:'GooseHonk',10:'GooseHonk',11:'GooseHonk'}
  for kind,offset,wall in re.findall(r'CALDER_WILDLIFE_CALL kind=(\d+) .*?offsetSeconds=([0-9.]+) nativeWall=([0-9.]+)',log):
   kind=int(kind)
   if kind not in names:continue
   at=float(wall)-began;_,reference=read(directory/(names[kind]+'.wav'));template=reference[:,0][int(float(offset)*rate):]
   lo=max(0,round((at-.5)*rate));hi=min(len(mono),round((at+.5)*rate)+len(template))
   if len(template)<rate*.1 or hi-lo<len(template):continue
   score,index=correlation(mono[lo:hi],template);proof.append({'kind':kind,'expected_seconds':at,'matched_seconds':(lo+index)/rate,'normalized_source_correlation':score,'detected':score>=.05})
expected={5,6} if a.mode=='hiss' else {6,7} # All honk/echo buffers share the same dry call.
detected={7 if row['kind']>=7 else row['kind'] for row in proof if row['detected']}
audible=expected<=detected
locations=a.mode=='locations'
passed=bool(signals['passed'] and signals['submix_registration_verified'] and all(originals.values()) and all(r['passed'] for r in echoes) and (locations or (mix_nonzero and audible and faces and all(r['elevated_enclosing_ridge_region'] for r in faces))))
# Hiss run has no overhead V, so no ridge observation is expected there.
if a.mode=='hiss':passed=bool(signals['passed'] and signals['submix_registration_verified'] and all(originals.values()) and all(r['passed'] for r in echoes) and mix_nonzero and audible)
result={'passed':passed,'scope':'Objective actual source/mixer QA; listening and full animal/master acceptance remain separate','original_five_actual_wav_sha256_match':originals,'source_signal_analysis_passed':signals['passed'],'echoes':echoes,'actual_honk_ridge_observations':faces,'mix_seconds':duration,'mix_nonzero':mix_nonzero,'native_record_start_metadata':bool(record),'audible_call_source_correlations':proof,'expected_call_kinds_detected_in_mix':audible,'subjective_listening_accepted':False,'full_performance_accepted':False,'whole_phase3_complete':False}
out=directory/('goose_audio_analysis'+('_'+a.revision if a.revision else '')+'.json');assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'passed':passed,'original_five_unchanged':all(originals.values()),'echoes_passed':all(r['passed'] for r in echoes),'audible_calls_detected':audible,'valid_ridge_observations':bool(faces and all(r['elevated_enclosing_ridge_region'] for r in faces)),'report':str(out)}));raise SystemExit(0 if passed else 1)
