"""Owner-directed rhythm demo and matching owned-code replacement; no install."""
from pathlib import Path
import hashlib, json, math, subprocess, wave
import numpy as np

HERE = Path(__file__).parent
ROOT = HERE.parents[2]
RATE = 32000
# A slow four-bar fragment at 96 bpm: five-stroke pickup, seven-stroke
# response, an empty bar, then an incomplete quiet answer. Paired strokes
# retain a common pulse; small authored offsets follow a player's phrasing.
strokes = [
    (0, 2, .91, 0), (1, 1, .49, .003),
    (1.5, 0, .21, .001), (1.625, 0, .18, .0015),
    (1.75, 1, .35, .002), (1.875, 1, .31, .0025), (2, 2, .86, .004),
    (4, 1, .58, .005), (5, 0, .23, .007), (5.5, 1, .40, .006),
    (6.25, 0, .16, .008), (6.375, 0, .19, .0085),
    (6.5, 0, .22, .009), (6.625, 0, .25, .0095),
    (6.75, 1, .33, .010), (6.875, 1, .36, .0105), (7, 2, .74, .013),
    (11.875, 0, .13, .008), (11.9375, 0, .16, .0085),
    (12, 1, .45, .010), (13, 0, .24, .016), (14, 0, .13, .024),
]
events = []
for i, (beat, strength, velocity, offset) in enumerate(strokes):
    events.append((round(.20 + beat * .625 + offset, 6), velocity, strength * 2 + i % 2))
delays = [.193, .307, .479, .713]
gains = [.10, .075, .045, .025]
bank = []
for level in (11, 13, 22, 24, 31, 33):
    with wave.open(str(HERE / f'snare_center_vl{level}_16k.wav'), 'rb') as w:
        assert w.getframerate() == 16000 and w.getnchannels() == 1
        x = np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float) / 32768
    bank.append(np.interp(np.arange((len(x) - 1) * 2) * .5, np.arange(len(x)), x))
dry = np.zeros(RATE * 11)
for at, velocity, layer in events:
    start = int(at * RATE + .5)
    clip = bank[layer] * velocity
    dry[start:start + len(clip)] += clip
wet = dry.copy()
for seconds, gain in zip(delays, gains):
    d = int(seconds * RATE)
    wet[d:] += dry[:-d] * gain
alpha = 1 - math.exp(-2 * math.pi * 1800 / RATE)
low = 0
for i, value in enumerate(wet):
    low += (value - low) * alpha
    wet[i] = low
wet *= .85 / max(abs(wet).max(), .0001)
candidate = (HERE / 'recorded_snare_candidate.inc').read_text()
start = candidate.index(' const TCHAR* Bars[]')
end = candidate.index(' TArray<int16> Out;', start)
rows = ',\n'.join(f'  {{{at:.6f}f,{velocity:.6f}f,{layer}}}' for at, velocity, layer in events)
replacement = ''' // Owner-directed sparse, slow ghost cadence: natural paired rudiments,
 // one empty bar and a fading unfinished answer, rather than a busy loop.
 struct FGhostStroke{float Seconds,Velocity;int32 Layer;};
 const FGhostStroke Strokes[]={
''' + rows + '''
 };
 TArray<float> Dry;Dry.SetNumZeroed(Rate*11);
 for(const FGhostStroke& Stroke:Strokes){const int32 At=FMath::RoundToInt(Stroke.Seconds*Rate);const auto& Clip=Bank[Stroke.Layer];for(int32 J=0;J<(SourceSamples-1)*2&&At+J<Dry.Num();++J){const int32 A=J/2;const float Value=FMath::Lerp(float(Clip[A]),float(Clip[A+1]),(J%2)*.5f)/32768.f;Dry[At+J]+=Value*Stroke.Velocity;}}
 const int32 Delay[]={int32(.193f*Rate),int32(.307f*Rate),int32(.479f*Rate),int32(.713f*Rate)};const float Gain[]={.10f,.075f,.045f,.025f};TArray<float> Wet=Dry;
 for(int32 I=0;I<Wet.Num();++I)for(int32 J=0;J<4;++J)if(I>=Delay[J])Wet[I]+=Dry[I-Delay[J]]*Gain[J];
 const float Alpha=1.f-FMath::Exp(-2.f*PI*1800.f/Rate);float Low=0,Peak=.0001f;for(float& V:Wet){Low+=(V-Low)*Alpha;V=Low;Peak=FMath::Max(Peak,FMath::Abs(V));}
'''
out = HERE / 'creepy_snare_candidate.inc'
assert not out.exists()
out.write_text(candidate[:start] + replacement + candidate[end:])
wav = HERE / 'creepy_snare_rhythm_demo.wav'
with wave.open(str(wav), 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
    w.writeframes(np.rint(wet * (.22 / .85) * 32767).astype('<i2').tobytes())
subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-i', str(wav), '-codec:a', 'libmp3lame', '-b:a', '192k', str(wav.with_suffix('.mp3'))], check=True)
(HERE / 'creepy_snare_preparation.json').write_text(json.dumps({
    'status': 'Prepared rhythm demo only; not installed, compiled or in-game accepted.',
    'owner_direction': 'Make it creepier; previous rhythm feels unnatural.',
    'tempo_bpm': 96, 'bars': 4, 'seconds': 11,
    'strokes': [{'seconds': t, 'velocity': v, 'sample_layer': layer} for t, v, layer in events],
    'audition': 'Rhythm demo before game attenuation; demo peak .22. Runtime gain/timers remain unchanged.',
    'candidate_sha256': hashlib.sha256(out.read_bytes()).hexdigest(),
    'demo_sha256': hashlib.sha256(wav.read_bytes()).hexdigest(),
}, indent=2) + '\n')
print(wav.with_suffix('.mp3'))
