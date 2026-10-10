import bpy,json,collections
from pathlib import Path
B=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/StairwellWall')
rows=json.load(open(B/'evidence/actors.json'));parts=[]
for r in rows:
 if not r.get('export') or not r['visible']:continue
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.fbx(filepath=r['export'])
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH');m=o.data
 # Export is in metres in the asset's source frame; actor scales convert to cm.
 pos=[o.matrix_world@v.co for v in m.vertices]
 world=[(p.x*100*r['scale'][0]+r['position'][0],-p.y*100*r['scale'][1]+r['position'][1],p.z*100*r['scale'][2]+r['position'][2]) for p in pos]
 parent=list(range(len(m.vertices)))
 def find(i):
  while i!=parent[i]:parent[i]=parent[parent[i]];i=parent[i]
  return i
 for f in m.polygons:
  for v in f.vertices[1:]:parent[find(v)]=find(f.vertices[0])
 g=collections.defaultdict(list)
 for f in m.polygons:g[find(f.vertices[0])].append(f.index)
 for fs in g.values():
  cs=[world[i] for fi in fs for i in m.polygons[fi].vertices];lo=[min(c[k] for c in cs) for k in range(3)];hi=[max(c[k] for c in cs) for k in range(3)]
  if lo[0]>-1250 or hi[0]<-2350 or lo[1]>1320 or hi[1]<800:continue
  mats=sorted({m.materials[m.polygons[i].material_index].name for i in fs})
  parts.append(dict(mesh=r['mesh'],faces=fs,lo=lo,hi=hi,materials=mats))
json.dump(parts,open(B/'evidence/parts.json','w'),indent=2)
print('PARTS',len(parts))
for i,p in enumerate(parts):
 if any(x in ' '.join(p['materials']) for x in ['WallGrey','Masonry','MetalDark','HallGrey','HallSkirting','PaintGrey','PaintWhite']):print(i,len(p['faces']),p['materials'],[round(v,1) for v in p['lo']],[round(v,1) for v in p['hi']])
