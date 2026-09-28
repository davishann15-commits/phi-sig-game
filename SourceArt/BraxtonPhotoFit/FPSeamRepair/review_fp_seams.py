"""CPU-only isolated seam previews; not an Unreal or network validation."""
import bpy,json,math,hashlib
from pathlib import Path
from mathutils import Vector,Matrix
HERE=Path(__file__).resolve().parent;SOURCE=HERE.parent
OUT=Path('/home/davis/Documents/ChatGPT/Senior Sendoff/ArtSource/HouseRebuild/Analysis/FirstPersonSleeveAudit20260928')
REPAIR=json.loads((OUT/'repair_validation.json').read_text())
BOUNDS=json.loads((OUT/'source_body_bounds.json').read_text())
NAMES=['C01_Body.male_casualsuit02_Arms','C01_Body_Arms']
CASES=[('Idle',0.,0.),('Windup',-1.,0.),('Release',1.,0.),('ReachCatch',.25,1.)]
results=[]
def material(name,color):
 m=bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=.78;return m
for label,file in [('Baseline',SOURCE/'SK_C01_Arms.fbx'),('Repaired',HERE/'SK_C01_Arms_SeamRepair.fbx')]:
 before=hashlib.sha256(file.read_bytes()).hexdigest()
 if label=='Repaired':assert before==REPAIR['outputSha256'],'Stale repair validation report'
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(file),use_anim=False)
 rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');rig.animation_data_clear()
 for p in rig.pose.bones:p.matrix_basis=Matrix.Identity(4)
 before_animation=set(bpy.data.objects)
 bpy.ops.import_scene.fbx(filepath=str(SOURCE/'AN_C01_ArmsIdle.fbx'))
 animation_objects=set(bpy.data.objects)-before_animation
 animation_rig=next(o for o in animation_objects if o.type=='ARMATURE')
 assert set(rig.pose.bones.keys())==set(animation_rig.pose.bones.keys())
 rig.animation_data_create();rig.animation_data.action=animation_rig.animation_data.action;rig.animation_data.action_slot=animation_rig.animation_data.action_slot
 for ob in animation_objects:bpy.data.objects.remove(ob,do_unlink=True)
 bpy.context.scene.frame_set(17);bpy.context.view_layer.update()
 base_pose={p.name:p.matrix.copy()for p in rig.pose.bones}
 rig.animation_data_clear()
 def pose_chain(side,target,pole):
  # Equivalent two-bone position/shortest-arc rotation construction
  # to AnimationCore/Private/TwoBoneIK.cpp; avoids Blender IK bone-tail/roll
  # conventions. The source ArmsIdle is used; native C++ finger curl is omitted.
  ub=rig.pose.bones['upperarm_'+side];lb=rig.pose.bones['lowerarm_'+side];hb=rig.pose.bones['hand_'+side]
  a=ub.matrix.translation.copy();b=lb.matrix.translation.copy();c=hb.matrix.translation.copy()
  direction=(target-a).normalized();dist=(target-a).length;u=(b-a).length;v=(c-b).length
  bend=pole-a; bend=(bend-direction*bend.dot(direction)).normalized()
  if dist>=u+v:end=a+direction*(u+v);joint=a+direction*u
  else:
   along=(u*u+dist*dist-v*v)/(2*dist);height=math.sqrt(max(0.,u*u-along*along))
   end=target;joint=a+direction*along+bend*height
  ur=(b-a).normalized().rotation_difference((joint-a).normalized())@base_pose[ub.name].to_quaternion()
  lr=(c-b).normalized().rotation_difference((end-joint).normalized())@base_pose[lb.name].to_quaternion()
  finger=base_pose['middle_01_'+side].translation-c
  desired=Vector((.6 if side=='r' else -.6,-.8,-.12)).normalized()
  hr=finger.normalized().rotation_difference(desired)@base_pose[hb.name].to_quaternion()
  ub.matrix=Matrix.LocRotScale(a,ur,Vector((1,1,1)));bpy.context.view_layer.update()
  lb.matrix=Matrix.LocRotScale(joint,lr,Vector((1,1,1)));bpy.context.view_layer.update()
  hb.matrix=Matrix.LocRotScale(end,hr,Vector((1,1,1)));bpy.context.view_layer.update()
  assert (hb.matrix.translation-end).length<1e-5
  return dict(side=side,shoulder=list(a),elbow=list(joint),wrist=list(end),target=list(target))
 gray=material('Review cloth',(.14,.17,.21));skin=material('Review skin',(.53,.29,.19))
 for name in NAMES:
  mesh=bpy.data.objects[name];mesh.data.materials.clear();mesh.data.materials.append(gray if 'casualsuit' in name else skin)
 camera_data=bpy.data.cameras.new('ReviewCamera');camera=bpy.data.objects.new('ReviewCamera',camera_data);bpy.context.scene.collection.objects.link(camera)
 camera.location=(0,BOUNDS['estimatedCameraSourceY'],BOUNDS['estimatedCameraSourceZ']);camera.rotation_euler=(Vector((0,-1,0))).to_track_quat('-Z','Y').to_euler();camera_data.type='PERSP';camera_data.angle=math.radians(106.52);camera_data.clip_start=.10
 scene=bpy.context.scene;scene.camera=camera;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=20;scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=4
 scene.world=bpy.data.worlds.new('SeamPreviewWorld');scene.render.resolution_x=960;scene.render.resolution_y=540;scene.render.resolution_percentage=100;scene.world.color=(.15,.15,.15)
 scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.23,.26,.3,1);scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.7
 for name,loc,power,size in [('Key',(0,-1.5,2.5),300,3),('Fill',(2,-.5,1.8),120,2)]:
  d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,1.3))-o.location).to_track_quat('-Z','Y').to_euler()
 for case,motion,flight in CASES:
  for p in rig.pose.bones:
   p.matrix=base_pose[p.name];bpy.context.view_layer.update()
  posechecks=[]
  for side in ['r','l']:
   right=side=='r';u=Vector((42,35,124)if right else(14,-35,100));u+=Vector((15*motion,30*min(motion,0)+18*max(motion,0),12*abs(motion))if right else(0,0,-6*abs(motion)));u+=Vector((-8,5,7)if right else(-6,-4,-18))*flight
   p=Vector((8,65,120)if right else(8,-65,115));to_bl=lambda v:Vector((-v.y,-v.x,v.z))/100
   posechecks.append(pose_chain(side,to_bl(u),to_bl(p)))
  bpy.context.view_layer.update()
  checks=[]
  if label=='Repaired':
   deps=bpy.context.evaluated_depsgraph_get()
   for r in REPAIR['meshes']:
    ob=bpy.data.objects[r['name']];evalob=ob.evaluated_get(deps);mesh=evalob.to_mesh();forward=Vector((0,-1,0))
    distances=[((ob.matrix_world@mesh.vertices[i].co)-camera.location).dot(forward)for i in r['capVertices']]
    assert max(distances)<-.2,(case,ob.name,max(distances))
    checks.append(dict(mesh=ob.name,maximumCapForwardFromCameraM=max(distances)));evalob.to_mesh_clear()
  scene.render.filepath=str(OUT/(label+'_'+case+'.png'));bpy.ops.render.render(write_still=True)
  results.append(dict(asset=label,case=case,image=scene.render.filepath,terminalCapChecks=checks,poseChecks=posechecks))
 assert before==hashlib.sha256(file.read_bytes()).hexdigest()
(OUT/'cpu_seam_preview.json').write_text(json.dumps(dict(status='PASS_CPU_SEAM_PREVIEW',scope='Isolated CPU Cycles seam views with CPU two-bone solve transcribed from engine mathematics and C++ pose targets, on source ArmsIdle frame 17; neutral review materials and no hat. Not native Unreal animation/rendering/network proof.',horizontalFov=106.52,clipStartM=.1,cameraSourceLocation=[0,BOUNDS['estimatedCameraSourceY'],BOUNDS['estimatedCameraSourceZ']],cameraEstimateNote=BOUNDS['note'],assetSha256=REPAIR['outputSha256'],results=results),indent=2)+'\n')
print('SSO_FP_CPU_SEAM_PREVIEW_PASS')
