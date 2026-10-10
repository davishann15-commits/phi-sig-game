import bpy,json
from pathlib import Path
B=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/StairwellWall')
p=json.load(open(B/'evidence/parts.json'));a=json.load(open(B/'evidence/actors.json'))
for j in [75,81,130,131,132,134,179,180,182]:
 q=p[j];r=next(r for r in a if r['mesh']==q['mesh']);bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);bpy.ops.import_scene.fbx(filepath=r['export']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH')
 print('PART',j)
 d={}
 for i in q['faces']:
  f=o.data.polygons[i];cs=[o.matrix_world@o.data.vertices[k].co for k in f.vertices];ws=[(x.x*180,-x.y*180,x.z*100)for x in cs];mat=o.data.materials[f.material_index].name;lo=[min(x[k] for x in ws)for k in range(3)];hi=[max(x[k]for x in ws)for k in range(3)];key=(mat,tuple(round(x,1)for x in lo),tuple(round(x,1)for x in hi));d[key]=d.get(key,0)+1
 for k,n in d.items():
  if j==182 or (j in [179,180]and n>1):print(n,k)
