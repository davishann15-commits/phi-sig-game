import bpy,json,collections
from pathlib import Path
B=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HallFloorsPorch');rows=json.load(open(B/'context/actors.json'));exports=json.load(open(B/'context/exports.json'));out=[]
for a in rows:
 if not a['label'].startswith(('SM_Entrance','SM_Upper')):continue
 for c in a['components']:
  if c['mesh'] not in exports:continue
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=exports[c['mesh']]);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX'));m=o.data
  def wp(i):
   v=o.matrix_world@m.vertices[i].co;return (v.x*a['scale'][0]*100,-v.y*a['scale'][1]*100,v.z*a['scale'][2]*100)
  p=list(range(len(m.vertices)))
  def find(i):
   while p[i]!=i:p[i]=p[p[i]];i=p[i]
   return i
  for f in m.polygons:
   for i in f.vertices[1:]:p[find(i)]=find(f.vertices[0])
  groups=collections.defaultdict(list)
  for f in m.polygons:groups[find(f.vertices[0])].append(f)
  for fs in groups.values():
   ps=[wp(i)for i in {i for f in fs for i in f.vertices}];lo=[min(p[k]for p in ps)for k in range(3)];hi=[max(p[k]for p in ps)for k in range(3)]
   mats=sorted({m.materials[f.material_index].name for f in fs})
   out.append(dict(label=a['label'],mesh=c['mesh'],lo=lo,hi=hi,materials=mats,faces=[f.index for f in fs]))
json.dump(out,open(B/'context/parts.json','w'),indent=2);print('PARTS',len(out))
for i,r in enumerate(out):
 lo=r['lo'];hi=r['hi'];mats=r['materials']
 if (any(k in n for n in mats for k in ['Floor','Concrete','Stair','Gallery']) and hi[2]-lo[2]<30 and hi[0]-lo[0]>80) or (hi[2]>300 and lo[2]<570 and lo[1]>1180 and hi[0]>-2350 and lo[0]<-2080) or ('Upper' in r['label'] and hi[1]<-825 and any(k in n for n in mats for k in ['Leather','Wood','Fabric'])):
  print(i,r['label'],mats,[round(x,1)for x in lo],[round(x,1)for x in hi],len(r['faces']))
