import bpy,json,math,hashlib
from pathlib import Path
from mathutils import Vector
ROOT=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1')
OUT=ROOT/'geometry';OUT.mkdir(exist_ok=True)
SHELL='HallEnd_SM_HallEnd_ThreeRoomsShell'; FLOOR='HallEnd_SM_HallEnd_ThreeRoomsFloors';TRIM='HallEnd_SM_HallEnd_ThreeRoomsTrim';COL='HallEnd_COL_HallEnd_ThreeRoomsShell'
oldwalls=[([-2992,-1391.8425,297.3],[-2972,-865,582.1]),([-2972,-1391.8425,297.3],[-2349.2328,-1371.8425,582.1]),([-2972,-885,297.3],[-2349.2328,-865,582.1]),([-2972,-1371.8425,566.1],[-2349.2328,-885,582.1])]
oldfloor=[([-2972,-1371.8425,297.3],[-2320.4328,-885,302.3])]
oldtrim=[([-2972,-1371.8425,302.3],[-2970,-885,316.3]),([-2972,-1371.8425,302.3],[-2349.2328,-1369.8425,316.3]),([-2972,-887,302.3],[-2349.2328,-885,316.3])]
ownership=json.load(open(OUT/'west-owned-face-indices.json'))['files']
report={};manifest={'meshes':[],'materials':{},'textures':[],'actors':[],'replacements':[]}
def ue(p):return [100*p.x,-100*p.y,100*p.z]
def contains_surface(ps,box):
 lo,hi=box;tol=.06
 if not all(all(lo[i]-tol<=p[i]<=hi[i]+tol for i in range(3)) for p in ps):return False
 return any(all(abs(p[i]-v)<tol for p in ps) for i in range(3) for v in (lo[i],hi[i]))
def export(o,name,info={}):
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,mesh_smooth_type='OFF',use_mesh_modifiers=True,add_leaf_bones=False)
 pts=[ue(o.matrix_world@v.co) for v in o.data.vertices];bb={'min':[min(p[i] for p in pts) for i in range(3)],'max':[max(p[i] for p in pts) for i in range(3)]}
 row={'name':name,'fbx':str(OUT/(name+'.fbx')),'boundsCm':bb,'materials':[m.name for m in o.data.materials]};row.update(info);manifest['meshes'].append(row)
 return row
for label,boxes in [(SHELL,oldwalls),(FLOOR,oldfloor),(TRIM,oldtrim),(COL,oldwalls+oldfloor)]:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 source=ROOT/'context'/(label+'.fbx')
 own=ownership[label]
 if hashlib.sha256(source.read_bytes()).hexdigest()!=own['sourceSha256']:raise RuntimeError('Stale ownership '+label)
 bpy.ops.import_scene.fbx(filepath=str(source))
 remove_indices=set(own['removePolygonIndices'])
 verts=[];faces=[];uvs=[];norms=[];mi=[];mats=[];removed=[0]*len(boxes);before=0
 for src in list(bpy.context.scene.objects):
  if src.type!='MESH':continue
  mesh=src.data;mesh.calc_loop_triangles();pts=[src.matrix_world@v.co for v in mesh.vertices];wpts=[ue(v) for v in pts];normalmat=src.matrix_world.to_3x3().inverted().transposed()
  names=[m.name.split('.')[0] for m in mesh.materials]
  for n in names:
   if n not in mats:mats.append(n)
  for f in mesh.polygons:
   before+=1;ps=[wpts[i] for i in f.vertices];matches=[j for j,b in enumerate(boxes) if contains_surface(ps,b)]
   if f.index in remove_indices:
    if not matches:raise RuntimeError('Ownedface outside intendedcube '+label+str(f.index))
    removed[matches[0]]+=1;continue
   start=len(verts);verts.extend([tuple(pts[i]) for i in f.vertices]);faces.append(tuple(range(start,len(verts))))
   for li in f.loop_indices:
    uvs.append(tuple(mesh.uv_layers.active.data[li].uv) if mesh.uv_layers.active else (0,0));norms.append(tuple((normalmat@mesh.corner_normals[li].vector).normalized()))
   mi.append(mats.index(names[f.material_index]) if names else 0)
 if sum(removed)!=own['removePolygonCount'] or len(faces)!=own['retainPolygonCount']:raise RuntimeError('Wrong retained components '+label+str(removed))
 me=bpy.data.meshes.new('retained');me.from_pydata(verts,[],faces);me.update();layer=me.uv_layers.new(name='UVMap')
 for i,uv in enumerate(uvs):layer.data[i].uv=uv
 for n in mats:me.materials.append(bpy.data.materials.get(n) or bpy.data.materials.new(n))
 for f,i in zip(me.polygons,mi):f.material_index=i;f.use_smooth=True
 me.normals_split_custom_set(norms)
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 name={'HallEnd_SM_HallEnd_ThreeRoomsShell':'PIG_SM_OriginalShellRemainder',FLOOR:'PIG_SM_OriginalFloorsRemainder',TRIM:'PIG_SM_OriginalTrimRemainder',COL:'PIG_COL_OriginalShellRemainder'}[label]
 o=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(o);export(o,name,{'complex_collision':label==COL})
 manifest['replacements'].append({'actor':label,'mesh':name,'collision':label==COL})
 report[label]={'before':before,'retained':len(faces),'removedPerCube':removed,'retainedCoordinatesUnchanged':True,'retainedUVsAndNormalsCopied':True,'protectedDoorCapsUnchanged':True,'ownedComponentsOnly':True}
# Private expanded room shell. Every cube is a collision hull but one combined triangle mesh.
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
verts=[];faces=[];uvs=[];mids=[];matnames=[]
def cube(lo,hi,mat):
 if mat not in matnames:matnames.append(mat)
 x0,y0,z0=lo;x1,y1,z1=hi
 pts=[(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
 fs=[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
 for f in fs:
  start=len(verts);pp=[pts[i] for i in f];verts.extend([(p[0]/100,-p[1]/100,p[2]/100) for p in pp]);faces.append((start+3,start+2,start+1,start));mids.append(matnames.index(mat))
  axis=next(i for i in range(3) if max(p[i] for p in pp)-min(p[i] for p in pp)<.001)
  for p in reversed(pp):
   if axis==2:uv=((p[0]+2972)/622.7672,(p[1]+1371.8425)/606.8425) if mat=='PST_M_DampCeiling' else (p[0]/200,p[1]/200)
   elif axis==0:uv=(p[1]/200,p[2]/200)
   else:uv=(p[0]/200,p[2]/200)
   uvs.append(uv)
for lo,hi,mat in [([-2992,-1391.8425,297.3],[-2972,-745,582.1],'PST_M_DirtyOlivePlaster'),([-2972,-1391.8425,297.3],[-2349.2328,-1371.8425,582.1],'PST_M_DirtyOlivePlaster'),([-2972,-765,297.3],[-2349.2328,-745,582.1],'PST_M_DirtyOlivePlaster'),([-2972,-1371.8425,566.1],[-2349.2328,-765,582.1],'PST_M_DampCeiling'),([-2972,-1371.8425,297.3],[-2320.4328,-765,302.3],'PST_M_StainedConcrete')]:cube(lo,hi,mat)
# A thin lining on the existing hall side covers bare wall while retaining the original opening.
for lo,hi in [([-2355,-1371.8425,302.3],[-2349.2328,-1326.9,566.1]),([-2355,-1163.1,302.3],[-2349.2328,-765,566.1]),([-2355,-1326.9,513.3],[-2349.2328,-1163.1,566.1])]:cube(lo,hi,'PST_M_DirtyOlivePlaster')
me=bpy.data.meshes.new('ExpandedPigsty');me.from_pydata(verts,[],faces);me.update();layer=me.uv_layers.new(name='UVMap')
for i,uv in enumerate(uvs):layer.data[i].uv=uv
for n in matnames:me.materials.append(bpy.data.materials.new(n))
for f,i in zip(me.polygons,mids):f.material_index=i
ob=bpy.data.objects.new('PIG_SM_ExpandedRoom',me);bpy.context.collection.objects.link(ob);export(ob,ob.name)
co=ob.copy();co.data=me.copy();co.name='PIG_COL_ExpandedRoom';bpy.context.collection.objects.link(co);export(co,co.name,{'complex_collision':True})
manifest['actors'] += [{'label':'PIG_ExpandedRoom','mesh':ob.name},{'label':'PIG_ExpandedRoomCollision','mesh':co.name,'collision':'BlockAll','hidden':True}]
for name,w,h,mat in [('PIG_SM_KeepShutPlaque',40,25,'PST_M_KeepShutPlaque'),('PIG_SM_GrimeScratch',100,150,'PST_M_GrimeScratchDecal')]:
 pp=[(-w/200,0,0),(w/200,0,0),(w/200,0,h/100),(-w/200,0,h/100)]
 mm=bpy.data.meshes.new(name);mm.from_pydata(pp,[],[(3,2,1,0)]);mm.update();mm.materials.append(bpy.data.materials.get(mat) or bpy.data.materials.new(mat));uv=mm.uv_layers.new(name='UVMap')
 uv_by_vertex=[(1,0),(0,0),(0,1),(1,1)]
 for li in mm.polygons[0].loop_indices:uv.data[li].uv=uv_by_vertex[mm.loops[li].vertex_index]
 oo=bpy.data.objects.new(name,mm);bpy.context.collection.objects.link(oo);export(oo,name)
json.dump(manifest,open(OUT/'incremental_manifest.json','w'),indent=2);json.dump(report,open(OUT/'retained-face-audit.json','w'),indent=2)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'expanded_room.blend'));print('PIGSTY_GEOMETRY_BUILT',report,flush=True)
