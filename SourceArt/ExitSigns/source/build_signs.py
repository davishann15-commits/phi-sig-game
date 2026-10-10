import bpy,math,os,json
from mathutils import Vector
base='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ExitSigns/'
os.makedirs(base+'models',exist_ok=True)
font=bpy.data.fonts.load('/System/Library/Fonts/Supplemental/Arial Bold.ttf')
colors={'Housing':(.73,.72,.66),'Face':(.85,.85,.79),'RedLens':(.6,.004,.002),'GreenLens':(.005,.45,.04),'Seam':(.15,.16,.15),'Screw':(.42,.43,.41),'Status':(.005,.32,.035),'Unlit':(.08,.018,.012)}
mats={n:bpy.data.materials.new('EX_'+n) for n in colors}
for n,m in mats.items():m.diffuse_color=(*colors[n],1)
def box(n,p,d,mat,b=.002):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.name=n;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mats[mat]);be=o.modifiers.new('Molded edges','BEVEL');be.width=b;be.segments=3;bpy.ops.object.modifier_apply(modifier=be.name);return o
def text(t,p,size,mat,back=False):
 bpy.ops.object.text_add(location=p);o=bpy.context.object;o.data.body=t;o.data.font=font;o.data.align_x='CENTER';o.data.size=size;o.data.extrude=.00025;o.data.bevel_depth=.0001;o.data.resolution_u=3;o.rotation_euler=(math.pi/2,0,math.pi if back else 0);o.data.materials.append(mats[mat]);bpy.ops.object.convert(target='MESH')
def disk(p,r,mat):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=6,radius=r,location=p);o=bpy.context.object;o.scale=(1,.25,1);o.data.materials.append(mats[mat])
def sign(name,arrow=0,double=False,green=False,damage=0):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 box('Back shell',(0,.005,0),(.366,.038,.214),'Housing',.006)
 box('Housing split seam',(0,-.012,0),(.368,.0025,.214),'Seam',.005)
 box('Beveled cover',(0,-.018,0),(.37,.018,.216),'Housing',.006)
 for side in ([1,-1] if double else [1]):
  y=-.028*side
  if not damage>=3:box('Inset diffuser',(0,y,0),(.334,.003,.179),'Face',.003)
  lens='Unlit' if damage>=2 else ('GreenLens' if green else 'RedLens')
  shift=.018*arrow
  text('EXIT',(-shift,y-.002*side,-.061 if arrow else -.068),.17 if arrow else .19,lens,side<0)
  if arrow:
   a=arrow;x=.147*a;pts=[(x,y-.003*side,.003),(x-.028*a,y-.003*side,.035),(x-.028*a,y-.003*side,-.029)]
   me=bpy.data.meshes.new('Chevron');me.from_pydata(pts,[],[(0,1,2)]);ob=bpy.data.objects.new('Directional knockout',me);bpy.context.collection.objects.link(ob);ob.data.materials.append(mats[lens])
  for x in [-.173,.173]:
   disk((x,y-.001*side,0),.0023,'Screw');box('Screw slot',(x,y-.0017*side,0),(.0028,.0005,.0004),'Seam',.0001)
  disk((.143,y-.002*side,-.088),.0018,'Status' if damage==0 else 'Unlit')
  disk((.125,y-.002*side,-.088),.0024,'Seam')
  text('TEST',(.105,y-.002*side,-.089),.008,'Seam',side<0)
  if damage:
   for x,z,angle in [(-.11,.035,.4),(-.092,.015,-.5),(.08,-.03,.3)]:
    o=box('Fine lens fracture',(x,y-.003*side,z),(.035,.0005,.0008),'Seam',.0001);o.rotation_euler.y=angle
 for x in [-.12,-.10,-.08,.08,.10,.12]:box('Bottom vent',(x,-.002,-.1075),(.011,.013,.0006),'Seam',.0002)
 if not double:box('Wall mounting plate',(0,.027,0),(.10,.008,.08),'Housing',.002)
 bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=bpy.context.selected_objects[0];bpy.ops.object.join();o=bpy.context.object;o.name=name;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 bpy.ops.export_scene.fbx(filepath=base+'models/'+name+'.fbx',use_selection=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',add_leaf_bones=False)
for a,label in [(0,'0'),(-1,'L'),(1,'R')]:sign('SM_EX_Double_'+label,a,True)
for a,label in [(0,'0'),(-1,'L')]:
 for stage in range(4):sign('SM_EX_Wall_'+label+'_'+str(stage),a,damage=stage)
sign('SM_EX_Wall_GreenL',-1,green=True)
json.dump(colors,open(base+'materials.json','w'),indent=2)
