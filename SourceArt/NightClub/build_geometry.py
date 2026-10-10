import bpy,math,json,pathlib
from mathutils import Vector
B=pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/NightClub'); O=B/'geometry';O.mkdir(exist_ok=True)
actors=json.load(open(B/'context/actors.json'));manifest={'assets':[],'materials':{'M_NC_Charcoal':{'color':[.014,.016,.026],'roughness':.72},'M_NC_FloorBlack':{'color':[.025,.028,.045],'roughness':.24},'M_NC_FloorSlate':{'color':[.04,.045,.065],'roughness':.32},'M_NC_CeilingBlack':{'color':[.008,.009,.016],'roughness':.85},'M_NC_Trim':{'color':[.10,.11,.14],'roughness':.25,'metallic':.8},'M_NC_FloorGlow':{'color':[.015,.07,.12],'roughness':.35,'emissive':[.008,.08,.15],'emissive_strength':2}}}
# Convex clipping preserves all unaffected original faces, material slots and UVs.
def clip(poly,n,d,inside=True):
 out=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  da=sum(n[i]*a[i] for i in range(3))-d;db=sum(n[i]*b[i]for i in range(3))-d
  ina=da<=1e-7 if inside else da>=-1e-7;inb=db<=1e-7 if inside else db>=-1e-7
  if ina:out.append(a)
  if ina!=inb:
   t=da/(da-db);out.append(tuple(a[i]+t*(b[i]-a[i])for i in range(len(a))))
 return out

def subtract(poly,planes):
 result=[];remainder=poly
 for n,d in planes:
  if len(remainder)<3:break
  outside=clip(remainder,n,d,False)
  if len(outside)>=3:result.append(outside)
  remainder=clip(remainder,n,d,True)
 return result

def circleplanes(zlo,zhi):
 planes=[((0,0,1),zhi),((0,0,-1),-zlo)]
 for i in range(48):
  t=2*math.pi*i/48;n=(math.cos(t),math.sin(t),0);planes.append((n,n[0]*-1935+n[1]*-1175+100))
 return planes

def export(o,name,source=None):
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.fbx(filepath=str(O/(name+'.fbx')),use_selection=True,global_scale=1,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False,use_mesh_modifiers=True)
 manifest['assets'].append(dict(name=name,fbx=str(O/(name+'.fbx')),sourceActor=source,materials=[m.name for m in o.data.materials]))

def scoped(label,name,planes,bounds):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(B/'context/fbx'/(label+'.fbx')))
 o=next(x for x in bpy.context.scene.objects if x.type=='MESH'and not x.name.startswith('UCX'));s=next(r for r in actors if r['label']==label)['scale'];old=o.data;uv=old.uv_layers.active;verts=[];faces=[];slots=[];uvs=[];sm=[];affected=0
 for f in old.polygons:
  poly=[]
  for li in f.loop_indices:
   p=o.matrix_world@old.vertices[old.loops[li].vertex_index].co;u=uv.data[li].uv if uv else(0,0)
   poly.append((p.x*s[0]*100,-p.y*s[1]*100,p.z*s[2]*100,u[0],u[1]))
  intersect=all(max(v[i]for v in poly)>=bounds[0][i]-1e-5 and min(v[i]for v in poly)<=bounds[1][i]+1e-5 for i in range(3))
  pieces=subtract(poly,planes)if intersect else[poly]
  if intersect:affected+=1
  for piece in pieces:
   if len(piece)<3:continue
   ids=[]
   for p in piece:ids.append(len(verts));verts.append((p[0]/s[0]/100,-p[1]/s[1]/100,p[2]/s[2]/100));uvs.append(p[3:5])
   faces.append(ids);slots.append(f.material_index);sm.append(f.use_smooth)
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
 for m in old.materials:mesh.materials.append(m)
 for f,slot,smooth in zip(mesh.polygons,slots,sm):f.material_index=slot;f.use_smooth=smooth
 layer=mesh.uv_layers.new(name='UVMap')
 for f in mesh.polygons:
  for li in f.loop_indices:layer.data[li].uv=uvs[mesh.loops[li].vertex_index]
 o.data=mesh;o.matrix_world.identity();export(o,name,label);print('CLIP',label,'affected',affected,'source faces',len(old.polygons),'output',len(mesh.polygons))

cp=circleplanes(320,338)
for label,name in [('HallEnd_SM_HallEnd_ThreeRoomsFloors','SM_NC_OriginalUpperFloorsWithHole'),('HallEnd_SM_HallEnd_ThreeRoomsShell','SM_NC_OriginalUpperShellWithHole'),('HallEnd_COL_HallEnd_ThreeRoomsShell','COL_NC_OriginalUpperShellWithHole')]:scoped(label,name,cp,[[-2036,-1276,320],[-1834,-1074,338]])
dp=[((1,0,0),-2200),((-1,0,0),2281),((0,1,0),-1288),((0,-1,0),1467),((0,0,1),222),((0,0,-1),0)]
for label,name in [('SM_Entrance_m1_m2_Solid','SM_NC_OriginalLowerHallDoorClear'),('COL_Entrance_m1_m2_Solid','COL_NC_OriginalLowerHallDoorClear')]:scoped(label,name,dp,[[-2281,-1467,0],[-2200,-1288,222]])

bpy.ops.wm.read_factory_settings(use_empty=True)
mat={}
for n,r in manifest['materials'].items():m=bpy.data.materials.new(n);m.diffuse_color=(*r['color'],1);mat[n]=m
parts=[]
def box(name,lo,hi,m):
 p=[((lo[i]+hi[i])/2)/100*( -1 if i==1 else 1)for i in range(3)];bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.name=name;o.dimensions=Vector([(hi[i]-lo[i])/100 for i in range(3)]);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mat[m]);parts.append(o);return o

def joined(name):
 bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name=name;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);parts.clear();return o

xmin,xmax=-2212.51019,-1636.8;ymin,ymax=-1509.02675,-973.5
box('LowerFloor',[xmin-16,ymin-12,-10],[xmax+12,ymax+12,.88],'M_NC_FloorBlack')
box('DoorThreshold',[-2281,-1467,-10],[xmin,-1288,.88],'M_NC_FloorBlack')
box('EastWall',[xmax,ymin-12,.88],[xmax+12,ymax+12,332.53],'M_NC_Charcoal')
box('SouthWall',[xmin-16,ymin-12,.88],[xmax,ymin,332.53],'M_NC_Charcoal')
box('NorthWall',[xmin-16,ymax,.88],[xmax,ymax+12,332.53],'M_NC_Charcoal')
box('WestSouth',[xmin-16,ymin,.88],[xmin,-1467,332.53],'M_NC_Charcoal')
box('WestNorth',[xmin-16,-1288,.88],[xmin,ymax,332.53],'M_NC_Charcoal')
box('WestHeader',[xmin-16,-1467,222],[xmin,-1288,332.53],'M_NC_Charcoal')
export(joined('SM_NC_LowerRoomShell'),'SM_NC_LowerRoomShell')
# Polygon floor / ceiling slab with true aperture, including vertical inner faces.
def slab(name,z0,z1,m):
 box(name,[xmin,ymin,z0],[xmax,ymax,z1],m);o=joined(name);old=o.data;verts=[];faces=[]
 for f in old.polygons:
  poly=[(old.vertices[j].co.x*100,-old.vertices[j].co.y*100,old.vertices[j].co.z*100)for j in f.vertices]
  for p in subtract(poly,circleplanes(z0-1,z1+1)):
   faces.append(list(range(len(verts),len(verts)+len(p))));verts.extend([(a/100,-b/100,c/100)for a,b,c in p])
 for i in range(48):
  t=2*math.pi*(i+.5)/48;u=2*math.pi*(i+1.5)/48;r=100/math.cos(math.pi/48)
  xy=[(-1935+r*math.cos(t),-1175+r*math.sin(t)),(-1935+r*math.cos(u),-1175+r*math.sin(u))]
  faces.append(list(range(len(verts),len(verts)+4)));verts.extend([(xy[0][0]/100,-xy[0][1]/100,z0/100),(xy[1][0]/100,-xy[1][1]/100,z0/100),(xy[1][0]/100,-xy[1][1]/100,z1/100),(xy[0][0]/100,-xy[0][1]/100,z1/100)])
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.materials.append(mat[m]);mesh.update();o.data=mesh;export(o,name)
slab('SM_NC_LowerCeilingWithHole',327.03,332.53,'M_NC_CeilingBlack')
slab('SM_NC_UpperDarkFloorWithHole',332.60,333.10,'M_NC_FloorBlack')
# Interior lining keeps all existing walls/furniture outside these volumes untouched.
for zlo,zhi in [(333.1,622.71)]:
 box('EastLiner',[xmax-1.2,ymin,zlo],[xmax,ymax,zhi],'M_NC_Charcoal');box('SouthLiner',[xmin,ymin,zlo],[xmax,ymin+1.2,zhi],'M_NC_Charcoal');box('NorthLiner',[xmin,ymax-1.2,zlo],[xmax,ymax,zhi],'M_NC_Charcoal')
 box('WestSouth',[xmin,ymin,zlo],[xmin+1.2,-1467,zhi],'M_NC_Charcoal');box('WestNorth',[xmin,-1288,zlo],[xmin+1.2,ymax,zhi],'M_NC_Charcoal');box('WestHeader',[xmin,-1467,576],[xmin+1.2,-1288,zhi],'M_NC_Charcoal')
 box('UpperCeiling',[xmin,ymin,621.6],[xmax,ymax,622.7],'M_NC_CeilingBlack')
export(joined('SM_NC_UpperWallLining'),'SM_NC_UpperWallLining')
# Fine square dance floor inlay downstairs, with restrained cyan grout.
for xi in range(6):
 for yi in range(6):
  x=xmin+28+xi*87;y=ymin+28+yi*80
  box('DanceTile',[x,y,.90],[min(x+84,xmax-18),min(y+77,ymax-18),1.04],'M_NC_FloorBlack'if(xi+yi)%2 else'M_NC_FloorSlate')
for i in range(7):
 x=xmin+26+i*87;box('DanceSeam',[x,ymin+26,1.05],[min(x+1.0,xmax-14),ymax-16,1.12],'M_NC_FloorGlow')
for i in range(7):
 y=ymin+26+i*80;box('DanceSeam',[xmin+26,y,1.05],[xmax-16,min(y+1.0,ymax-14),1.12],'M_NC_FloorGlow')
export(joined('SM_NC_LowerDanceFloorInlay'),'SM_NC_LowerDanceFloorInlay')
# Lower entrance jamb fits actual oldhall wall aperture; no block across entry.
for y in [-1467,-1288]:box('EntryJamb',[-2280,y-5,.88],[xmin,y+5,222],'M_NC_Trim')
box('EntryHeader',[-2280,-1467,222],[xmin,-1288,232],'M_NC_Trim')
export(joined('SM_NC_LowerDoorTrim'),'SM_NC_LowerDoorTrim')
json.dump(manifest,open(O/'geometry_manifest.json','w'),indent=2);print('NIGHTCLUB_GEOMETRY_PASS')
