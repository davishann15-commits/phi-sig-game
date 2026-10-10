import bpy,json
from pathlib import Path
ROOT=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1')
rows={r['label']:r for r in json.load(open(ROOT/'context/actors.json'))}
out={}
for label in ['SM_Upper_m1_m2_Solid','COL_Upper_m1_m2_Solid']:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.fbx(filepath=str(ROOT/'context'/f'{label}.fbx'))
 r=rows[label];hits=[]
 for o in bpy.context.scene.objects:
  if o.type!='MESH':continue
  pts=[o.matrix_world@v.co for v in o.data.vertices]
  pts=[[p.x*100*r['scale'][0],-p.y*100*r['scale'][1],p.z*100*r['scale'][2]] for p in pts]
  for f in o.data.polygons:
   ps=[pts[i] for i in f.vertices];lo=[min(p[i] for p in ps) for i in range(3)];hi=[max(p[i] for p in ps) for i in range(3)]
   if hi[2]-lo[2]<12:continue
   if all(lo[i]<[-2356,-765,560][i] and hi[i]>[-2965,-884,310][i] for i in range(3)):
    hits.append({'bounds':[lo,hi],'material':o.data.materials[f.material_index].name if o.data.materials else '', 'normal':list(f.normal)})
 out[label]={'verticalObstacles':hits,'count':len(hits)}
json.dump(out,open(ROOT/'geometry/expansion-audit.json','w'),indent=2)
print('PIGSTY_EXPANSION_AUDIT', {k:r['count'] for k,r in out.items()},flush=True)
