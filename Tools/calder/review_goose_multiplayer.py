"""Goose-background two-process native replication test through the real lobby.

Requires actual client pond ISM roots within1.2m of the host's shared-clock
position at the same wall time. Native capture follows all timing samples.
This covers background movement and roster replication, not shore reactions.
"""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
EXE='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
p=argparse.ArgumentParser();p.add_argument('name');args=p.parse_args()
assert re.fullmatch('[a-z0-9_]+',args.name)
out=ROOT/'Saved/CalderWork/runs'/args.name;assert not out.exists(),out
assert subprocess.run(['pgrep','-f','UnrealEditor.*SeniorSendoff.uproject.*-game'],stdout=subprocess.DEVNULL).returncode!=0,'A game is already running'
out.mkdir();session=out/'session';session.mkdir()
canonical=ROOT/'Saved/CalderWork/tools/campus_mp.py';guard=hashlib.sha256(canonical.read_bytes()).hexdigest()
flags=['-game','-RenderOffScreen','-MetalOffscreenOnly','-ResX=960','-ResY=540','-NoSplash','-muteaudio','-CalderGoosePondTrial','-CalderGoosePondNetworkReview',f'-CalderWildlifeSessionDir={session}','-CalderWildlifeClockOffset=1035','-port=7789','-ExecCmds=t.MaxFPS 30,r.VSync 0,p.NetShowCorrections 1']
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
jobs=[];result={};started=time.monotonic()
def read(path):return path.read_text(errors='replace') if path.exists() else ''
def launch(role,map_name):
 log=out/(role+'.log');stream=(out/(role+'-stdout.log')).open('w')
 process=subprocess.Popen([EXE,str(ROOT/'SeniorSendoff.uproject'),map_name,*flags,f'-UserDir={out/("user-"+role)}',f'-AbsLog={log}'],stdout=stream,stderr=subprocess.STDOUT)
 jobs.append((role,process,log,stream));return process,log
try:
 host,hostlog=launch('host','/Game/Story/Maps/Lobby?listen')
 while time.monotonic()-started<150:
  if 'listening on port 7789' in read(hostlog):break
  if host.poll() is not None:raise RuntimeError('Host exited before listening')
  time.sleep(.5)
 else:raise RuntimeError('Host never listened on7789')
 launch('client','127.0.0.1:7789');print('POND_MP host listening; client launched',flush=True)
 while time.monotonic()-started<420:
  if all(proc.poll() is not None for _,proc,_,_ in jobs):break
  if (session/'failed').exists() or any('Fatal error:' in read(log) or 'CALDER_POND_MP FAIL' in read(log) for _,_,log,_ in jobs):
   time.sleep(2);break
  time.sleep(.5)
 else:raise RuntimeError('Two-process wildlife test timed out')
 peers=[];terrain={};habitat={}
 for role,proc,log,_ in jobs:
  text=read(log);match=re.search(r'CALDER_TERRAIN hash=([a-f0-9]{8})',text);terrain[role]=match[1] if match else None
  match=re.search(r'CALDER_WILDLIFE_PATHS clear=([0-9]+)/([0-9]+).*hash=([a-f0-9]{8})',text);habitat[role]=match[3] if match and match[1]==match[2] else None
  record=read(session/(role+'-result.txt'))
  peers.append({'role':role,'passed':proc.returncode==0 and record.startswith('PASS') and 'CALDER_POND_MP PASS' in text,'exit_code':proc.returncode,'result':record,'events':[line for line in text.splitlines() if 'CALDER_POND_MP' in line][-16:]})
 same=terrain.get('host') is not None and terrain.get('host')==terrain.get('client') and habitat.get('host') is not None and habitat.get('host')==habitat.get('client')
 result={'passed':all(peer['passed'] for peer in peers) and same,'peers':peers,'terrain_hashes':terrain,'habitat_hashes':habitat,'hashes_match':same,'client_background_checked':read(session/'client-checked'), 'client_csv_samples':sum(1 for _ in (session/'client-samples.csv').open())-1 if (session/'client-samples.csv').exists() else 0}
except Exception as exc:result={'passed':False,'error':str(exc)}
finally:
 for _,proc,_,stream in jobs:
  if proc.poll() is None:
   proc.terminate()
   try:proc.wait(timeout=15)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
  stream.close()
 assert hashlib.sha256(canonical.read_bytes()).hexdigest()==guard,'Original multiplayer harness changed'
 result.update({'seconds':round(time.monotonic()-started,2),'logic_probe_only':False,'native_captures':[str(session/'detail_wild_pond_client.png'),str(session/'detail_wild_pond_host.png')],'original_campus_mp_sha256':guard,'original_hill_route_gate_pending':True,'whole_phase3_complete':False,'scope':'Native background pond roster and live shared-clock arrival/water roots; reactions/audio/full roster/final performance remain separate.'})
 unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items());result['source_config_frozen']=unchanged;result['source_sha256']=freeze;result['passed']=bool(result.get('passed')) and unchanged;result['captures_inspection_pending']=True
 if (session/'failed').exists():result['failure_reason']=read(session/'failed')
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!="source_sha256"},indent=2),flush=True)
raise SystemExit(0 if result.get('passed') else 1)
