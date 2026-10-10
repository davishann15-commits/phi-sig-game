import bpy,json,os,math
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));doc=json.load(open(R+'/extraction-manifest.json'));preview=json.load(open(R+'/inventory/damage-v2-preview.json'));rows=[p for p in doc['pieces']if p['asset_name']in preview['assets']];uniq={p['asset_name']:p for p in rows};rows=list(uniq.values())
bpy.ops.wm.read_factory_settings(use_empty=True)
for ri,p in enumerate(rows):
 paths=[p['intact_fbx']]+[preview['assets'][p['asset_name']][k+'_fbx']for k in ('damaged','heavily_damaged','broken')]
 refsize=max(b-a for a,b in zip(*p['bounds_blender_metres']));factor=2.6/refsize
 for ci,path in enumerate(paths):
  pre=set(bpy.context.scene.objects);bpy.ops.import_scene.fbx(filepath=path);obs=[o for o in bpy.context.scene.objects if o not in pre]
  for o in obs:
   if o.type!='MESH' or o.name.startswith('UCX_'):bpy.data.objects.remove(o,do_unlink=True);continue
   # All four stages share original scale and floor height, exposing distortion.
   o['audit_row']=ri;o.scale*=factor;o.location=Vector(((ci-1.5)*3.4,(len(rows)-ri-1)*3.9,0));minz=min((o.matrix_world@v.co).z for v in o.data.vertices)
   o.location.z-=p['bounds_blender_metres'][0][2]*0 # exports are centered
   localmin=-(p['bounds_blender_metres'][1][2]-p['bounds_blender_metres'][0][2])*.5*factor
   o.location.z=-localmin
   for m in o.data.materials:
    t=m.name.lower()
    col=(.40,.41,.39,1)
    if 'splinterwood'in t:col=(.60,.37,.14,1)
    elif 'm_hi_foam'in t:col=(.80,.66,.39,1)
    elif 'rawmetal'in t:col=(.17,.19,.21,1)
    elif 'fracturecore'in t:col=(.65,.62,.52,1)
    elif 'olive'in t:col=(.25,.28,.15,1)
    elif 'bronze'in t or 'cognac'in t:col=(.39,.19,.085,1)
    elif any(k in t for k in ('walnut','wood','oak','darkstained')):col=(.20,.085,.038,1)
    elif any(k in t for k in ('metal','steel','black','rubber')):col=(.06,.065,.07,1)
    elif 'chrome'in t:col=(.65,.66,.69,1)
    elif 'enamel'in t:col=(.65,.66,.67,1)
    elif 'ivory'in t:col=(.77,.73,.62,1)
    elif any(k in t for k in ('art','collage')):col=(.25,.42,.55,1)
    m.diffuse_color=col
  bpy.ops.mesh.primitive_plane_add(size=3.05,location=((ci-1.5)*3.4,(len(rows)-ri-1)*3.9,-.02));floor=bpy.context.object;floor['audit_row']=ri;mat=bpy.data.materials.new('floor');mat.diffuse_color=(.70,.70,.68,1);floor.data.materials.append(mat)
  bpy.ops.object.text_add(location=((ci-1.5)*3.4-1.48,(len(rows)-ri-1)*3.9-1.65,.025));txt=bpy.context.object;txt['audit_row']=ri;txt.data.body=['INTACT','CHIPPED / TORN','PARTS FAILING','BROKEN PARTS'][ci];txt.data.size=.17;txt.data.extrude=0
  mat=bpy.data.materials.new('label');mat.diffuse_color=(.06,.06,.06,1);txt.data.materials.append(mat)
for m in bpy.data.materials:
 t=m.name.lower()
 if t.startswith(('floor','label')):continue
 col=(.40,.41,.39,1)
 if 'splinterwood'in t:col=(.60,.37,.14,1)
 elif 'm_hi_foam'in t:col=(.80,.66,.39,1)
 elif 'rawmetal'in t:col=(.17,.19,.21,1)
 elif 'fracturecore'in t:col=(.65,.62,.52,1)
 elif 'olive'in t:col=(.25,.28,.15,1)
 elif 'bronze'in t or 'cognac'in t:col=(.39,.19,.085,1)
 elif any(k in t for k in ('walnut','wood','oak','darkstained')):col=(.20,.085,.038,1)
 elif any(k in t for k in ('metal','steel','black','rubber')):col=(.06,.065,.07,1)
 elif 'chrome'in t:col=(.65,.66,.69,1)
 elif 'enamel'in t:col=(.65,.66,.67,1)
 elif 'ivory'in t:col=(.77,.73,.62,1)
 elif any(k in t for k in ('art','collage')):col=(.25,.42,.55,1)
 m.diffuse_color=col
scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.studiolight_rotate_z=.6;scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH';scene.display.shading.curvature_ridge_factor=1.4;scene.display.shading.curvature_valley_factor=1.3;scene.display.shading.background_type='WORLD';scene.world=bpy.data.worlds.new('World');scene.world.color=(.84,.84,.84)
cy=(len(rows)-1)*3.9/2
bpy.ops.object.camera_add(location=(0,cy-21,28));camera=bpy.context.object;camera.rotation_euler=(Vector((0,cy,0))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=29;scene.camera=camera
scene.render.resolution_x=2600;scene.render.resolution_y=3000;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG';scene.render.filepath=R+'/inventory/damage-v2-contact.png';bpy.ops.render.render(write_still=True)
# Detailed chair/sofa/table view retains enough pixels to inspect cap faces.
for idx in (0,1,2,3,4,5):
 
 for ob in bpy.context.scene.objects:
  if 'audit_row'in ob:ob.hide_render=ob['audit_row']!=idx
 y=(len(rows)-idx-1)*3.9;camera.location=(0,y-7,7);camera.rotation_euler=(Vector((0,y,.6))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=14.7;scene.render.resolution_x=2400;scene.render.resolution_y=800;scene.render.filepath=R+'/inventory/damage-v2-row-%d.png'%idx;bpy.ops.render.render(write_still=True)
print('PREVIEW_DONE')
