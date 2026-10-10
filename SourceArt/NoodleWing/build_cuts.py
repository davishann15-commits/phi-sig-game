"""Cut only the surveyed shaft and three new passages. Preserve existing UVs and materials."""
import bpy,json,pathlib
from mathutils import Vector
B=pathlib.Path(__file__).resolve().parent;O=B/'art';O.mkdir(exist_ok=True);(O/'models').mkdir(exist_ok=True)
rows=json.load(open(B/'context/saved_actors.json'));manifest=[]
CUTS=[([1468,-667,-49],[2025,627,620]), # shaft, only the unused enclosed volume
 ([2005,-620,-.05],[2110,-448,239]), # main hallway access at the blank wall
 ([2020,-417,332.5],[2090,-241,570]), # upstairs locked landing door
 ([1738,-600,-362.2],[1790,-429,-119]), # remove the solid wall behind basement leaf
 ([1380,474,-362],[1480,605,-126]),
 ([1468,-389,-360],[2025,627,-48])] # basement exit to lower stair landing

def clip(poly,n,d,inside=True):
 out=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  da=sum(n[i]*a[i]for i in range(3))-d;db=sum(n[i]*b[i]for i in range(3))-d;ina=da<=1e-7 if inside else da>=-1e-7;inb=db<=1e-7 if inside else db>=-1e-7
  if ina:out.append(a)
  if ina!=inb:
   t=da/(da-db);out.append(tuple(a[i]+t*(b[i]-a[i])for i in range(len(a))))
 return out

def sub(poly,lo,hi):
 res=[];p=poly
 for axis in range(3):
  for sign,bound in[(1,hi[axis]),(-1,-lo[axis])]:
   n=[0,0,0];n[axis]=sign
   outside=clip(p,n,bound,False)
   if len(outside)>=3:res.append(outside)
   p=clip(p,n,bound,True)
   if len(p)<3:return res
 return res

def intersect(p,lo,hi):return all(max(v[i]for v in p)>lo[i]+1e-5 and min(v[i]for v in p)<hi[i]-1e-5 for i in range(3))
for a in rows:
 if a['className']!='StaticMeshActor':continue
 if not a['label'].startswith(('SM_Entrance','SM_Upper','SM_Basement','COL_Entrance','COL_Upper','COL_Basement')):continue
 r=a['meshes'][0]if a['meshes']else{}
 if not r.get('fbx') or not any(all(a['bounds'][1][i]>lo[i] and a['bounds'][0][i]<hi[i]for i in range(3))for lo,hi in CUTS):continue
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(B/'context/fbx'/pathlib.Path(r['fbx']).name));o=next(x for x in bpy.context.scene.objects if x.type=='MESH' and not x.name.startswith('UCX'));old=o.data;s=r['scale'];loc=r['location'];uv=old.uv_layers.active;verts=[];uvs=[];faces=[];slots=[];smooth=[];changed=0
 assert max(abs(v)for v in r['rotation'])<.01,(a['label'],r['rotation'])
 for f in old.polygons:
  poly=[]
  for li in f.loop_indices:
   p=o.matrix_world@old.vertices[old.loops[li].vertex_index].co;u=uv.data[li].uv if uv else(0,0);poly.append((p.x*s[0]*100+loc[0],-p.y*s[1]*100+loc[1],p.z*s[2]*100+loc[2],u[0],u[1]))
  pieces=[poly]
  for lo,hi in CUTS:
   new=[]
   for p in pieces:
    if intersect(p,lo,hi):new+=sub(p,lo,hi);changed+=1
    else:new.append(p)
   pieces=new
  for p in pieces:
   if len(p)<3:continue
   faces.append(list(range(len(verts),len(verts)+len(p))));verts.extend([((q[0]-loc[0])/s[0]/100,-(q[1]-loc[1])/s[1]/100,(q[2]-loc[2])/s[2]/100)for q in p]);uvs.extend([q[3:5]for q in p]);slots.append(f.material_index);smooth.append(f.use_smooth)
 if not changed:continue
 # Prefix must remain recognizable to the existing black-hall runtime replacement.
 name=r['asset'].split('/')[-1].split('.')[0]+'_NoodleCut';m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
 for ma in old.materials:m.materials.append(ma)
 for f,mi,sm in zip(m.polygons,slots,smooth):f.material_index=mi;f.use_smooth=sm
 uv=m.uv_layers.new(name='UVMap')
 for f in m.polygons:
  for li in f.loop_indices:uv.data[li].uv=uvs[m.loops[li].vertex_index]
 o.data=m;o.matrix_world.identity();o.name=name;bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 path=O/'models'/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False)
 manifest.append(dict(name=name,fbx=str(path),sourceActor=a['label'],old_asset=r['asset'],materials=r['materials'],cut_faces=changed,faces_before=len(old.polygons),faces_after=len(m.polygons)))
 print('NOODLE_CUT',a['label'],changed,flush=True)
json.dump(dict(cuts=CUTS,assets=manifest),open(O/'cuts.json','w'),indent=2);print('NOODLE_CUT_PASS',len(manifest),flush=True)
