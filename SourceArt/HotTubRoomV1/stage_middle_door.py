"""Stage only the middle doorway's frame removal and matching sealed wall.
Read current original-room source and exported map collision; no project edits.
"""
import bpy,bmesh,collections,json,hashlib,os
from mathutils import Vector
ROOT='/Users/Stewart/Projects/Senior sendoff/outputs/hot-tub-room-v1'
OUT=os.path.join(ROOT,'models');os.makedirs(OUT,exist_ok=True)
VIS='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseDoors/SM_OriginalRoom_Entrance_m1_m2_Solid.fbx'
COL=os.path.join(ROOT,'work/COL_Entrance_m1_m2_Solid.fbx')
report={'coordinateSystem':'FBX raw Blender metres; actor scales XY1.8,Z1.0','protected':{}}
def reset():
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
def read(path):
 reset();bpy.ops.import_scene.fbx(filepath=path);bpy.context.view_layer.update()
 meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
 assert len(meshes)==1,(path,len(meshes))
 return meshes[0]
def parts(obj):
 m=obj.data;par=list(range(len(m.vertices)))
 def find(v):
  while par[v]!=v:par[v]=par[par[v]];v=par[v]
  return v
 for p in m.polygons:
  for v in p.vertices[1:]:par[find(v)]=find(p.vertices[0])
 gs=collections.defaultdict(list)
 for p in m.polygons:gs[find(p.vertices[0])].append(p.index)
 for ids in gs.values():
  vs={v for i in ids for v in m.polygons[i].vertices};cs=[obj.matrix_world@m.vertices[v].co for v in vs]
  lo=[min(v[i]for v in cs)for i in range(3)];hi=[max(v[i]for v in cs)for i in range(3)]
  name=m.materials[m.polygons[ids[0]].material_index].name.split('.')[0]if len(m.materials)else''
  yield ids,lo,hi,[(lo[i]+hi[i])/2 for i in range(3)],name
def fingerprint(obj,exclude=()):
 exclude=set(exclude);m=obj.data;rows=[];uv=m.uv_layers.active
 for p in m.polygons:
  if p.index in exclude:continue
  name=m.materials[p.material_index].name.split('.')[0]if len(m.materials)else''
  data=[]
  for li in p.loop_indices:
   loop=m.loops[li];co=obj.matrix_world@m.vertices[loop.vertex_index].co
   item=[round(x,6)for x in co]
   if uv:item.extend(round(x,6)for x in uv.data[li].uv)
   data.append(item)
  # Sorted representation ignores harmless reindexing through bmesh/export.
  rows.append(json.dumps([name,sorted(data)],separators=(',',':')))
 return hashlib.sha256('\n'.join(sorted(rows)).encode()).hexdigest()
def delete_faces(obj,ids):
 bm=bmesh.new();bm.from_mesh(obj.data);bm.faces.ensure_lookup_table();bmesh.ops.delete(bm,geom=[bm.faces[i]for i in ids],context='FACES');bm.to_mesh(obj.data);bm.free();obj.data.update()
def add_box(obj,name,lo,hi,mat_name,omit_caps=False):
 m=obj.data
 slot=next((i for i,x in enumerate(m.materials)if x and x.name.split('.')[0]==mat_name),None)
 if slot is None:
  material=bpy.data.materials.get(mat_name)or bpy.data.materials.new(mat_name);m.materials.append(material);slot=len(m.materials)-1
 x0,y0,z0=lo;x1,y1,z1=hi
 coords=[(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
 # Omit top/bottom caps of visual filler to avoid coplanar caps where native
 # header/floor touch the new section. Front/back/end wall faces remain closed.
 fs=[(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
 if not omit_caps:fs=[(0,3,2,1),(4,5,6,7)]+fs
 # Imported Unreal FBXs use a 0.01 object scale. Bounds above are world
 # metres; convert them to the source object's centimetre vertex space.
 inverse=obj.matrix_world.inverted()
 bm=bmesh.new();bm.from_mesh(m);vs=[bm.verts.new(inverse@Vector(co))for co in coords]
 for f in fs:
  face=bm.faces.new([vs[i]for i in f]);face.material_index=slot
 bm.to_mesh(m);bm.free();m.update()
def export(obj,name):
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 path=os.path.join(OUT,name+'.fbx');bpy.ops.export_scene.fbx(filepath=path,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y');return path
obj=read(VIS);remove=set();removed=[];header=None
for ids,lo,hi,c,mat in parts(obj):
 wx,wy=c[0]*1.8,c[1]*1.8
 if mat=='MI_SSO_PaintWhite' and -23.75<wx<-23.3 and 3.25<wy<5.48:
  remove.update(ids);removed.append({'faces':len(ids),'boundsRaw':[lo,hi]})
 if mat=='MI_SSO_S03HallGrey' and -23.75<wx<-23.3 and 3.8<wy<4.9 and lo[2]>2:
  assert header is None,'Multiple middle headers';header=(lo,hi)
assert len(removed)==3 and len(remove)==564,(len(removed),len(remove))
assert header,'Missing original middle header'
expected=fingerprint(obj,remove);before=len(obj.data.polygons);delete_faces(obj,remove)
retained=fingerprint(obj);assert expected==retained,'Protected visual geometry/UV/material fingerprint mismatch'
wall_lo=[header[0][0],header[0][1],0];wall_hi=[header[1][0],header[1][1],header[0][2]]
add_box(obj,'Middle sealed wall',wall_lo,wall_hi,'MI_SSO_S03HallGrey',True)
# Native hall skirting is a shallow thicker band at the same hall-wall face.
skirt_lo=[wall_lo[0]-.012,wall_lo[1],0];skirt_hi=[wall_hi[0]+.013,wall_hi[1],.11]
add_box(obj,'Middle continuous skirting',skirt_lo,skirt_hi,'MI_SSO_S03HallSkirting',True)
vis_path=export(obj,'SM_SPA_Entrance_m1_m2_MiddleSealed')
report['visual']={'source':VIS,'staged':vis_path,'removedComponents':removed,'removedFaceCount':len(remove),'beforeFaces':before,'afterFaces':len(obj.data.polygons),'retainedFingerprint':retained,'expectedProtectedFingerprint':expected,'protectionPass':retained==expected,'newWallBoundsRaw':[wall_lo,wall_hi],'newSkirtingBoundsRaw':[skirt_lo,skirt_hi]}
obj=read(COL);before=len(obj.data.polygons);protected=fingerprint(obj)
# Collision retains every current triangle and adds one solid wall slab. Its
# height joins the existing header; no other room/door opening is altered.
add_box(obj,'Middle wall collision',wall_lo,wall_hi,'SPA_Collision',False)
added=set(range(before,len(obj.data.polygons)));after_protected=fingerprint(obj,added)
assert protected==after_protected,'Protected collision fingerprint mismatch'
col_path=export(obj,'COL_SPA_Entrance_m1_m2_MiddleSealed')
report['collision']={'source':COL,'staged':col_path,'beforeFaces':before,'afterFaces':len(obj.data.polygons),'addedFaces':len(added),'retainedFingerprint':after_protected,'expectedProtectedFingerprint':protected,'protectionPass':after_protected==protected,'newWallBoundsRaw':[wall_lo,wall_hi]}
report['owners']={'visualMapActor':'SM_Entrance_m1_m2_Solid','visualCurrentAsset':'/Game/ExistingHouseDoors/SM_OriginalRoom_Entrance_m1_m2_Solid','collisionMapActor':'COL_Entrance_m1_m2_Solid','collisionCurrentAsset':'/Game/SeniorSendOff/House/Meshes/COL_Entrance_m1_m2_Solid','removeDoorActor':'OriginalDoor_Entrance_m1_m2_Solid_02'}
json.dump(report,open(os.path.join(ROOT,'work/middle_door_stage_report.json'),'w'),indent=2)
print('SPA_MIDDLE_DOOR_STAGED',len(remove),'removed faces; protected visual/collision match')
