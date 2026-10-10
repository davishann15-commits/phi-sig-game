import bpy,json,re,collections,numpy as np
from pathlib import Path
B=Path(__file__).resolve().parent
rows=json.load(open(B/'shell_plan.json'));parts=json.load(open(B/'context/parts.json'));exports=json.load(open(B/'context/exports.json'))
allowed={'SM_Entrance_m1_m2_Solid':[264],'SM_Entrance_p0_m2_Solid':[934,948,949,950,946],'SM_Upper_m1_m2_Solid':[1732],'SM_Upper_p0_m2_Solid':[2312,2313,2349],'SM_Upper_m1_m1_Solid':[1667,1668],'SM_Upper_m1_p0_Solid':[1918]}
report=[]
def read(file):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=file);o=next(x for x in bpy.context.scene.objects if x.type=='MESH' and not x.name.startswith('UCX'));m=o.data
 result=[]
 for f in m.polygons:
  pts=np.array([o.matrix_world@m.vertices[i].co for i in f.vertices]);name=re.sub(r'[._]\d+$','',m.materials[f.material_index].name);result.append((pts,name))
 return result
for row in rows:
 original=read(exports[row['old_mesh']]);new=read(row['file']);excluded=set(fi for pid in allowed[row['label']]for fi in parts[pid]['faces']);checked=0
 for i,(pts,mat) in enumerate(original):
  if i in excluded or not any(n in mat for n in ['FloorWood','GalleryWoodFloor']):continue
  center=pts.mean(axis=0)
  candidates=[pp for pp,mm in new if mm==mat and len(pp)==len(pts)and np.max(np.abs(pp.mean(axis=0)-center))<.0002]
  matches=[pp for pp in candidates if np.max(np.min(np.sum((pts[:,None,:]-pp[None,:,:])**2,axis=2),axis=1))<.0002**2]
  assert matches,(row['label'],i,mat,pts.tolist());checked+=1
 report.append(dict(label=row['label'],protected_wood_floor_faces_verified=checked))
json.dump(dict(status='PASS',coordinate_tolerance_cm=.02,meshes=report),open(B/'evidence/protected_geometry.json','w'),indent=2)
print('HFP_PROTECTED_WOOD_GEOMETRY_PASS',sum(x['protected_wood_floor_faces_verified']for x in report))
