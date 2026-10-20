import bpy,json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
B=Path(__file__).resolve().parent
rows=json.load(open(B/'collision-manifest.json'))
actors={a['label']:a for a in json.load(open(B/'context/actors.json'))}
for row in rows:
 if row['actor']!='SM_Upper_p0_m1_Solid':continue
 c=actors[row['actor']]['components'][0]
 print('CUTS',row['cuts'])
 for p in [B/'context/fbx'/(row['source_mesh'].split('/')[-1].split('.')[0]+'.fbx'),Path(row['fbx'])]:
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(p))
  o=next(o for o in bpy.context.scene.objects if o.type=='MESH')
  points=[o.matrix_world@v.co for v in o.data.vertices]
  tree=BVHTree.FromPolygons(points,[list(f.vertices) for f in o.data.polygons])
  for x in [-356.4,-301.775,-411.025]:
   at=Vector((x/c['scale'][0]/100,-790/c['scale'][1]/100,526/c['scale'][2]/100))
   print(p.name,x,tree.ray_cast(at,Vector((0,-1,0)),3))
