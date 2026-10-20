import bpy,json,runpy
from pathlib import Path
from mathutils import Vector,Quaternion
B=Path(__file__).resolve().parent
s=runpy.run_path(str(B.parent/'HouseInteractivity/source/extract_interactivity.py'))
actors=json.load(open(B/'context/actors.json'));rows=[]
for a in actors:
 if not(a['label'].startswith(('SM_','COL_')) and 'Solid' in a['label']):continue
 for c in a['components']:
  if c['name']!='StaticMeshComponent0':continue
  path=B/'context/fbx'/(c['mesh'].split('/')[-1].split('.')[0]+'.fbx')
  if not path.exists():continue
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(path))
  o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_')))
  sc=Vector(c['scale']);loc=Vector(c['loc']);q=c['quat'];rot=Quaternion((q[3],q[0],q[1],q[2]))
  def world(v):return rot@(Vector((v.x*100,-v.y*100,v.z*100))*sc)+loc
  for p in s['components'](o):
   lo,hi=p['lo'],p['hi'];points=[world(Vector((x,y,z))) for x in [lo.x,hi.x] for y in [lo.y,hi.y] for z in [lo.z,hi.z]]
   low=[round(min(v[k] for v in points),3) for k in range(3)];high=[round(max(v[k] for v in points),3) for k in range(3)]
   size=[high[i]-low[i] for i in range(3)]
   if size[2]<65 or min(size[0],size[1])>100:continue
   rows.append(dict(actor=a['label'],mesh=c['mesh'],lo=low,hi=high,size=size,mats=p['materials'],faces=len(p['faces'])))
(B/'context/wall-parts.json').write_text(json.dumps(rows,indent=2));print('WALL_PARTS',len(rows))
