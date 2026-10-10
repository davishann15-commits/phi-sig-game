import bpy,math,json,random
from mathutils import Vector
from pathlib import Path
B=Path(__file__).resolve().parent
random.seed(2355)
plans=[]; objs=[]
def reset():
 global objs
 bpy.ops.wm.read_factory_settings(use_empty=True);objs=[]
def mat(name):
 return bpy.data.materials.get(name) or bpy.data.materials.new(name)
def finish(o,name,material,smooth=False):
 o.name=name;o.data.materials.append(mat(material));objs.append(o)
 if smooth:
  for p in o.data.polygons:p.use_smooth=True
 return o
def box(name,p,d,m,bevel=0):
 bpy.ops.mesh.primitive_cube_add(size=1,location=(p[0]/100,-p[1]/100,p[2]/100));o=bpy.context.object;o.scale=Vector((d[0]/100,d[1]/100,d[2]/100))
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 if bevel:
  mod=o.modifiers.new('Rounded edges','BEVEL');mod.width=bevel/100;mod.segments=3;bpy.ops.object.modifier_apply(modifier=mod.name)
  mod=o.modifiers.new('Weighted normals','WEIGHTED_NORMAL');bpy.ops.object.modifier_apply(modifier=mod.name)
 return finish(o,name,m)
def ell(name,p,d,m):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=32,ring_count=16,location=(p[0]/100,-p[1]/100,p[2]/100));o=bpy.context.object;o.scale=Vector((d[0]/200,d[1]/200,d[2]/200));bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,name,m,True)
def rod(name,a,b,r,m,verts=16):
 a=Vector((a[0]/100,-a[1]/100,a[2]/100));b=Vector((b[0]/100,-b[1]/100,b[2]/100));delta=b-a
 bpy.ops.mesh.primitive_cylinder_add(vertices=verts,radius=r/100,depth=delta.length,location=(a+b)/2);o=bpy.context.object;o.rotation_euler=delta.to_track_quat('Z','Y').to_euler();return finish(o,name,m,True)
def curve(name,points,r,m):
 cu=bpy.data.curves.new(name,'CURVE');cu.dimensions='3D';cu.bevel_depth=r/100;cu.bevel_resolution=3;s=cu.splines.new('POLY');s.points.add(len(points)-1)
 for v,p in zip(s.points,points):v.co=(p[0]/100,-p[1]/100,p[2]/100,1)
 o=bpy.data.objects.new(name,cu);bpy.context.collection.objects.link(o);bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.convert(target='MESH');o=bpy.context.object;return finish(o,name,m,True)
def cloth(name,x0,x1,z0,z1,y,m,fold=2,damage=False):
 vs=[];fs=[];nx=60;nz=26
 for j in range(nz+1):
  v=j/nz;z=z0+(z1-z0)*v
  for i in range(nx+1):
   u=i/nx;x=x0+(x1-x0)*u;yy=y+math.sin(u*math.pi*24)*fold*(1-.25*v)+math.sin(v*8+u*20)*.25
   vs.append((x/100,-yy/100,(z+math.sin(u*20)*(1-v)*.9)/100))
 for j in range(nz):
  for i in range(nx):
   if damage and j<4 and i%19<2:continue
   a=j*(nx+1)+i;fs.append((a,a+1,a+nx+2,a+nx+1))
 me=bpy.data.meshes.new(name);me.from_pydata(vs,[],fs);me.update();o=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(o);finish(o,name,m,True)
 return o
def export(name,collision=False):
 colliders=[]
 if name.startswith('SM_HFP_Chair'):
  for part in objs:
   if any(part.name.startswith(n)for n in ['Chair lower base','Seat support','Upholstered arm','Tall chair back','Stuffed back cushion','Walnut foot']):
    points=[part.matrix_world@Vector(v)for v in part.bound_box];lo=Vector([min(v[k]for v in points)for k in range(3)]);hi=Vector([max(v[k]for v in points)for k in range(3)])
    colliders.append((lo,hi))
 bpy.ops.object.select_all(action='DESELECT')
 for o in objs:o.select_set(True)
 bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join();o=bpy.context.object
 # Bake object transforms, then reset the FBX origin to the authored pivot.
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True);bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');o.name=name
 uv=o.data.uv_layers.new(name='UVMap') if not o.data.uv_layers else o.data.uv_layers[0]
 for p in o.data.polygons:
  n=p.normal;axes=(0,2) if abs(n.y)>.6 else ((1,2)if abs(n.x)>.6 else(0,1))
  for li in p.loop_indices:
   v=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(v[axes[0]],v[axes[1]])
 mats=[m.name for m in o.data.materials]
 for i,(lo,hi)in enumerate(colliders):
  bpy.ops.mesh.primitive_cube_add(size=1,location=(lo+hi)/2);co=bpy.context.object;co.scale=hi-lo;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);co.name='UCX_'+name+'_'+str(i).zfill(2)
  co.select_set(True)
 for item in bpy.context.scene.objects:item.select_set(item==o or item.name.startswith('UCX_'+name+'_'))
 bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.fbx(filepath=str(B/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
 plans.append(dict(name=name,file=str(B/(name+'.fbx')),materials=mats,collision=collision,convex_collision=bool(colliders)))
# Porch door: existing hinge origin, inside on +Y.
reset();w=158.4;h=220
for x in [-7,-w+7]:box('Door stile',(x,0,h/2),(14,5.8,h),'HFP_PaintedWhite',1)
for z,dz in [(6,12),(93,13),(213,14)]:box('Door rail',(-w/2,0,z),(w-24,5.8,dz),'HFP_PaintedWhite',.6)
box('Lower raised panel',(-w/2,0,49),(w-26,3.8,73),'HFP_PaintedWhite',.8)
for yy in [-3.25,3.25]:
 for z in [15,83]:box('Panel moulding',(-w/2,yy,z),(w-31,1.5,2.5),'HFP_PaintedWhite',.5)
 for x in [-18,-w+18]:box('Panel moulding',(x,yy,49),(2.5,1.5,70),'HFP_PaintedWhite',.5)
 for x in [-13,-w+13]:box('Glass beading',(x,yy,153),(2,1.2,104),'HFP_PaintedWhite',.35)
 for z in [101,205]:box('Glass beading',(-w/2,yy,z),(w-24,1.2,2),'HFP_PaintedWhite',.35)
for x in [-w/3,-2*w/3]:box('Vertical window muntin',(x,0,153),(2.3,6,103),'HFP_PaintedWhite',.3)
box('Horizontal window muntin',(-w/2,0,153),(w-26,6,2.3),'HFP_PaintedWhite',.3)
for z in [27,108,195]:
 rod('Hinge barrel',(-2,3.4,z-6),(-2,3.4,z+6),1.1,'HFP_Brass');box('Hinge leaf',(-3.6,3.0,z),(5,1,12),'HFP_Brass',.3)
for yy in [-1,1]:
 box('Latch escutcheon',(-w+11,yy*3.9,112),(7,1.4,18),'HFP_Brass',.7)
 rod('Lever neck',(-w+11,yy*4,112),(-w+11,yy*9.5,112),1.8,'HFP_Chrome')
 curve('Return lever',[(-w+11,yy*9.5,112),(-w+7,yy*10,112),(-w+1,yy*10,111),(-w-1,yy*7,110)],1.4,'HFP_Chrome')
 for z in [105,119]:ell('Plate screw',(-w+11,yy*4.9,z),(1,0.5,1),'HFP_Chrome')
export('SM_HFP_PorchDoor',True)
reset()
for x0,x1 in [(-w+14,-2*w/3-1.4),(-2*w/3+1.4,-w/3-1.4),(-w/3+1.4,-14)]:
 for z0,z1 in [(102,151.4),(154.6,204)]:box('Pane',((x0+x1)/2,0,(z0+z1)/2),(x1-x0,.45,z1-z0),'HFP_Glass')
export('SM_HFP_PorchDoorGlass')
reset();cloth('Navy door curtain',-151,-7,97,207,7.5,'HFP_NavyLinen',2.2)
for z in [99,206]:curve('Curtain hem',[(-151+i*144/60,7.5+math.sin(i/60*math.pi*24)*2.2,z)for i in range(61)],.32,'HFP_NavyLinen')
export('SM_HFP_DoorCurtain')
reset();rod('Curtain rod',(-153,6,209),(-5,6,209),.7,'HFP_Chrome')
for x in [-150,-8]:box('Curtain clip',(x,4.5,209),(2,2,3),'HFP_Chrome',.4)
export('SM_HFP_CurtainRod')
# Tall stair landing sash window, local center pivot at bottom of opening.
reset()
for x in [-97,97]:box('Casing',(x,-4,107.5),(14,9,237),'HFP_PaintedWhite',.65)
for z in [-4,222]:box('Casing',(0,-4,z),(206,9,13),'HFP_PaintedWhite',.6)
box('Projecting sill',(0,-10,-7),(218,26,8),'HFP_PaintedWhite',.9)
box('Apron',(0,-6,-16),(196,5,11),'HFP_PaintedWhite',.5)
for x in [-86,86]:box('Sash stile',(x,3,108),(7,12,211),'HFP_PaintedWhite',.4)
for z in [4,109,211]:box('Sash rail',(0,3,z),(178,12,7),'HFP_PaintedWhite',.4)
box('Center muntin',(0,3,108),(3.2,12,210),'HFP_PaintedWhite',.4)
for x in [-91,91]:box('Reveal',(x,15,108),(5,30,216),'HFP_PaintedWhite',.3)
for z in [0,216]:box('Reveal',(0,15,z),(180,30,4),'HFP_PaintedWhite',.3)
box('Latch foot',(7,-4,112),(7,2,3),'HFP_Chrome',.4);curve('Sash latch',[(5,-5,113),(7,-7,114),(11,-7,113)],.65,'HFP_Chrome')
export('SM_HFP_LandingWindowFrame',True)
reset()
for x0,x1 in [(-82,-2),(2,82)]:
 for z0,z1 in [(8,105),(113,207)]:box('Window pane',((x0+x1)/2,3,(z0+z1)/2),(x1-x0,.5,z1-z0),'HFP_Glass')
export('SM_HFP_LandingWindowGlass')
reset();rod('Window guard',(-107,-14,68),(107,-14,68),2.8,'HFP_BlackSteel',32)
for x in [-97,97]:
 rod('Guard elbow',(x,-14,68),(x,-2,68),2,'HFP_BlackSteel',24);rod('Round mounting plate',(x,-2,68),(x,0,68),5,'HFP_BlackSteel',32)
 for dx,dz in [(-2.8,0),(2.8,0),(0,2.8),(0,-2.8)]:ell('Mount screw',(x+dx,-2.1,68+dz),(1,.4,1),'HFP_Chrome')
export('SM_HFP_LandingGuard',True)
# Timber park bench. All stages retain supporting legs and persistent broken silhouette.
for stage in range(4):
 reset()
 for x in [-105,105]:
  for y in [-41,39]:
   box('Bench leg',(x,y,44),(10,10,88),'HFP_PorchTimber',1.4)
  box('Seat bearer',(x,0,84),(10,113,11),'HFP_PorchTimber',1.5)
  box('Back upright',(x,44,113),(10,10,86),'HFP_PorchTimber',1.4)
  rod('Brace',(x,-39,18),(x,38,78),4,'HFP_PorchTimber')
 box('Underseat stretcher',(0,31,40),(216,8,10),'HFP_PorchTimber',1)
 for j in range(5):
  y=-43+j*22
  if stage>=2 and j in [1,3]:
   for x0,x1 in [(-140,-22),(20,140)]:box('Split seat plank',((x0+x1)/2,y,91),(x1-x0,19,7),'HFP_PorchTimberRaw',1)
  else:box('Seat plank',(0,y,91),(280,19,7),'HFP_PorchTimber',1.2)
 for j in range(4):
  z=108+j*14
  if stage==3 and j in [1,2]:
   box('Broken back slat',(-79,46,z),(122,6,11),'HFP_PorchTimberRaw',.8);box('Broken back slat',(96,46,z),(88,6,11),'HFP_PorchTimberRaw',.8)
  else:box('Back plank',(0,46,z),(280,6,11),'HFP_PorchTimber',1)
 for x in [-106,106]:
  box('Arm support',(x,-32,114),(7,7,44),'HFP_PorchTimber',1)
  box('Armrest',(x,4,137),(15,104,7),'HFP_PorchTimber',2)
  for z in [108,122,136,150]:rod('Back bolt',(x,42.5,z),(x,41.5,z),1,'HFP_BlackSteel')
  for y in [-43,-21,1,23,45]:rod('Seat bolt',(x,y,94.6),(x,y,95.1),.85,'HFP_BlackSteel')
 if stage:
  for i in range(7+stage*5):
   x=random.uniform(-125,125);y=random.choice([-43,-21,1,23]);length=random.uniform(10,35)
   curve('Wood crack',[(x,y,94.6),(x+length/2,y+.5,94.6),(x+length,y-.3,94.6)],.12+stage*.04,'HFP_DarkCrack')
 export(['SM_HFP_Bench','SM_HFP_Bench_Damaged','SM_HFP_Bench_Heavy','SM_HFP_Bench_Broken'][stage],True)
# Brown leather club chair and separate loose seat cushion.
for stage in range(4):
 reset()
 for x in [-64,64]:
  for y in [-60,55]:box('Walnut foot',(x,y,10),(14,14,20),'HFP_Walnut',2)
 box('Chair lower base',(0,0,32),(171,161,40),'HFP_Leather',8)
 box('Seat support',(0,-5,57),(124,134,14),'HFP_Leather',5)
 for x in [-74,74]:
  box('Upholstered arm',(x,0,86),(32,170,78),'HFP_Leather',15)
  ell('Rolled arm',(x,-4,121),(35,170,27),'HFP_Leather')
  # Piping follows rounded front panel, rather than disconnected lines.
  curve('Front arm piping',[(x-13,-86,120),(x-15,-86,112),(x-15,-86,55),(x,-87,50),(x+15,-86,55),(x+15,-86,112),(x+13,-86,120)],.6,'HFP_LeatherSeam')
 box('Tall chair back',(0,62,102),(149,42,116),'HFP_Leather',12)
 o=box('Stuffed back cushion',(0,35,126),(117,35,65),'HFP_Leather',14);o.rotation_euler.x=math.radians(-5)
 curve('Back cushion welt',[(-49,12,96),(-56,12,101),(-56,12,146),(-48,12,154),(48,12,154),(56,12,146),(56,12,101),(49,12,96),(-49,12,96)],.7,'HFP_LeatherSeam')
 for x in [-74,74]:
  for i in range(7):
   z=59+i*8;curve('Leather arm creases',[(x-8,-87,z),(x,-88,z+1),(x+7,-87,z)],.1,'HFP_LeatherSeam')
 if stage:
  for i in range(stage*5):
   x=random.uniform(-49,49);z=random.uniform(90,145)
   curve('Leather tear',[(x,16,z),(x+3,14,z-6),(x-1,16,z-13)],.25*stage,'HFP_DarkCrack')
 if stage>=2:
  box('Exposed cushion filling',(25,14,105),(18,1,16),'HFP_Foam',2)
 if stage==3:
  box('Exposed back frame',(15,12,127),(63,7,5),'HFP_PorchTimberRaw',1)
 export(['SM_HFP_Chair','SM_HFP_Chair_Damaged','SM_HFP_Chair_Heavy','SM_HFP_Chair_Broken'][stage],True)
for stage in range(4):
 reset();box('Loose seat cushion',(0,0,12),(114,110,24),'HFP_Leather',8)
 # Subtle tuft depressions are supported by the leather shader, stitching by geometry.
 for z in [5,20]:curve('Seat cushion welt',[(-49,-56,z),(49,-56,z),(57,-48,z),(57,48,z),(49,56,z),(-49,56,z),(-57,48,z),(-57,-48,z),(-49,-56,z)],.55,'HFP_LeatherSeam')
 if stage:
  for i in range(stage*3):
   x=random.uniform(-45,45);y=random.uniform(-50,50);curve('Split leather',[(x,y,24),(x+4,y+9,24),(x-1,y+14,24)],.22*stage,'HFP_DarkCrack')
 if stage>=2:box('Cushion exposed stuffing',(15,16,24),(18,23,1.2),'HFP_Foam',2)
 export(['SM_HFP_SeatCushion','SM_HFP_SeatCushion_Damaged','SM_HFP_SeatCushion_Heavy','SM_HFP_SeatCushion_Broken'][stage],True)
# Flush grates, transition strip and rubber doormat.
reset();box('Drain recess',(0,0,.1),(26,26,.2),'HFP_BlackSteel',.2)
for x in [-12,12]:box('Drain rim',(x,0,.3),(1.8,26,.4),'HFP_Chrome',.15)
for y in [-12,12]:box('Drain rim',(0,y,.3),(26,1.8,.4),'HFP_Chrome',.15)
for t in range(-10,11,4):
 box('Drain grille',(t,0,.4),(.7,22,.5),'HFP_Chrome',.1);box('Drain grille',(0,t,.4),(22,.7,.5),'HFP_Chrome',.1)
export('SM_HFP_FloorGrate')
reset();box('Transition',(0,0,.3),(7,174,.6),'HFP_DarkBronze',.25);export('SM_HFP_WoodThreshold')
reset();box('Rubber doormat',(0,0,.55),(90,130,1.1),'HFP_Rubber',.5)
for i in range(-11,12):
 for j in range(-8,9):
  x=i*3.6;y=j*7
  if abs(x)<40 and abs(y)<59:
   o=box('Herringbone ridge',(x,y,.98),(2.5,6,.35),'HFP_Rubber',.13);o.rotation_euler.z=math.radians(30 if i%2 else-30)
export('SM_HFP_RubberMat')
json.dump(plans,open(B/'detail_plan.json','w'),indent=2)
print('HFP_DETAIL_BUILD_PASS',len(plans))
