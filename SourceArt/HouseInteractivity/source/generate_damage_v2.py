"""Component-aware furniture damage. Original intact FBXs are read-only.
Each fracture is a capped planar cut; every detached part moves rigidly.
No global compression, per-face stretching, noisy vertex displacement or spikes.
"""
import bpy,bmesh,math,json,os,re,sys,hashlib,random,runpy,collections
import numpy as np
from mathutils import Vector,Matrix,Quaternion
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S=runpy.run_path(R+'/source/extract_interactivity.py')
OUT=R+'/models/damage-v2';os.makedirs(OUT,exist_ok=True)
DEST='/Game/SeniorSendOff/House/Interactivity/'
DEFS=[dict(asset_name='M_HI_SplinterWood',base_color=[.34,.21,.095],roughness=.95,metallic=0),dict(asset_name='M_HI_Foam',base_color=[.48,.38,.20],roughness=1.,metallic=0),dict(asset_name='M_HI_RawMetal',base_color=[.12,.13,.14],roughness=.8,metallic=.65),dict(asset_name='M_HI_FractureCore',base_color=[.42,.40,.34],roughness=.95,metallic=0)]
REP=['SMV_GR_ClubChair1_Solid__001','SM_GR3_NearSeating__003','BR_SM_Bedroom_OvalCoffeeTable__001','Room102_Chair_01__001','BR_BedDetail_FramedCollage_08__001','R204_GalanzFridgeAndMicrowave__001']

def bbox(ob):
 cs=[v.co for v in ob.data.vertices]
 return Vector(tuple(min(v[k] for v in cs)for k in range(3))),Vector(tuple(max(v[k]for v in cs)for k in range(3)))
def discard(ob):
 me=ob.data;bpy.data.objects.remove(ob,do_unlink=True)
 if me.users==0:bpy.data.meshes.remove(me)
def copy(ob):
 n=bpy.data.objects.new('part',ob.data.copy());bpy.context.collection.objects.link(n);return n

def cap_kind(names):
 st=' '.join(names).lower()
 if any(x in st for x in ('leather','fabric','cloth','linen','pillow','quilt','cord','microfiber','towel','mattress')):return 1
 if any(x in st for x in ('wood','walnut','oak','timber','bamboo','cardboard','paper','frame')):return 0
 if any(x in st for x in ('metal','steel','chrome','brass','iron','silver','stainless','aluminium','aluminum')):return 2
 return 3

def fill_cut(bm,plane_co,plane_no,cap):
 # Only freshly cut boundaries on the cutting plane are filled; original
 # holes and thin paper surfaces remain untouched.
 es=[e for e in bm.edges if e.is_boundary and all(abs((v.co-plane_co).dot(plane_no))<.00003 for v in e.verts)]
 if es:
  try:
   fs=bmesh.ops.holes_fill(bm,edges=es,sides=0).get('faces',[])
   for f in fs:f.material_index=cap;f.smooth=False
   if fs:bmesh.ops.triangulate(bm,faces=list(fs),quad_method='BEAUTY',ngon_method='EAR_CLIP')
  except (RuntimeError,ValueError):pass
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))

def clipped(ob,co,no,positive,cap):
 bm=bmesh.new();bm.from_mesh(ob.data)
 bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.000001,plane_co=co,plane_no=no,clear_inner=positive,clear_outer=not positive)
 if len(bm.faces)==0:bm.free();return None
 fill_cut(bm,co,no,cap)
 out=copy(ob);bm.to_mesh(out.data);bm.free();out.data.update();return out

def cut_pair(ob,co,no,cap):
 a=clipped(ob,co,no,False,cap);b=clipped(ob,co,no,True,cap)
 if not a or not b:
  if a:discard(a)
  if b:discard(b)
  return [copy(ob)]
 return [a,b]

def rigid(ob,pivot,axis,angle,offset=Vector((0,0,0))):
 rot=Matrix.Rotation(angle,3,axis)
 for v in ob.data.vertices:v.co=rot@(v.co-pivot)+pivot+offset
 ob.data.update()

def part_mesh(o,fs):
 ob,used=S['submesh'](o,fs,'part')
 # Retain the complete original slot ordering on every fragment.
 remap={i:k for i,k in enumerate(used)}
 indices=[remap[p.material_index] for p in ob.data.polygons]
 ob.data.materials.clear()
 for mat in o.data.materials:ob.data.materials.append(mat)
 for p,idx in zip(ob.data.polygons,indices):p.material_index=idx
 return ob

def groups_for(o,row):
 ps=S['components'](o);lo,hi=bbox(o);size=hi-lo;overall=max(size)
 planar=min(size)<max(.025,overall*.065) and sorted(size)[1]>.12
 if planar:return [dict(faces=list(range(len(o.data.polygons))),parts=len(ps),names=[m.name for m in o.data.materials],planar=True)]
 vmax=max(max(.000001,p['size'].x*p['size'].y*p['size'].z)for p in ps)
 anchors=[]
 for i,p in enumerate(ps):
  sz=sorted(p['size']);mt=' '.join(p['materials']).lower();vol=max(.000001,p['size'].x*p['size'].y*p['size'].z)
  thin_detail=any(w in mt for w in ('stitch','seam','logo','statusled','print','label','screw'))
  support=sz[2]>.2*overall and sz[2]>4*max(.0001,sz[1]) and not any(w in mt for w in ('fabric','cloth','cord','linen','leather'))
  if not thin_detail and ((vol>.028*vmax and sz[2]>.12*overall) or support):anchors.append(i)
 if not anchors:anchors=[max(range(len(ps)),key=lambda i:np.prod(ps[i]['size']))]
 # Limit meaningful part count. Small knobs, stitches and pads follow their
 # nearest parent; they never become disconnected spaghetti fragments.
 anchors=sorted(anchors,key=lambda i:np.prod(ps[i]['size']),reverse=True)[:20]
 gs={i:dict(faces=[],parts=0,names=[],planar=False)for i in anchors}
 for i,p in enumerate(ps):
  if i in gs:j=i
  else:
   def distance(a):
    q=ps[a];gap=sum(max(q['lo'][k]-p['hi'][k],p['lo'][k]-q['hi'][k],0)**2 for k in range(3))
    return gap*8+(p['center']-q['center']).length_squared*.15
   j=min(anchors,key=distance)
  gs[j]['faces']+=p['faces'];gs[j]['parts']+=1;gs[j]['names']+=p['materials']
 return list(gs.values())

def fracture_plane(ob,seed,fraction=.53):
 lo,hi=bbox(ob);sz=hi-lo;axis=max(range(3),key=lambda k:sz[k]);n=Vector((0,0,0));n[axis]=1
 # Oblique facets are local cuts rather than geometry deformations.
 others=[k for k in range(3)if k!=axis]
 n[others[0]]=.16 if seed%2 else -.19;n[others[1]]=.09;n.normalize()
 dots=sorted(v.co.dot(n)for v in ob.data.vertices);q=dots[0]+(dots[-1]-dots[0])*fraction
 return n*q,n

def chip(ob,seed,cap,amount):
 lo,hi=bbox(ob);sz=hi-lo
 # Choose an exposed upper/front corner; keep all exterior polygons and UVs.
 n=Vector((.65 if seed%2 else -.65,-.8,.9))
 if min(sz)<max(sz)*.09:
  small=min(range(3),key=lambda k:sz[k]);n[small]=0
 n.normalize();dots=[v.co.dot(n)for v in ob.data.vertices];q=max(dots)-(max(dots)-min(dots))*amount
 cut=clipped(ob,n*q,n,False,cap)
 if cut and len(cut.data.polygons)>0:
  # Two additional shallow facets roughen the chipped edge; the underlying
  # skin and intact vertices remain undeformed.
  for j in range(2):
   nn=(n+Vector((.13*(-1 if j else 1),.08*(1 if j else -1),-.10))).normalized()
   dd=[v.co.dot(nn)for v in ob.data.vertices];qq=max(dd)-(max(dd)-min(dd))*amount*.76
   c=clipped(cut,nn*qq,nn,False,cap)
   if c and len(c.data.polygons)>0:discard(cut);cut=c
   elif c:discard(c)
  return cut
 if cut:discard(cut)
 return copy(ob)

def floor_part(ob,center,ground,rng,scatter):
 coords=np.array([v.co[:]for v in ob.data.vertices]);c=coords.mean(axis=0)
 # A rigid rotation puts the thinnest principal axis down, as a real loose
 # cushion, door panel, tabletop or leg would rest on the floor.
 cov=np.cov((coords-c).T);vals,axes=np.linalg.eigh(cov);normal=Vector(axes[:,0])
 if normal.z<0:normal=-normal
 q=normal.rotation_difference(Vector((0,0,1)))
 rot=q.to_matrix();yaw=Matrix.Rotation(rng.uniform(-.42,.42),3,'Z')
 rot=yaw@rot
 old_center=Vector(c);new_center=Vector((center.x+rng.uniform(-scatter,scatter),center.y+rng.uniform(-scatter,scatter),0))
 for v in ob.data.vertices:v.co=rot@(v.co-old_center)+new_center
 lo,hi=bbox(ob)
 for v in ob.data.vertices:v.co.z+=ground-lo.z
 ob.data.update()

def join_meshes(parts,name,mats):
 # Joining with explicit slot remap retains original exterior indices exactly.
 verts=[];faces=[];mis=[];sm=[];uvs=[]
 for ob in parts:
  m=ob.data;off=len(verts);verts.extend(v.co.copy()for v in m.vertices)
  layer=m.uv_layers.active
  for p in m.polygons:
   faces.append([off+k for k in p.vertices]);mis.append(p.material_index);sm.append(p.use_smooth)
   uvs.append([layer.data[k].uv[:]for k in p.loop_indices]if layer else [(0,0)]*len(p.vertices))
 me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update()
 for m in mats:me.materials.append(m)
 uv=me.uv_layers.new(name='UVMap')
 for p,mi,s,tu in zip(me.polygons,mis,sm,uvs):
  p.material_index=mi;p.use_smooth=s
  for li,t in zip(p.loop_indices,tu):uv.data[li].uv=t
 ob=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(ob);return ob

def hulls_for(parts,name):
 # Convex hulls use deformed stage fragments, never the intact object's hull.
 groups=[]
 for o in parts:
  coords=[v.co.copy()for v in o.data.vertices];lo,hi=bbox(o)
  if len(coords)>2:groups.append(dict(coords=coords,center=(lo+hi)*.5,vol=max(.00001,np.prod(hi-lo))))
 while len(groups)>12:
  a=min(range(len(groups)),key=lambda i:groups[i]['vol']);g=groups.pop(a)
  b=min(range(len(groups)),key=lambda i:(groups[i]['center']-g['center']).length_squared)
  groups[b]['coords']+=g['coords'];groups[b]['vol']+=g['vol']
 out=[]
 for i,g in enumerate(groups):
  cs=g['coords'];arr=np.array([v[:]for v in cs]);lo=arr.min(axis=0);hi=arr.max(axis=0)
  # Extremely thin paper/glass needs a minimal finite collision thickness.
  if min(hi-lo)<.006:
   ax=int(np.argmin(hi-lo));cs=[v.copy()for v in cs]+[v.copy()for v in cs]
   half=len(cs)//2
   for k,v in enumerate(cs):v[ax]+=(-.003 if k<half else .003)
  # Extremes in 26 directions bound the shape tightly without huge UCX hulls.
  dirs=[Vector((x,y,z)).normalized()for x in (-1,0,1)for y in (-1,0,1)for z in (-1,0,1)if x or y or z]
  chosen={max(range(len(cs)),key=lambda j:cs[j].dot(d))for d in dirs}
  bm=bmesh.new();vs=[bm.verts.new(cs[j])for j in chosen]
  try:
   result=bmesh.ops.convex_hull(bm,input=vs,use_existing_faces=False)
   interior=result.get('geom_interior',[])+result.get('geom_unused',[])
   if interior:bmesh.ops.delete(bm,geom=[v for v in interior if isinstance(v,bmesh.types.BMVert)],context='VERTS')
   if len(bm.faces)==0:bm.free();continue
   me=bpy.data.meshes.new('UCX');bm.to_mesh(me);bm.free();me.update();ob=bpy.data.objects.new('UCX_'+name+'_%02d'%i,me);bpy.context.collection.objects.link(ob);out.append(ob)
  except (RuntimeError,ValueError):bm.free()
 if not out:
  allcs=[v.co.copy()for p in parts for v in p.data.vertices];lo=Vector(tuple(min(v[k]for v in allcs)for k in range(3)));hi=Vector(tuple(max(v[k]for v in allcs)for k in range(3)))
  for k in range(3):
   if hi[k]-lo[k]<.006:lo[k]-=.003;hi[k]+=.003
  verts=[(x,y,z)for x in (lo.x,hi.x)for y in (lo.y,hi.y)for z in (lo.z,hi.z)];faces=[(0,4,6,2),(1,3,7,5),(0,1,5,4),(2,6,7,3),(0,2,3,1),(4,5,7,6)]
  me=bpy.data.meshes.new('UCX');me.from_pydata(verts,[],faces);me.update();ob=bpy.data.objects.new('UCX_'+name+'_00',me);bpy.context.collection.objects.link(ob);out.append(ob)
 return out

def export_stage(parts,name,mats):
 ob=join_meshes(parts,name,mats);hulls=hulls_for(parts,name)
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True)
 for h in hulls:h.select_set(True)
 bpy.context.view_layer.objects.active=ob;path=OUT+'/'+name+'.fbx'
 bpy.ops.export_scene.fbx(filepath=path,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',bake_space_transform=False,add_leaf_bones=False)
 lo,hi=bbox(ob);audit=dict(vertices=len(ob.data.vertices),faces=len(ob.data.polygons),hulls=len(hulls),bounds=[S['raw'](lo),S['raw'](hi)],fragments=len(parts))
 for h in hulls:discard(h)
 discard(ob)
 return path,audit

def create_stages(row):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=row['intact_fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
 # Apply imported FBX object basis to vertices exactly once. Mesh coordinate
 # positions are already centered in the original export's coordinate system.
 bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 orig_slots=list(o.data.materials);orig_count=len(orig_slots)
 for definition in DEFS:
  m=bpy.data.materials.new(definition['asset_name']);m.diffuse_color=(*definition['base_color'],1);m.roughness=definition['roughness'];m.metallic=definition['metallic'];o.data.materials.append(m)
 mats=list(o.data.materials);paths=list(row['material_slots'])+[DEST+x['asset_name']+'.'+x['asset_name']for x in DEFS]
 # extra empty material slots do not affect component classification
 gs=groups_for(o,row);bases=[]
 for g in gs:
  q=part_mesh(o,g['faces']);lo,hi=bbox(q)
  areas=collections.defaultdict(float)
  for po in q.data.polygons:areas[po.material_index]+=po.area
  dominant=max(areas,key=areas.get);kind=cap_kind([mats[dominant].name])
  g.update(ob=q,lo=lo,hi=hi,size=hi-lo,center=(lo+hi)*.5,cap=orig_count+kind,kind=kind);bases.append(g)
 lo,hi=bbox(o);sz=hi-lo;scale=max(sz);seed=int(hashlib.md5(row['asset_name'].encode()).hexdigest()[:8],16)
 bysize=sorted(range(len(bases)),key=lambda i:np.prod(np.maximum(np.array(bases[i]['size']),.012)),reverse=True)
 major=set(bysize[:max(1,min(5,len(bysize)//2+1))])
 nonmetal=[i for i in bysize if bases[i]['kind']!=2]
 if nonmetal:major.add(nonmetal[0])
 planar=len(bases)==1 and bases[0]['planar'];all_metal=all(g['kind']==2 for g in bases);change={};audit={'asset_name':row['asset_name'],'id':row['id'],'groups':len(gs),'stages':{},'policy':'capped rigid part fracture; no vertex stretching; floor-rested final fragments'}
 for stage,key,suffix in [(1,'damaged','_DamageV2'),(2,'heavily_damaged','_HeavyV2'),(3,'broken','_BrokenV2')]:
  rng=random.Random(seed+stage*101);parts=[]
  for i,g in enumerate(bases):
   base=g['ob'];cap=g['cap'];pseed=seed+i*47;center=g['center'];partsize=g['size'];primary=i in major
   if stage<3:
    # Damage never globally rescales the object. Cut corners expose real
    # wood/foam core, then selected attached components loosen rigidly.
    elongated=partsize.z>max(partsize.x,partsize.y)*2.5 and partsize.z>.20 and g['lo'].z<lo.z+sz.z*.25
    chip_allowed=primary and ((not elongated and g['kind']!=2) or all_metal or len(bases)==1)
    amount=(.018 if stage==1 else .055)if all_metal else (.038 if stage==1 else .10)
    q=chip(base,pseed,cap,amount)if chip_allowed else copy(base)
    if stage==2 and primary and g['kind']==1 and len(bases)>1 and center.z>lo.z+sz.z*.52:
     pivot=Vector((center.x,center.y,g['lo'].z));axis='X' if partsize.y<partsize.x else 'Y'
     rigid(q,pivot,axis,(-1 if i%2 else 1)*math.radians(7))
    # Long load-bearing legs get a snapped lower segment in the heavy stage.
    elongated=partsize.z>max(partsize.x,partsize.y)*2.5 and partsize.z>.20 and g['lo'].z<lo.z+sz.z*.25
    if stage==2 and elongated and i%2==0:
     co=Vector((center.x,center.y,g['lo'].z+partsize.z*.3));no=Vector((.2,-.17,1)).normalized();cut=cut_pair(q,co,no,cap);discard(q)
     if len(cut)==2:
      lower=min(cut,key=lambda ob:bbox(ob)[0].z);rigid(lower,co,'X',math.radians(35));ll,hh=bbox(lower)
      for v in lower.data.vertices:v.co.z+=lo.z-ll.z
     parts.extend(cut)
    else:parts.append(q)
   else:
    q=chip(base,pseed,cap,.12 if primary else .04)
    # Fine loose seam loops become lost threads after destruction; leaving
    # unsupported wire-thin trim attached to a different fragment is avoided.
    seam_slots={k for k,m in enumerate(mats)if any(t in m.name.lower()for t in ('stitch','seam'))}
    if seam_slots:
     bm=bmesh.new();bm.from_mesh(q.data);drop=[f for f in bm.faces if f.material_index in seam_slots]
     if drop:bmesh.ops.delete(bm,geom=drop,context='FACES')
     bm.to_mesh(q.data);bm.free();q.data.update()
    # Break large base/back/door panels and single-piece props into bounded
    # recognizable chunks, carrying their seams/labels with each chunk.
    split=primary or planar
    fragments=[q]
    if split:
     co,no=fracture_plane(q,pseed,.48+(.04 if i%2 else -.03));fragments=cut_pair(q,co,no,cap);discard(q)
     if planar or (len(bases)<3 and scale>.25):
      extra=[]
      for j,f in enumerate(fragments):
       if j==0:
        co,no=fracture_plane(f,pseed+1,.5);extra+=cut_pair(f,co,no,cap);discard(f)
       else:extra.append(f)
      fragments=extra
    for j,f in enumerate(fragments):
     if len(f.data.polygons)<1:discard(f);continue
     flo, fhi=bbox(f);fc=(flo+fhi)*.5
     scatter=min(.16,scale*.065)
     # Thin posters and rugs become separated torn sections. Other parts
     # settle by rigid rotation onto their broadest stable face.
     floor_part(f,fc,lo.z+rng.uniform(0,.004),rng,scatter)
     parts.append(f)
  if stage<3 and row['asset_name'] in {'HI_PIG_Crumbs_001','HI_SMF_Basement_ALL_044','HI_SMF_Entrance_ALL_117'}:
   # Sparse crumb clusters and tall mixed-metal fixtures can have no exposed
   # eligible corner in the main family rule. Cut their dominant section.
   idx=max(range(len(parts)),key=lambda k:len(parts[k].data.polygons))
   old=parts[idx];q=chip(old,seed+71,orig_count+3,.12 if stage==1 else .27);discard(old);parts[idx]=q
  name=row['asset_name']+suffix;path,stats=export_stage(parts,name,mats)
  change[key+'_fbx']=path;change[key+'_asset_name']=name;change[key+'_material_slots']=paths;change[key+'_collision']='imported_fragment_ucx';audit['stages'][key]=stats
  for q in parts:discard(q)
 for g in bases:discard(g['ob'])
 change['damage_geometry_version']=2
 return change,audit

if __name__=='__main__':
 args=sys.argv[sys.argv.index('--')+1:]if '--'in sys.argv else[];representatives='--representatives'in args;resume='--resume'in args or '--repair'in args;repair_mode='--repair'in args
 doc=json.load(open(R+'/extraction-manifest.json'));unique={}
 for p in doc['pieces']:
  if p['behavior']=='furniture' and (not representatives or p['id']in REP):unique.setdefault(p['asset_name'],p)
 results={};audits=[];outjson=R+'/inventory/damage-v2-'+('preview'if representatives else 'assets')+'.json'
 if resume and os.path.exists(outjson):
  old=json.load(open(outjson));results=old.get('assets',{});audits=old.get('audit',[])
 if repair_mode:
  repair={r['asset_name']for r in json.load(open(R+'/inventory/damage-v2-validation.json'))['repair']}
  results={k:v for k,v in results.items()if k not in repair};audits=[a for a in audits if a['asset_name']not in repair]
 before={p['intact_fbx']:hashlib.sha256(open(p['intact_fbx'],'rb').read()).hexdigest()for p in unique.values()}
 for index,(stem,row)in enumerate(unique.items()):
  if stem in results and all(os.path.exists(results[stem][key+'_fbx'])for key in ('damaged','heavily_damaged','broken')):continue
  changes,audit=create_stages(row);results[stem]=changes;audits.append(audit)
  json.dump(dict(assets=results,audit=audits,damage_materials=DEFS),open(outjson,'w'),indent=2)
  print('DAMAGE_V2_PROGRESS',index+1,len(unique),stem,flush=True)
 for path,h in before.items():
  assert hashlib.sha256(open(path,'rb').read()).hexdigest()==h,'Intact asset changed: '+path
 if not representatives:
  for p in doc['pieces']:
   if p['behavior']=='furniture':p.update(results[p['asset_name']])
  doc['damage_materials']=DEFS
  json.dump(doc,open(R+'/extraction-manifest.json','w'),indent=2)
 json.dump(dict(assets=results,audit=audits,damage_materials=DEFS,intact_sha256=before,intact_unchanged=True,passed=True),open(outjson,'w'),indent=2)
 print('DAMAGE_V2_DONE',len(results),flush=True)
