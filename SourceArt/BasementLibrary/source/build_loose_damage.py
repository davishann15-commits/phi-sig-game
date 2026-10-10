import json, runpy, hashlib
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BasementLibrary/loose-items')
dm=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/generate_damage_v2.py')
out=R/'models/damage';out.mkdir(exist_ok=True)
g=dm['create_stages'].__globals__;g['OUT']=str(out)
original_hulls=g['hulls_for']
def fitted_hulls(parts,name):
    hulls=original_hulls(parts,name)
    if 'Loose_Paper' in name or 'Loose_LoosePaper' in name:
        for hull in hulls:
            lo,hi=dm['bbox'](hull)
            if hi.z-lo.z<.009:
                center=(lo.z+hi.z)*.5
                for v in hull.data.vertices:v.co.z=center+(v.co.z-center)*.001/(hi.z-lo.z)
    return hulls
g['hulls_for']=fitted_hulls
doc=json.loads((R/'split-manifest.json').read_text());progress=R/'damage-progress.json'
done=json.loads(progress.read_text()) if progress.exists() else {}
for i,a in enumerate(doc['assets']):
    h=hashlib.sha256(Path(a['intact_fbx']).read_bytes()).hexdigest();old=done.get(a['asset_name'])
    if old and old['sha256']==h and all(Path(old['changes'][k+'_fbx']).exists()for k in ['damaged','heavily_damaged','broken']):
        a.update(old['changes']);continue
    change,audit=dm['create_stages'](a);a.update(change)
    assert hashlib.sha256(Path(a['intact_fbx']).read_bytes()).hexdigest()==h
    done[a['asset_name']]=dict(sha256=h,changes=change,audit=audit)
    progress.write_text(json.dumps(done,indent=2));print('LOOSE_DAMAGE',i+1,len(doc['assets']),a['id'],flush=True)
doc['damage_materials']=dm['DEFS'];(R/'split-manifest.json').write_text(json.dumps(doc,indent=2))
print('LIBRARY_LOOSE_DAMAGE_READY',len(doc['assets']),flush=True)
