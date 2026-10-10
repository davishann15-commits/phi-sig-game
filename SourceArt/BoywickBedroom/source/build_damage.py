import json,runpy,hashlib
from pathlib import Path
P=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff');R=P/'SourceArt/BoywickBedroom'
dm=runpy.run_path(str(P/'SourceArt/HouseInteractivity/source/generate_damage_v2.py'))
out=R/'models/damage';out.mkdir(exist_ok=True);dm['create_stages'].__globals__['OUT']=str(out)
doc=json.loads((R/'manifest.json').read_text());progress=R/'damage-progress.json';done=json.loads(progress.read_text())if progress.exists()else{}
assets=[a for a in doc['assets']if a['damage']]
for i,a in enumerate(assets):
 h=hashlib.sha256(Path(a['intact_fbx']).read_bytes()).hexdigest();old=done.get(a['asset_name'])
 if old and old['sha256']==h:
  a.update(old['changes']);continue
 change,audit=dm['create_stages'](a);a.update(change);done[a['asset_name']]=dict(sha256=h,changes=change,audit=audit);progress.write_text(json.dumps(done,indent=2));print('BWR_DAMAGE',i+1,len(assets),a['asset_name'],flush=True)
doc['damage_materials']=dm['DEFS'];(R/'manifest.json').write_text(json.dumps(doc,indent=2));print('BWR_DAMAGE_READY',len(assets),flush=True)
