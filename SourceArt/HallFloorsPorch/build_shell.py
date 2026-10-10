import bpy,bmesh,json,numpy as np
from mathutils import Vector
from pathlib import Path
B=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HallFloorsPorch');rows=json.load(open(B/'context/actors.json'));parts=json.load(open(B/'context/parts.json'));exports=json.load(open(B/'context/exports.json'))
# Only these existing architectural floor components are edited. Bedroom overlays stay intact.
plans={
'SM_Entrance_m1_m2_Solid':[(264,'HFP_HallCoating',[(-2359.8,-2025,-1360,838),(-2359.8,-1313.9,-891,-544)])],
'SM_Entrance_p0_m2_Solid':[(934,'HFP_BathCoating',None),(948,'HFP_LandingCoating',None),(949,'HFP_LandingCoating',None),(950,'HFP_LandingCoating',None)],
'SM_Upper_m1_m2_Solid':[(1732,'HFP_HallCoating',[(-2335.4,-2026.3,-1386.3,854.9),(-2335.4,-1298.5,-840.6,-534.8)])],
'SM_Upper_p0_m2_Solid':[(2312,'HFP_LandingCoating',None),(2313,'HFP_LandingCoating',None),(2349,'HFP_HallCoating',None)],
'SM_Upper_m1_m1_Solid':[(1667,'HFP_PorchCoating',None),(1668,'HFP_PorchCoating',None)],
'SM_Upper_m1_p0_Solid':[(1918,'HFP_GalleryOak',None)]}
# Linear clipping interpolates all existing UV coordinates at new vertices.
def cut(poly,axis,bound,greater):
 out=[]
 if not poly:return out
 for a,b in zip(poly,poly[1:]+poly[:1]):
  va,vb=a[axis]-bound,b[axis]-bound;ia=va>=-1e-6 if greater else va<=1e-6;ib=vb>=-1e-6 if greater else vb<=1e-6
  if ia:out.append(a)
  if ia!=ib:out.append(a+(b-a)*(va/(va-vb)))
 return out

def split_rect(poly,r,axes=(0,1)):
 inside=poly;outside=[]
 for axis,bound,gt in [(axes[0],r[0],True),(axes[0],r[1],False),(axes[1],r[2],True),(axes[1],r[3],False)]:
  p=cut(inside,axis,bound,not gt)
  if len(p)>=3:outside.append(p)
  inside=cut(inside,axis,bound,gt)
  if len(inside)<3:inside=[];break
 return inside,outside
report=[]
for label,plan in plans.items():
 a=next(x for x in rows if x['label']==label);c=a['components'][0];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=exports[c['mesh']]);o=next(x for x in bpy.context.scene.objects if x.type=='MESH' and not x.name.startswith('UCX'));m=o.data;uv0=m.uv_layers[0];uvname=uv0.name
 matidx={};modified=0;drop=set();new=[]
 for pid,name,rects in plan:
  if name not in matidx:m.materials.append(bpy.data.materials.new(name));matidx[name]=len(m.materials)-1
  for fi in parts[pid]['faces']:
   f=m.polygons[fi];cs=[o.matrix_world@m.vertices[i].co for i in f.vertices];normal=(cs[1]-cs[0]).cross(cs[2]-cs[0]).normalized()
   if normal.z<.9:continue
   modified+=1
   if rects is None:f.material_index=matidx[name];continue
   p=[np.array([v.x*180,-v.y*180,v.z*100,*uv0.data[li].uv],dtype=float) for v,li in zip(cs,f.loop_indices)]
   remaining=[p];assigned=[]
   for rect in rects:
    nextremain=[]
    for poly in remaining:
     inner,outer=split_rect(poly,rect);nextremain+=outer
     if inner:assigned.append(inner)
    remaining=nextremain
   if assigned:
    drop.add(fi);new.extend((p,matidx[name])for p in assigned);new.extend((p,f.material_index)for p in remaining)
 # Stair treads receive a dark, scuffed version, without changing their steel sides/supports.
 if label in ['SM_Entrance_p0_m2_Solid','SM_Upper_p0_m2_Solid']:
  m.materials.append(bpy.data.materials.new('HFP_StairPaint'));mi=len(m.materials)-1
  for f in m.polygons:
   if 'StairTreadSteel' not in m.materials[f.material_index].name:continue
   cs=[o.matrix_world@m.vertices[i].co for i in f.vertices]
   if (cs[1]-cs[0]).cross(cs[2]-cs[0]).normalized().z>.9:f.material_index=mi;modified+=1
 # Cut a real opening in the upper landing's north masonry across from the stair door.
 if label=='SM_Entrance_p0_m2_Solid':
  for fi in parts[946]['faces']:
   f=m.polygons[fi];cs=[o.matrix_world@m.vertices[i].co for i in f.vertices]
   p=[np.array([v.x*180,-v.y*180,v.z*100,*uv0.data[li].uv],dtype=float) for v,li in zip(cs,f.loop_indices)]
   # The top, bottom and end caps do not overlap the opening.
   inner,outer=split_rect(p,(-2300,-2120,340,555),(0,2))
   if inner:drop.add(fi);new.extend((p,f.material_index) for p in outer)
 bm=bmesh.new();bm.from_mesh(m);bm.faces.ensure_lookup_table();bmesh.ops.delete(bm,geom=[bm.faces[i]for i in drop],context='FACES');bm.to_mesh(m);bm.free();m.update()
 if new:
  vs=[];fs=[];mis=[];uvs=[]
  for poly,mi in new:
   k=len(vs);vs.extend((p[0]/180,-p[1]/180,p[2]/100)for p in poly);uvs.extend((p[3],p[4])for p in poly);fs.append(tuple(range(k,k+len(poly))));mis.append(mi)
  add=bpy.data.meshes.new('Clipped floor and window');add.from_pydata(vs,[],fs)
  for mat in m.materials:add.materials.append(mat)
  layer=add.uv_layers.new(name=uvname)
  for f,mi in zip(add.polygons,mis):
   f.material_index=mi
   for li in f.loop_indices:layer.data[li].uv=uvs[add.loops[li].vertex_index]
  ob=bpy.data.objects.new('Clipped floor and window',add);bpy.context.collection.objects.link(ob)
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);ob.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.join()
 name='SM_HFP_'+label.replace('SM_','');o.name=name
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 file=B/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE')
 report.append(dict(label=label,name=name,file=str(file),old_mesh=c['mesh'],materials=c['materials'],modified_floor_faces=modified,clipped_faces=len(drop)))
json.dump(report,open(B/'shell_plan.json','w'),indent=2);print('HFP_SHELL_PASS',[(x['label'],x['modified_floor_faces'])for x in report])
