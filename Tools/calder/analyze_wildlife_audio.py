"""Inspect real PCM/submix WAVs; NumPy only, optional ffmpeg spectrograms.
Run with Codex's bundled Python runtime. This is objective signal QA, not a
claim of subjective naturalness or complete wildlife/master acceptance.
"""
import argparse,hashlib,json,math,struct,subprocess
from pathlib import Path
import numpy as np

def read(path):
    buf=path.read_bytes();assert buf[:4]==b'RIFF' and buf[8:12]==b'WAVE',path
    offset=12;fmt=data=None
    while offset+8<=len(buf):
        kind,size=struct.unpack_from('<4sI',buf,offset);assert offset+8+size<=len(buf),('truncated WAV chunk',path,kind,size);chunk=memoryview(buf)[offset+8:offset+8+size]
        if kind==b'fmt ':fmt=chunk
        if kind==b'data':data=chunk
        offset+=8+size+(size&1)
    assert fmt is not None and data is not None,path
    tag,channels,rate,_,_,bits=struct.unpack_from('<HHIIHH',fmt)
    if tag==65534:tag=struct.unpack_from('<H',fmt,24)[0]
    if tag==3:samples=np.frombuffer(data,dtype='<f4' if bits==32 else '<f8').astype(np.float32)
    elif tag==1 and bits in (16,32):samples=np.frombuffer(data,dtype='<i2' if bits==16 else '<i4').astype(np.float32)/(2**(bits-1))
    else:raise ValueError((path,tag,bits))
    samples=samples.reshape(-1,channels);assert samples.size and np.all(np.isfinite(samples)),path
    return rate,samples

def analyze(directory,spectrograms=False,require_locations=False,log_path=None):
    rows=[];passed=True
    log=log_path if log_path is not None else directory.parent/'Logs/SeniorSendoff.log';registered=log.exists() and 'CALDER_WILDLIFE_SOUND_SUBMIX_REGISTERED id=' in log.read_text(errors='replace')
    for path in sorted(directory.glob('*.wav')):
        rate,x=read(path);rms=float(np.sqrt(np.mean(x*x,dtype=np.float64)));peak=float(np.max(np.abs(x)))
        row={'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'rate':rate,'channels':x.shape[1],'seconds':len(x)/rate,'rms':rms,'rms_dbfs':20*math.log10(max(rms,1e-12)),'peak':peak,'clipped_samples':int(np.count_nonzero(np.abs(x)>=.999))}
        row['passed']=row['clipped_samples']==0
        if path.stem=='NightBed':
            block=rate//200;y=x[:,0];env=np.sqrt(np.mean(y[:len(y)//block*block].reshape(-1,block)**2,axis=1))
            candidates=np.flatnonzero((env[1:-1]>=env[:-2])&(env[1:-1]>env[2:])&(env[1:-1]>.1))+1;peaks=[]
            for index in sorted(candidates,key=lambda i:-env[i]):
                if all(abs(index-other)>=160 for other in peaks):peaks.append(int(index))
            row['snowy_chirps_per_60s']=len(peaks);row['passed'] &= len(peaks)==60 and row['seconds']==60.
        if path.stem.startswith('Wildlife_') and path.stem!='WildlifeMix':row['passed'] &= 7.5<=row['seconds']<=8.5
        if path.stem=='Wildlife_frat_lawn':row['passed'] &= row['rms_dbfs']<=-36.
        if path.stem in ('Wildlife_inside_house','Wildlife_inside_campus'):
            row['all_samples_zero']=bool(np.all(x==0));row['passed'] &= row['all_samples_zero']
        if path.stem=='Wildlife_near_creek':row['passed'] &= rms>0.
        if spectrograms:
            png=directory/(path.stem+'_'+row['sha256'][:8]+'_spectrogram.png')
            if not png.exists():
                subprocess.run(['ffmpeg','-v','error','-nostdin','-n','-i',str(path),'-lavfi','showspectrumpic=s=1280x720:legend=1:color=viridis:scale=log:fscale=log','-frames:v','1','-update','1',str(png)],check=True)
            row['spectrogram']=png.name
        passed &= row['passed'];rows.append(row)
    names={row['file'] for row in rows};expected={'Wildlife_'+s+'.wav' for s in ('frat_lawn','inside_house','inside_campus','near_creek')}
    complete=expected<=names
    if require_locations:passed &= complete and registered
    report={'passed':bool(passed and rows),'location_recordings_complete':complete,'submix_registration_verified':registered,'require_locations':require_locations,'subjective_listening_pending':True,'whole_phase3_complete':False,'rows':rows}
    path=directory/'waveform_analysis.json';path.unlink(missing_ok=True);path.write_text(json.dumps(report,indent=2)+'\n');return report

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);parser.add_argument('--spectrograms',action='store_true');parser.add_argument('--require-locations',action='store_true');args=parser.parse_args()
    result=analyze(args.directory,args.spectrograms,args.require_locations)
    print(json.dumps({'passed':result['passed'],'location_recordings_complete':result['location_recordings_complete'],'files':len(result['rows']),'report':str(args.directory/'waveform_analysis.json')}))
    raise SystemExit(0 if result['passed'] else 1)
