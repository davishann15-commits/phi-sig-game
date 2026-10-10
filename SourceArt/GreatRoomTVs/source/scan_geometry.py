import bpy,runpy,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs')
s=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/extract_interactivity.py')
results={}
for f in (R/'context/fbx').glob('*.fbx'):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(f));o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
 parts=s['components'](o);a=[]
 for i,p in enumerate(parts):
  if any(x in ' '.join(p['materials']).lower() for x in ['black','blue','screen']) and p['center'].z>1 and p['center'].z<2.6:
   a.append(dict(index=i,faces=len(p['faces']),lo=list(p['lo']),hi=list(p['hi']),size=list(p['size']),materials=p['materials']))
 results[f.stem]=a
(R/'context/tv-candidates.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=1))
