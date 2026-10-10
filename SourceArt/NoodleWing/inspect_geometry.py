import bpy,json,math,struct
from pathlib import Path
from mathutils import Vector,Euler
B=Path(__file__).resolve().parent
# Exported meshes carry cm to metre conversion in their FBX transform.
rows=json.load(open(B/'context/saved_actors.json'))
bpy.ops.wm.read_factory_settings(use_empty=True)
polys=[];audit=[]
for a in rows:
 if not a['label'].startswith(('SM_Basement','SM_Entrance','SM_Upper','Retained_','COL_')) and 'ArchitecturalRemainder' not in a['label']:continue
 if a['label'].startswith('COL_'):continue
 for r in a['meshes']:
  if not r.get('fbx'):continue
  before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=r['fbx']);obs=[o for o in set(bpy.data.objects)-before if o.type=='MESH']
  lo=[1e9]*3;hi=[-1e9]*3
  for o in obs:
   vs=[]
   for v in o.data.vertices:
    v=o.matrix_world@v.co;p=[v.x*100*r['scale'][0]+r['location'][0],-v.y*100*r['scale'][1]+r['location'][1],v.z*100*r['scale'][2]+r['location'][2]];vs.append(p)
    lo=[min(l,x)for l,x in zip(lo,p)];hi=[max(h,x)for h,x in zip(hi,p)]
   mats=[m.name for m in o.data.materials]
   for f in o.data.polygons:
    pts=[vs[i]for i in f.vertices]
    polys.append(dict(label=a['label'],p=pts,m=mats[f.material_index]if f.material_index<len(mats)else ''))
   bpy.data.objects.remove(o,do_unlink=True)
  audit.append(dict(label=a['label'],lo=lo,hi=hi,expected=a['bounds']))
json.dump(polys,open(B/'context/polygons.json','w'));json.dump(audit,open(B/'context/geometry_audit.json','w'),indent=2)
print('GEOMETRY_CONTEXT_PASS',len(polys),len(audit))
