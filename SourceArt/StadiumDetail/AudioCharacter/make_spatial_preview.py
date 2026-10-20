"""Audition an already-exported real cadence phase; never grants acceptance."""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, wave
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser()
p.add_argument('tag')
p.add_argument('--phase', type=int, choices=(2, 6), default=2)
p.add_argument('--gain-db', type=float, default=42)
a = p.parse_args()
assert re.fullmatch('[a-z0-9_]+', a.tag) and 0 <= a.gain_db <= 42
saved = ROOT / 'Saved/CalderWork/runs' / a.tag / 'Saved'
log = (saved / 'Logs/SeniorSendoff.log').read_text(errors='replace')
assert re.search(rf'CALDER_STADIUM_SOUND_RECORD_STOP phase={a.phase} ', log)
name = 'first_snare' if a.phase == 2 else 'recovery'
parts, sources, rate = [], [], None
for voice in ('Flood', 'PA', 'Halyard', 'Snare'):
    path = saved / 'StadiumSoundReview' / f'P{a.phase}_{name}_{voice}.wav'
    with wave.open(str(path), 'rb') as w:
        assert w.getsampwidth() == 2 and rate in (None, w.getframerate())
        rate = w.getframerate()
        parts.append(np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, w.getnchannels()).astype(float) / 32768)
    sources.append({'file': str(path.relative_to(ROOT)), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
lengths = [len(x) for x in parts]
assert (max(lengths) - min(lengths)) / rate < .25
n = min(lengths)
parts = [x[:n] for x in parts]
block = rate // 4
energy = [float(np.mean(parts[3][i:i + block] ** 2)) for i in range(0, n - block, block)]
assert max(energy) > 0
peak = int(np.argmax(energy)) * block / rate
start = max(0, min(peak - 5, n / rate - 15))
x = sum(parts)[int(start * rate):int((start + 15) * rate)] * 10 ** (a.gain_db / 20)
assert abs(x).max() < .99
out = ROOT / 'Saved/StadiumDetailWork' / f'{a.tag}_{name}_plus{a.gain_db:g}dB.wav'
assert not out.exists()
with wave.open(str(out), 'wb') as w:
    w.setnchannels(x.shape[1]); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes(np.rint(x * 32767).astype('<i2').tobytes())
mp3 = out.with_suffix('.mp3')
subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-i', str(out), '-codec:a', 'libmp3lame', '-b:a', '192k', str(mp3)], check=True)
out.with_suffix('.json').write_text(json.dumps({
    'tag': a.tag, 'phase': a.phase, 'actual_native_stems': sources,
    'added_gain_db': a.gain_db, 'start_in_recording_seconds': start,
    'seconds': len(x) / rate, 'common_window_tail_trim_seconds': (max(lengths) - n) / rate,
    'scope': 'Actual four isolated stadium stems summed; other world audio omitted. Audition gain only; runtime unchanged. Full native test, PCM analysis and subjective acceptance remain separate.',
    'files': [{'file': str(f.relative_to(ROOT)), 'sha256': hashlib.sha256(f.read_bytes()).hexdigest()} for f in (out, mp3)],
}, indent=2) + '\n')
print(mp3)
