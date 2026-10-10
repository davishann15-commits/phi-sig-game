import bpy,json,os,math
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));d=json.load(open(R+'/extraction-manifest.json'));byid={p['id']:p for p in d['pieces']}
bpy.ops.wm.read_factory_settings(use_empty=True)
def put(path,x,y,size=2.8):
 before=set(bpy.context.scene.objects);bpy.ops.import_scene.fbx(filepath=path);o=next(o for o in bpy.context.scene.objects if o not in before and o.type=='MESH')
 pts=[o.matrix_world@v.co for v in o.data.vertices];lo=Vector(tuple(min(p[i] for p in pts) for i in range(3)));hi=Vector(tuple(max(p[i] for p in pts) for i in range(3)));sc=size/max(hi-lo)
 for v in o.data.vertices:v.co=(o.matrix_world@v.co-(lo+hi)*.5)*sc
 o.matrix_world.identity();o.location=(x,y,(hi.z-lo.z)*sc*.5+.05)
 for m in o.data.materials:
  if not m:continue
  n=m.name.lower();m.diffuse_color=(.3,.11,.04,1) if any(s in n for s in ('bronze','leather','walnut')) else (.21,.3,.17,1) if 'green' in n else(.12,.13,.14,1) if any(s in n for s in ('metal','black')) else(.74,.71,.59,1)
 return o
sofa=byid['SM_GR3_NearSeating__003']
for i,key in enumerate(['intact_fbx','damaged_fbx','heavily_damaged_fbx','broken_fbx']):
 path=sofa.get(key) or sofa['intact_fbx'].replace('.fbx',{'damaged_fbx':'_Damaged.fbx','heavily_damaged_fbx':'_HeavilyDamaged.fbx'}.get(key,'') )
 if os.path.exists(path):put(path,(i-1.5)*3.4,1.9)
for i,id in enumerate(['SM_GR3_NearSeating__004','SMF_Upper_ALL__040','BR_SM_Bedroom_CreamCurtainsPartOpen__001','SM_Entrance_m1_m1_Glass__001']):
 if id in byid:put(byid[id]['intact_fbx'],(i-1.5)*3.4,-2,2.0)
# Neutral stage and four directional labels make the shape audit readable.
for x,y,text in [(i*3.4-5.1,3.6,t) for i,t in enumerate(['Intact','Chipped','Badly damaged','Broken'])]+[(i*3.4-5.1,-3.9,t) for i,t in enumerate(['Throw pillow','Original chandelier','Curtain panel','Window pane'])]:
 bpy.ops.object.text_add(location=(x,y,.02));o=bpy.context.object;o.data.body=text;o.data.align_x='CENTER';o.data.size=.24
 bpy.ops.object.convert(target='MESH')
bpy.ops.mesh.primitive_plane_add(size=200);floor=bpy.context.object;floor.location.z=-.02;m=bpy.data.materials.new('Backdrop');m.diffuse_color=(.62,.65,.68,1);floor.data.materials.append(m)
bpy.ops.object.camera_add(location=(0,-14,13));camera=bpy.context.object;direction=Vector((0,0,.2))-camera.location;camera.rotation_euler=direction.to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=15;bpy.context.scene.camera=camera
scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH';scene.display.shading.background_type='WORLD';scene.world=bpy.data.worlds.new('World');scene.world.color=(.6,.63,.66);scene.render.resolution_x=1600;scene.render.resolution_y=1080;scene.render.resolution_percentage=100;scene.render.filepath=R+'/inventory/geometry-contact.png';bpy.ops.render.render(write_still=True)
