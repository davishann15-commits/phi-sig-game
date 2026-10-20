"""Deer-only two-process replication test through the game's real lobby.

--logic is explicitly nonrendered and cannot accept appearance or visible timing.
Native mode requires a client's actual updated ISM and a GPU-completed view
frame within0.5s of the authority's transition. A later screenshot proves
appearance separately; PNG encoding/readback is excluded from visibility time. Original hill-route gate is separate.
"""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
EXE='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('--logic',action='store_true');p.add_argument('--scene-time',type=float,default=300.);p.add_argument('--normal',action='store_true');args=p.parse_args()
assert re.fullmatch('[a-z0-9_]+',args.name)
out=ROOT/'Saved/CalderWork/runs'/args.name;assert not out.exists(),out
assert subprocess.run(['pgrep','-f','UnrealEditor.*SeniorSendoff.uproject.*-game'],stdout=subprocess.DEVNULL).returncode!=0,'A game is already running'
out.mkdir();session=out/'session';session.mkdir()
canonical=ROOT/'Saved/CalderWork/tools/campus_mp.py';guard=hashlib.sha256(canonical.read_bytes()).hexdigest()
flags=['-game','-RenderOffScreen','-MetalOffscreenOnly','-ResX=960','-ResY=540','-NoSplash','-muteaudio','-CalderWildlifeTrial','-CalderWildlifeNetworkReview',f'-CalderWildlifeSessionDir={session}',f'-CalderSceneTime={args.scene_time}','-port=7787','-ExecCmds=t.MaxFPS 30,r.VSync 0,p.NetShowCorrections 1']
freeze={str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for folder in ('Source','Config') for q in (ROOT/folder).rglob('*') if q.is_file()}
if args.logic:flags+=['-nullrhi','-CalderWildlifeLogicProbe','-nosound']
if args.normal:flags.remove('-CalderWildlifeTrial')
jobs=[];result={};started=time.monotonic()
def read(path):return path.read_text(errors='replace') if path.exists() else ''
def launch(role,map_name):
 log=out/(role+'.log');stream=(out/(role+'-stdout.log')).open('w')
 process=subprocess.Popen([EXE,str(ROOT/'SeniorSendoff.uproject'),map_name,*flags,f'-UserDir={out/("user-"+role)}',f'-AbsLog={log}'],stdout=stream,stderr=subprocess.STDOUT)
 jobs.append((role,process,log,stream));return process,log
try:
 host,hostlog=launch('host','/Game/Story/Maps/Lobby?listen')
 while time.monotonic()-started<150:
  if 'listening on port 7787' in read(hostlog):break
  if host.poll() is not None:raise RuntimeError('Host exited before listening')
  time.sleep(.5)
 else:raise RuntimeError('Host never listened on7787')
 launch('client','127.0.0.1:7787');print('WILDLIFE_MP host listening; client launched',flush=True)
 while time.monotonic()-started<420:
  if all(proc.poll() is not None for _,proc,_,_ in jobs):break
  if (session/'failed').exists() or any('Fatal error:' in read(log) or 'CALDER_WILDLIFE_MP_FAIL' in read(log) for _,_,log,_ in jobs):
   time.sleep(2);break
  time.sleep(.5)
 else:raise RuntimeError('Two-process wildlife test timed out')
 peers=[];terrain={};habitat={}
 for role,proc,log,_ in jobs:
  text=read(log);match=re.search(r'CALDER_TERRAIN hash=([a-f0-9]{8})',text);terrain[role]=match[1] if match else None
  match=re.search(r'CALDER_WILDLIFE_PATHS clear=([0-9]+)/([0-9]+).*hash=([a-f0-9]{8})',text);habitat[role]=match[3] if match and match[1]==match[2] else None
  record=read(session/(role+'-result.txt'))
  peers.append({'role':role,'passed':proc.returncode==0 and record.startswith('PASS') and 'CALDER_WILDLIFE_MP_PASS' in text,'exit_code':proc.returncode,'result':record,'events':[line for line in text.splitlines() if 'CALDER_WILDLIFE_MP' in line][-16:]})
 same=terrain.get('host') is not None and terrain.get('host')==terrain.get('client') and habitat.get('host') is not None and habitat.get('host')==habitat.get('client')
 result={'passed':all(peer['passed'] for peer in peers) and same,'peers':peers,'terrain_hashes':terrain,'habitat_hashes':habitat,'hashes_match':same,'client_ack':read(session/'client-flee-ack')}
except Exception as exc:result={'passed':False,'error':str(exc)}
finally:
 for _,proc,_,stream in jobs:
  if proc.poll() is None:
   proc.terminate()
   try:proc.wait(timeout=15)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
  stream.close()
 assert hashlib.sha256(canonical.read_bytes()).hexdigest()==guard,'Original multiplayer harness changed'
 result.update({'seconds':round(time.monotonic()-started,2),'logic_probe_only':args.logic,'native_capture':str(session/'detail_wild_deer_client_flee.png') if not args.logic else None,'original_campus_mp_sha256':guard,'original_hill_route_gate_pending':True,'whole_phase3_complete':False,'scope':'Deer replication first slice. Native appearance/feet/sound/full roster and final performance remain separate requirements.'})
 if (session/'failed').exists():result['failure_reason']=read(session/'failed')
 unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items());result['source_config_frozen']=unchanged;result['source_sha256']=freeze;result['normal_defaults']=args.normal;result['passed']=bool(result.get('passed')) and unchanged
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
raise SystemExit(0 if result.get('passed') else 1)
