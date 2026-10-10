import bpy,bmesh,json,collections,re,numpy as np
from mathutils import Vector
from pathlib import Path
B=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/StairwellWall')
parts=json.load(open(B/'evidence/parts.json'));rows=json.load(open(B/'evidence/actors.json'))
rects=[[-2334.6,-1355.4,-98.4,0],[-2334.6,-2304.1,0,262.2],[-2147.3,-1355.4,0,262.2],[-2304.1,-2147.3,220.7,262.2],[-2334.6,-1355.4,262.2,300],[-2334.6,-2304.8,300,577.5],[-2124.8,-1355.4,300,577.5],[-2304.8,-2124.8,512.1,577.5]]
Y=869.2
# Match the existing masonry's UV scale and course height.
r=next(r for r in rows if 'HI_WindowCleared_SM_Upper' in r['mesh']);bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=r['export']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH');m=o.data
uv=m.uv_layers.active.data;X=[];V=[]
for fi in parts[179]['faces']:
 f=m.polygons[fi];cs=[o.matrix_world@m.vertices[k].co for k in f.vertices];ps=[Vector((p.x*180,-p.y*180,p.z*100))for p in cs];n=(ps[1]-ps[0]).cross(ps[2]-ps[0]).normalized()
 if n.y<-.999999 and sum(p.y for p in ps)/len(ps)>835.9 and (ps[1]-ps[0]).cross(ps[2]-ps[0]).length>10000:
  for p,li in zip(ps,f.loop_indices):X.append([p.x,p.z,1]);V.append(list(uv[li].uv))
assert len(X)>=3
X=X[:3];V=V[:3]
fit=np.linalg.lstsq(np.array(X),np.array(V),rcond=None)[0];print('UV_FIT',fit.tolist())
report=[]
for label,kind,wallids,rr in [('SM_Entrance_p0_m2_Solid','Entrance',[130,131,132,134],rects[:4]),('SM_Upper_p0_m2_Solid','Upper',[179,180,182],rects[4:])]:
 r=next(r for r in rows if r['label']==label);bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=r['export']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH');m=o.data
 def wp(v):
  p=o.matrix_world@v;return Vector((p.x*180,-p.y*180,p.z*100))
 railids=[i for i,p in enumerate(parts) if p['mesh']==r['mesh'] and any('MetalDark' in x for x in p['materials']) and p['lo'][1]>790 and p['hi'][1]<900]
 drop={f for i in railids for f in parts[i]['faces']};frontdrop=0
 for i in wallids:
  for fi in parts[i]['faces']:
   f=m.polygons[fi];ps=[wp(m.vertices[v].co)for v in f.vertices];n=(ps[1]-ps[0]).cross(ps[2]-ps[0]).normalized()
   if n.y<-.15 and sum(p.y for p in ps)/len(ps)>815:drop.add(fi);frontdrop+=1
 bm=bmesh.new();bm.from_mesh(m);bm.faces.ensure_lookup_table();bmesh.ops.delete(bm,geom=[bm.faces[i] for i in drop],context='FACES');bm.to_mesh(m);bm.free();m.update()
 # New wall shares one exact plane and one UV projection. Opposite hallway faces stay intact.
 mat=bpy.data.materials.new('MI_SSO_S03MasonryCream');mat.diffuse_color=(.63,.56,.37,1)
 verts=[];faces=[]
 for x0,x1,z0,z1 in rr:
  k=len(verts);verts.extend([(x0/180,-Y/180,z0/100),(x1/180,-Y/180,z0/100),(x1/180,-Y/180,z1/100),(x0/180,-Y/180,z1/100)]);faces.append((k,k+1,k+2,k+3))
 mesh=bpy.data.meshes.new('Flush brick courses');mesh.from_pydata(verts,[],faces);mesh.materials.append(mat);ob=bpy.data.objects.new('Flush brick courses',mesh);bpy.context.collection.objects.link(ob);uvs=mesh.uv_layers.new(name=m.uv_layers[0].name)
 for f in mesh.polygons:
  for li in f.loop_indices:
   v=mesh.vertices[mesh.loops[li].vertex_index].co;u=np.array([v.x*180,v.z*100,1])@fit;uvs.data[li].uv=u
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);ob.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.join();o.name='SM_STAIR_'+kind+'_Flush'
 path=B/(o.name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE')
 report.append(dict(label=label,name=o.name,file=str(path),removed_triangles=len(drop),rail_pieces_removed=len(railids),wall_faces_removed=frontdrop,materials=r['materials']))
json.dump(dict(meshes=report,rectangles=rects,front_plane_y=Y,uv_fit=fit.tolist()),open(B/'evidence/build.json','w'),indent=2)
print('STAIR_GEOMETRY_PASS',report)
