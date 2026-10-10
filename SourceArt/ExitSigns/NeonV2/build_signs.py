import bpy,math,os,json
from mathutils import Vector
base='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ExitSigns/NeonV2/'
os.makedirs(base+'models',exist_ok=True)
font=bpy.data.fonts.load('/System/Library/Fonts/Supplemental/Arial Bold.ttf')
colors={'Housing':(.18,.185,.19),'Face':(.12,.125,.13),'RedLens':(.65,.001,.0003),'GreenLens':(.005,.45,.04),'Seam':(.035,.04,.043),'Screw':(.42,.43,.41),'Status':(.005,.32,.035),'Unlit':(.028,.004,.002)}
mats={n:bpy.data.materials.new('NEX_'+n) for n in colors}
for n,m in mats.items():m.diffuse_color=(*colors[n],1)
def box(n,p,d,mat,b=.002):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.name=n;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mats[mat]);be=o.modifiers.new('Molded edges','BEVEL');be.width=b;be.segments=3;bpy.ops.object.modifier_apply(modifier=be.name);return o
def text(t,p,size,mat,back=False):
 bpy.ops.object.text_add(location=p);o=bpy.context.object;o.data.body=t;o.data.font=font;o.data.align_x='CENTER';o.data.size=size;o.data.extrude=.00025;o.data.bevel_depth=.0001;o.data.resolution_u=3;o.scale.x=.58 if t=='EXIT' else 1;o.rotation_euler=(math.pi/2,0,math.pi if back else 0);o.data.materials.append(mats[mat]);bpy.ops.object.convert(target='MESH')
def disk(p,r,mat):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=6,radius=r,location=p);o=bpy.context.object;o.scale=(1,.25,1);o.data.materials.append(mats[mat])
def sign(name,arrow=0,double=False,green=False,damage=0):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 box('Back shell',(0,.005,0),(.366,.038,.294),'Housing',.006)
 box('Housing split seam',(0,-.012,0),(.368,.0025,.294),'Seam',.005)
 box('Beveled cover',(0,-.018,0),(.37,.018,.296),'Housing',.006)
 for side in ([1,-1] if double else [1]):
  y=-.028*side
  if True:box('Inset diffuser',(0,y,0),(.334,.003,.265),'Face',.003)
  lens='Unlit' if damage>=2 else ('GreenLens' if green else 'RedLens')
  shift=.011*arrow
  text('EXIT',(-shift,y-.002*side,-.09),.26,lens,side<0)
  for a in ([arrow] if arrow else [-1,1]):
   x=.158*a; yy=y-.003*side
   pts=[(x,yy,.003),(x-.023*a,yy,.044),(x-.032*a,yy,.038),(x-.012*a,yy,.003),(x-.032*a,yy,-.032),(x-.023*a,yy,-.038)]
   me=bpy.data.meshes.new('Chevron');me.from_pydata(pts,[],[tuple(range(6))]);ob=bpy.data.objects.new('Glowing directional lens',me);bpy.context.collection.objects.link(ob);ob.data.materials.append(mats[lens])
  for x in [-.173,.173]:
   disk((x,y-.001*side,0),.0023,'Screw');box('Screw slot',(x,y-.0017*side,0),(.0028,.0005,.0004),'Seam',.0001)
  disk((.143,y-.002*side,-.131),.0018,'Status' if damage==0 else 'Unlit')
  disk((.125,y-.002*side,-.131),.0024,'Seam')
  text('TEST',(.105,y-.002*side,-.132),.008,'Seam',side<0)
  if damage:
   for x,z,angle in [(-.11,.035,.4),(-.092,.015,-.5),(.08,-.03,.3)]:
    o=box('Fine lens fracture',(x,y-.003*side,z),(.035,.0005,.0008),'Seam',.0001);o.rotation_euler.y=angle
 for x in [-.12,-.10,-.08,.08,.10,.12]:box('Bottom vent',(x,-.002,-.1475),(.011,.013,.0006),'Seam',.0002)
 if not double:box('Wall mounting plate',(0,.027,0),(.10,.008,.08),'Housing',.002)
 bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=bpy.context.selected_objects[0];bpy.ops.object.join();o=bpy.context.object;o.name=name;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

 if damage>=3:
  for v in o.data.vertices:
   if v.co.x>.075 and v.co.z<.04:v.co.y-=(v.co.x-.075)*.12;v.co.z-=(v.co.x-.075)*.08
 bpy.ops.export_scene.fbx(filepath=base+'models/'+name+'.fbx',use_selection=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',add_leaf_bones=False)
for double,kind in [(False,'Wall'),(True,'Double')]:
 for a,label in [(0,'0'),(-1,'L'),(1,'R')]:
  for stage in range(4):sign('SM_NEX_'+kind+'_'+label+'_'+str(stage),a,double,damage=stage)
json.dump(colors,open(base+'materials.json','w'),indent=2)
