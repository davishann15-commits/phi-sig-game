def mesh(n,v,f,m,g,smooth=False):
 me=bpy.data.meshes.new(n);me.from_pydata(v,[],f);me.update();o=bpy.data.objects.new(n,me);bpy.context.collection.objects.link(o)
 if m:me.materials.append(m)
 for p in me.polygons:p.use_smooth=smooth
 if g:G[g].append(o)
 return o
def box(n,c,s,m,g,bevel=.006,segments=3):
 x,y,z=[a/2 for a in s];o=mesh(n,[(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),(-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)],[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],m,g);o.location=c
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 if bevel:
  b=o.modifiers.new('Rounded joinery','BEVEL');b.width=min(bevel,min(s)*.46);b.segments=segments;bpy.ops.object.modifier_apply(modifier=b.name)
  for p in o.data.polygons:p.use_smooth=True
  try:
   b=o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL');b.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=b.name)
  except RuntimeError:pass
 o.select_set(False);return o
def cyl(n,c,r,h,m,g,vertices=24,axis='Z'):
 bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=h,location=c);o=bpy.context.object;o.name=n
 if axis=='Y':o.rotation_euler.x=math.pi/2
 if axis=='X':o.rotation_euler.y=math.pi/2
 o.data.materials.append(m)
 for p in o.data.polygons:p.use_smooth=len(p.vertices)==4
 G[g].append(o);return o
def ell(n,c,s,m,g):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=20,ring_count=12,location=c);o=bpy.context.object;o.name=n;o.scale=s;o.data.materials.append(m)
 for p in o.data.polygons:p.use_smooth=True
 G[g].append(o);return o
def tube(n,pts,r,m,g,sides=7,closed=False):
 pts=[Vector(p)for p in pts];v=[];f=[];N=len(pts)
 for i,p in enumerate(pts):
  t=(pts[(i+1)%N]if closed or i<N-1 else pts[i])-(pts[(i-1)%N]if closed or i else pts[i]);t.normalize();ref=Vector((0,0,1))if abs(t.z)<.92 else Vector((0,1,0));u=t.cross(ref).normalized();w=t.cross(u)
  for j in range(sides):a=j*math.tau/sides;v.append(tuple(p+r*(math.cos(a)*u+math.sin(a)*w)))
 for i in range(N if closed else N-1):
  for j in range(sides):f.append((i*sides+j,i*sides+(j+1)%sides,((i+1)%N)*sides+(j+1)%sides,((i+1)%N)*sides+j))
 if not closed:f += [tuple(range(sides-1,-1,-1)),tuple((N-1)*sides+j for j in range(sides))]
 return mesh(n,v,f,m,g,True)
def text(n,s,c,size,m,g,rotation=(math.pi/2,0,0),align='CENTER'):
 cu=bpy.data.curves.new(n,'FONT');cu.body=s;cu.align_x=align;cu.size=size;cu.extrude=.0003;cu.resolution_u=2;o=bpy.data.objects.new(n,cu);bpy.context.collection.objects.link(o);o.location=c;o.rotation_euler=rotation;cu.materials.append(m)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH');o=bpy.context.object;o.select_set(False);G[g].append(o);return o
def flat(n,c,w,h,m,g,vertical=False,uv=None):
 x,y,z=c;v=[(x-w/2,y,z-h/2),(x+w/2,y,z-h/2),(x+w/2,y,z+h/2),(x-w/2,y,z+h/2)]if vertical else [(x-w/2,y-h/2,z),(x+w/2,y-h/2,z),(x+w/2,y+h/2,z),(x-w/2,y+h/2,z)]
 o=mesh(n,v,[(0,1,2,3)],m,g);layer=o.data.uv_layers.new(name='UVMap')
 for i,p in enumerate(uv or [(0,0),(1,0),(1,1),(0,1)]):layer.data[i].uv=p
 return o
def uv_auto(o):
 if o.data.uv_layers:return
 layer=o.data.uv_layers.new(name='UVMap')
 for p in o.data.polygons:
  ax=max(range(3),key=lambda k:abs(p.normal[k]));axes=[k for k in range(3)if k!=ax]
  for li in p.loop_indices:
   q=o.data.vertices[o.data.loops[li].vertex_index].co;layer.data[li].uv=(q[axes[0]]*1.4,q[axes[1]]*1.4)
def finalize(g,mass=12,health=24,movable=True,wall=False,damage=True):
 objects=G[g]
 for o in objects:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);uv_auto(o)
  for v in o.data.vertices:v.co.x*=1.8;v.co.y*=1.8
 mats=[]
 for o in objects:
  for m in o.data.materials:
   if m not in mats:mats.append(m)
 for o in objects:
  old=list(o.data.materials);ix=[mats.index(old[p.material_index])for p in o.data.polygons];o.data.materials.clear()
  for m in mats:o.data.materials.append(m)
  for p,i in zip(o.data.polygons,ix):p.material_index=i
 name='SM_Kitchen_'+g;ob=DM['join_meshes'](objects,name,mats);hulls=DM['hulls_for'](objects,name) if damage else []
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True)
 for h in hulls:h.select_set(True)
 bpy.context.view_layer.objects.active=ob;file=D/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
 lo,hi=DM['bbox'](ob)
 row=dict(id=g,asset_name=name,intact_fbx=str(file),material_slots=[DEST+'/'+m.name+'.'+m.name for m in mats],material_names=[m.name for m in mats],bounds_blender_metres=[list(lo),list(hi)],behavior='furniture' if damage else 'architecture',mass_kg=mass,break_health=health,movable=movable,wall=wall,damage=damage,hulls=len(hulls),vertices=len(ob.data.vertices),faces=len(ob.data.polygons))
 manifest['assets'].append(row)
 for o in [ob,*hulls,*objects]:DM['discard'](o)
 G.pop(g,None);return row
def place(g,u,v,h=0,yaw=0,name=None):
 manifest['placements'].append(dict(asset=g,name=name or g,location=[round(u,3),round(v,3),round(h,3)],yaw=yaw))
