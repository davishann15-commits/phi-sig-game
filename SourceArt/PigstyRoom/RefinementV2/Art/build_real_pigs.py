import bpy, math, json, random
from pathlib import Path
from mathutils import Vector
O=Path(__file__).parent
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
materials={};parts=[]

def material(name,color,rough=.6,prefix=None,normal=.6):
 m=bpy.data.materials.new(name);m.use_nodes=True;b=m.node_tree.nodes.get('Principled BSDF');b.inputs['Base Color'].default_value=(*color,1);b.inputs['Roughness'].default_value=rough;m.diffuse_color=(*color,1)
 if prefix:
  for suffix,channel in [('BaseColor','Base Color'),('Roughness','Roughness'),('Normal','Normal')]:
   t=m.node_tree.nodes.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(O/'textures'/f'{prefix}_{suffix}.png'))
   if suffix!='BaseColor':t.image.colorspace_settings.name='Non-Color'
   if suffix=='Normal':
    nm=m.node_tree.nodes.new('ShaderNodeNormalMap');nm.inputs['Strength'].default_value=normal;m.node_tree.links.new(t.outputs['Color'],nm.inputs['Color']);m.node_tree.links.new(nm.outputs['Normal'],b.inputs['Normal'])
   else:m.node_tree.links.new(t.outputs['Color'],b.inputs[channel])
 materials[name]=m;return m
material('PIG_M_BlackHide',(.011,.012,.013),.70,'PIG_T_BlackHide',.55)
material('PIG_M_BlackSnout',(.012,.010,.010),.57,'PIG_T_BlackSnout',.45)
material('PIG_M_CharcoalEarInner',(.010,.008,.009),.58,'PIG_T_BlackHide',.5)
material('PIG_M_HoofKeratin',(.009,.008,.007),.47,'PIG_T_HoofKeratin',.5)
material('PIG_M_GlossBlackEye',(.0015,.0017,.0018),.045)
material('PIG_M_NostrilMouth',(.0007,.0006,.0006),.80)
material('PIG_M_BlackBristle',(.004,.0036,.0035),.59)

def apply(o,mod):
 bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
def uv(o):
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.15,island_margin=.008);bpy.ops.object.mode_set(mode='OBJECT')
def assign(o,m):o.data.materials.append(materials[m]);parts.append(o);return o
def ellipsoid(name,center,radii,m,segments=40,rings=24):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,location=center);o=bpy.context.object;o.name=name;o.scale=radii;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for p in o.data.polygons:p.use_smooth=True
 return assign(o,m)
def mesh(name,v,f,m):
 d=bpy.data.meshes.new(name);d.from_pydata(v,[],f);d.update();o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o)
 for p in d.polygons:p.use_smooth=True
 return assign(o,m)
def join(name,pivot=(0,0,0),do_uv=True):
 global parts
 bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name=name;bpy.context.scene.cursor.location=pivot;bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 if do_uv:uv(o)
 parts=[];return o

def union(name,size,target):
 o=join(name,do_uv=False);r=o.modifiers.new('Anatomical continuous hide','REMESH');r.mode='VOXEL';r.voxel_size=size;r.use_smooth_shade=True;apply(o,r)
 sm=o.modifiers.new('Soft anatomical blends','SMOOTH');sm.factor=1.0;sm.iterations=4;apply(o,sm)
 o.data.calc_loop_triangles();t=len(o.data.loop_triangles)
 if t>target:
  de=o.modifiers.new('Realtime organic topology','DECIMATE');de.ratio=target/t;apply(o,de)
 for p in o.data.polygons:p.use_smooth=True
 uv(o);parts.append(o);return o

def tube(name,path,radii,m,sides=8):
 v=[]
 for i,p in enumerate(path):
  p=Vector(p);t=Vector(path[min(i+1,len(path)-1)])-Vector(path[max(0,i-1)]);t.normalize();a=t.cross(Vector((0,0,1)))
  if a.length<.01:a=t.cross(Vector((0,1,0)))
  a.normalize();b=t.cross(a).normalized()
  for j in range(sides):
   angle=2*math.pi*j/sides;v.append(tuple(p+radii[i]*(math.cos(angle)*a+math.sin(angle)*b)))
 f=[]
 for i in range(len(path)-1):
  for j in range(sides):f.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
 f.extend([tuple(range(sides-1,-1,-1)),tuple((len(path)-1)*sides+i for i in range(sides))]);return mesh(name,v,f,m)

# Long deep chest and barrel, pronounced haunches/shoulders rather than spherical primitives.
ellipsoid('Long rib barrel',(-.035,0,.665),(.534,.252,.230),'PIG_M_BlackHide',64,32)
ellipsoid('Muscular rear quarter',(-.353,0,.636),(.235,.246,.229),'PIG_M_BlackHide',48,28)
ellipsoid('Dropped belly',(-.044,0,.479),(.356,.195,.083),'PIG_M_BlackHide',48,24)
ellipsoid('Broad shoulder chest',(.284,0,.613),(.229,.235,.219),'PIG_M_BlackHide',48,28)
ellipsoid('Forward neck base',(.438,0,.673),(.135,.150,.135),'PIG_M_BlackHide',48,24)
# The thighs are a continuous part of the torso above each articulation joint.
for x in [-.35,.35]:
 for s in [-1,1]:ellipsoid('Fleshy shoulder and haunch attachment',(x,s*.188,.516),(.114,.079,.119),'PIG_M_BlackHide',32,20)
body_hide=union('Sculpted black pig body',.0055,11000)
# Soft folds at neck / rear leg flexures, drawn into surface rather than separate dark cords.
for v in body_hide.data.vertices:
 p=v.co
 envelope=math.exp(-((p.x-.415)/.095)**2)*max(0,1-(abs(p.y)-.18)**2/.02)
 if p.z<.75 and abs(p.y)>.15:
  indent=.003*(.5+.5*math.sin((p.x-.39)*150+p.z*30))*envelope
  p.y-=math.copysign(indent,p.y)
body_hide.data.update()
# Full curly tail with an organic taper.
path=[(-.574,0,.68),(-.60,.005,.695),(-.616,.009,.722)]
rr=[.014,.013,.0115]
for i in range(62):
 t=i/61*math.pi*2*1.55;r=.034*(1-.60*i/61);path.append((-.625-.043*i/61,r*math.cos(t),.733+r*math.sin(t)));rr.append(.010*(1-.87*i/61))
tube('Tapering curly tail',path,rr,'PIG_M_BlackHide',12)
# Sparse bristles across back, neck and sides. Thin curves laid along skin, not spikes.
rng=random.Random(57104)
for i in range(350):
 x=rng.uniform(-.47,.37);theta=rng.uniform(-1.25,1.25)
 z=.665+.23*math.sqrt(max(.12,1-((x+.035)/.534)**2))*math.cos(theta);y=.252*math.sqrt(max(.12,1-((x+.035)/.534)**2))*math.sin(theta)
 length=rng.uniform(.005,.018);normal=Vector((0,math.sin(theta),math.cos(theta)))
 p=Vector((x,y,z));tip=p+Vector((-length*.65,0,0))+normal*length*.48;mid=(p+tip)*.5+normal*.001
 tube('Fine sparse black bristle',[tuple(p-normal*.001),tuple(mid),tuple(tip)],[.00036,.00019,.00002],'PIG_M_BlackBristle',3)
# Small paired abdominal papillae, visible in the side-sleeping pose.
for x in [-.28,-.18,-.08,.02,.12,.22]:
 for sign in [-1,1]:
  hit,p,n,index=body_hide.ray_cast(Vector((x,sign*.073,.10)),Vector((0,0,1)))
  if hit:ellipsoid('Natural abdominal skin papilla',(x,sign*.073,p.z-.002),(.0035,.0035,.008),'PIG_M_BlackHide',12,8)
body=join('SM_PIG_BlackBody')

# Continuous wedge-shaped head: sloping forehead, deep jowl, taper toward low snout.
# Authored around the existing head pivot (60,0,76 cm).
segments=64;sections=[(-.225,-.105,.089,.099),(-.189,-.106,.126,.128),(-.135,-.106,.151,.139),(-.070,-.108,.145,.143),(-.015,-.109,.135,.137),(.040,-.119,.119,.115),(.100,-.138,.106,.086),(.170,-.155,.092,.063),(.217,-.160,.080,.051)]
v=[]
for x,cz,ry,rz in sections:
 for i in range(segments):
  t=2*math.pi*i/segments;v.append((x,ry*math.cos(t),cz+rz*math.sin(t)))
f=[]
for k in range(len(sections)-1):
 for i in range(segments):f.append((k*segments+i,k*segments+(i+1)%segments,(k+1)*segments+(i+1)%segments,(k+1)*segments+i))
f.extend([tuple(range(segments-1,-1,-1)),tuple((len(sections)-1)*segments+i for i in range(segments))])
base=mesh('Long sloping facial planes',v,f,'PIG_M_BlackHide');sub=base.modifiers.new('Sculpted head contours','SUBSURF');sub.levels=2;apply(base,sub)
ellipsoid('Heavy anatomical jaw',(-.069,0,-.196),(.174,.132,.069),'PIG_M_BlackHide',48,28)
head_hide=union('Continuous jowl and facial hide',.0035,7200)
# Subtle muzzle folds in actual surface.
for v in head_hide.data.vertices:
 p=v.co
 if p.x>.012 and abs(p.y)>.075 and p.z>-.19:
  ripple=.0014*math.sin((p.x-.015)*130+p.z*22)*math.exp(-((p.x-.075)/.09)**2)
  p.y+=math.copysign(ripple,p.y)
head_hide.data.update()
# Rounded rubber snout with nostrils cut INTO the mesh.
seg=64;v=[]
for x,ry,rz in [(.120,.098,.060),(.166,.110,.067),(.211,.112,.064),(.242,.103,.059),(.249,.095,.054)]:
 for i in range(seg):
  a=2*math.pi*i/seg;ir=.0012*math.sin(a*3);v.append((x,(ry+ir)*math.cos(a),-.160+(rz+ir)*math.sin(a)))
f=[]
for k in range(4):
 for i in range(seg):f.append((k*seg+i,k*seg+(i+1)%seg,(k+1)*seg+(i+1)%seg,(k+1)*seg+i))
f.extend([tuple(range(seg-1,-1,-1)),tuple(4*seg+i for i in range(seg))])
nose=mesh('Leathery blunt snout with recessed nostrils',v,f,'PIG_M_BlackSnout');be=nose.modifiers.new('Soft snout rim','BEVEL');be.width=.002;be.segments=3;apply(nose,be)
for s in [-1,1]:
 cutter=ellipsoid('Nostril cavity cutter',(.249,s*.045,-.151),(.025,.0155,.010),'PIG_M_NostrilMouth',32,20);parts.remove(cutter)
 boolean=nose.modifiers.new('True recessed nostril','BOOLEAN');boolean.operation='DIFFERENCE';boolean.solver='EXACT';boolean.object=cutter;apply(nose,boolean);bpy.data.objects.remove(cutter,do_unlink=True)
 ellipsoid('Shadow at nostril base',(.225,s*.045,-.151),(.009,.012,.007),'PIG_M_NostrilMouth',24,12)
# Lips, recessed lateral eyes and eyelids form a natural almond profile.
for s in [-1,1]:
 mouth=[]
 for x,z in [(.015,-.209),(.06,-.219),(.12,-.221),(.185,-.213),(.224,-.202)]:
  candidates=[]
  for surface in [head_hide,nose]:
   hit,point,norm,index=surface.ray_cast(Vector((x,s*.5,z)),Vector((0,-s,0)))
   if hit:candidates.append(point)
  if not candidates:raise RuntimeError('Mouth skin projection missed')
  p=max(candidates,key=lambda p:abs(p.y));mouth.append((x,p.y+s*.0003,z))
 tube('Fine quiet upper lip',mouth,[.0006,.0008,.0009,.0006,.0004],'PIG_M_NostrilMouth',7)
 eye_x,eye_z=-.034,-.043
 hit,point,norm,index=head_hide.ray_cast(Vector((eye_x,s*.5,eye_z)),Vector((0,-s,0)))
 if not hit:raise RuntimeError('Eye surface projection missed')
 eye_center=Vector((eye_x,point.y+s*.0015,eye_z))
 eye=ellipsoid('Small wet lateral black eye',tuple(eye_center),(.0145,.007,.0102),'PIG_M_GlossBlackEye',32,20)
 # Upper heavy lid arcs and lower tear-line follow the sclera edge.
 for upper in [True,False]:
  p=[];r=[]
  for i in range(25):
   t=i/24*math.pi;x=eye_center.x+.016*math.cos(t);z=eye_center.z+(.011 if upper else -.0075)*math.sin(t)
   hit,point,norm,index=head_hide.ray_cast(Vector((x,s*.5,z)),Vector((0,-s,0)))
   y=point.y+s*.001 if hit else eye_center.y
   p.append((x,y,z));r.append((.0033 if upper else .0018)*(0.5+.5*math.sin(t)))
  tube('Fleshy eyelid' if upper else 'Lower tear rim',p,r,'PIG_M_BlackHide' if upper else 'PIG_M_BlackSnout',8)
 # Brow subtle weight and crease beyond eye.
 brow=[]
 for x,z in [(-.084,-.018),(-.05,-.012),(-.025,-.018),(.009,-.036)]:
  hit,p,n,index=head_hide.ray_cast(Vector((x,s*.5,z)),Vector((0,-s,0)))
  brow.append((x,p.y+s*.0005,z))
 tube('Low brow fold',brow,[.001,.002,.002,.0008],'PIG_M_BlackHide',8)
 # Broad floppy ear ribbon: arched base falls forward/down over face.
 v=[];cross=21;rows=20
 for j in range(rows):
  t=j/(rows-1);cx=-.156+.210*t;cy=s*(.112+.054*math.sin(t*math.pi));cz=.025+.060*math.sin(math.pi*t)-.050*t**1.2
  width=.060*math.sin(math.pi*t)**.55+.024*(1-t)
  for k in range(cross):
   q=2*k/(cross-1)-1
   v.append((cx+q*width*.53,cy+s*q*width*.50,cz-.024*q*q+.006*math.sin(q*math.pi)*math.sin(t*math.pi)))
 f=[]
 for j in range(rows-1):
  for k in range(cross-1):f.append((j*cross+k,j*cross+k+1,(j+1)*cross+k+1,(j+1)*cross+k))
 ear=mesh('Thick softly folded lop ear',v,f,'PIG_M_BlackHide');sub=ear.modifiers.new('Soft ear anatomy','SUBSURF');sub.levels=1;apply(ear,sub)
 ear.data.materials.append(materials['PIG_M_CharcoalEarInner'])
 sol=ear.modifiers.new('Fleshy ear underside','SOLIDIFY');sol.thickness=.0045;sol.material_offset=1;sol.material_offset_rim=0;apply(ear,sol)
 # Fine sculpted ridge along ear, never a flat triangular cutout.
 p=[]
 for j in range(24):
  t=.16+.77*j/23;p.append((-.156+.210*t,s*(.112+.054*math.sin(t*math.pi)),.025+.060*math.sin(math.pi*t)-.050*t**1.2+.0025))
 tube('Soft cartilage ear fold',p,[.0018]*len(p),'PIG_M_BlackHide',7)
 # Small facial whiskers, shorter than the eye aperture.
 for i in range(22):
  x=rng.uniform(.055,.16);z=rng.uniform(-.210,-.166);y=s*(.107-(x-.07)*.11)
  tube('Short coarse muzzle whisker',[(x,y,z),(x+.005,y+s*.006,z-.001)],[.00024,.000015],'PIG_M_BlackBristle',3)
head=join('SM_PIG_BlackHead')

def leg_profile(name,profile,m,seg=36,levels=2):
 v=[]
 for cx,z,rx,ry in profile:
  for i in range(seg):
   a=2*math.pi*i/seg;v.append((cx+rx*math.cos(a),ry*math.sin(a),z))
 f=[]
 for k in range(len(profile)-1):
  for i in range(seg):f.append((k*seg+i,k*seg+(i+1)%seg,(k+1)*seg+(i+1)%seg,(k+1)*seg+i))
 f.extend([tuple(range(seg-1,-1,-1)),tuple((len(profile)-1)*seg+i for i in range(seg))]);o=mesh(name,v,f,m);sub=o.modifiers.new('Rounded muscle and joint contours','SUBSURF');sub.levels=levels;apply(o,sub);return o
# Tapered upper legs overlap torso through a thick fleshy shoulder. Knee/hock bulge prevents tube silhouette.
leg=leg_profile('Organic shoulder thigh and knee',[(0,.05,.057,.061),(-.005,.02,.072,.077),(-.012,-.035,.076,.073),(-.019,-.090,.065,.059),(-.020,-.14,.048,.045),(-.009,-.195,.037,.035),(.012,-.24,.041,.038),(.012,-.256,.035,.034)],'PIG_M_BlackHide',32,1)
upper=join('SM_PIG_BlackUpperLeg')
# Pastern carries visible tendon, joint and two distinct forward-pointing cloven toes.
leg_profile('Bent tendon and hock pastern',[(.012,.040,.030,.028),(.01,.024,.040,.040),(.004,-.004,.044,.041),(-.002,-.050,.032,.032),(.002,-.103,.024,.027),(.017,-.153,.025,.030),(.028,-.186,.036,.043),(.028,-.199,.034,.042)],'PIG_M_BlackHide',32,1)
for s in [-1,1]:
 # Rounded asymmetrical toe grows forward and narrows at tip, natural horn capsule.
 profile=[(-.186,.019,.023,.019),(-.194,.025,.027,.024),(-.208,.028,.031,.025),(-.224,.029,.037,.026),(-.23,.029,.038,.025)]
 v=[];seg=32
 for z,cx,rx,ry in profile:
  for i in range(seg):
   a=2*math.pi*i/seg
   # Flatten plantar sole, rounded front and distinct central cleft.
   v.append((cx+rx*math.cos(a),s*.0295+ry*math.sin(a),z))
 f=[]
 for k in range(len(profile)-1):
  for i in range(seg):f.append((k*seg+i,k*seg+(i+1)%seg,(k+1)*seg+(i+1)%seg,(k+1)*seg+i))
 f.extend([tuple(range(seg-1,-1,-1)),tuple((len(profile)-1)*seg+i for i in range(seg))]);toe=mesh('Rounded black horn cloven toe',v,f,'PIG_M_HoofKeratin');be=toe.modifiers.new('Worn horn sole edge','BEVEL');be.width=.003;be.segments=2;apply(toe,be)
 ellipsoid('Small rear dewclaw',(-.002,s*.024,-.167),(.014,.009,.016),'PIG_M_HoofKeratin',24,12)
 # Shallow horn coronet crease around toe.
 path=[]
 for i in range(28):
  a=2*math.pi*i/27;path.append((.022+.026*math.cos(a),s*.0295+.023*math.sin(a),-.194))
 tube('Hoof coronet horn ridge',path,[.0007]*len(path),'PIG_M_BlackSnout',5)
lower=join('SM_PIG_BlackLowerLeg')
# Keep native runtime budget close to 60k assembled triangles.
head.data.calc_loop_triangles()
if len(head.data.loop_triangles)>19500:
 de=head.modifiers.new('Face detail runtime budget','DECIMATE');de.ratio=19500/len(head.data.loop_triangles);apply(head,de)
assets=[body,head,upper,lower]

def bounds(o):
 p=[o.matrix_world@Vector(c) for c in o.bound_box];lo=[min(q[i] for q in p)*100 for i in range(3)];hi=[max(q[i] for q in p)*100 for i in range(3)];return {'min':[round(x,6) for x in lo],'max':[round(x,6) for x in hi],'size':[round(hi[i]-lo[i],6) for i in range(3)]}
records=[];audit=[]
for o in assets:
 o.data.calc_loop_triangles();r={'name':o.name,'fbx':str(O/'meshes'/f'{o.name}.fbx'),'destinationPath':'/Game/SeniorSendOff/House/PigstyRoom/Pigs','sourceBoundsCm':bounds(o),'materialSlots':[m.name for m in o.data.materials],'triangleCount':len(o.data.loop_triangles),'uvLayerCount':len(o.data.uv_layers),'importScale':1,'collision':'Runtime capsule and animated hitbox; visual meshes NoCollision'}
 export=o.copy();export.data=o.data.copy();bpy.context.collection.objects.link(export);export.name=o.name+'_Export'
 for vert in export.data.vertices:vert.co*=100
 bpy.context.scene.unit_settings.scale_length=.01;bpy.ops.object.select_all(action='DESELECT');export.select_set(True);bpy.context.view_layer.objects.active=export
 bpy.ops.export_scene.fbx(filepath=r['fbx'],use_selection=True,global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',use_tspace=True,add_leaf_bones=False,bake_anim=False,path_mode='STRIP',use_triangles=True)
 bpy.data.objects.remove(export,do_unlink=True);bpy.context.scene.unit_settings.scale_length=1
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=r['fbx'],use_custom_normals=True);imports=[x for x in bpy.data.objects if x not in before and x.type=='MESH'];a={'name':o.name,'roundtripObjectCount':len(imports)}
 for im in imports:
  im.data.calc_loop_triangles();a.update({'roundtripBoundsCm':bounds(im),'triangles':len(im.data.loop_triangles),'uvLayers':len(im.data.uv_layers),'finiteNormals':all(math.isfinite(c) for p in im.data.polygons for c in p.normal)});a['maxBoundsDifferenceCm']=max(abs(r['sourceBoundsCm'][q][i]-a['roundtripBoundsCm'][q][i]) for q in ['min','max'] for i in range(3));bpy.data.objects.remove(im,do_unlink=True)
 b=r['sourceBoundsCm'];r['boundsCm']={'min':[b['min'][0],-b['max'][1],b['min'][2]],'max':[b['max'][0],-b['min'][1],b['max'][2]],'size':b['size'],'coordinateSpace':'Unreal local numeric centimeters after native Y reflection'};records.append(r);audit.append(a)
mat_records=[]
for name,m in materials.items():
 b=m.node_tree.nodes.get('Principled BSDF');r={'name':name,'baseColor':list(b.inputs['Base Color'].default_value)[:3],'roughness':b.inputs['Roughness'].default_value,'metallic':0,'textures':{},'destinationPath':'/Game/SeniorSendOff/House/PigstyRoom/Pigs'}
 for n in m.node_tree.nodes:
  if n.type=='TEX_IMAGE':r['textures'][Path(n.image.filepath).stem.split('_')[-1]]=Path(n.image.filepath).stem
 for n in m.node_tree.nodes:
  if n.type=='NORMAL_MAP':r['normalStrength']=n.inputs['Strength'].default_value
 mat_records.append(r)
textures=[{'name':p.stem,'path':str(p),'kind':p.stem.split('_')[-1],'sRGB':p.stem.endswith('BaseColor'),'flipGreenChannel':p.stem.endswith('Normal'),'destinationPath':'/Game/SeniorSendOff/House/PigstyRoom/Pigs'} for p in sorted((O/'textures').glob('*.png'))]
assembled=[]
for r in records:
 if r['name'].endswith('Body'):assembled.append((r['boundsCm'],[0,0,0]))
 elif r['name'].endswith('Head'):assembled.append((r['boundsCm'],[60,0,76]))
 else:
  for x in [-35,35]:
   for y in [-21,21]:assembled.append((r['boundsCm'],[x,y,48 if r['name'].endswith('UpperLeg') else 23]))
lo=[min(b['min'][i]+off[i] for b,off in assembled) for i in range(3)];hi=[max(b['max'][i]+off[i] for b,off in assembled) for i in range(3)]
manifest={'schema':'pig-modular-assets-2','units':'Unreal centimeters','assetForward':'+X','assetUp':'+Z','exportConvention':'Numeric cm, scene scale_length0.01, FBX_SCALE_UNITS, apply_unit_scaleTrue, unitfactor1; Unreal importScale1. Native Unreal reflects sourceY. No extra vertex Y flip.','meshes':records,'materials':mat_records,'textures':textures,'recommendedPivotsCm':{'Body':[0,0,0],'Head':[60,0,76],'FrontLeftHip':[35,-21,48],'FrontRightHip':[35,21,48],'RearLeftHip':[-35,-21,48],'RearRightHip':[-35,21,48],'KneeRelativeToUpper':[0,0,-25]},'legFloorNote':'Lower hoof minimumZ exactly -23; hip48 + knee-25 + hoof-23 = floor0.','assembledBoundsCm':{'min':lo,'max':hi,'size':[hi[i]-lo[i] for i in range(3)]},'triangleCountUnique':sum(r['triangleCount'] for r in records),'triangleCountAssembled':records[0]['triangleCount']+records[1]['triangleCount']+4*(records[2]['triangleCount']+records[3]['triangleCount']),'reference':'Large Black anatomy informed by Rare Breeds Survival Trust https://www.rbst.org.uk/watchlist-breed/large-black/ ; all geometry and texture maps procedurally authored for this project.','preview':str(O/'previews'/'PIG_RealBlackPig_Assembled.png'),'materialNotes':['Same seven material names as version1. Three hide texture names updated in place. Six additional maps for snout and hoof.','Normal maps OpenGL; flipGreenChannelTrue on Unreal import.','Black pigs only: subtle charcoal hide, no pink or white areas.','Organic drooping ears, actual recessed nostril cavities, dark almond eyes, shaped jaw/neck, tapered toes, sparse fine bristles.']}
(O/'incremental_manifest.json').write_text(json.dumps(manifest,indent=2));(O/'fbx_audit.json').write_text(json.dumps(audit,indent=2))
# Assembled preview exactly matches runtime articulation origins.
head.location=(.60,0,.76);upper.hide_render=True;lower.hide_render=True
for x in [-.35,.35]:
 for y in [-.21,.21]:
  for original,z in [(upper,.48),(lower,.23)]:
   o=original.copy();o.data=original.data;bpy.context.collection.objects.link(o);o.hide_render=False;o.location=(x,y,z)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.003));floor=bpy.context.object;floor.name='Preview only floor';m=bpy.data.materials.new('Preview warm dark floor');m.diffuse_color=(.038,.035,.03,1);floor.data.materials.append(m)
world=bpy.context.scene.world;world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.09,.11,.13,1);world.node_tree.nodes['Background'].inputs[1].default_value=.28

def area(name,loc,power,color,size,target):
 bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.name=name;o.data.energy=power;o.data.color=color;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
area('Wide neutral anatomy key',(2.0,-2.4,3),260,(.94,.96,1),2.7,(0,0,.5))
area('Cool shoulder rim',(-2.3,1.4,2.5),380,(.65,.79,1),2.1,(0,0,.60))
area('Warm soft face fill',(2.3,2.0,1.7),125,(1,.86,.73),1.8,(.65,0,.65))
bpy.ops.object.camera_add(location=(2.8,-3.6,1.65));camera=bpy.context.object;camera.rotation_euler=(Vector((.03,0,.47))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=2.16
scene=bpy.context.scene;scene.camera=camera;scene.render.engine='CYCLES';scene.cycles.samples=40;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=manifest['preview']
bpy.ops.wm.save_as_mainfile(filepath=str(O/'PIG_RealBlackPig_Source.blend'));bpy.ops.render.render(write_still=True)
camera.location=(2.5,-3.6,1.5);camera.rotation_euler=(Vector((.54,-.005,.66))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=.92;scene.render.filepath=str(O/'previews'/'PIG_RealHead_Detail.png');bpy.ops.render.render(write_still=True)
camera.location=(.08,-3.6,1.01);camera.rotation_euler=(Vector((.07,0,.48))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=1.98;scene.render.filepath=str(O/'previews'/'PIG_RealBlackPig_Side.png');bpy.ops.render.render(write_still=True)
print('PIG_REALISM_READY',json.dumps({'uniqueTriangles':manifest['triangleCountUnique'],'assembledTriangles':manifest['triangleCountAssembled'],'assembledBoundsCm':manifest['assembledBoundsCm'],'roundtrip':audit}))
