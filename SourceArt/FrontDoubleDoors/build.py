"""Craft beveled walnut entry doors; remove only the old entrance frame islands."""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
B=Path(__file__).resolve().parent;O=B/'models';O.mkdir(exist_ok=True)
rows=json.loads((B/'audit.json').read_text());cuts=[]
for a in rows:
 if not a['label'].startswith('SM_Entrance'):continue
 for c in a['components']:
  if not c.get('exported'):continue
  src=B/'meshes'/(c['mesh'].split('.')[-1]+'.fbx')
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(src))
  o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX','UBX')))
  bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001)
  seen=set();remove=[];removed_bounds=[]
  for v in list(bm.verts):
   if v in seen:continue
   stack=[v];seen.add(v);vs=[]
   while stack:
    w=stack.pop();vs.append(w)
    for e in w.link_edges:
     z=e.other_vert(w)
     if z not in seen:seen.add(z);stack.append(z)
   ps=[o.matrix_world@w.co for w in vs]
   ps=[(p.x*100*c['scale'][0]+c['loc'][0],-p.y*100*c['scale'][1]+c['loc'][1],p.z*100*c['scale'][2]+c['loc'][2]) for p in ps]
   lo=[min(p[j] for p in ps) for j in range(3)];hi=[max(p[j] for p in ps) for j in range(3)]
   # Whole frame and brass islands only: walls and floors extend past this box.
   if all(lo[j]>=(-211,-955,-5)[j] and hi[j]<=(140,-897,289)[j] for j in range(3)):
    remove.extend(vs);removed_bounds.append([lo,hi])
  if not remove:bm.free();continue
  name=a['label']+'_FrontDoorCut';bmesh.ops.delete(bm,geom=remove,context='VERTS')
  dead=[f for f in bm.faces if f.calc_area()<1e-14]
  if dead:bmesh.ops.delete(bm,geom=dead,context='FACES_ONLY')
  bm.to_mesh(o.data);bm.free();o.data.update()
  # Some original scan faces have collapsed UVs. Repair just those faces,
  # keeping the existing mapping on every valid face and all geometry intact.
  uv=o.data.uv_layers[0]
  for f in o.data.polygons:
   lis=list(f.loop_indices)
   if len(lis)<3:continue
   u,v,w=[uv.data[i].uv.copy() for i in lis[:3]]
   if abs((v.x-u.x)*(w.y-u.y)-(v.y-u.y)*(w.x-u.x))<1e-12:
    n=f.normal;ax=max(range(3),key=lambda j:abs(n[j]));js=[j for j in range(3) if j!=ax]
    for li in lis:
     p=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(p[js[0]],p[js[1]])
  o.name=name;bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
  path=O/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False)
  cuts.append(dict(name=name,fbx=str(path),source_actor=a['label'],old_asset=c['mesh'],materials=c['materials'],removed_islands=removed_bounds))

bpy.ops.wm.read_factory_settings(use_empty=True)
mats={n:bpy.data.materials.new(n) for n in ['M_FD_Walnut','M_FD_WalnutRail','M_FD_WalnutPanel','M_FD_WalnutDark','M_FD_Brass','M_FD_BrassDark','M_FD_Black']}
parts=[]
def mat(o,n):o.data.materials.append(mats[n]);parts.append(o);return o
def uvgrain(o,horizontal=False):
 uv=o.data.uv_layers.active or o.data.uv_layers.new(name='UVMap')
 for p in o.data.polygons:
  for li in p.loop_indices:
   co=o.data.vertices[o.data.loops[li].vertex_index].co
   n=p.normal
   if abs(n.z)>max(abs(n.x),abs(n.y)):
    u,v=(co.y/.32,co.x/1.4) if horizontal else(co.x/.32,co.y/1.4)
   elif abs(n.x)>abs(n.y):
    u,v=(co.z/.32,co.y/1.4) if horizontal else(co.y/.32,co.z/1.4)
   else:u,v=(co.z/.32,co.x/1.4) if horizontal else(co.x/.32,co.z/1.4)
   uv.data[li].uv=(u+.19,v+.07)
def bevel(o,w,segments=3):
 if w:
  m=o.modifiers.new('Crafted softened edges','BEVEL');m.width=w/100;m.segments=segments
  bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=m.name)
 for p in o.data.polygons:p.use_smooth=True
 m=o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL');m.keep_sharp=True;m.weight=50
 bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=m.name)
def box(n,x0,x1,y0,y1,z0,z1,ma='M_FD_Walnut',b=.3):
 bpy.ops.mesh.primitive_cube_add(size=1,location=((x0+x1)/200,-(y0+y1)/200,(z0+z1)/200));o=bpy.context.object;o.name=n;o.dimensions=((x1-x0)/100,(y1-y0)/100,(z1-z0)/100)
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);bevel(o,b);uvgrain(o,ma=='M_FD_WalnutRail');return mat(o,ma)
def tube(n,a,b,r,ma='M_FD_Brass',sides=20):
 aa=Vector((a[0]/100,-a[1]/100,a[2]/100));bb=Vector((b[0]/100,-b[1]/100,b[2]/100));d=bb-aa
 bpy.ops.mesh.primitive_cylinder_add(vertices=sides,radius=r/100,depth=d.length,location=(aa+bb)/2);o=bpy.context.object;o.name=n;o.rotation_euler=d.to_track_quat('Z','Y').to_euler()
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);bevel(o,.12,2);return mat(o,ma)
def sphere(n,p,r,ma='M_FD_Brass'):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=20,ring_count=12,radius=r/100,location=(p[0]/100,-p[1]/100,p[2]/100));o=bpy.context.object;o.name=n
 for f in o.data.polygons:f.use_smooth=True
 return mat(o,ma)
def ring(n,x0,x1,z0,z1,side,profile,ma):
 vs=[];fs=[]
 for inset,depth in profile:
  vs.extend([(x0+inset,side*depth,z0+inset),(x1-inset,side*depth,z0+inset),(x1-inset,side*depth,z1-inset),(x0+inset,side*depth,z1-inset)])
 for j in range(len(profile)-1):
  for k in range(4):fs.append((j*4+k,j*4+(k+1)%4,(j+1)*4+(k+1)%4,(j+1)*4+k))
 if side<0:fs=[tuple(reversed(f)) for f in fs]
 me=bpy.data.meshes.new(n);me.from_pydata([(x/100,-y/100,z/100) for x,y,z in vs],[],fs);me.update();o=bpy.data.objects.new(n,me);bpy.context.collection.objects.link(o);uvgrain(o);return mat(o,ma)
def export(name,obs,collisions):
 bpy.ops.object.select_all(action='DESELECT')
 for o in obs:o.select_set(True)
 bpy.context.view_layer.objects.active=obs[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');o.select_set(False)
 cs=[]
 for i,b in enumerate(collisions):
  q=box('UCX_'+name+'_'+str(i).zfill(2),*b,ma='M_FD_Black',b=0);q.data.materials.clear();cs.append(q)
 bpy.ops.object.select_all(action='DESELECT')
 for q in [o]+cs:q.select_set(True)
 bpy.context.view_layer.objects.active=o
 path=O/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',add_leaf_bones=False)
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);parts.clear()
 return str(path)

W=138.6;H=257.4
box('Solid walnut core',.25,W-.25,-2.8,2.8,.15,H-.15,'M_FD_WalnutDark',.22)
for x0,x1 in [(.3,12.5),(W-12.5,W-.3)]:box('Walnut stile',x0,x1,-3.05,3.05,.4,H-.4,b=.35)
for z0,z1 in [(0.4,13.2),(71,83.8),(168.2,180.8),(245,H-.4)]:box('Cross grain rail',12,W-12,-3.1,3.1,z0,z1,'M_FD_WalnutRail',.3)
box('Center muntin',W/2-3.5,W/2+3.5,-3.05,3.05,13,H-12,b=.25)
for side in [-1,1]:
 for x0,x1 in [(13.4,W/2-4.5),(W/2+4.5,W-13.4)]:
  for z0,z1 in [(14.1,70),(84.6,167.4),(181.6,244)]:
   ring('Ogee panel molding',x0,x1,z0,z1,side,[(0,3.07),(.65,3.58),(1.5,3.82),(2.4,3.72),(3.0,3.25),(4.2,3.0),(5.1,4.05),(5.8,4.25)],'M_FD_Walnut')
   a,b=sorted((side*2.78,side*4.2));box('Raised walnut field',x0+5.7,x1-5.7,a,b,z0+5.7,z1-5.7,'M_FD_WalnutPanel',.45)
 hx=W-8.0
 a,b=sorted((side*3.2,side*3.85));box('Brass handle backplate',hx-2.25,hx+2.25,a,b,98,136,'M_FD_BrassDark',.7)
 for zz in [101,133]:
  sphere('Backplate screw',(hx,side*3.98,zz),.48,'M_FD_BrassDark')
  a,b=sorted((side*3.95,side*4.08));box('Screw slot',hx-.25,hx+.25,a,b,zz-.035,zz+.035,'M_FD_Black',.015)
 curve=[(hx,side*(3.9+5*math.sin(math.pi*i/20)),105+24*i/20) for i in range(21)]
 for a,b in zip(curve,curve[1:]):tube('Arched brass pull',a,b,.88)
 for p in [curve[0],curve[-1]]:sphere('Pull end cap',p,1.04)
 tube('Round lock surround',(hx,side*3.65,145),(hx,side*4.05,145),1.85,'M_FD_BrassDark')
 a,b=sorted((side*4.05,side*4.15));box('Keyhole',hx-.16,hx+.16,a,b,144.2,145.7,'M_FD_Black',.06)
 a,b=sorted((side*3.12,side*3.35));box('Brass kick plate',12,W-12,a,b,2.4,9.2,'M_FD_BrassDark',.2)
 for x in [14,W-14]:
  for z in [3.8,7.8]:sphere('Kick plate screw',(x,side*3.42,z),.24,'M_FD_BrassDark')
for z in [27,129,231]:
 tube('Hinge barrel',(0,-3.3,z-4),(0,-3.3,z+4),.85,'M_FD_BrassDark')
 for zz in [z-3.8,z-2,z,z+2,z+3.8]:tube('Hinge knuckle seam',(0,-3.3,zz-.045),(0,-3.3,zz+.045),.89,'M_FD_Black')
 box('Hinge plate',.25,4,-3.36,-3.14,z-4,z+4,'M_FD_BrassDark',.2)
leaf=export('SM_FD_WalnutLeaf',list(parts),[(.25,W-.25,-3.05,3.05,.15,H-.15)])

# Frame origin is the shared doorway center at (-36.65,-989,0).
XL=-149.47;XR=149.51;IL=-139.15;IR=139.15
box('Deep left jamb',XL,IL,-22,88,0,264.3,b=.25)
box('Deep right jamb',IR,XR,-22,88,0,264.3,b=.25)
box('Deep lintel',XL,XR,-22,88,260.5,270.6,'M_FD_WalnutRail',.25)
for y in [-24,90]:
 for x0,x1 in [(XL-10,IL-1),(IR+1,XR+10)]:
  box('Stepped vertical casing',x0,x1,y-1.4,y+1.4,0,271,'M_FD_Walnut',.38)
  box('Rounded casing bead',x0+3,x0+5,y-2,y+2,1,270,'M_FD_WalnutPanel',.9)
 box('Header crown',XL-13,XR+13,y-2,y+2,271,280,'M_FD_WalnutRail',.8)
 box('Crown cap',XL-15,XR+15,y-3.2,y+3.2,278.5,282,'M_FD_WalnutRail',.6)
box('Left stop',IL-.8,IL+.55,4,5.7,1.4,260.5,'M_FD_WalnutDark',.2)
box('Right stop',IR-.55,IR+.8,4,5.7,1.4,260.5,'M_FD_WalnutDark',.2)
box('Threshold',XL,XR,-24,77,-1,1.1,'M_FD_BrassDark',.55)
frame=export('SM_FD_WalnutFrame',list(parts),[(XL,IL,-22,88,0,264.3),(IR,XR,-22,88,0,264.3),(XL,XR,-22,88,260.5,270.6),(XL,XR,-24,77,-1,1.1)])
manifest=dict(cuts=cuts,leaf=leaf,frame=frame,width=W,height=H,hinges=[[-175.8,-989,1.7],[102.5,-989,1.7]],center=[-36.65,-989,129.5],materials=list(mats))
(B/'manifest.json').write_text(json.dumps(manifest,indent=2));print('FRONT_DOORS_BUILT',len(cuts),flush=True)
