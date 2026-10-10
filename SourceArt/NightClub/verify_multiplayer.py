"""Two real game processes, owning client RPCs, synced audio and pole travel."""
import json,pathlib,subprocess,time,uuid
base=pathlib.Path(__file__).resolve().parent
repo=base.parent.parent
evidence=base/'evidence'
exe='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
if subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode==0:
 raise SystemExit('Sequence this LAN check after the other editor/game exits.')
common=[exe,str(repo/'SeniorSendoff.uproject')]
tail=['-game','-ClubPoleMultiplayerSmoke','-ResX=640','-ResY=360','-windowed','-NoSplash']
jobs=[];started=time.monotonic();result={}
run_id=uuid.uuid4().hex[:8]
def launch(name,map_arg):
 log=evidence/f'pole-{name}.log'
 log.unlink(missing_ok=True)
 stream=(evidence/f'pole-{name}-stdout.log').open('w')
 p=subprocess.Popen(common+[map_arg]+tail+[f'-UserDir=/tmp/nightclub-mp-{name}-{run_id}','-port=7781',f'-AbsLog={log}'],stdout=stream,stderr=subprocess.STDOUT)
 jobs.append((name,p,log,stream));return p,log
def read(log):return log.read_text(errors='replace')if log.exists()else''
try:
 # Join through the game's normal lobby, ready both players, and let the native
 # opt-in smoke call StartStory. Mid-story joins are correctly forbidden.
 host,hostlog=launch('host','/Game/Story/Maps/Lobby?listen')
 while time.monotonic()-started<100:
  t=read(hostlog)
  if 'listening on port 7781'in t:break
  if host.poll()is not None:raise RuntimeError('Host exited before its listen socket became ready.')
  time.sleep(.5)
 else:raise RuntimeError('Host listen socket did not become ready.')
 client,clientlog=launch('client','127.0.0.1:7781')
 print('Listen host and separate client running',flush=True)
 while time.monotonic()-started<260:
  if all(p.poll()is not None for _,p,_,_ in jobs):break
  if any('CLUB_POLE_MP_FAIL'in read(log)for _,_,log,_ in jobs):raise RuntimeError('Native multiplayer check reported failure.')
  if any('NetworkFailure: PendingConnectionFailure'in read(log)for _,_,log,_ in jobs):raise RuntimeError('Client connection was rejected.')
  time.sleep(.5)
 else:raise RuntimeError('Multiplayer check exceeded its bounded run.')
 peers=[]
 for name,p,log,_ in jobs:
  t=read(log);ok=p.returncode==0 and'CLUB_POLE_MP_PASS'in t and'CLUB_AMBIENCE_MP_PASS'in t
  peers.append(dict(peer=name,exit_code=p.returncode,status='PASS'if ok else'FAIL',log=str(log)))
 result=dict(status='PASS'if all(x['status']=='PASS'for x in peers)else'FAIL',peers=peers)
except Exception as e:result=dict(status='FAIL',error=str(e))
finally:
 for _,p,_,stream in jobs:
  if p.poll()is None:
   p.terminate()
   try:p.wait(timeout=12)
   except subprocess.TimeoutExpired:p.kill();p.wait()
  stream.close()
 result['seconds']=round(time.monotonic()-started,2)
 (evidence/'multiplayer-verification.json').write_text(json.dumps(result,indent=2))
 print(json.dumps(result,indent=2),flush=True)
raise SystemExit(0 if result['status']=='PASS' else 1)
