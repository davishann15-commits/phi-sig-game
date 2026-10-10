"""Make a seamless, gently balanced loop from the saved CC0 field recording."""
from pathlib import Path
import array
import json
import math
import wave

base = Path(__file__).resolve().parent
with wave.open(str(base/'assets/source_mono.wav'), 'rb') as w:
    rate = w.getframerate()
    assert w.getnchannels() == 1 and w.getsampwidth() == 2
    samples = array.array('h', w.readframes(w.getnframes()))

# Overlap the tail with the beginning, preserving the original dry-leaf detail.
fade = int(rate * .8)
x = list(samples[int(rate*.08):-int(rate*.06)])
mean = sum(x)/len(x)
x = [v-mean for v in x]
joined = [x[-fade+i]*math.cos(i/(fade-1)*math.pi/2)
          + x[i]*math.sin(i/(fade-1)*math.pi/2) for i in range(fade)]
joined.extend(x[fade:-fade])
rms = math.sqrt(sum(v*v for v in joined)/len(joined))
gain = min(32767*.72/max(abs(v) for v in joined), 32767*.12/rms)
pcm = array.array('h', (round(v*gain) for v in joined))
destination = base/'assets/LeavesRustlingLoop.wav'
with wave.open(str(destination), 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes(pcm.tobytes())
report = {
    'rate_hz': rate, 'channels': 1, 'duration_seconds': len(pcm)/rate,
    'peak_dbfs': 20*math.log10(max(abs(v) for v in pcm)/32767),
    'rms_dbfs': 20*math.log10(math.sqrt(sum(v*v for v in pcm)/len(pcm))/32767),
    'loop_boundary_step': abs(pcm[-1]-pcm[0]), 'clipped_samples': sum(abs(v)>=32767 for v in pcm),
    'source': 'Rustling leaves by giddster, CC0 1.0; see assets/source.json',
}
assert report['clipped_samples'] == 0
(base/'evidence/audio-analysis.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
