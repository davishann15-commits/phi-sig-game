"""Offline Fab doe -> conventional weighted static LODs, no runtime skeleton.
Run in Blender background. Source donor and source materials are read-only.
"""
import json,hashlib,math
from pathlib import Path
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'SourceArt/CalderLandscape/phase3/deer';OUT.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'SourceArt/CalderLandscape/phase2/doe.fbx'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(SOURCE))
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
models={int(o.name.rsplit('LOD',1)[1]):o for o in bpy.context.scene.objects if o.type=='MESH' and 'LOD' in o.name}
base=lambda p:Vector((-p.y*100,p.x*100,p.z*100+.08724985))
bone=lambda n:base(rig.matrix_world@rig.data.bones['DeerDoe_ '+n].head_local)
pivots=[bone('Spine1'),bone('Neck'),bone('Neck1'),bone('Neck2')]+[Vector() for _ in range(17)]
hips=[];knees=[];ankles=[];toes=[];bends=[];lengths=[]
map_groups={}
for i,(side,front) in enumerate([('R',True),('L',True),('R',False),('L',False)]):
 prefix=side+' ';hi=prefix+('UpperArm' if front else 'Thigh');lo=prefix+('Forearm' if front else 'Calf')
 j=4+i*4
 hips.append(bone(hi));knees.append(bone(lo));ankles.append(bone(prefix+('Finger0' if front else 'Foot')))
 for k,n in enumerate([hi,lo,prefix+('Hand' if front else 'HorseLink'),prefix+('Finger0' if front else 'Foot')]):
  pivots[j+k]=bone(n);map_groups['DeerDoe_ '+n]=j+k
map_groups['DeerDoe_ Neck']=1;map_groups['DeerDoe_ Neck1']=2;map_groups['DeerDoe_ Neck2']=3
pivots[20]=bone('Head')
for n in ['Head','Ponytail1','Ponytail2']:map_groups['DeerDoe_ '+n]=20
source=models[0];groups={g.index:g.name for g in source.vertex_groups};points=[base(source.matrix_world@v.co) for v in source.data.vertices]
for i in range(4):
 candidates=[points[v.index] for v in source.data.vertices if sum(g.weight for g in v.groups if map_groups.get(groups[g.group],-1)in [5+i*4,6+i*4,7+i*4])>.5]
 bottom=min(p.z for p in candidates);sole=[p for p in candidates if p.z<bottom+1.1];toe=sum(sole,Vector())/len(sole);toes.append(toe)
 ray=(ankles[i]-hips[i]).normalized();bend=(knees[i]-hips[i])-ray*(knees[i]-hips[i]).dot(ray);bends.append(bend.normalized())
 lengths.append([(knees[i]-hips[i]).length,(ankles[i]-knees[i]).length])
report={'source':'/Game/AnimalVarietyPack/DeerStagAndDoe/Meshes/SK_DeerDoe','source_fbx_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'runtime_skeleton':False,'controls':21,'custom_floats':253,'pivots_cm':[list(p) for p in pivots],'hips_cm':[list(p) for p in hips],'knees_cm':[list(p) for p in knees],'ankles_cm':[list(p) for p in ankles],'hooves_cm':[list(p) for p in toes],'bends':[list(p) for p in bends],'lengths_cm':lengths,'tail_root_cm':list(bone('Tail')),'lods':[]}
for lod in range(3):
 source=models[lod];groups={g.index:g.name for g in source.vertex_groups};ps=[base(source.matrix_world@v.co) for v in source.data.vertices];weights=[];tail=[]
 for v,p in zip(source.data.vertices,ps):
  totals={};tail_weight=0
  for g in v.groups:
   name=groups[g.group];tail_weight+=g.weight if 'Tail' in name else 0
   key=map_groups.get(name,1 if any(s in name for s in ['Neck','Head','Ponytail']) else 0);totals[key]=totals.get(key,0)+g.weight
  if p.z<2.0:totals={7+min(range(4),key=lambda i:(p-toes[i]).length_squared)*4:1.}
  row=sorted(totals.items(),key=lambda a:-a[1])[:4];total=sum(v for _,v in row);assert total>0
  weights.append([(k,v/total) for k,v in row]+[(0,0.)]*(4-len(row)));tail.append(tail_weight)
 selected=[poly for poly in source.data.polygons if sum(tail[i] for i in poly.vertices)/len(poly.vertices)<.45]
 indices=sorted({i for poly in selected for i in poly.vertices});remap={i:n for n,i in enumerate(indices)}
 mesh=bpy.data.meshes.new('CalderDeerLOD%d'%lod);mesh.from_pydata([(ps[i].x/100,-ps[i].y/100,ps[i].z/100) for i in indices],[],[[remap[i] for i in poly.vertices] for poly in selected]);mesh.update()
 for material in source.data.materials:mesh.materials.append(material)
 uv=mesh.uv_layers.new(name='UVMap');packed=[mesh.uv_layers.new(name='RigidWeights%d'%i) for i in range(4)];old_uv=source.data.uv_layers.active.data
 for poly,old in zip(mesh.polygons,selected):
  poly.material_index=old.material_index;poly.use_smooth=True
  for loop,oldloop,vi in zip(poly.loop_indices,old.loop_indices,old.vertices):
   uv.data[loop].uv=old_uv[oldloop].uv
   for n,layer in enumerate(packed):k,v=weights[vi][n];layer.data[loop].uv=(k,1-v)
  poly.flip()
 mesh.update();obj=bpy.data.objects.new('SM_CalderDeer_LOD%d'%lod,mesh);bpy.context.collection.objects.link(obj)
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 dest=OUT/(obj.name+'.fbx');dest.unlink(missing_ok=True)
 bpy.ops.export_scene.fbx(filepath=str(dest),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z')
 report['lods'].append({'file':dest.name,'vertices':len(mesh.vertices),'triangles':sum(len(p.vertices)-2 for p in mesh.polygons),'uv_channels':len(mesh.uv_layers),'bounds_cm':[[min(ps[i][a] for i in indices) for a in range(3)],[max(ps[i][a] for i in indices) for a in range(3)]],'hoof_vertices_rigid':True})
 obj.hide_render=lod!=0
for o in list(bpy.context.scene.objects):
 if not o.name.startswith('SM_CalderDeer_LOD'):bpy.data.objects.remove(o,do_unlink=True)
# Geometry preview only; runtime Fab textures and native captures are the art verdict.
mat=bpy.data.materials.new('GeometryPreviewOnly');mat.diffuse_color=(.26,.22,.18,1)
for o in bpy.context.scene.objects:
 if o.type=='MESH':
  o.data.materials.clear();o.data.materials.append(mat)
  for poly in o.data.polygons:poly.material_index=0
bpy.ops.object.camera_add(location=(3.4,-4.5,2.3));cam=bpy.context.object;cam.rotation_euler=(Vector((0,0,.8))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=2.8
scene=bpy.context.scene;scene.camera=cam;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL';scene.display.shading.show_cavity=True;scene.world.color=(.12,.12,.12);scene.render.resolution_x=1100;scene.render.resolution_y=800;scene.render.resolution_percentage=100;scene.render.filepath=str(OUT/'geometry_preview.png');bpy.ops.render.render(write_still=True)
p=OUT/'anatomy.json';p.unlink(missing_ok=True);p.write_text(json.dumps(report,indent=2)+'\n')
def vec(v):return 'FVector(%.8f,%.8f,%.8f)'%tuple(v)
header='// Generated offline by Tools/calder/build_deer_static.py; source Fab assets read-only.\nnamespace HouseCalderDeerData {\n'
for name,values in [('Pivot',pivots),('Hip',hips),('Knee',knees),('Ankle',ankles),('Hoof',toes),('Bend',bends)]:
 header+='inline FVector '+name+'(int32 I){static const FVector A[]={'+','.join(vec(p) for p in values)+'};return A[I];}\n'
header+='inline FVector TailRoot(){return '+vec(Vector(report['tail_root_cm']))+';}\n}\n'
p=ROOT/'Source/SeniorSendoff/HouseCalderDeerData.inl';p.unlink(missing_ok=True);p.write_text(header)
print('CALDER_DEER_STATIC_OFFLINE PASS',json.dumps(report['lods']))
