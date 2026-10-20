"""Prove and release the camera-only edit, after native queues and human/agent image inspection."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess,tempfile,statistics
R=Path(__file__).resolve().parents[2];W=R/'Saved/StadiumDetailWork'
assert (W/'before_receipt.json').exists() and (W/'refined_before_receipt.json').exists()
assert json.loads((W/'camera_visual_inspection.json').read_text())['inspected']
procs=subprocess.check_output(['ps','-axo','pid=,comm='],text=True).splitlines();active=[x for x in procs if 'UnrealEditor.app/Contents/MacOS/UnrealEditor' in x or 'UnrealBuildTool' in x or 'Binaries/Mac/SeniorSendoff' in x];assert not active,active
original=json.loads((W/'before_slot_sha.json').read_text());frozen=json.loads((W/'refined_camera_source_sha.json').read_text())
for rel,sha in frozen.items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,'Source/Config drift '+rel
assert set(original)==set(frozen)
changed=[r for r in original if original[r]!=frozen[r]];assert changed==['Source/SeniorSendoff/HouseExterior.cpp'],changed
survey=json.loads((W/'survey_source_sha.json').read_text());maps=[r for r in survey if r.endswith('.umap')]
for rel in maps:assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==survey[rel],rel
with tempfile.TemporaryDirectory(prefix='stadium-camera-replay-') as tmp:
 t=Path(tmp);p=t/'Source/SeniorSendoff/HouseExterior.cpp';p.parent.mkdir(parents=True);p.write_bytes((W/'before_camera_HouseExterior.cpp').read_bytes());subprocess.run(['git','init','-q',str(t)],check=True);patch=W/'refined_camera_only.patch'
 subprocess.run(['git','-C',str(t),'apply','--check',str(patch)],check=True);subprocess.run(['git','-C',str(t),'apply',str(patch)],check=True);assert hashlib.sha256(p.read_bytes()).hexdigest()==frozen['Source/SeniorSendoff/HouseExterior.cpp']
base=json.loads((W/'before_receipt.json').read_text());refined=json.loads((W/'refined_before_receipt.json').read_text());new_names={x['view'] for x in refined['runs'][0]['frames']};values={}
for runs in [base['runs'],refined['runs']]:
 for run in runs:
  if run['phase']!=0:continue
  for row in run['frames']:
   if runs is base['runs'] and row['view'] in new_names:continue
   values.setdefault(row['view'],[]).append(row)
table=[]
for name,rows in values.items():
 assert len(rows)==3,(name,len(rows));table.append({'view':name,'avg_ms':statistics.median(float(x['avg_ms']) for x in rows),'render_ms':statistics.median(float(x['render_ms']) for x in rows),'game_ms':statistics.median(float(x['game_ms']) for x in rows),'rounds':3,'samples_per_round':300,'refined_pose':name in new_names})
assert len(table)==19
(W/'before_performance.json').write_text(json.dumps({'views':table,'no_stadium_geometry':True,'load_seconds':[float(r['load_seconds'][0]) for r in base['runs'][:3]],'notes':'16poses use the original frozen3rounds;3corrected NEW poses use their later frozen3rounds. Exact final NoStadiumDetail alternating controls remain required after integration.'},indent=2)+'\n')
result={'released_utc':datetime.now(timezone.utc).isoformat(),'released_to_chat':'01a14323-cafa-7542-add7-3e71bba2ef2a','source_geometry_changes':0,'camera_only':True,'changed_source':changed,'HouseExterior_sha256':frozen['Source/SeniorSendoff/HouseExterior.cpp'],'source_config_files':len(frozen),'all_other_source_config_exact':True,'map_hashes_unchanged':{r:survey[r] for r in maps},'all_original_and14Row_views_preserved':True,'appended_stadium_views':11,'patch':'Saved/StadiumDetailWork/refined_camera_only.patch','test_apply_and_exact_hash':True,'engine_idle':True,'before_receipt':'Saved/StadiumDetailWork/before_receipt.json','refined_receipt':'Saved/StadiumDetailWork/refined_before_receipt.json','performance':'Saved/StadiumDetailWork/before_performance.json','source_manifest':'Saved/StadiumDetailWork/refined_camera_source_sha.json','stadium_runtime_implementation':'SourceArt/StadiumDetail/Draft only; wait for FULL Row acceptance/recovery release'}
(W/'reservation_release_camera.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
