import bpy,json
from pathlib import Path
B=Path(__file__).resolve().parent
for a in json.load(open(B/'context/saved_actors.json')):
 if a['label'] not in ['OriginalDoor_Basement_m1_p1_Solid_01','OriginalDoor_Upper_m1_p1_Solid_05']:continue
 bpy.ops.wm.read_factory_settings(use_empty=True);r=a['meshes'][0]
 bpy.ops.import_scene.fbx(filepath=str(B/'context/fbx'/Path(r['fbx']).name))
 by={}
 for o in bpy.data.objects:
  if o.type!='MESH':continue
  vs=[]
  for v in o.data.vertices:
   v=o.matrix_world@v.co;vs.append([v.x*100*r['scale'][0],-v.y*100*r['scale'][1],v.z*100*r['scale'][2]])
  for f in o.data.polygons:
   m=o.data.materials[f.material_index].name if len(o.data.materials)>f.material_index else o.name;by.setdefault(m,[]).extend(vs[i]for i in f.vertices)
 print(a['label'],{m:([min(p[i]for p in pts)for i in range(3)],[max(p[i]for p in pts)for i in range(3)])for m,pts in by.items()})
