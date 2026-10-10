import bpy,runpy,json,math,random
from pathlib import Path
from mathutils import Vector,Matrix
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ShelfFireplaceRefinement');O=R/'models';O.mkdir(exist_ok=True)
BASE='/Game/SeniorSendOff/House/FireplaceRefinement';dm=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/generate_damage_v2.py');dm['export_stage'].__globals__['OUT']=str(O)
lib=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BasementLibrary/manifest.json');d=json.loads(lib.read_text());row=next(a for a in d['assets']if a['id']=='SixShelfBookcase')
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=row['intact_fbx']);ob=next(o for o in bpy.context.scene.objects if o.type=='MESH'and not o.name.startswith('UCX_'));bpy.context.view_layer.objects.active=ob;ob.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
for definition in dm['DEFS']:
 m=bpy.data.materials.new(definition['asset_name']);m.diffuse_color=(*definition['base_color'],1);ob.data.materials.append(m)
mats=list(ob.data.materials);parts=[];rng=random.Random(621)
for i,c in enumerate(dm['S']['components'](ob)):
 part=dm['part_mesh'](ob,c['faces']);lo,hi=dm['bbox'](part);size=hi-lo
 cap=len(row['material_slots'])
 fragments=dm['cut_pair'](part,*dm['fracture_plane'](part,i,.37+(i%3)*.13),cap)if max(size)>.55 else[dm['copy'](part)]
 dm['discard'](part)
 for j,f in enumerate(fragments):
  center=(dm['bbox'](f)[0]+dm['bbox'](f)[1])*.5
  dm['floor_part'](f,Vector((center.x*.70,center.y*.55,0)),0,rng,.13)
  center=(dm['bbox'](f)[0]+dm['bbox'](f)[1])*.5
  angle=math.radians(rng.uniform(-17,17));rot=Matrix.Rotation(angle,3,'X')@Matrix.Rotation(math.radians(rng.uniform(-9,9)),3,'Y')
  for v in f.data.vertices:v.co=rot@(v.co-center)+center
  lo,hi=dm['bbox'](f);level=.02+(i%5)*.047+(j*.022)
  for v in f.data.vertices:v.co.z+=level-lo.z
  parts.append(f)
name='SM_Lib_SixShelfBookcase_BrokenV3';file,audit=dm['export_stage'](parts,name,mats)
shelf=dict(row,broken_fbx=file,broken_asset_name=name,broken_material_slots=row['material_slots']+[dm['DEST']+x['asset_name']+'.'+x['asset_name']for x in dm['DEFS']],broken_material_names=[m.name for m in mats],broken_audit=audit)
# Remove only the old fireplace parts; keep every other architectural face.
inv={a['label']:a for a in json.loads((R/'inventory.json').read_text())};retained=[]
remove={'SM_Entrance_p0_m1_Solid':list(range(63,78))+[22,92,93,97,98,99,100,101,3,4], 'SM_Entrance_p0_p0_Solid':list(range(2,13))+[64,65,220,231,232,240,241,242,243,244], 'SM_Entrance_m1_p0_Solid':[7,8,9,10]}
for label,indices in remove.items():
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(R/'exports'/(label+'.fbx')));ob=next(o for o in bpy.context.scene.objects if o.type=='MESH'and not o.name.startswith('UCX_'));bpy.context.view_layer.objects.active=ob;ob.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 cs=dm['S']['components'](ob);excluded=set(f for i in indices for f in cs[i]['faces']);keep=[p.index for p in ob.data.polygons if p.index not in excluded];new,used=dm['S']['submesh'](ob,keep,'SM_FPR_Retained_'+label)
 mats0=inv[label]['components'][0]['materials'];slots=[mats0[i]for i in used]
 bpy.ops.object.select_all(action='DESELECT');new.select_set(True);bpy.context.view_layer.objects.active=new;f=O/(new.name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(f),use_selection=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
 assert len(keep)+len(excluded)==len(ob.data.polygons)
 retained.append(dict(label=label,asset_name=new.name,intact_fbx=str(f),material_names=[m.name for m in new.data.materials],material_slots=slots,faces_before=len(ob.data.polygons),faces_retained=len(keep),fireplace_faces_replaced=len(excluded)))
# Detailed matching classic painted mantels, stone hearth, brick firebox and grate.
bpy.ops.wm.read_factory_settings(use_empty=True);materials={};pieces=[];hulls=[]
def mat(n,c,rough=.6,metal=0,emit=0):
 m=bpy.data.materials.new(n);m.diffuse_color=(*c,1);m.roughness=rough;m.metallic=metal;materials[n]=dict(base_color=c,roughness=rough,metallic=metal,emission=emit);return m
cream=mat('M_FPR_WarmPaint',[.74,.705,.61],.46);stone=mat('M_FPR_HonedStone',[.105,.115,.12],.68);brick=mat('M_FPR_SootBrick',[.085,.061,.045],.93);iron=mat('M_FPR_BlackIron',[.018,.022,.024],.48,.75);wood=mat('M_FPR_CharredOak',[.025,.016,.008],.98);bark=mat('M_FPR_BarkEdge',[.065,.039,.018],.97);ember=mat('M_FPR_Embers',[.8,.09,.009],.9,0,1.6)
def box(n,p,s,m=None,bevel=0,collision=False):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.name=n;o.scale=s;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 if m:o.data.materials.append(m)
 if bevel:
  b=o.modifiers.new('Rounded crafted edges','BEVEL');b.width=bevel;b.segments=3;bpy.ops.object.modifier_apply(modifier=b.name)
  for f in o.data.polygons:f.use_smooth=False
 (hulls if collision else pieces).append(o);return o
def rod(n,a,b,r,m,vertices=12):
 a,b=Vector(a),Vector(b);bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=(b-a).length,location=(a+b)*.5);o=bpy.context.object;o.name=n;o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);o.data.materials.append(m)
 for f in o.data.polygons:f.use_smooth=len(f.vertices)==4
 pieces.append(o);return o
box('Honed stone hearth',(0,-.58,.075),(4.48,1.35,.15),stone,.015)
box('Firebox back',(0,-.10,.65),(3.00,.12,1.07),brick,.005)
for side in [-1,1]:
 x=side*1.72
 box('Mantel pilaster',(x,-.40,.72),(.46,.43,1.44),cream,.012)
 box('Pilaster foot',(x,-.405,.16),(.56,.52,.30),cream,.010)
 box('Pilaster capital',(x,-.41,1.28),(.57,.53,.14),cream,.008)
 for xx in [x-.13,x,x+.13]:box('Reeded pilaster detail',(xx,-.628,.79),(.026,.024,.80),cream,.007)
 box('Firebox side',(side*1.445,-.29,.63),(.10,.37,1.04),brick,.006)
box('Mantel entablature',(0,-.37,1.245),(3.95,.45,.28),cream,.01)
for x in [-.95,0,.95]:
 box('Raised panel',(x,-.606,1.245),(.78,.026,.16),cream,.007)
 for z in [1.158,1.33]:box('Panel molding',(x,-.622,z),(.82,.012,.016),cream,.004)
for z,w,dep,h in [(1.395,4.31,.67,.08),(1.472,4.47,.77,.08),(1.54,4.608,.86,.055)]:box('Layered mantel cornice',(0,-dep*.5,z),(w,dep,h),cream,.008)
for i in range(30):box('Mantel dentil',(i*.132-1.914,-.715,1.40),(.063,.067,.048),cream,.003)
# Recessed, staggered brick faces with real mortar gaps.
for rowi in range(8):
 for k in range(13):
  x=-1.48+k*.235+(rowi%2)*.1175
  if x>1.43:continue
  box('Sooted firebrick',(x,-.171,.205+rowi*.117),(.226,.025,.108),brick,.002)
# Cast iron basket and natural bark ridges on irregular charred logs.
for x in [-.85,.85]:
 for y in [-.33,-.57]:rod('Grate leg',(x,y,.15),(x,y,.30),.018,iron)
for x in [-.96+i*.16 for i in range(13)]:rod('Grate rail',(x,-.65,.30),(x,-.27,.30),.014,iron)
for y in [-.27,-.65]:rod('Basket rim',(-1.04,y,.30),(1.04,y,.30),.025,iron)
for x in [-1.04,1.04]:rod('Grate upstand',(x,-.66,.30),(x,-.66,.49),.024,iron)
rng=random.Random(346)
for i in range(5):
 x=-.82+i*.41;a=(x,-.60,.34+(i%2)*.05);b=(x+rng.uniform(-.10,.10),-.23,.40+(i%2)*.06);rod('Charred oak log',a,b,.08,wood,20)
 for k in range(5):
  ang=k*math.tau/5;off=Vector((math.cos(ang)*.077,0,math.sin(ang)*.077));rod('Raised bark seam',Vector(a)+off,Vector(b)+off,.008,bark,7)
for i in range(20):box('Warm coal',(-.90+rng.random()*1.8,-.61+rng.random()*.35,.182),(.025+rng.random()*.033,.027,.018),ember,.006)
# Fitted solid collision includes the opening, keeping capsules out of the firebox.
for p,s in [((0,-.58,.075),(4.48,1.35,.15)),((0,-.28,.72),(3.99,.57,1.44)),((0,-.43,1.51),(4.61,.86,.16))]:box('collision',p,s,collision=True)
name='SM_FPR_ClassicFireplace';mats=list(bpy.data.materials)
for piece in pieces:
 original=piece.data.materials[0];index=mats.index(original);piece.data.materials.clear()
 for material in mats:piece.data.materials.append(material)
 for polygon in piece.data.polygons:polygon.material_index=index
mesh=dm['join_meshes'](pieces,name,mats)
for i,h in enumerate(hulls):h.name='UCX_'+name+'_%02d'%i
bpy.ops.object.select_all(action='DESELECT');mesh.select_set(True)
for h in hulls:h.select_set(True)
bpy.context.view_layer.objects.active=mesh;path=O/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
fp=dict(asset_name=name,intact_fbx=str(path),material_names=[m.name for m in mats],material_slots=[BASE+'/'+m.name+'.'+m.name for m in mats],faces=len(mesh.data.polygons),placements=[dict(label='GreatRoom_Fireplace_West',location=[-1158,189.9,0],yaw=-90),dict(label='GreatRoom_Fireplace_East',location=[1257,169.2,0],yaw=90)])
(R/'models-manifest.json').write_text(json.dumps(dict(shelf=shelf,retained=retained,fireplace=fp,materials=materials),indent=2));print('SHELF_FIREPLACE_MODELS_READY',audit,flush=True)
