"""Prepared offline actual-submix/source/schedule checks. Does not assert image review."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,csv,hashlib,json,math,re,wave
import numpy as np

ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser();parser.add_argument('tag');args=parser.parse_args()
assert re.fullmatch(r'[a-z0-9_]+',args.tag)
folder=ROOT/'Saved/CalderWork/runs'/args.tag/'Saved'
audio=folder/'StadiumSoundReview';work=ROOT/'Saved/StadiumDetailWork'
log=(folder/'Logs/SeniorSendoff.log').read_text(errors='replace')
summary=(audio/'result.txt').read_text()
assert summary.startswith('PASS ') and 'CALDER_STADIUM_SOUND_REVIEW PASS ' in log
assert not re.search(r'REVIEW FAIL|Fatal error|Assertion failed',log)
assert 'actualRegisteredSubmixes=4 defaultTimerUnchanged=1' in log


def read(path):
    with wave.open(str(path),'rb') as stream:
        rate=stream.getframerate();channels=stream.getnchannels();width=stream.getsampwidth();count=stream.getnframes()
        assert width==2,(path,width)
        samples=np.frombuffer(stream.readframes(count),dtype='<i2').astype(np.float64)/32768.
    samples=samples.reshape(-1,channels)
    return samples,rate


def inspect(path):
    samples,rate=read(path);peak=float(np.max(np.abs(samples)));rms=float(np.sqrt(np.mean(samples*samples)))
    block=max(1,int(rate));window=[float(np.sqrt(np.mean(samples[i:i+block]**2))) for i in range(0,len(samples),block) if len(samples[i:i+block])>rate*.5]
    return {'file':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'rate':rate,'channels':samples.shape[1],'seconds':len(samples)/rate,'peak':peak,'rms':rms,'rms_dbfs':20*math.log10(max(rms,1e-15)),'peak_second_rms_dbfs':20*math.log10(max(max(window,default=0),1e-15))}

phase_names=['ground','stands','first_snare','inside_stadium','inside_house','far','recovery','inside_coach_office']
seconds=[48,48,54,48,48,48,228,24];voices=['Flood','PA','Halyard','Snare'];results=[]
# Recording is on the normal real audio clock. A screenshot/game-thread hitch can
# make audio elapsed longer than world elapsed; compare WAV length to the actual
# logged audio window, while separately preserving every original world phase.
starts={int(i):(float(w),float(t)) for i,w,t in re.findall(r'CALDER_STADIUM_SOUND_RECORD_START phase=(\d+) name=[^ ]+ world=([0-9.]+) audio=([0-9.]+)',log)}
stops={int(i):(float(w),float(t)) for i,w,t in re.findall(r'CALDER_STADIUM_SOUND_RECORD_STOP phase=(\d+) world=([0-9.]+) audio=([0-9.]+)',log)}
assert set(starts)==set(stops)==set(range(8))
assert 'Abandoning existing write operation' not in log
windows=[]
for i in range(8):
    world=stops[i][0]-starts[i][0];actual_audio=stops[i][1]-starts[i][1]
    assert abs(world-(seconds[i]-3))<.20,(i,world,seconds[i])
    assert world-.10<=actual_audio<=world+5,(i,world,actual_audio)
    windows.append({'world_elapsed':world,'actual_audio_elapsed':actual_audio})

for phase,name in enumerate(phase_names):
    row={}
    for voice in voices:
        result=inspect(audio/f'P{phase}_{name}_{voice}.wav');assert abs(result['seconds']-windows[phase]['actual_audio_elapsed'])<.25,(phase,result,windows[phase])
        assert result['peak']<.9,result
        row[voice]=result
    if phase in (3,4,5,7):
        for voice in voices[:3]:assert row[voice]['peak']==0,(phase,voice,row[voice])
    if phase==7:
        for voice in voices:assert row[voice]['peak']==0,(phase,voice,row[voice])
    if phase in (0,2,6):
        for voice in voices[:2]:assert 1e-6<row[voice]['rms']<.05,(phase,voice,row[voice])
        assert row['Halyard']['peak']>1e-5,(phase,row['Halyard'])
    if phase==1:assert row['Flood']['rms']>1e-6
    if phase in (2,6):assert -85<row['Snare']['peak_second_rms_dbfs']<-35,(phase,row['Snare'])
    else:assert row['Snare']['peak']==0,(phase,row['Snare'])
    results.append({'phase':phase,'name':name,'recording_window':windows[phase],'voices':row})
for voice in voices[:2]:
    delta=results[6]['voices'][voice]['rms_dbfs']-results[0]['voices'][voice]['rms_dbfs'];assert abs(delta)<=2.,(voice,delta)

sources={name:inspect(audio/(name+'.wav')) for name in ['HumSource','HalyardSource']}
assert sources['HumSource']['rate']==sources['HalyardSource']['rate']==24000
assert sources['HumSource']['channels']==sources['HalyardSource']['channels']==1
assert abs(sources['HumSource']['seconds']-4)<1e-6 and abs(sources['HalyardSource']['seconds']-1)<1e-6
samples,rate=read(audio/'HumSource.wav');mono=samples[:,0];power=np.abs(np.fft.rfft(mono*np.hanning(len(mono))))**2;frequencies=np.fft.rfftfreq(len(mono),1/rate)
mask=sum((np.abs(frequencies-f)<2 for f in [60,120,360]),np.zeros_like(frequencies,dtype=bool))>0
assert float(np.sum(power[mask])/np.sum(power))>.9
starts=re.findall(r'CAMPUS_FALL_DRUMS start trigger=([^ ]+) seconds=([0-9.]+) play=([0-9]+) next=([0-9]+)s',log)
assert len(starts)>=2 and all(row[0]=='schedule' for row in starts),starts
assert all(abs(float(row[1])-11)<.1 and 219<=int(row[3])<=336 for row in starts),starts
assert [int(row[2]) for row in starts]==list(range(1,len(starts)+1)),starts
rows=list(csv.DictReader((audio/'live_sources.csv').open()));assert len(rows)>1000
for row in rows:
    if int(row['playing']):assert int(row['available_bytes'])>=0
images=[audio/f'P{i}_{name}.png' for i,name in enumerate(phase_names)]
images+=[audio/'flicker_dim.png',audio/'flicker_bright.png',audio/'flood_flicker_dim.png',audio/'flood_flicker_bright.png']
images+=[audio/f'paper_{i:02d}.png' for i in range(6)]+[audio/f'flags_{i:02d}.png' for i in range(6)]
images+=[audio/'banner_floor.png']
assert len(images)==25 and all(p.exists() and p.stat().st_size>1000 for p in images)
result={'native_and_pcm_passed':True,'final_acceptance':False,'images_inspection_pending':True,'tag':args.tag,'stage':7,'finished_utc':datetime.now(timezone.utc).isoformat(),'summary':summary,'actual_phases':results,'exact_source_pcm':sources,'default_timer_drums':starts,'images':[{'file':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in images],'evidence_files':[str((folder/'Logs/SeniorSendoff.log').relative_to(ROOT)),str((audio/'live_sources.csv').relative_to(ROOT)),str((audio/'result.txt').relative_to(ROOT))],'scope':'Real registered-submix output/source queue/indoor/far/recovery/quiet/defaulttimer/sourcePCM checks. All25actualimages and actualspatiallistening still require review; no false subjective acceptance.'}
p=work/(args.tag+'_sound_pcm.json');assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n');print('PASS native+actualPCM/defaulttimers; actual25imageinspection pending:',p)
