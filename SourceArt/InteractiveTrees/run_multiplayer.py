"""Opt-in tree interaction check through the normal two-player lobby."""
import json
import pathlib
import subprocess
import time
import uuid

base = pathlib.Path(__file__).resolve().parent
repo = base.parents[1]
exe = '/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
if subprocess.run(['pgrep', '-x', 'UnrealEditor'], capture_output=True).returncode == 0:
    raise SystemExit('Run this after the other editor/game has exited.')

run_id = uuid.uuid4().hex[:8]
jobs = []
started = time.monotonic()
result = {}


def launch(name, map_arg):
    log = base / f'network-{name}.log'
    log.unlink(missing_ok=True)
    stream = (base / f'network-{name}-stdout.log').open('w')
    user_dir = pathlib.Path(f'/tmp/sso-trees-{name}-{run_id}')
    settings = user_dir / 'Saved/Config/MacEditor/GameUserSettings.ini'
    settings.parent.mkdir(parents=True, exist_ok=True)
    settings.write_text('''[ScalabilityGroups]
sg.ResolutionQuality=100
sg.ViewDistanceQuality=2
sg.AntiAliasingQuality=2
sg.ShadowQuality=2
sg.GlobalIlluminationQuality=2
sg.ReflectionQuality=2
sg.PostProcessQuality=2
sg.TextureQuality=2
sg.EffectsQuality=2
sg.FoliageQuality=2
sg.ShadingQuality=2

[/Script/Engine.GameUserSettings]
bUseVSync=False
ResolutionSizeX=640
ResolutionSizeY=360
FullscreenMode=2
Version=5
''')
    args = [exe, str(repo / 'SeniorSendoff.uproject'), map_arg,
            '-game', '-HouseTreesNetworkReview', '-ResX=640', '-ResY=360',
            '-windowed', '-NoSplash', '-NoTraceServer', '-port=7783',
            '-ExecCmds=t.MaxFPS 30', f'-UserDir={user_dir}', f'-AbsLog={log}']
    process = subprocess.Popen(args, stdout=stream, stderr=subprocess.STDOUT)
    jobs.append((name, process, log, stream, user_dir))
    return process, log


def read(log):
    return log.read_text(errors='replace') if log.exists() else ''


try:
    host, host_log = launch('host', '/Game/Story/Maps/Lobby?listen')
    while time.monotonic() - started < 100:
        if 'listening on port 7783' in read(host_log):
            break
        if host.poll() is not None:
            raise RuntimeError('Host exited before its listen socket was ready.')
        time.sleep(.5)
    else:
        raise RuntimeError('Host did not open its listen socket.')
    launch('client', '127.0.0.1:7783')
    print('Listen host and separate client running through the normal lobby.', flush=True)
    while time.monotonic() - started < 300:
        if all(process.poll() is not None for _, process, _, _, _ in jobs):
            break
        if any('TREES_REVIEW FAIL' in read(log) for _, _, log, _, _ in jobs):
            raise RuntimeError('Native tree interaction check reported failure.')
        if any('NetworkFailure: PendingConnectionFailure' in read(log) for _, _, log, _, _ in jobs):
            raise RuntimeError('Client connection was rejected.')
        time.sleep(.5)
    else:
        raise RuntimeError('Multiplayer check exceeded its bounded run.')
    peers = []
    for name, process, log, _, user_dir in jobs:
        ok = process.returncode == 0 and 'TREES_REVIEW PASS' in read(log)
        peers.append(dict(peer=name, exit_code=process.returncode,
                          status='PASS' if ok else 'FAIL', log=str(log),
                          evidence=str(user_dir / 'Saved/TreesReview')))
    result = dict(status='PASS' if all(peer['status'] == 'PASS' for peer in peers)
                  else 'FAIL', peers=peers)
except Exception as error:
    result = dict(status='FAIL', error=str(error))
finally:
    for _, process, _, stream, _ in jobs:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=12)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        stream.close()
    result['seconds'] = round(time.monotonic() - started, 2)
    (base / 'multiplayer-verification.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2), flush=True)
raise SystemExit(0 if result['status'] == 'PASS' else 1)
