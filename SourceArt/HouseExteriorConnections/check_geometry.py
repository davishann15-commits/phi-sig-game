import bpy,json,runpy
from pathlib import Path
from mathutils import Vector
B=Path(__file__).resolve().parent;rows=json.load(open(B/'collision-manifest.json'));out=[]
def inspect(path):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(path));o=next(o for o in bpy.context.scene.objects if o.type=='MESH')
 vv=[o.matrix_world@v.co for v in o.data.vertices]
 return dict(lo=[min(v[i] for v in vv) for i in range(3)],hi=[max(v[i] for v in vv) for i in range(3)],faces=len(o.data.polygons))
for r in rows:
 old=inspect(B/'context/fbx'/(r['source_mesh'].split('/')[-1].split('.')[0]+'.fbx'));new=inspect(r['fbx']);delta=max(abs(old[k][i]-new[k][i]) for k in ['lo','hi'] for i in range(3))
 out.append(dict(actor=r['actor'],bounds_delta_m=delta,before=old,after=new))
(B/'geometry-check.json').write_text(json.dumps(out,indent=2));print([(r['actor'],r['bounds_delta_m']) for r in out])
