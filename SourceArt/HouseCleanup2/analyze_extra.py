import bpy,json,collections
from pathlib import Path
from mathutils import Vector,Quaternion
B=Path(__file__).resolve().parent;rows=json.load(open(B/'context/actors.json'));parts=[]
for a in rows:
 if a['label']!='SM_Upper_p0_p1_Solid':continue
 for c in a['meshes']:
  if not c.get('fbx') or not c['visible']:continue
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=c['fbx'])
  oo=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_'))]
  assert len(oo)==1,(a['label'],len(oo));o=oo[0];m=o.data;sc=Vector(c['scale']);loc=Vector(c['loc']);q=c['quat'];rot=Quaternion((q[3],q[0],q[1],q[2]))
  vv=[]
  for v in m.vertices:
   p=o.matrix_world@v.co;vv.append(rot@(Vector((p.x*100,-p.y*100,p.z*100))*sc)+loc)
  parent=list(range(len(vv)))
  def find(i):
   while i!=parent[i]:parent[i]=parent[parent[i]];i=parent[i]
   return i
  for f in m.polygons:
   for v in f.vertices[1:]:parent[find(v)]=find(f.vertices[0])
  groups=collections.defaultdict(list)
  for f in m.polygons:groups[find(f.vertices[0])].append(f.index)
  for fs in groups.values():
   ids={v for fi in fs for v in m.polygons[fi].vertices};pp=[vv[i] for i in ids];lo=[round(min(p[k]for p in pp),3)for k in range(3)];hi=[round(max(p[k]for p in pp),3)for k in range(3)];mats=sorted({m.materials[m.polygons[f].material_index].name for f in fs})
   parts.append(dict(actor=a['label'],mesh=c['mesh'],lo=lo,hi=hi,mats=mats,faces=fs))
(B/'context/extra_parts.json').write_text(json.dumps(parts,indent=2));print('JOIN_PARTS',len(parts))
