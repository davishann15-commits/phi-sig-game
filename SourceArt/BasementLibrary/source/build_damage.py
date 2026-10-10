import json,runpy,hashlib
from pathlib import Path
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
dm=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/generate_damage_v2.py')
out=R/'models/damage';out.mkdir(exist_ok=True)
dm['create_stages'].__globals__['OUT']=str(out)
doc=json.loads((R/'manifest.json').read_text());progress=R/'damage-progress.json'
done=json.loads(progress.read_text()) if progress.exists() else {}
assets=[a for a in doc['assets'] if a['damage']]
for i,a in enumerate(assets):
 h=hashlib.sha256(Path(a['intact_fbx']).read_bytes()).hexdigest()
 old=done.get(a['asset_name'])
 if old and old['sha256']==h and all(Path(old['changes'][k+'_fbx']).exists()for k in ['damaged','heavily_damaged','broken']):
  a.update(old['changes']);continue
 change,audit=dm['create_stages'](a);a.update(change)
 if hashlib.sha256(Path(a['intact_fbx']).read_bytes()).hexdigest()!=h:raise RuntimeError('Intact mesh changed')
 done[a['asset_name']]=dict(sha256=h,changes=change,audit=audit)
 progress.write_text(json.dumps(done,indent=2));print('LIBRARY_DAMAGE',i+1,len(assets),a['asset_name'],flush=True)
doc['damage_materials']=dm['DEFS'];(R/'manifest.json').write_text(json.dumps(doc,indent=2));print('LIBRARY_DAMAGE_READY',len(assets),flush=True)
