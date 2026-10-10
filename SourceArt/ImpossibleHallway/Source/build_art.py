"""World-centimetre hallway art plus surgical retained shell surgery.

Every complete module is a translation of one prototype by exactly 180 cm.
Only the initial 0..80 cm throat differs. No engine/map writes occur here.
"""
import bpy, json, hashlib, collections, math
from pathlib import Path
from mathutils import Vector

PROJECT=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff')
BASE=Path('/Users/Stewart/Projects/Senior sendoff/outputs/impossible-hallway')
OUT=PROJECT/'SourceArt/ImpossibleHallway/Art'
OUT.mkdir(parents=True,exist_ok=True)
CX=-2177.0; Y0=-1386.2425; FZ=302.3; CZ=566.1; PERIOD=180.; COUNT=18
PLAN={'schema':'SeniorSendoff.ImpossibleHallway.v1','textures':json.load(open(OUT/'textures.json'))['textures'],
 'materials':{},'meshes':[],'replacements':[],'actors':[],'lights':[],
 'runtime':{'label':'InfiniteHall_Illusion','location':[CX,Y0,FZ],'yaw':-90.,
 'properties':{'entrance_door_id':'HallEnd_End','module_length':180.,'loop_depth':360.,'return_min_depth':90.,'corridor_half_width':110.,'corridor_height':263.8}},
 'layout':{'doorId':'HallEnd_End','centerX':CX,'doorPlaneY':Y0,'floorZ':FZ,'ceilingZ':CZ,'moduleLength':PERIOD,'moduleCount':COUNT,'lengthCm':PERIOD*COUNT,'corridorWidthCm':220,'throatLengthCm':80}}
REPORT={'retained':{},'modulePeriodCm':PERIOD,'moduleCount':COUNT,'corridorLengthCm':PERIOD*COUNT}

def reset():
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
def ue(p):return [p.x*100,-p.y*100,p.z*100]
def world(p):return Vector((p[0]/100,-p[1]/100,p[2]/100))
def point(x,d,z):return [CX+x,Y0-d,FZ+z]
def material(name,color=(.3,.3,.3),rough=.8,metal=0,emission=0,texture=None):
 if name not in PLAN['materials']:
  spec={'color':list(color),'roughness':rough,'metallic':metal}
  if emission:spec['emission']=emission
  if texture:spec.update(base_texture=texture+'_BaseColor.png',roughness_texture=texture+'_Roughness.png',normal_texture=texture+'_Normal.png',normal_strength=.65)
  PLAN['materials'][name]=spec
 return bpy.data.materials.get(name) or bpy.data.materials.new(name)
def export(o,name,collision=False):
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 path=OUT/(name+'.fbx')
 bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,mesh_smooth_type='OFF',use_mesh_modifiers=True,add_leaf_bones=False)
 pts=[ue(o.matrix_world@v.co) for v in o.data.vertices]
 row={'name':name,'file':name+'.fbx','complex_collision':collision,'boundsCm':{'min':[min(p[i] for p in pts) for i in range(3)],'max':[max(p[i] for p in pts) for i in range(3)]},'materials':[m.name for m in o.data.materials],'polygons':len(o.data.polygons),'triangles':sum(len(f.vertices)-2 for f in o.data.polygons)}
 PLAN['meshes'].append(row);return row

def remove_owned(source,name,boxes,actor,collision=False,strict_cube=True):
 reset();bpy.ops.import_scene.fbx(filepath=str(source))
 verts=[];faces=[];uvs=[];norms=[];mids=[];names=[];old=[];kept=[];removed=[]
 tol=.06
 for obj in list(bpy.context.scene.objects):
  if obj.type!='MESH':continue
  mesh=obj.data;pts=[obj.matrix_world@v.co for v in mesh.vertices];w=[ue(p) for p in pts]
  normalmat=obj.matrix_world.to_3x3().inverted().transposed()
  mats=[m.name.split('.')[0] for m in mesh.materials]
  for n in mats:
   if n not in names:names.append(n)
  for f in mesh.polygons:
   poly=[w[i] for i in f.vertices]
   signature=(mats[f.material_index] if mats else '',tuple(sorted(tuple(round(v,5) for v in p) for p in poly)))
   old.append(signature)
   def is_owned(lo,hi):
    if not all(all(lo[k]-tol<=p[k]<=hi[k]+tol for k in range(3)) for p in poly):return False
    if not strict_cube:return True
    # A full cube-face triangle spans both dimensions of thatface. This also
    # distinguishes the adjacent ceiling/floor endcaps on the same boundary.
    pmin=[min(p[k] for p in poly) for k in range(3)];pmax=[max(p[k] for p in poly) for k in range(3)]
    return any(all(abs(p[k]-edge)<tol for p in poly) and all(abs(pmin[j]-lo[j])<tol and abs(pmax[j]-hi[j])<tol for j in range(3) if j!=k) for k in range(3) for edge in(lo[k],hi[k]))
   hits=[i for i,(lo,hi) in enumerate(boxes) if is_owned(lo,hi)]
   if hits:removed.append((hits[0],f.index));continue
   kept.append(signature);start=len(verts);verts.extend(tuple(pts[i]) for i in f.vertices);faces.append(tuple(range(start,len(verts))))
   for li in f.loop_indices:
    uvs.append(tuple(mesh.uv_layers.active.data[li].uv) if mesh.uv_layers.active else (0,0));norms.append(tuple((normalmat@mesh.corner_normals[li].vector).normalized()))
   mids.append(names.index(mats[f.material_index]) if mats else 0)
 if not removed:raise RuntimeError('No exact owned geometry removed '+name)
 if strict_cube and len(removed)!=12*len(boxes):raise RuntimeError('Expected exactly6surfaces/12triangles of each cube '+name+str(len(removed)))
 me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update();uv=me.uv_layers.new(name='UVMap')
 for i,v in enumerate(uvs):uv.data[i].uv=v
 for n in names:me.materials.append(bpy.data.materials.get(n) or bpy.data.materials.new(n))
 for f,i in zip(me.polygons,mids):f.material_index=i;f.use_smooth=True
 me.normals_split_custom_set(norms)
 reset();o=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(o)
 export(o,name,collision)
 # Compare exact retained position and material signatures before export.
 after=[]
 for f in me.polygons:after.append((names[f.material_index],tuple(sorted(tuple(round(v,5) for v in ue(me.vertices[i].co)) for i in f.vertices))))
 if collections.Counter(kept)!=collections.Counter(after):raise RuntimeError('Retained geometry changed '+name)
 REPORT['retained'][name]={'source':str(source),'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),'inputPolygons':len(old),'removedPolygons':len(removed),'outputPolygons':len(kept),'removedPerOwnedBox':dict(collections.Counter(i for i,_ in removed)),'allRetainedCoordinatesAndMaterialsExact':True,'uvsAndNormalsCopied':True,'targetActor':actor}
 PLAN['replacements'].append({'actor':actor,'mesh':name,'collision':'BlockAll' if collision else 'NoCollision','hidden':collision})

far=([-2490,-1860,297.3],[-1890,-1840,582.1])
remove_owned(PROJECT/'SourceArt/PigstyRoom/Import/PIG_SM_OriginalShellRemainder.fbx','IH_SM_RetainedShell',[far],'HallEnd_SM_HallEnd_ThreeRoomsShell')
remove_owned(PROJECT/'SourceArt/PigstyRoom/Import/PIG_COL_OriginalShellRemainder.fbx','IH_COL_RetainedShell',[far],'HallEnd_COL_HallEnd_ThreeRoomsShell',True)
fartrim=([-2490,-1840,302.3],[-1890,-1838,316.3])
remove_owned(PROJECT/'SourceArt/PigstyRoom/Import/PIG_SM_OriginalTrimRemainder.fbx','IH_SM_RetainedTrim',[fartrim],'HallEnd_SM_HallEnd_ThreeRoomsTrim')
fixture=([-2219,-1674,558],[-2161,-1616,565])
remove_owned(PROJECT/'SourceArt/PokerRoom/POK_SM_OtherHallPracticalLights.fbx','IH_SM_RetainedPracticals',[fixture],'HallEnd_SM_HallEnd_ThreeRoomsPracticalLights',strict_cube=False)

reset();GROUPS=collections.defaultdict(list);PROTOTYPE=[]
wall=material('IH_M_DampOlivePlaster',texture='IH_T_DampOlivePlaster')
floor=material('IH_M_OldFloor',texture='IH_T_OldFloor')
ceiling=material('IH_M_DampCeiling',texture='IH_T_DampCeiling')
trim=material('IH_M_WornTrim',(.18,.19,.145),.8)
dark=material('IH_M_BlackIron',(.025,.03,.024),.52,.62)
brass=material('IH_M_OldBrass',(.105,.078,.034),.58,.6)
glass=material('IH_M_WeakLampGlass',(.38,.30,.18),.65,0,.45)
door=material('IH_M_OldDoor',(.20,.215,.177),.84)

def add_cube(name,lo,hi,mat,group,bevel=0,prototype=True,uv_kind=None):
 # lo/hi are local side, forward depth, above-floor coordinates.
 c=[(lo[i]+hi[i])/2 for i in range(3)];bpy.ops.mesh.primitive_cube_add(size=1,location=world(point(*c)))
 o=bpy.context.object;o.name=name;o.dimensions=[(hi[0]-lo[0])/100,(hi[1]-lo[1])/100,(hi[2]-lo[2])/100]
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mat)
 if uv_kind:
  uv=o.data.uv_layers.active
  for f in o.data.polygons:
   for li in f.loop_indices:
    p=ue(o.matrix_world@o.data.vertices[o.data.loops[li].vertex_index].co);s=p[0]-CX;d=Y0-p[1];z=p[2]-FZ
    uv.data[li].uv=(d/PERIOD,z/(CZ-FZ)) if uv_kind=='wall' else (s/220+.5,d/PERIOD)
 if bevel:
  mod=o.modifiers.new('Subtle worn edge','BEVEL');mod.width=bevel/100;mod.segments=2
  bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
  for p in o.data.polygons:p.use_smooth=False
 GROUPS[group].append(o)
 if prototype:PROTOTYPE.append((group,o))
 return o

# A module is constant across all18translations. The first throat is an extra.
add_cube('ModuleFloor',[-110,0,-.4],[110,180,.25],floor,'IH_SM_CorridorSurfaces',uv_kind='floor')
add_cube('ModuleCeiling',[-110,0,263.65],[110,180,267.8],ceiling,'IH_SM_CorridorSurfaces',uv_kind='floor')
for s in(-1,1):
 lo=[-114 if s<0 else 110,0,0];hi=[-110 if s<0 else 114,180,263.8]
 add_cube('ModuleWall'+str(s),lo,hi,wall,'IH_SM_CorridorSurfaces',uv_kind='wall')
 add_cube('Skirting'+str(s),[-110 if s<0 else 107,0,.3],[-107 if s<0 else 110,180,13.3],trim,'IH_SM_CorridorTrim',.35)
 add_cube('SkirtingCap'+str(s),[-107.5 if s<0 else 106.3,0,12.4],[-106.3 if s<0 else 107.5,180,14.0],trim,'IH_SM_CorridorTrim',.25)
 add_cube('Cornice'+str(s),[-110 if s<0 else 104,0,255.8],[-104 if s<0 else 110,180,263.5],trim,'IH_SM_CorridorTrim',.4)

# A shallow sealed door on the right gives the infinite run a convincing scale.
add_cube('RepeatedSealedDoor',[107.8,41,2],[109.5,139,211],door,'IH_SM_CorridorTrim',.5)
for d0,d1,z0,z1 in[(39,43,0,216),(137,141,0,216),(41,139,212,217)]:
 add_cube('DoorSurround',[105.8,d0,z0],[109.8,d1,z1],trim,'IH_SM_CorridorTrim',.35)
for d0,d1,z0,z1 in[(52,84,120,195),(95,128,120,195),(52,84,18,105),(95,128,18,105)]:
 add_cube('DoorRaisedPanel',[107.0,d0,z0],[107.9,d1,z1],trim,'IH_SM_CorridorTrim',.4)
add_cube('HandleBackplate',[103.9,125,93],[106.1,129,107],dark,'IH_SM_CorridorTrim',.25)
add_cube('HandleLever',[100.8,116.5,99.8],[103.9,127,102.2],brass,'IH_SM_CorridorTrim',.5)

# Black caged sconces on the left; tiny emissive glass reads in the distance.
add_cube('LampBackPlate',[-109.9,82,170],[-105.8,98,201],dark,'IH_SM_CorridorFixtures',.8)
add_cube('LampGlass',[-105.8,84,172],[-98.5,96,199],glass,'IH_SM_CorridorFixtures',1.1)
for z0,z1 in[(170,173),(198,202)]:add_cube('LampCageCap',[-107,81,z0],[-97.8,99,z1],dark,'IH_SM_CorridorFixtures',.45)
for d in(81.5,97.5):add_cube('LampCageEdge',[-98.4,d,172],[-97.5,d+1,199],dark,'IH_SM_CorridorFixtures',.1)
add_cube('LampCageCenter',[-98.4,89.5,172],[-97.5,90.5,199],dark,'IH_SM_CorridorFixtures',.1)

prototype=list(PROTOTYPE)
for index in range(1,COUNT):
 for group,o in prototype:
  cp=o.copy();cp.data=o.data.copy();cp.name=o.name+'_Period_%02d'%index;cp.location.y+=index*PERIOD/100;bpy.context.collection.objects.link(cp);GROUPS[group].append(cp)

# The narrow native doorway widens gently over80cm; its lining hides the end
# room wings and the open door's swept volume. This section precedes all loops.
for s in(-1,1):
 x=81.9*s
 add_cube('ThroatSide'+str(s),[min(x,110*s)-4,0,0],[max(x,110*s)+4,80,263.8],wall,'IH_SM_EntranceThroat',prototype=False,uv_kind='wall')
 # Replace the box with a wedge so the inside widens gradually.
 o=GROUPS['IH_SM_EntranceThroat'][-1]
 for v in o.data.vertices:
  p=ue(o.matrix_world@v.co);d=Y0-p[1];inner=(81.9+(110-81.9)*max(0,min(80,d))/80)*s
  outer=114*s
  current=p[0]-CX
  target=inner if (current<0 and current>-110) or (current>0 and current<110) else outer
  p[0]=CX+target;v.co=o.matrix_world.inverted()@world(p)
 co=o.copy();co.data=o.data.copy();co.name='ThroatCollision'+str(s);bpy.context.collection.objects.link(co);GROUPS['IH_COL_CorridorNear'].append(co)

# Physical side rails and ceiling cover thefullvisiblecorridor, includingprops
# held infront of theplayer and thrownobjects. Existingfloor supplies0..454.
for s in(-1,1):
 add_cube('NearCollisionWall'+str(s),[-114 if s<0 else 110,80,0],[-110 if s<0 else 114,PERIOD*COUNT,263.8],wall,'IH_COL_CorridorNear',prototype=False)
add_cube('NearCollisionCeiling',[-110,80,263.8],[110,PERIOD*COUNT,267.8],wall,'IH_COL_CorridorNear',prototype=False)
add_cube('FarFloorBridge',[-110,435,-5],[110,PERIOD*COUNT,0],floor,'IH_COL_CorridorNear',prototype=False)

# Six bounded, unshadowed lights. Near90..540 illumination is periodic because
# the140cm attenuation cannot reach the preceding absent light atdepth-90.
for i in range(6):
 PLAN['lights'].append({'label':'InfiniteHall_Lamp_%02d'%i,'location':point(-89,90+i*PERIOD,187),'intensity':115.,'radius':140.,'color':[1,.69,.36],'cast_shadows':False})

for name,objects in GROUPS.items():
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
 row=export(o,name,name.startswith('IH_COL_'))
 PLAN['actors'].append({'label':'InfiniteHall_'+name,'mesh':name,'location':[0,0,0],'scale':[1,1,1],'collision':'BlockAll' if name.startswith('IH_COL_') else 'NoCollision','hidden':name.startswith('IH_COL_')})

REPORT['prototypeObjectCount']=len(prototype)
REPORT['prototypeTranslationsExact']=True
REPORT['newMeshTriangles']=sum(r['triangles'] for r in PLAN['meshes'] if 'Retained'not in r['name'])
REPORT['newLightCount']=len(PLAN['lights']);REPORT['newLightRadiusCm']=140
REPORT['doorRegistryWritten']=False
json.dump(PLAN,open(PROJECT/'SourceArt/ImpossibleHallway/import_manifest.json','w'),indent=2)
json.dump(REPORT,open(BASE/'geometry-audit.json','w'),indent=2)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ImpossibleHallway.blend'))
print('IMPOSSIBLE_HALL_ART_READY',REPORT,flush=True)
