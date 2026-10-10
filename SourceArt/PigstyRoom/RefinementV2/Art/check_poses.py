import bpy,math,json
from pathlib import Path
from mathutils import Vector
O=Path(__file__).parent
bpy.ops.wm.open_mainfile(filepath=str(O/'PIG_RealBlackPig_Source.blend'))
scene=bpy.context.scene;scene.cycles.samples=28;scene.render.resolution_x=1400;scene.render.resolution_y=1000
body=bpy.data.objects['SM_PIG_BlackBody'];head=bpy.data.objects['SM_PIG_BlackHead'];pigparts=[o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('SM_PIG_')];state={o.name:(o.location.copy(),o.rotation_euler.copy(),o.hide_render) for o in pigparts}
# Face/body junction at actual runtime feeding head pitch -55 (Blender Y+55).
body.location.z=-.26;head.location=(.60,0,.30);head.rotation_euler.y=math.radians(55)
for o in pigparts:
 if 'Leg' in o.name:o.hide_render=True
cam=scene.camera;cam.location=(2.3,-3.4,.96);target=Vector((.52,0,.26));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=1.09;scene.render.filepath=str(O/'previews'/'PIG_Feeding_Junction.png');bpy.ops.render.render(write_still=True)
for o in pigparts:
 o.location,o.rotation_euler,o.hide_render=state[o.name]
# Side rest inspection: assemble joint-parented tucked legs, then rotate onto flank.
root=bpy.data.objects.new('Side rest art review root',None);bpy.context.collection.objects.link(root)
for o in pigparts:
 if o.hide_render:continue
 o.parent=root
for x in [-.35,.35]:
 for y in [-.21,.21]:
  uppers=[o for o in pigparts if 'UpperLeg' in o.name and not o.hide_render and abs(o.location.x-x)<.001 and abs(o.location.y-y)<.001]
  lowers=[o for o in pigparts if 'LowerLeg' in o.name and not o.hide_render and abs(o.location.x-x)<.001 and abs(o.location.y-y)<.001]
  if uppers and lowers:
   u,l=uppers[0],lowers[0];l.parent=u;l.location=(0,0,-.25);u.rotation_euler.y=math.radians(-65 if x>.0 else 65);l.rotation_euler.y=math.radians(110 if x>.0 else -110)
root.rotation_euler.x=math.radians(90);root.location.z=.284
cam.location=(2.4,-3.8,2.2);target=Vector((.06,-.11,.29));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=2.00;scene.render.filepath=str(O/'previews'/'PIG_SideRest_Underside.png');bpy.ops.render.render(write_still=True)
report={'feedingHeadPivotCm':[60,0,30],'feedingHeadBlenderYDegrees':55,'bodyCrouchCm':26,'sideRestRollDegrees':90,'assetsChanged':False,'reviewOnlyPoseImages':['PIG_Feeding_Junction.png','PIG_SideRest_Underside.png']};(O/'pose_review.json').write_text(json.dumps(report,indent=2));print('PIG_ART_POSE_REVIEW_READY')
