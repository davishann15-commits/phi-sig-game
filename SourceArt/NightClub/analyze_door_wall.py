import bpy,json,pathlib,numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
B=pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/NightClub');a=json.load(open(B/'context/actors.json'))
results={}
for label in ['SM_Entrance_m1_m2_Solid','COL_Entrance_m1_m2_Solid','SM_Upper_m1_m2_Solid','COL_Upper_m1_m2_Solid']:
 row=next(r for r in a if r['label']==label);bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(B/'context/fbx'/(label+'.fbx')))
 o=next(x for x in bpy.context.scene.objects if x.type=='MESH' and not x.name.startswith('UCX'));s=row['scale'];pts=[Vector((v.x*s[0]*100,-v.y*s[1]*100,v.z*s[2]*100)) for v in[o.matrix_world@v.co for v in o.data.vertices]]
 mesh=o.data;tree=BVHTree.FromPolygons(pts,[list(f.vertices)for f in mesh.polygons]);hits={}
 for xy in [(-2400,-1386),(-2300,-1386),(-2250,-1375.2)]:
  start=Vector((*xy,110));lst=[]
  for i in range(10):
   hit=tree.ray_cast(start,Vector((1,0,0)),1000)
   if hit[0] is None:break
   lst.append([round(x,3)for x in hit[0]]);start=hit[0]+Vector((.01,0,0))
  hits[str(xy)]=lst
 results[label]=dict(bounds=[[min(p[i]for p in pts)for i in range(3)],[max(p[i]for p in pts)for i in range(3)]],vertical_hits=hits,materials=[m.name for m in mesh.materials])
print(json.dumps(results,indent=2));json.dump(results,open(B/'context/door-wall-analysis.json','w'),indent=2)
