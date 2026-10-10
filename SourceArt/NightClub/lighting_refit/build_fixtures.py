"""Compact club fixture art, offline Blender only. No Unreal/map/light writes.
Lens/beam optical exit origin(0,0,0), forward Unreal+X = Blender+X.
"""
import bpy,bmesh,os,json,math,collections
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parent;MODEL=R/'models';MODEL.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
G=collections.defaultdict(list)
MAN={'coordinateSystem':'Blender metres to Unrealcm [100X,-100Y,100Z]. Optical axis Unreal+X; lens exit at local0.',
 'assets':[],'materials':{},'notes':['No real lights; all objects visual NoCollision.',
 'Bind moving optical head and beam to existing spotlight transform; both have origin at lens exit.',
 'Ceiling mount/yoke uses matching optical basis, plate50cm behind lens; keep this part static.',
 'Moving head has dedicated dynamic emission lens material slot. Strobe uses separate lens slot.',
 'Beam is open at both ends,32sides,100cm long with near radius3cm and far26cm. Scale X to set length.',
 'Preserve cheap meshes, no ray tracing/distance field/GI or dynamic shadows from fixtures/beam.']}
D='/Game/SeniorSendOff/House/NightClub/'
def mat(name,c,rough=.5,metal=0,emission=0,reuse=None):
 m=bpy.data.materials.new(name);m.diffuse_color=(*c,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 if emission:p.inputs['Emission Color'].default_value=(*c,1);p.inputs['Emission Strength'].default_value=emission
 row={'color':list(c),'roughness':rough,'metallic':metal,'emissive_strength':emission,'emissive':list(c) if emission else[0,0,0]}
 if reuse:row['reuse_asset']=reuse
 MAN['materials'][name]=row;return m
BLACK=mat('M_NC_PowderCoat',(.009,.011,.019),.48,.52,reuse=D+'M_NC_PowderCoat')
ALU=mat('M_NC_BrushedAluminium',(.28,.31,.37),.25,.95,reuse=D+'M_NC_BrushedAluminium')
CHROME=mat('M_NC_Chrome',(.62,.67,.73),.14,1,reuse=D+'M_NC_Chrome')
RUBBER=mat('M_NC_Rubber',(.005,.006,.008),.88,reuse=D+'M_NC_Rubber')
LENS=mat('M_NCLF_DynamicOpticalLens',(.11,.48,.70),.15,.02,4)
STROBE=mat('M_NCLF_DynamicStrobeLens',(.68,.75,.85),.20,0,3)
BEAM=mat('M_NCLF_DynamicBeam',(.06,.32,.55),.8,0,.8)
MAN['materials'][BEAM.name].update(unlit=True,blend_mode='additive',opacity=.04,two_sided=True,depth_fade_cm=25,dynamic_parameters=['BeamColor','BeamStrength'])
MAN['materials'][LENS.name].update(dynamic_parameters=['LensColor','LensStrength'])
MAN['materials'][STROBE.name].update(dynamic_parameters=['LensColor','LensStrength'])
SRC=(R.parent/'assets/club_assets.py').read_text();exec(SRC[SRC.index('def mesh('):SRC.index('# Neon stroke font')])
def optical_ring(name,x,inner,outer,h,ma,g,sides=40):
 o=ring(name,(0,0,0),inner,outer,h,ma,g,sides);o.location.x=x;o.rotation_euler.y=math.pi/2;return o
def screws_x(x,positions,g):
 for y,z in positions:
  cyl('Recessed hex fixing head',(x,y,z),.0032,.0025,ALU,g,12,'X')
  cube('Fine fastening head slot',(x+.002,y,z),(.001,.004,.0007),RUBBER,g)

# Optical head: compact rounded chassis, vent grilles, concentric real lens ring.
g='SM_NCLF_MovingOpticalHead'
cube('Rounded black moving-head chassis',(-.123,0,0),(.246,.210,.240),BLACK,g,.035,5)
cube('Rear service cap',(-.250,0,0),(.018,.187,.206),BLACK,g,.017,4)
cube('Upper service metal rim',(-.15,0,.109),(.142,.174,.018),ALU,g,.006,3)
cube('Dark upper cooling grille bed',(-.15,0,.120),(.132,.159,.007),RUBBER,g,.005,3)
for i in range(12):cube('Precise upper cooling vent louvre',(-.21+i*.011,0,.126),(.004,.149,.003),BLACK,g,.0013,2)
for side in(-1,1):
 cube('Side cooling grille inset',(-.181,side*.105,0),(.102,.007,.145),RUBBER,g,.009,3)
 for i in range(9):cube('Side intake vent louvre',(-.219+i*.010,side*.110,0),(.004,.003,.130),BLACK,g,.0012,2)
 cyl('Yoke pivot outer axle',(-.124,side*.110,0),.031,.026,ALU,g,32,'Y')
 cyl('Yoke pivot black centre',(-.124,side*.124,0),.022,.008,BLACK,g,28,'Y')
 cyl('Pivot hex shaft',(-.124,side*.130,0),.008,.004,ALU,g,6,'Y')
optical_ring('Satin aluminium lens bezel',-.010,.056,.075,.012,ALU,g,48)
optical_ring('Black focus barrel',-.015,.044,.058,.024,BLACK,g,48)
optical_ring('Fine chrome focus index ring',-.001,.050,.054,.005,CHROME,g,48)
# Lens is a shallow convex optical surface, actual exit centre atX0.
n=40;v=[(0,0,0)];f=[]
for j in range(n):a=j*math.tau/n;v.append((-.003,.049*math.cos(a),.049*math.sin(a)))
for j in range(n):f.append((0,j+1,(j+1)%n+1))
mesh('Dedicated luminous optical exit',v,f,LENS,g,True)
for a in(0,math.pi/2,math.pi,math.pi*1.5):
 y=.065*math.cos(a);z=.065*math.sin(a);cyl('Lens bezel screw',(-.002,y,z),.003,.003,ALU,g,12,'X')
screws_x(-.260,[(-.066,-.074),(.066,-.074),(-.066,.074),(.066,.074)],g)
cube('Rear connector plate',(-.262,0,-.028),(.006,.073,.042),ALU,g,.004,3)
cyl('Rear power inlet',(-.267,-.021,-.028),.010,.013,RUBBER,g,16,'X')
cyl('Rear data port',(-.267,.018,-.028),.008,.013,RUBBER,g,16,'X')
# Thin safety marking at the back is a quiet physical detail, not glowing text.
cube('Caution plate',(-.261,0,.055),(.002,.064,.019),ALU,g,.001,2)

# Fixed mount/yoke, same optical basis: plate behind head attaches above after downrotation.
g='SM_NCLF_StaticCeilingMount'
cube('Ceiling rectangular mounting shoe',(-.494,0,0),(.024,.30,.22),BLACK,g,.012,4)
cube('Satin mounting plate rim',(-.508,0,0),(.007,.281,.198),ALU,g,.006,3)
for y in(-.117,.117):
 for z in(-.077,.077):cyl('Mount fixing socket',(-.516,y,z),.009,.007,BLACK,g,20,'X')
cyl('Short ceiling stem',(-.397,0,0),.031,.177,BLACK,g,32,'X')
optical_ring('Stem silver lock collar',-.329,.031,.044,.016,ALU,g,32)
cube('Yoke central bridge',(-.304,0,0),(.055,.286,.077),BLACK,g,.012,4)
for side in(-1,1):
 cube('Smooth sturdy yoke arm',(-.212,side*.142,0),(.211,.031,.085),BLACK,g,.017,5)
 cyl('Yoke fixed pivot bracket',(-.124,side*.140,0),.041,.030,BLACK,g,36,'Y')
 cyl('Yoke pivot satin washer',(-.124,side*.160,0),.030,.011,ALU,g,28,'Y')
 cyl('Yoke pivot fixing cap',(-.124,side*.168,0),.015,.006,BLACK,g,24,'Y')
# A real hanging safety loop follows the bracket behind the moving body.
tube('Fixture safety cable',[(-.505,.101,.069),(-.457,.120,.086),(-.405,.115,.080),(-.336,.091,.047),(-.309,.090,.032)],.0035,ALU,g,8)

# Dedicated compact twelve LED strobe, localfront+X; six columns, two rows.
g='SM_NCLF_CompactStrobeBar'
cube('Compact black LED bar chassis',(-.032,0,0),(.064,.44,.106),BLACK,g,.012,4)
cube('Satin LED bar face surround',(-.004,0,0),(.012,.422,.094),ALU,g,.009,3)
cube('Black recessed LED cell bed',(.001,0,0),(.008,.400,.081),RUBBER,g,.006,3)
for row,z in enumerate((-.021,.021)):
 for col in range(6):
  y=-.165+col*.066
  o=optical_ring('Individual LED aluminium reflector',.008,.009,.016,.010,ALU,g,24);o.location.y=y;o.location.z=z
  # Dedicated dynamic strobe slot on actual inset lens quads.
  mesh('Bright strobe lens quad',[(.013,y-.009,z-.009),(.013,y+.009,z-.009),(.013,y+.009,z+.009),(.013,y-.009,z+.009)],[(0,1,2,3)],STROBE,g)
for side in(-1,1):
 cube('Strobe side attachment bracket',(-.032,side*.226,0),(.069,.015,.077),BLACK,g,.007,3)
 cyl('Strobe attachment screw',(-.032,side*.237,0),.008,.008,ALU,g,16,'Y')
for i in range(14):cube('Rear strobe heatsink fin',(-.072,-.195+i*.030,0),(.017,.008,.078),BLACK,g,.003,3)
cube('Strobe rear mounting strip',(-.082,0,0),(.012,.18,.038),ALU,g,.006,3)

# Small translucent beam cone. True open far end so it never adds a bright disk.
g='SM_NCLF_OpenBeamCone';n=32;v=[];f=[]
for x,r in((0,.03),(1,.26)):
 for j in range(n):a=j*math.tau/n;v.append((x,r*math.cos(a),r*math.sin(a)))
for j in range(n):k=(j+1)%n;f.append((j,k,n+k,n+j))
beam=mesh('Open tapered beam shell',v,f,BEAM,g,True)
uv=beam.data.uv_layers.new(name='UVMap')
for j,poly in enumerate(beam.data.polygons):
 tex=[(0,j/n),(0,(j+1)/n),(1,(j+1)/n),(1,j/n)]
 for li,q in zip(poly.loop_indices,tex):uv.data[li].uv=q

OBJS={}
def finish(name,objects):
 for o in objects:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
  bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='FIXED',ngon_method='BEAUTY');bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
  if o.get('weighted'):
   md=o.modifiers.new('Clean softened metal normals','WEIGHTED_NORMAL');md.weight=50;md.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=md.name)
  if not o.data.uv_layers:
   uv=o.data.uv_layers.new(name='UVMap')
   for p in o.data.polygons:
    axis=max(range(3),key=lambda k:abs(p.normal[k]));aa,bb=((1,2),(0,2),(0,1))[axis]
    for li in p.loop_indices:q=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(q[aa],q[bb])
  o.select_set(False)
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0]
 if len(objects)>1:bpy.ops.object.join()
 o=bpy.context.object;o.name=name;o.data.name=name;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 path=str(MODEL/(name+'.fbx'));bpy.ops.export_scene.fbx(filepath=path,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',use_mesh_modifiers=True,bake_space_transform=False,add_leaf_bones=False)
 coords=[vv.co for vv in o.data.vertices];lo=[min(vv[k] for vv in coords)for k in range(3)];hi=[max(vv[k]for vv in coords)for k in range(3)]
 row={'name':name,'fbx':path,'triangles':len(o.data.polygons),'vertices':len(o.data.vertices),'materials':[m.name for m in o.data.materials],
  'boundsUnrealCm':[[100*lo[0],-100*hi[1],100*lo[2]],[100*hi[0],-100*lo[1],100*hi[2]]],
  'dimensionsCm':[(hi[k]-lo[k])*100 for k in range(3)],'origin':'Lens optical exit local0; local Unreal+X forward.','opticalAxisUnreal':[1,0,0],'collision':'NoCollision','supportRayTracing':False,'castShadow':False}
 if name=='SM_NCLF_MovingOpticalHead':row['dynamicLensSlot']=row['materials'].index(LENS.name)
 if name=='SM_NCLF_CompactStrobeBar':row['dynamicLensSlot']=row['materials'].index(STROBE.name)
 if name=='SM_NCLF_OpenBeamCone':row.update(nearRadiusCm=3,farRadiusCm=26,lengthCm=100,openFarEnd=True,openNearEnd=True,uvMap='U=local UnrealX /100cm; V=circumference0..1 with seam')
 MAN['assets'].append(row);o.select_set(False);OBJS[name]=o;return o
for name,objects in list(G.items()):finish(name,objects)
json.dump(MAN,open(R/'fixture_manifest.json','w'),indent=2)
bpy.ops.wm.save_as_mainfile(filepath=str(R/'NightClubLighting_source.blend'))
# Studio QA only; ceiling-hung orientation has local +X aimed down.
for o in OBJS.values():o.hide_render=True
def show(name,loc,down=True):
 o=OBJS[name].copy();o.data=OBJS[name].data;bpy.context.collection.objects.link(o);o.hide_render=False;o.location=loc
 if down:o.rotation_euler.y=math.pi/2
 return o
show('SM_NCLF_StaticCeilingMount',(-.29,0,1.35));show('SM_NCLF_MovingOpticalHead',(-.29,0,1.35))
show('SM_NCLF_CompactStrobeBar',(.34,.03,1.67));show('SM_NCLF_OpenBeamCone',(-.29,0,1.32))
# Make only the preview beam translucent; runtime will use the documented native dynamic material.
p=BEAM.node_tree.nodes.get('Principled BSDF');p.inputs['Alpha'].default_value=.09;BEAM.surface_render_method='DITHERED'
ground=mat('M_NCLF_PreviewGround',(.032,.039,.052),.9)
cube('Preview floor',(0,0,-.027),(7,7,.05),ground,'__PREVIEW__')
cube('Studio ceiling',(0,0,1.88),(2.3,1.4,.08),ground,'__PREVIEW__',.005,3)
sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=48;sc.render.resolution_x=1300;sc.render.resolution_y=1300;sc.render.resolution_percentage=100;sc.world.color=(.08,.08,.08);sc.view_settings.view_transform='AgX'
for loc,power,col,size in(((1,-2,2.8),180,(.65,.78,1),2),((-2,-1,2.3),180,(1,.78,.58),2),((0,2,2),150,(.6,.75,1),1.5)):
 d=bpy.data.lights.new('Soft studio','AREA');d.energy=power;d.color=col;d.shape='DISK';d.size=size;o=bpy.data.objects.new('Soft studio',d);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,1.25))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Fixture showcase');o=bpy.data.objects.new('Fixture showcase',d);bpy.context.collection.objects.link(o);o.location=(2.4,-3.8,1.00);o.rotation_euler=(Vector((0,0,1.35))-o.location).to_track_quat('-Z','Y').to_euler();d.lens=75;sc.camera=o;sc.render.filepath=str(R/'fixture-preview.png');bpy.ops.render.render(write_still=True)
print('NCL_FIXTURES_ASSETS_PASS assets='+str(len(MAN['assets']))+' triangles='+str(sum(a['triangles']for a in MAN['assets'])))
