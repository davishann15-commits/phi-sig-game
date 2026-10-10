import bpy,runpy,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs')
s=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/extract_interactivity.py')
rows=[]
for label in ['SM_Entrance_p0_m1_Solid','SM_Entrance_p0_p0_Solid']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.fbx(filepath=str(R/'context/fbx'/('Actual_'+label+'.fbx')))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
 parts=s['components'](o)
 near=[p for p in parts if 1.7<p['center'].z<3.1 and abs(abs(p['center'].x)-6.5)<.5 and p['size'].z>.5 and p['size'].y>1.2 and p['size'].x<.50]
 rows.append(dict(label=label,polygons=len(o.data.polygons),near_tv_components=[dict(lo=list(p['lo']),hi=list(p['hi']),materials=p['materials'],faces=len(p['faces'])) for p in near]))
(R/'actual-shell-audit.json').write_text(json.dumps(rows,indent=2))
print(json.dumps(rows))
