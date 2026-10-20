"""Offline acquired crow conversion; native shader/art acceptance is separate.
Offline acquired Fab crow -> weighted static LODs; no runtime skeleton.
Run only after the deer-native checkpoint is verified and no timing job runs.
"""
import hashlib,json,math
from pathlib import Path
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'SourceArt/CalderLandscape/phase2/crow.fbx'
OUT=ROOT/'SourceArt/CalderLandscape/phase3/crow'
assert not OUT.exists(),'Never overwrite a prepared or verified conversion'
OUT.mkdir(parents=True)
source_sha=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(SOURCE))
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
original=next(o for o in bpy.context.scene.objects if o.type=='MESH')
def cm(v):return Vector((-v.y*100,v.x*100,v.z*100))
def bone(name):return cm(rig.matrix_world@rig.data.bones[name].head_local)
# 0body,1neck,2head,3..5leftwing,6..8rightwing,9tail,
# 10/11leftleg,12/13rightleg,14jaw,15spare; common16-control shader.
keys=['CROW_ Pelvis','CROW_ Neck','CROW_ Head','CROW_ L UpperArm','CROW_ L Forearm','CROW_ L Hand','CROW_ R UpperArm','CROW_ R Forearm','CROW_ R Hand',None,'CROW_ L Thigh','CROW_ L Calf','CROW_ R Thigh','CROW_ R Calf','CROW_ Queue de cheval 1','CROW_ Pelvis']
tails=[bone(n) for n in rig.data.bones.keys() if n.startswith('CrowTail')];assert tails
pivots=[bone(n) if n else sum(tails,Vector())/len(tails) for n in keys]
mapping={'CROW_ Neck':1,'CROW_ Head':2,'CROW_ Queue de cheval 1':14}
for side,start,leg in [('L',3,10),('R',6,12)]:
 for suffix,index in [('Clavicle',start),('UpperArm',start),('Forearm',start+1),('Hand',start+2),('Thigh',leg),('Calf',leg+1),('HorseLink',leg+1),('Foot',leg+1)]:mapping['CROW_ '+side+' '+suffix]=index
# Follow the actual donor hierarchy for feather and toe influences.
def control(name):
 if name.startswith('CrowTail'):return 9
 seen=set()
 while name and name not in seen:
  if name in mapping:return mapping[name]
  seen.add(name);b=rig.data.bones.get(name);name=b.parent.name if b and b.parent else None
 return 0
rows=[]
for lod,ratio in enumerate((1.,.65,.32)):
 obj=original.copy();obj.data=original.data.copy();bpy.context.collection.objects.link(obj)
 # Remove runtime deformers; preserve bind-pose geometry and vertex groups.
 for mod in list(obj.modifiers):obj.modifiers.remove(mod)
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 if lod:
  dec=obj.modifiers.new('StaticLOD','DECIMATE');dec.ratio=ratio;dec.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=dec.name)
 src=obj.data;groups={g.index:g.name for g in obj.vertex_groups};points=[cm(obj.matrix_world@v.co) for v in src.vertices];weights=[]
 for v in src.vertices:
  totals={}
  for g in v.groups:
   k=control(groups[g.group]);totals[k]=totals.get(k,0.)+g.weight
  row=sorted(totals.items(),key=lambda x:-x[1])[:4];total=sum(w for _,w in row)
  assert total>0 and math.isfinite(total)
  row=[(k,w/total) for k,w in row]+[(0,0.)]*(4-len(row));assert abs(sum(w for _,w in row)-1.)<1e-6;weights.append(row)
 mesh=bpy.data.meshes.new('CalderCrowLOD%d'%lod);mesh.from_pydata([(v.x/100,-v.y/100,v.z/100) for v in points],[],[list(p.vertices) for p in src.polygons]);mesh.update()
 for m in src.materials:mesh.materials.append(m)
 uv=mesh.uv_layers.new(name='UVMap');packed=[mesh.uv_layers.new(name='RigidWeights%d'%i) for i in range(4)];old=src.uv_layers.active.data
 for p,op in zip(mesh.polygons,src.polygons):
  p.material_index=op.material_index;p.use_smooth=True
  for loop,ol,vi in zip(p.loop_indices,op.loop_indices,op.vertices):
   uv.data[loop].uv=old[ol].uv
   for n,layer in enumerate(packed):k,w=weights[vi][n];layer.data[loop].uv=(k,1-w)
  p.flip()
 mesh.update();result=bpy.data.objects.new('SM_CalderCrow_LOD%d'%lod,mesh);bpy.context.collection.objects.link(result)
 bpy.ops.object.select_all(action='DESELECT');result.select_set(True);bpy.context.view_layer.objects.active=result
 target=OUT/(result.name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z')
 rows.append({'file':target.name,'vertices':len(mesh.vertices),'triangles':sum(len(p.vertices)-2 for p in mesh.polygons),'uv_channels':len(mesh.uv_layers),'bounds_cm':[[min(v[a] for v in points) for a in range(3)],[max(v[a] for v in points) for a in range(3)]],'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
 bpy.data.objects.remove(obj,do_unlink=True)
assert all(rows[i+1]['triangles']<rows[i]['triangles'] for i in range(2))
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==source_sha
report={'source':'/Game/AnimalVarietyPack/Crow/Meshes/SK_Crow','source_fbx_sha256':source_sha,'runtime_skeleton':False,'controls':16,'custom_floats':225,'pivots_cm':[list(p) for p in pivots],'control_parents':[-1,0,1,0,3,4,0,6,7,0,0,10,0,12,2,0],'lods':rows,'native_quality_acceptance_pending':True}
(OUT/'anatomy.json').write_text(json.dumps(report,indent=2)+'\n')
# Generated header is deliberately left in SourceArt for review; integrating it
# into Source is a separate step after the native deer recovery checkpoint.
vec=lambda p:'FVector(%.8f,%.8f,%.8f)'%tuple(p)
text='// Offline crow rigid pivots; acquired donor read-only.\nnamespace HouseCalderCrowData {\ninline FVector Pivot(int32 I){static const FVector A[]={'+','.join(vec(p) for p in pivots)+'};return A[I];}\n}\n'
(OUT/'HouseCalderCrowData.inl.candidate').write_text(text)
print('CALDER_CROW_STATIC_OFFLINE PASS '+json.dumps(rows))

# Static geometry preview only; native donor textures/posed flight remain required.
for obj in list(bpy.context.scene.objects):
 if not obj.name.startswith('SM_CalderCrow_LOD'):bpy.data.objects.remove(obj,do_unlink=True)
for obj in bpy.context.scene.objects:obj.hide_render=not obj.name.startswith('SM_CalderCrow_LOD0')
mat=bpy.data.materials.new('CrowGeometryPreviewOnly');mat.diffuse_color=(.055,.06,.065,1)
for obj in bpy.context.scene.objects:
 if obj.type=='MESH':
  obj.data.materials.clear();obj.data.materials.append(mat)
  for poly in obj.data.polygons:poly.material_index=0
bpy.ops.object.camera_add(location=(1.7,-2.,1.4));cam=bpy.context.object;cam.rotation_euler=(Vector((0,0,.25))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=1.5
scene=bpy.context.scene;scene.camera=cam;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL';scene.display.shading.show_cavity=True;scene.world.color=(.16,.16,.16);scene.render.resolution_x=1100;scene.render.resolution_y=800;scene.render.resolution_percentage=100;scene.render.filepath=str(OUT/'geometry_preview.png');bpy.ops.render.render(write_still=True)
