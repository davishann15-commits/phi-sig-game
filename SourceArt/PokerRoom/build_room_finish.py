"""Independent room lining, rug and surgical removal of only the west lamp."""
import bpy, json, math
from pathlib import Path
from mathutils import Vector

ROOT=Path('/Users/Stewart/Projects/Senior sendoff/outputs/poker-room-v1')
OUT=ROOT/'geometry'; OUT.mkdir(exist_ok=True)
PLAN={'meshes':[],'actors':[],'replacements':[],'materials':{},'textures':[]}
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
GROUPS={}
SPECS={
 'POK_M_WallWine':{'color':[.115,.025,.033],'roughness':.89,'base_texture':'POK_T_DecoWallpaper.png'},
 'POK_M_PanelWalnut':{'color':[.12,.05,.025],'roughness':.47},
 'POK_M_RecessWalnut':{'color':[.064,.025,.013],'roughness':.59},
 'POK_M_RoomBrass':{'color':[.44,.26,.075],'roughness':.36,'metallic':.83},
 'POK_M_Rug':{'color':[.22,.025,.035],'roughness':.93,'base_texture':'POK_T_Rug.png'},
 'POK_M_SconceGlass':{'color':[.93,.60,.26],'roughness':.4,'emission':1.4},
}
def cm(p):return Vector((p[0]/100,-p[1]/100,p[2]/100))
def material(name):
 m=bpy.data.materials.get(name)
 if not m:
  m=bpy.data.materials.new(name);m.diffuse_color=(*SPECS[name]['color'],1)
  PLAN['materials'][name]=SPECS[name]
 return m
def add(o,name,group,ma):
 o.name=name;o.data.materials.append(material(ma));GROUPS.setdefault(group,[]).append(o);return o
def cube(name,lo,hi,ma,group,bevel=.25):
 p=[(lo[i]+hi[i])/2 for i in range(3)]
 bpy.ops.mesh.primitive_cube_add(size=1,location=cm(p));o=bpy.context.object
 o.dimensions=[(hi[i]-lo[i])/100 for i in range(3)]
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 add(o,name,group,ma)
 if bevel:
  mod=o.modifiers.new('Soft crafted edges','BEVEL');mod.width=bevel/100;mod.segments=2
  bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
  for f in o.data.polygons:f.use_smooth=True
  n=o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL');n.keep_sharp=True
  bpy.ops.object.modifier_apply(modifier=n.name)
 return o
def plane(name,points,uvs,ma,group):
 if ma=='POK_M_WallWine':
  axis=1 if abs(points[0][0]-points[1][0])<.01 else 0
  low=min(p[axis] for p in points);zlo=min(p[2] for p in points)
  uvs=[((p[axis]-low)/65,(p[2]-zlo)/65) for p in points]
 m=bpy.data.meshes.new(name);m.from_pydata([cm(p) for p in points],[],[(3,2,1,0)]);m.update()
 u=m.uv_layers.new(name='UVMap')
 for loop in m.loops:u.data[loop.index].uv=uvs[loop.vertex_index]
 o=bpy.data.objects.new(name,m);bpy.context.collection.objects.link(o);add(o,name,group,ma);return o
def export_group(group):
 obs=GROUPS[group];bpy.ops.object.select_all(action='DESELECT')
 for o in obs:o.select_set(True)
 bpy.context.view_layer.objects.active=obs[0];bpy.ops.object.join();o=bpy.context.object;o.name=group
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 o.data.calc_loop_triangles()
 pts=[v.co for v in o.data.vertices]
 lo=[min(p[i] for p in pts) for i in range(3)];hi=[max(p[i] for p in pts) for i in range(3)]
 path=OUT/(group+'.fbx')
 bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',bake_space_transform=False)
 bounds=[[100*lo[0],-100*hi[1],100*lo[2]],[100*hi[0],-100*lo[1],100*hi[2]]]
 PLAN['meshes'].append({'name':group,'file':str(path),'boundsCm':bounds,'triangles':len(o.data.loop_triangles),'materialSlots':[m.name for m in o.data.materials]})
 PLAN['actors'].append({'label':group,'mesh':group,'location':[0,0,0],'scale':[1,1,1],'yaw':0,'collision':'NoCollision'})

FZ=302.3;CZ=566.1;X0=-2972;X1=-2349.2328;Y0=-1371.8425;Y1=-885
# Separate surface planes avoid any shared material changes in the house.
W='POK_SM_Wallpaper';P='POK_SM_Wainscot';B='POK_SM_BrassTrim'
segments=[('west',X0+1.2,Y0,Y1),('east',X1-1.2,Y0,-1342),('east',X1-1.2,-1148,Y1)]
for side,x,a,b in segments:
 pts=[[x,a,FZ+90],[x,b,FZ+90],[x,b,CZ-.3],[x,a,CZ-.3]]
 if side=='east':pts=pts[::-1]
 plane(side+'Paper'+str(a),pts,[[0,0],[(b-a)/65,0],[(b-a)/65,(CZ-FZ-90)/65],[0,(CZ-FZ-90)/65]],'POK_M_WallWine',W)
 xlo,xhi=(x-.3,x+2.0) if side=='west' else (x-2.0,x+.3)
 cube(side+'PanelBase'+str(a),[xlo,a,FZ],[xhi,b,FZ+88],'POK_M_RecessWalnut',P)
 for z in [FZ+5,FZ+14,FZ+78,FZ+86]:cube(side+'Rail'+str(z),[xlo-1,a,z],[xhi+1,b,z+3.5],'POK_M_PanelWalnut',P)
 count=max(1,round((b-a)/70))
 for i in range(count+1):
  yy=a+(b-a)*i/count
  cube(side+'Stile'+str(i),[xlo-.7,yy-2,FZ+14],[xhi+.7,yy+2,FZ+82],'POK_M_PanelWalnut',P)
 cube(side+'BrassLine'+str(a),[xlo-.9,a,FZ+89],[xhi+.9,b,FZ+89.6],'POK_M_RoomBrass',B,.08)
for side,y in [('south',Y0+1.2),('north',Y1-1.2)]:
 pts=[[X0,y,FZ+90],[X1,y,FZ+90],[X1,y,CZ-.3],[X0,y,CZ-.3]]
 if side=='south':pts=pts[::-1]
 plane(side+'Paper',pts,[[0,0],[(X1-X0)/65,0],[(X1-X0)/65,(CZ-FZ-90)/65],[0,(CZ-FZ-90)/65]],'POK_M_WallWine',W)
 yl,yh=(y-.3,y+2) if side=='south' else (y-2,y+.3)
 cube(side+'PanelBase',[X0,yl,FZ],[X1,yh,FZ+88],'POK_M_RecessWalnut',P)
 for z in [FZ+5,FZ+14,FZ+78,FZ+86]:cube(side+'Rail'+str(z),[X0,yl-1,z],[X1,yh+1,z+3.5],'POK_M_PanelWalnut',P)
 count=9
 for i in range(count+1):
  xx=X0+(X1-X0)*i/count
  cube(side+'Stile'+str(i),[xx-2,yl-.7,FZ+14],[xx+2,yh+.7,FZ+82],'POK_M_PanelWalnut',P)
 cube(side+'BrassLine',[X0,yl-.9,FZ+89],[X1,yh+.9,FZ+89.6],'POK_M_RoomBrass',B,.08)
# Soft carpet on the original solid floor; no artificial step or collider.
plane('Burgundy woven rug',[[-2905,-1280,FZ+.18],[-2435,-1280,FZ+.18],[-2435,-940,FZ+.18],[-2905,-940,FZ+.18]],[[0,0],[1,0],[1,1],[0,1]],'POK_M_Rug','POK_SM_Rug')
# Two brass wall sconces, backed by matching actual point lights in the import.
for i,y in enumerate([-1230,-1010]):
 cube('SconceBack'+str(i),[X0+3,y-9,449],[X0+6,y+9,490],'POK_M_RoomBrass','POK_SM_WallSconces',1)
 cube('SconceGlass'+str(i),[X0+6,y-6,455],[X0+12,y+6,486],'POK_M_SconceGlass','POK_SM_WallSconces',1.5)
 cube('SconceCap'+str(i),[X0+5,y-8,486],[X0+13,y+8,489],'POK_M_RoomBrass','POK_SM_WallSconces',.6)
 cube('SconceBase'+str(i),[X0+5,y-8,452],[X0+13,y+8,455],'POK_M_RoomBrass','POK_SM_WallSconces',.6)
for g in list(GROUPS):export_group(g)

# Surgical fixture replacement: keep every east/end/hall triangle and UV.
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(ROOT/'context/HallEnd_SM_HallEnd_ThreeRoomsPracticalLights.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH');m=o.data
pts=[o.matrix_world@v.co for v in m.vertices];normalmatrix=o.matrix_world.to_3x3().inverted().transposed()
verts=[];faces=[];uvs=[];normals=[];mis=[];removed=0;kept=0
for f in m.polygons:
 center=sum((pts[i] for i in f.vertices),Vector())/len(f.vertices)
 ue=[center.x*100,-center.y*100,center.z*100]
 if ue[0]<-2349 and -1372<ue[1]<-885:removed+=1;continue
 face=[]
 for vi,li in zip(f.vertices,f.loop_indices):
  face.append(len(verts));verts.append(pts[vi]);uvs.append(m.uv_layers.active.data[li].uv.copy());normals.append((normalmatrix@m.corner_normals[li].vector).normalized())
 faces.append(face);mis.append(f.material_index);kept+=1
assert removed>0 and kept>0,(removed,kept)
mesh=bpy.data.meshes.new('POK_SM_OtherHallPracticalLights');mesh.from_pydata(verts,[],faces);mesh.update()
for ma in m.materials:mesh.materials.append(ma)
for f,mi in zip(mesh.polygons,mis):f.material_index=mi;f.use_smooth=True
uv=mesh.uv_layers.new(name='UVMap')
for i,v in enumerate(uvs):uv.data[i].uv=v
mesh.normals_split_custom_set(normals)
new=bpy.data.objects.new('POK_SM_OtherHallPracticalLights',mesh);bpy.context.collection.objects.link(new)
bpy.ops.object.select_all(action='DESELECT');new.select_set(True);bpy.context.view_layer.objects.active=new
path=OUT/'POK_SM_OtherHallPracticalLights.fbx'
bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE')
mesh.calc_loop_triangles()
PLAN['meshes'].append({'name':new.name,'file':str(path),'triangles':len(mesh.loop_triangles),'materialSlots':[ma.name for ma in mesh.materials]})
PLAN['replacements'].append({'actor':'HallEnd_SM_HallEnd_ThreeRoomsPracticalLights','mesh':new.name,'collision':'NoCollision'})
PLAN['fixtureAudit']={'westFacesRemoved':removed,'otherRoomFacesPreserved':kept,'method':'Exact original positions UVs materials and normals copied for every retained polygon'}
PLAN['textures']=['POK_T_DecoWallpaper.png','POK_T_Rug.png']
json.dump(PLAN,open(OUT/'incremental_manifest.json','w'),indent=2)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'room_finish.blend'))
print('POKER_FINISH_READY',sum(r['triangles'] for r in PLAN['meshes']),PLAN['fixtureAudit'],flush=True)
