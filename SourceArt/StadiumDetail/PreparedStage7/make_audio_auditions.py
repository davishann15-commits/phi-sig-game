"""Build listening excerpts from actual isolated native stems; does not approve audio."""
from pathlib import Path
import argparse,hashlib,json,wave
import numpy as np
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args()
work=ROOT/'Saved/StadiumDetailWork'
receipt=json.loads((work/(a.tag+'_sound_pcm.json')).read_text())
assert receipt['native_and_pcm_passed']
audio=ROOT/'Saved/CalderWork/runs'/a.tag/'Saved/StadiumSoundReview'
out=work/(a.tag+'_auditions');out.mkdir(exist_ok=False)
voices=('Flood','PA','Halyard','Snare')
def read(path):
 with wave.open(str(path),'rb') as w:
  assert w.getsampwidth()==2
  rate=w.getframerate();channels=w.getnchannels()
  return np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').reshape(-1,channels).astype(float)/32768,rate
items=[]
for phase,name in ((0,'ground'),(2,'first_snare'),(6,'recovery')):
 arrays=[];rate=None
 for voice in voices:
  samples,r=read(audio/f'P{phase}_{name}_{voice}.wav')
  assert rate in (None,r);rate=r;arrays.append(samples)
 # Stop/export copies run sequentially on the game thread. Keep the common
 # recorded interval and disclose any trailing audio-block trimming; never pad,
 # stretch, normalize or change runtime gain to manufacture an audition.
 lengths=[len(x) for x in arrays];tail=max(lengths)-min(lengths)
 assert tail/rate<.25,(name,lengths,rate)
 common=min(lengths);arrays=[x[:common] for x in arrays]
 mixed=sum(arrays)
 if phase==0:
  excerpts=[('hum',0,10)]
 else:
  snare=arrays[3];block=rate//4
  energies=[float(np.mean(snare[i:i+block]**2)) for i in range(0,len(snare)-block,block)]
  peak=int(np.argmax(energies))*block/rate
  assert energies and max(energies)>0
  start=max(0,min(peak-5,len(mixed)/rate-15))
  excerpts=[('snare',start,15)]
  if phase==6:excerpts.append(('returned_hum',0,10))
 for kind,start,seconds in excerpts:
  piece=mixed[int(start*rate):int((start+seconds)*rate)]
  for gain_db,suffix in ((0,'raw'),(18,'audition_plus18dB')):
   data=piece*(10**(gain_db/20));assert np.max(np.abs(data))<.99
   target=out/f'{name}_{kind}_{suffix}.wav'
   with wave.open(str(target),'wb') as w:
    w.setnchannels(data.shape[1]);w.setsampwidth(2);w.setframerate(rate);w.writeframes(np.rint(data*32767).astype('<i2').tobytes())
   items.append({'file':str(target.relative_to(ROOT)),'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'actual_native_stems':True,'added_gain_db':gain_db,'phase':phase,'start_in_recording_seconds':start,'seconds':len(piece)/rate,'trailing_common_window_trim_seconds':tail/rate})
manifest=out/'auditions.json';manifest.write_text(json.dumps({'tag':a.tag,'actual_audio_review_pending':True,'scope':'Sum of the four real isolated stadium submix stems; omits other world audio. Raw is unchanged gain. Explicit +18dB copy is for hearing quiet details only. No runtime loudness change.','files':items},indent=2)+'\n')
print(manifest)
