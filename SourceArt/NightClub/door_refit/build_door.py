"""Photo matched club entrance art. Blender only; never edits Unreal/map.

Final-size hinge pivot; local Unreal +X leaf, hallway face Unreal -Y.
Reuses the established Room101Entry mesh helpers and worn paint textures.
"""
import bpy,bmesh,os,math,json,collections,random,shutil
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parent;MODEL=str(R/'models');TEX=R/'textures'
Path(MODEL).mkdir(parents=True,exist_ok=True);TEX.mkdir(exist_ok=True)
OLD=R.parent.parent/'Room101Entry'
for suffix in ('BaseColor','Roughness'):
 shutil.copyfile(OLD/'textures'/('T_R101Entry_OffWhitePaint_'+suffix+'.png'),TEX/('T_NCD101_WornPaint_'+suffix+'.png'))
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
G=collections.defaultdict(list)
manifest={'assets':[],'materials':{},'textures':['T_NCD101_WornPaint_BaseColor.png','T_NCD101_WornPaint_Roughness.png'],
 'coordinateSystem':'Blender metres to Unreal cm [100X,-100Y,100Z]',
 'placementScale':1.0,'hingeOriginCm':[0,0,0],'panelCenterLocalCm':[79,0,109.5],
 'panelHalfExtentLocalCm':[79,3,109.5],'widthCm':158,'heightCm':219,
 'hallwayFace':'Unreal local -Y. Closed yaw -90 means hallway world -X.',
 'collision':'ExistingHouseDoor movable panel box only; visual leaf NoCollision.',
 'notes':['True recessed six panels both sides. Black keypad and silver lever on hallway face.',
          'Black serif 101 on both sides. Warm worn white paint, quiet bottom kick scuffs and small chips.',
          'All leaf details joined so they rotate with original door hinge. No emissive CLUB label.']}
def mat(name,c,rough=.6,metal=0,texture=False):
 m=bpy.data.materials.new('M_NCD101_'+name);m.diffuse_color=(*c,1);m.use_nodes=True
 p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 row={'color':list(c),'roughness':rough,'metallic':metal,'uv_scale':1}
 if texture:
  bp=str(TEX/'T_NCD101_WornPaint_BaseColor.png');rp=str(TEX/'T_NCD101_WornPaint_Roughness.png')
  tx=m.node_tree.nodes.new('ShaderNodeTexImage');tx.image=bpy.data.images.load(bp);m.node_tree.links.new(tx.outputs['Color'],p.inputs['Base Color'])
  rt=m.node_tree.nodes.new('ShaderNodeTexImage');rt.image=bpy.data.images.load(rp);rt.image.colorspace_settings.name='Non-Color';m.node_tree.links.new(rt.outputs['Color'],p.inputs['Roughness'])
  row.update(base_texture=bp,roughness_texture=rp)
 manifest['materials'][m.name]=row;return m
PAINT=mat('WarmWornWhite',(.74,.72,.66),.64,texture=True)
STEEL=mat('BrushedSilver',(.46,.49,.51),.28,.95)
DARK=mat('KeypadHousing',(.010,.012,.014),.42)
KEY=mat('RubberKeys',(.037,.043,.047),.68)
LABEL=mat('KeyLegends',(.68,.71,.72),.55)
SCREW=mat('DarkHardware',(.023,.026,.029),.40,.7)
NUMBER=mat('Black101',(.004,.005,.006),.55)
CHIP=mat('PaintChip',(.42,.35,.27),.94)
SRC=(OLD/'door_assets.py').read_text()
exec(SRC[SRC.index('def mesh('):SRC.index('# Door structural slab')])
exec(SRC[SRC.index('def finish(name,objects):'):SRC.index('# Separate101/102leaves')])
g='SM_NCD101_PhotoDoor'
W=1.58;H=2.19
xgrid=[0,.16,.70,.88,1.42,W];zgrid=[0,.16,.82,.94,1.61,1.73,2.055,H]
v=[];f=[]
for side in(-1,1):
 off=len(v)
 for z in zgrid:
  for x in xgrid:v.append((x,side*.025,z))
 for j in range(len(zgrid)-1):
  for i in range(len(xgrid)-1):
   if i in(1,3) and j in(1,3,5):continue
   a=off+j*len(xgrid)+i;q=(a,a+1,a+1+len(xgrid),a+len(xgrid));f.append(q if side<0 else q[::-1])
for x in(0,W):
 for j in range(len(zgrid)-1):
  z0,z1=zgrid[j:j+2];a=len(v);v.extend([(x,-.025,z0),(x,.025,z0),(x,.025,z1),(x,-.025,z1)]);f.append((a,a+1,a+2,a+3))
for z in(0,H):
 for i in range(len(xgrid)-1):
  x0,x1=xgrid[i:i+2];a=len(v);v.extend([(x0,-.025,z),(x1,-.025,z),(x1,.025,z),(x0,.025,z)]);f.append((a,a+1,a+2,a+3))
o=mesh('Painted stiles and cross rails',v,f,PAINT,g);o['weighted']=True
for side in(-1,1):
 for x0,x1 in((.16,.70),(.88,1.42)):
  for z0,z1 in((.16,.82),(.94,1.61),(1.73,2.055)):
   v=[];f=[]
   for inset,depth in((0,.025),(.006,.027),(.021,.014),(.028,.008)):
    v.extend([(x0+inset,side*depth,z0+inset),(x1-inset,side*depth,z0+inset),(x1-inset,side*depth,z1-inset),(x0+inset,side*depth,z1-inset)])
   for k in range(3):
    for j in range(4):
     q=(k*4+j,k*4+(j+1)%4,(k+1)*4+(j+1)%4,(k+1)*4+j);f.append(q if side<0 else q[::-1])
   f.append((12,13,14,15) if side<0 else (15,14,13,12))
   o=mesh('Recessed painted panel and profiled moulding',v,f,PAINT,g);o['weighted']=True
for x in(.002,W-.002):cube('Eased vertical leaf edge',(x,0,H/2),(.004,.050,H),PAINT,g,.0018,3)
for z in(.002,H-.002):cube('Eased upper lower leaf edge',(W/2,0,z),(W-.008,.050,.004),PAINT,g,.0018,3)
# Numeric keypad on hallway side (Blender +Y). Existing club latch is at the +X free edge.
LX=1.465;LZ=1.035
cube('Tall black electronic lock housing',(LX,.042,1.155),(.092,.033,.365),DARK,g,.009,5)
cube('Numeric rubber keypad bed',(LX,.061,1.243),(.079,.010,.127),KEY,g,.005,4)
for row,digits in enumerate(('123','456','789','*0#')):
 for col,digit in enumerate(digits):
  x=LX+(col-1)*.024;z=1.279-row*.025
  cube('Rounded rubber keypad '+digit,(x,.069,z),(.019,.008,.019),KEY,g,.0026,3)
  d=bpy.data.curves.new('Key numeral '+digit,'FONT');d.body=digit;d.size=.012;d.align_x='CENTER';d.align_y='CENTER';d.extrude=.00008;d.resolution_u=3
  o=bpy.data.objects.new('Key numeral '+digit,d);bpy.context.collection.objects.link(o);o.location=(x,.074,z);o.rotation_euler=(math.pi/2,0,math.pi);d.materials.append(LABEL)
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH');G[g].append(bpy.context.object)
cube('Small status window',(LX,.061,1.325),(.030,.003,.008),SCREW,g,.002,3)
cyl('Mechanical key cylinder',(LX,.061,.998),.011,.008,STEEL,g,'Y',28)
cube('Cylinder key slot',(LX,.067,.998),(.0025,.001,.012),SCREW,g)
for side in(-1,1):
 if side<0:cube('Interior black latch escutcheon',(LX,-.038,LZ),(.074,.026,.197),DARK,g,.006,4)
 cyl('Circular satin silver lever hub',(LX,side*.065,LZ),.027,.023,STEEL,g,'Y',32)
 pts=[]
 for j in range(20):
  t=j/19;pts.append((LX-.153*t,side*(.079+.024*math.sin(math.pi*t/2)),LZ+.012*math.sin(math.pi*t*.8)))
 tube('Smooth brushed silver inward lever',pts,.012,STEEL,g,20)
 for z in(LZ-.017,LZ+.017):cyl('Small lever mount screw',(LX,side*.079,z),.0034,.002,SCREW,g,'Y',12)
for z in(.26,1.095,1.93):
 cyl('Hinge interlocking knuckle',(0,0,z),.0115,.115,SCREW,g,'Z',28)
 for zz in(z-.058,z+.058):cyl('Hinge silver pin cap',(0,0,zz),.0125,.004,STEEL,g,'Z',24)
 cube('Hinge door leaf plate',(.025,0,z),(.050,.046,.095),SCREW,g,.002,3)
cube('Free edge latch face',(W+.001,0,LZ),(.002,.025,.096),STEEL,g,.001)
cube('Small latch tongue',(W+.003,0,LZ),(.004,.016,.027),STEEL,g,.001)
font=bpy.data.fonts.load('/System/Library/Fonts/Supplemental/Times New Roman Italic.ttf')
for side in(-1,1):
 d=bpy.data.curves.new('Black serif room 101','FONT');d.body='1 0 1';d.font=font;d.size=.175;d.align_x='CENTER';d.align_y='CENTER';d.extrude=.00035;d.resolution_u=6
 o=bpy.data.objects.new('Black individually spaced 101',d);bpy.context.collection.objects.link(o);o.location=(.79,side*.0278,1.666);o.rotation_euler=(math.pi/2,0,0) if side<0 else(math.pi/2,0,math.pi);d.materials.append(NUMBER)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH');G[g].append(bpy.context.object)
# Paint chips kept on solid rails so none float above the recesses.
rr=random.Random(101)
for x,z,rx,rz in((.79,1.37,.007,.013),(1.49,.55,.005,.008),(.12,.32,.006,.011),(.81,.89,.005,.008)):
 pts=[]
 for j in range(9):
  a=j*math.tau/9;s=rr.uniform(.65,1.13);pts.append((x+rx*math.cos(a)*s,.0257,z+rz*math.sin(a)*s))
 mesh('Small irregular chipped paint',pts,[tuple(range(9))],CHIP,g)
# UV all geometry in canvas coordinates before origin/transform baking.
for o in G[g]:
 uv=o.data.uv_layers.new(name='UVMap')
 for poly in o.data.polygons:
  for li in poly.loop_indices:
   q=o.matrix_world@o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(q.x/W,q.z/H)
leaf=finish(g,G[g]);manifest['assets'][0].update(origin='Hinge floor local(0,0,0); scale1 in map',front='Hallway Unreal-Y; leaf +X; hinge +Z')
json.dump(manifest,open(R/'door_manifest.json','w'),indent=2)
bpy.ops.wm.save_as_mainfile(filepath=str(R/'PhotoDoor101_source.blend'))
# Independent neutral product render, the hallway face only.
ground=mat('PreviewGround',(.19,.18,.17),.88)
cube('Preview floor',(.8,0,-.04),(9,8,.08),ground,'__PREVIEW__',.001)
sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=48;sc.render.resolution_x=1000;sc.render.resolution_y=1100;sc.render.resolution_percentage=100
sc.world.color=(.13,.13,.13);sc.view_settings.view_transform='AgX'
for loc,power,size in(((1.0,3.5,3.8),650,3),((-2,1.5,2.6),450,2),((2,-2,3),450,2)):
 d=bpy.data.lights.new('Soft studio','AREA');d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new('Soft studio',d);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((.79,0,1.1))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Hallway door preview');o=bpy.data.objects.new('Hallway door preview',d);bpy.context.collection.objects.link(o);o.location=(-1.5,5.5,2.1);o.rotation_euler=(Vector((.79,0,1.095))-o.location).to_track_quat('-Z','Y').to_euler();d.lens=70;sc.camera=o;sc.render.filepath=str(R/'door-preview.png');bpy.ops.render.render(write_still=True)
print('NCD101_DOOR_ASSET_PASS triangles='+str(manifest['assets'][0]['triangles']))
