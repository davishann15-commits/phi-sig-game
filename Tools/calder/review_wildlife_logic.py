"""Nonrendered wildlife diagnostics; never substitutes for native visual/MP/audio gates.

Sequential unique runs through the existing game launcher. Source hashes are
frozen across each batch; originals and failed evidence are never overwritten.
"""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('name');parser.add_argument('--cases',nargs='+',choices=['walk','sprint','crouch','campus','paths','farm_paths','off','terrain_off'],default=['walk','sprint','crouch','campus','paths','farm_paths','off','terrain_off']);args=parser.parse_args()
assert re.fullmatch(r'[a-z0-9_]+',args.name),args.name
out=ROOT/'Saved/CalderWork'/args.name;assert not out.exists(),out;out.mkdir()
files=list((ROOT/'Source/SeniorSendoff').glob('HouseCalderWildlife*'))+[ROOT/'Source/SeniorSendoff/HouseCalderDeerData.inl']
source={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()}
rows=[];started=time.monotonic()
for case in args.cases:
 for p,digest in source.items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest,'Source changed during review batch: '+p
 name=args.name+'_'+case;run=ROOT/'Saved/CalderWork/runs'/name;assert not run.exists(),run
 flags=['-RenderOffScreen','-nosound','-nullrhi','-CalderSceneTime=300']
 if case in ['walk','sprint','crouch','campus']:
  flags+=['-CalderWildlifeReview','-CalderWildlifeLogicProbe','-CalderWildlifeReviewMode='+case];kind='CalderWildlifeReview';timeout=300
 else:
  flags+=['-server','-CalderWildlifePathsReview'];kind='CalderWildlifePathsReview';timeout=180
  if case=='farm_paths':flags+=['-CalderFarmTrial','-CalderFarmSolidTrial','-CalderFarmTreeTrial']
  if case=='off':flags+=['-NoCalderWildlife']
  if case=='terrain_off':flags+=['-NoCalderTerrain']
 print('WILDLIFE_LOGIC start '+case,flush=True)
 subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),name,str(timeout),*flags],cwd=ROOT,check=True)
 result=run/'Saved'/kind/'result.txt';text=result.read_text() if result.exists() else 'FAIL missing result'
 passed=bool(re.search(r'^PASS\b',text,re.M)) and not re.search(r'^FAIL\b',text,re.M)
 rows.append({'case':case,'passed':passed,'result':str(result.relative_to(ROOT)),'text':text})
 receipt={'passed':all(r['passed'] for r in rows),'scope':'Nonrendered native diagnostics only. Assets/render/feet/audio/MP/final performance remain required.','source_sha256':source,'cases':rows,'seconds':round(time.monotonic()-started,2)}
 target=out/'receipt.json';target.unlink(missing_ok=True);target.write_text(json.dumps(receipt,indent=2)+'\n')
 print('WILDLIFE_LOGIC '+('PASS ' if passed else 'FAIL ')+case,flush=True)
 if not passed:raise SystemExit(1)
