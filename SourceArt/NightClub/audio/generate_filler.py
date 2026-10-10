"""Original restrained 120 BPM club loop; no samples or third-party recordings."""
from pathlib import Path
import hashlib
import json
import wave
import numpy as np

ROOT = Path(__file__).resolve().parent
RATE = 44100
BPM = 120
BEAT = 60 / BPM
LENGTH = 16
COUNT = RATE * LENGTH
RNG = np.random.default_rng(101204)
mix = np.zeros((COUNT, 2), dtype=np.float64)


def add(signal, at, gain=1, pan=0):
    """Wrap tails at the loop boundary, so the track is cyclic with no dead gap."""
    indexes = (np.arange(len(signal)) + round(at * RATE)) % COUNT
    left = np.sqrt((1 - pan) / 2)
    right = np.sqrt((1 + pan) / 2)
    np.add.at(mix[:, 0], indexes, signal * gain * left)
    np.add.at(mix[:, 1], indexes, signal * gain * right)


def times(duration):
    return np.arange(round(duration * RATE)) / RATE


def hz(midi):
    return 440 * 2 ** ((midi - 69) / 12)


def note(midi, duration, brightness=.2):
    t = times(duration)
    f = hz(midi)
    attack = 1 - np.exp(-t * 160)
    release = np.minimum(1, (duration - t) / .07)
    envelope = attack * np.exp(-t * 4) * release
    return envelope * (np.sin(2 * np.pi * f * t)
                       + brightness * np.sin(2 * np.pi * 2 * f * t)
                       + brightness * .30 * np.sin(2 * np.pi * 3 * f * t))


for beat in range(32):
    # Rounded four-on-the-floor kick: sine pitch falls from110Hz to47Hz.
    t = times(.34)
    phase = 2 * np.pi * (47 * t + (110 - 47) * .033 * (1 - np.exp(-t / .033)))
    kick = np.sin(phase) * (1 - np.exp(-t * 500)) * np.exp(-t * 15)
    add(kick, beat * BEAT, .50)
    if beat % 2:
        t = times(.16)
        noise = RNG.standard_normal(len(t))
        clap = (noise - np.roll(noise, 1)) * np.exp(-t * 32)
        clap *= .55 + .45 * np.cos(t * 700)
        add(clap, beat * BEAT, .055, -.12)
    for eighth in (0, 1):
        t = times(.065)
        noise = RNG.standard_normal(len(t))
        hat = (noise - np.roll(noise, 1)) * np.exp(-t * 80)
        add(hat, (beat + eighth * .5) * BEAT, .028, .25 if eighth else -.25)

# C-minor/G-minor/A-flat/F-minor; simple placeholder, no borrowed melody.
bass_roots = (36, 43, 32, 41)
for bar in range(8):
    root = bass_roots[(bar // 2) % len(bass_roots)]
    for offset, interval, duration in ((.5, 0, .34), (1.5, 0, .25),
                                       (2.25, 7, .22), (3.5, 0, .28)):
        add(note(root + interval, duration, .11), (bar * 4 + offset) * BEAT, .27)

    # Sparse warm chord pluck and a quiet dotted repeat; leave room for gameplay.
    for tone in (root + 24, root + 27, root + 31):
        sound = note(tone, .60, .12)
        add(sound, (bar * 4 + .75) * BEAT, .042, -.30)
        add(sound, (bar * 4 + 2.25) * BEAT, .018, .30)

# Remove any DC and soft-limit the highest percussion peaks.
mix -= mix.mean(axis=0)
mix = np.tanh(mix * 1.55)
peak = float(np.max(np.abs(mix)))
mix *= .72 / max(.72, peak)
pcm = np.round(mix * 32767).astype('<i2')
destination = ROOT / 'NC_OriginalFiller_120BPM.wav'
with wave.open(str(destination), 'wb') as out:
    out.setnchannels(2)
    out.setsampwidth(2)
    out.setframerate(RATE)
    out.writeframes(pcm.tobytes())

manifest = {
    'file': str(destination),
    'asset_name': 'NC_OriginalFiller_120BPM',
    'suggested_asset_path': '/Game/SeniorSendOff/House/NightClub/NC_OriginalFiller_120BPM',
    'author': 'Original procedural composition generated for SeniorSendoff',
    'third_party_samples': [],
    'sample_rate': RATE,
    'channels': 2,
    'bit_depth': 16,
    'duration_seconds': LENGTH,
    'bpm': BPM,
    'beats_per_loop': 32,
    'bars_4_4': 8,
    'seamless_wrap': True,
    'peak_dbfs': round(20 * np.log10(np.max(np.abs(mix))), 2),
    'rms_dbfs': round(20 * np.log10(np.sqrt(np.mean(mix * mix))), 2),
    'sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
    'unreal_soundwave_settings': {
        'looping': True,
        'loading_behavior': 'ForceInline',
        'compression_quality': 80,
        'sound_class': None,
    },
    'runtime_music_volume': .30,
}
(ROOT / 'sound_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps({k: manifest[k] for k in ('file', 'duration_seconds', 'bpm', 'peak_dbfs', 'rms_dbfs', 'sha256')}, indent=2))
