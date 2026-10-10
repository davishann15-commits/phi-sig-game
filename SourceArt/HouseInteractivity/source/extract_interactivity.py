"""Losslessly separate saved house furniture; preserve materials and UVs.
Run in Blender, after root exports the active map's static mesh inventory.
Never modifies source assets or maps. Exports centered intact/broken FBXs and a
complete geometry-conservation manifest for root's native Unreal installer.
"""
import bpy, math, json, os, re, sys, collections, hashlib, random
from mathutils import Vector, Matrix
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT=os.path.join(ROOT,'models');os.makedirs(OUT,exist_ok=True)

def components(o):
 m=o.data;p=list(range(len(m.vertices)))
 def f(x):
  while p[x]!=x:p[x]=p[p[x]];x=p[x]
  return x
 for poly in m.polygons:
  for v in poly.vertices[1:]:p[f(v)]=f(poly.vertices[0])
 groups=collections.defaultdict(list)
 for poly in m.polygons:groups[f(poly.vertices[0])].append(poly.index)
 ps=[]
 for fs in groups.values():
  vs={v for fi in fs for v in m.polygons[fi].vertices};cs=[o.matrix_world@m.vertices[v].co for v in vs]
  lo=Vector(tuple(min(c[k] for c in cs) for k in range(3)));hi=Vector(tuple(max(c[k] for c in cs) for k in range(3)))
  ps.append(dict(faces=fs,lo=lo,hi=hi,size=hi-lo,center=(lo+hi)*.5,materials=sorted({m.materials[m.polygons[fi].material_index].name if m.materials[m.polygons[fi].material_index] else '' for fi in fs})))
 return ps

def bounds(parts):
 return (Vector(tuple(min(p['lo'][k] for p in parts) for k in range(3))),Vector(tuple(max(p['hi'][k] for p in parts) for k in range(3))))
def intersects(a,b,tol=.04):return all(a['lo'][i]<=b['hi'][i]+tol and a['hi'][i]>=b['lo'][i]-tol for i in range(3))
def merge_near(ps,tol=.04):
 groups=[]
 for p in ps:
  touch=[g for g in groups if any(intersects(p,q,tol) for q in g)]
  if not touch:groups.append([p]);continue
  first=touch[0];first.append(p)
  for g in touch[1:]:first.extend(g);groups.remove(g)
 return groups

def submesh(o,fs,name,origin=None):
 m=o.data;fs=sorted(fs); vs=sorted({v for fi in fs for v in m.polygons[fi].vertices});lookup={v:i for i,v in enumerate(vs)}
 origin=origin or Vector((0,0,0));verts=[o.matrix_world@m.vertices[v].co-origin for v in vs]
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],[[lookup[v] for v in m.polygons[i].vertices] for i in fs]);mesh.update()
 # Compact material slots, preserving the native index mapping for install.
 used=sorted({m.polygons[i].material_index for i in fs});remap={old:new for new,old in enumerate(used)}
 for i in used:mesh.materials.append(m.materials[i])
 for new,old in zip(mesh.polygons,fs):new.material_index=remap[m.polygons[old].material_index];new.use_smooth=m.polygons[old].use_smooth
 for layer in m.uv_layers:
  uv=mesh.uv_layers.new(name=layer.name)
  for new,old in zip(mesh.polygons,fs):
   for dst,src in zip(new.loop_indices,m.polygons[old].loop_indices):uv.data[dst].uv=layer.data[src].uv
 try:
  normal_xform=o.matrix_world.to_3x3().inverted().transposed()
  ns=[]
  for old in fs:
   for li in m.polygons[old].loop_indices:ns.append((normal_xform@m.corner_normals[li].vector).normalized())
  mesh.normals_split_custom_set(ns)
 except (AttributeError,RuntimeError):pass
 ob=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(ob)
 return ob,used

def export(o,name):
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;o.name=name
 path=os.path.join(OUT,name+'.fbx');bpy.ops.export_scene.fbx(filepath=path,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',bake_space_transform=False,add_leaf_bones=False)
 return path

def wreck(intact,name,behavior):
 m=intact.data;vs=m.vertices;lo=Vector(tuple(min(v.co[k] for v in vs) for k in range(3)));hi=Vector(tuple(max(v.co[k] for v in vs) for k in range(3)));sz=hi-lo
 # Split shared vertices along two oblique fault planes. The chunks preserve
 # original materials and UVs; bent frames and collapsed cushions remain
 # recognizable, with visible gaps instead of a uniform scaled replacement.
 verts=[];faces=[];oldloops=[];lookup={};matidx=[]
 for poly in m.polygons:
  c=sum((vs[v].co for v in poly.vertices),Vector())/len(poly.vertices)
  fault=(c.x-lo.x)/max(sz.x,.01)+.23*(c.y-lo.y)/max(sz.y,.01)
  piece=0 if fault<.39 else 1 if fault<.79 else 2;inds=[]
  for v in poly.vertices:
   key=(piece,v)
   if key not in lookup:
    p=vs[v].co.copy();side=piece-1
    p.x+=side*min(.12,sz.x*.10);p.y+=side*min(.09,sz.y*.08)
    p.z=lo.z+(p.z-lo.z)*(.38+.07*piece)
    p.z+=abs(math.sin(p.x*7.1+p.y*4.7))*min(.045,sz.z*.08)
    p.z+=abs(side)*.08*(p.x-lo.x)
    lookup[key]=len(verts);verts.append(p)
   inds.append(lookup[key])
  faces.append(inds);oldloops.append(poly.loop_indices);matidx.append(poly.material_index)
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
 for mat in m.materials:mesh.materials.append(mat)
 for poly,old,mi in zip(mesh.polygons,m.polygons,matidx):poly.material_index=mi;poly.use_smooth=old.use_smooth
 for layer in m.uv_layers:
  uv=mesh.uv_layers.new(name=layer.name)
  for new,old in zip(mesh.polygons,oldloops):
   for dst,src in zip(new.loop_indices,old):uv.data[dst].uv=layer.data[src].uv
 ob=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(ob);return ob

def cm(v):return [round(v.x*100,6),round(-v.y*100,6),round(v.z*100,6)]
def raw(v):return [round(float(k),6) for k in v]
def category(label):
 l=label.lower()
 if label.startswith('SMF_'):return 'baked'
 if label.startswith('SM_') and '_Glass' in label:return 'window'
 if label.startswith(('SMV_','Room102_Chair')):return 'individual'
 if label.startswith('SM_GR3_'):return 'greatroom'
 if label.startswith('SM_SpaRoom_'):
  if any(x in l for x in ('shell','architecturaldetails','tub')):return 'preserve'
  return 'spa'
 if label.startswith(('BR_','R204_','UpBath_','PIG_','HallEnd_')):
  if any(x in l for x in ('col_','collision','blocker','col_','collision','blocker','shell','woodfloor','grimyfloor','secondclosetfloor','_ceiling','walllining','hallchanges','skirting','interiorpaint','closetframe','door','frameworld','perimeterled','puddle','grime','scratch','straw','expandedroom','originalfloors','threerooms','vent','floordrain','floor_drain','secondclosettrim','fixture')):return 'preserve'
  if 'curtain' in l:return 'curtain'
  if any(x in l for x in ('fluorescent','light','lamp','fan')):return 'fixed_prop'
  return 'individual'
 return 'preserve'

def classify_groups(label,ps,seeds):
 kind=category(label);groups=[];remaining=[]
 if kind=='preserve':return [],ps
 if kind=='window':return [dict(parts=g,behavior='window',name='window pane') for g in merge_near(ps,.015)],[]
 if kind=='curtain':
  cloth=[p for p in ps if p['size'].z>.3 and any(w in ' '.join(p['materials']).lower() for w in ('cloth','fabric','curtain','linen','cotton'))]
  # Imported authored curtain-only actors may have generic material names.
  if not cloth:cloth=[p for p in ps if p['size'].z>.45]
  cgs=merge_near(cloth,.035)
  remaining=[p for p in ps if p not in cloth]
  for p in list(remaining):
   if max(p['size'])>.13:continue
   matches=[]
   for i,g in enumerate(cgs):
    lo,hi=bounds(g)
    if all(lo[k]-.07<=p['center'][k]<=hi[k]+.07 for k in (0,1)) and p['center'].z>hi.z-.17:matches.append(((p['center']-(lo+hi)*.5).length,i))
   if matches:cgs[min(matches)[1]].append(p);remaining.remove(p)
  return [dict(parts=g,behavior='curtain',name='curtain') for g in cgs],remaining
 if kind in ('individual','fixed_prop'):
  return [dict(parts=ps,behavior='furniture',name=label,movable=kind!='fixed_prop')],[]
 todo=list(ps)
 # Tall cloth panels are separate from their fixed rails and can gather.
 cloth=[p for p in todo if p['size'].z>1.35 and min(p['size'].x,p['size'].y)<.30 and max(p['size'].x,p['size'].y)>.30 and any(w in ' '.join(p['materials']).lower() for w in ('fabric','curtain','linen'))]
 for g in merge_near(cloth,.08):groups.append(dict(parts=g,behavior='curtain',name='curtain'))
 todo=[p for p in todo if p not in cloth]
 # Spa's decorative wall slats are part of architecture, not the nearby bench.
 if kind=='spa':
  fixed=[p for p in todo if p['size'].z>2.0 or (min(p['size'].x,p['size'].y)<.06 and max(p['size'].x,p['size'].y)>3.)]
  remaining.extend(fixed);todo=[p for p in todo if p not in fixed]
 # Loose fabric cushions are intentionally extracted before larger furniture
 # boxes so upholstered throw pillows do not become part of the sofa body.
 if kind=='baked':
  pillows=[p for p in todo if .07<min(p['size']) and max(p['size'])<1.2 and .09<p['size'].z<.8 and any('Fabric' in m for m in p['materials']) and not any('Rug' in m for m in p['materials'])]
  for g in merge_near(pillows,.04):groups.append(dict(parts=g,behavior='furniture',name='throw pillow'))
  todo=[p for p in todo if p not in pillows]
 # Per-item simple collision boxes provide exact anchors for baked furniture.
 assignments=[[] for s in seeds]
 unused=[]
 for p in todo:
  matches=[]
  for i,s in enumerate(seeds):
   if all(s['lo'][k]-.07<=p['center'][k]<=s['hi'][k]+.07 for k in range(3)):
    if any(p['size'][k]>s['size'][k]+(.045 if s.get('precise') else .16) for k in range(3)):continue
    volume=max(.001,s['size'].x*s['size'].y*s['size'].z);matches.append((volume,i))
  if matches:assignments[min(matches)[1]].append(p)
  else:unused.append(p)
 for i,g in enumerate(assignments):
  if g:groups.append(dict(parts=g,behavior='furniture',name=seeds[i].get('name','furniture '+str(i+1))))
 # Residual cushions, lamps, tabletop props and rugs remain individual groups.
 for g in merge_near(unused,.085):
  lo,hi=bounds(g);sz=hi-lo;floor=3.023 if 'Upper' in label else -2.45 if 'Basement' in label else 0
  behavior='furniture';name='loose furnishing'
  if lo.z-floor>2.05 and min(sz.x,sz.y)>.30 and .5<sz.x/max(sz.y,.01)<2 and any('Light' in m or 'Metal' in m for p in g for m in p['materials']):behavior='chandelier';name='hanging light'
  groups.append(dict(parts=g,behavior=behavior,name=name))
 return groups,remaining

# Native manifest adapter is deliberately independent of Unreal API versions.
def main():
 args=sys.argv[sys.argv.index('--')+1:];native=json.load(open(args[0]));actors=native.get('actors',{});meshmap=native.get('meshes',{})
 if isinstance(actors,list):
  flat={}
  for a in actors:
   if a.get('class')!='StaticMeshActor':continue
   cs=a.get('components',[])
   if len(cs)!=1:continue
   c=cs[0];flat[a['label']]={**a,**c,**c['transform']}
  actors=flat
 for v in meshmap.values():v['materials']=[m['material'] if isinstance(m,dict) else m for m in v.get('materials',[])]
 records=[];remainders=[];remove_colliders=[];coverage=[];cache={}
 for label,a in actors.items():
  path=a.get('mesh');kind=category(label)
  if not path or kind=='preserve':continue
  meshinfo=meshmap.get(path,{})
  fbx=meshinfo.get('fbx') or a.get('fbx')
  if not fbx or not os.path.exists(fbx):raise RuntimeError('Missing native mesh export '+label+' '+str(fbx))
  key=(path,kind)
  if key in cache:
   templ=cache[key]
   for old in templ['records']:
    row=dict(old);row['source_actor_label']=label;row['source_actor_labels']=a.get('source_actor_labels',[label]);row['source_transform']={k:a.get(k) for k in ('location','rotation','scale')};row['id']=label+'__'+old['suffix'];records.append(row)
   if templ['remainder']:row=dict(templ['remainder']);row['source_actor_label']=label;remainders.append(row)
   continue
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=fbx)
  meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_','USP_','UCP_'))];
  if not meshes:
   coverage.append(dict(actor=label,kind=kind,faces=0,pieces=0,retained_faces=0,conservation_pass=True));continue
  if len(meshes)!=1:raise RuntimeError('Expected one exported mesh: '+fbx)
  o=meshes[0];ps=components(o);seeds=[]
  collabel=label.replace('SMF_','COLF_',1) if label.startswith('SMF_') else None
  if collabel and collabel in actors:
   ca=actors[collabel];cf=meshmap.get(ca['mesh'],{}).get('fbx') or ca.get('fbx')
   before=set(bpy.context.scene.objects);bpy.ops.import_scene.fbx(filepath=cf)
   co=next(k for k in bpy.context.scene.objects if k not in before and k.type=='MESH');seeds=components(co);bpy.data.objects.remove(co,do_unlink=True);remove_colliders.extend(ca.get('source_actor_labels',[collabel]))
  if label.startswith('SM_GR3_') and 'Rug' not in label:
   layout=json.load(open('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomV3/layout_manifest.json'))
   seeds=[]
   for p in layout['pieces']:
    if 'boundsMetres' not in p:continue
    lo,hi=map(Vector,p['boundsMetres']);seeds.append(dict(lo=lo,hi=hi,size=hi-lo,name=p['name'],precise=True))
  groups,rem=classify_groups(label,ps,seeds);my=[];allocated=[]
  for i,g in enumerate(groups):
   gps=g['parts'];fs={f for p in gps for f in p['faces']};allocated.extend(fs)
   lo,hi=bounds(gps);origin=(lo+hi)*.5;sz=hi-lo;suffix=f'{i+1:03d}';stem='HI_'+re.sub('[^A-Za-z0-9_]','_',label)[:48]+'_'+suffix
   intact,slots=submesh(o,fs,stem,origin);curtain_meta={}
   if g['behavior']=='curtain':
    axis=0 if sz.x>sz.y else 1;anchor=lo[axis];initially_open=False;gather=.14
    if 'CreamCurtainsPartOpen' in label:
     anchor=lo[axis] if origin[axis]<0 else hi[axis];full=1.07;gather=sz[axis]/full;initially_open=True
    elif 'Gathered' in path:
     anchor=hi[axis];full=.94;gather=sz[axis]/full;initially_open=True
    if initially_open:
     for v in intact.data.vertices:v.co[axis]=(anchor-origin[axis])+(v.co[axis]-(anchor-origin[axis]))/gather
     intact.data.update()
    pivot=origin.copy();pivot[axis]=anchor;pivot.z=hi.z
    curtain_meta=dict(pivot_cm=cm(pivot-origin),curtain_axis=axis,curtain_gathered_scale=gather,initially_open=initially_open)
   intact_fbx=export(intact,stem);broken_fbx=None
   if g['behavior']=='furniture':
    br=wreck(intact,stem+'_Broken',g['behavior']);broken_fbx=export(br,stem+'_Broken');bpy.data.objects.remove(br,do_unlink=True)
   mats=a.get('materials') or meshinfo.get('materials',[]);usedmats=[mats[j] if j<len(mats) else None for j in slots]
   mass=max(.25,min(220,sz.x*sz.y*sz.z*110))
   row=dict(id=label+'__'+suffix,suffix=suffix,source_actor_label=label,source_actor_labels=a.get('source_actor_labels',[label]),source_mesh=path,source_transform={k:a.get(k) for k in ('location','rotation','scale')},intact_fbx=intact_fbx,broken_fbx=broken_fbx,asset_name=stem,local_origin_cm=cm(origin),bounds_blender_metres=[raw(lo),raw(hi)],material_slots=usedmats,material_source_indices=slots,behavior=g['behavior'],name=g['name'],mass_kg=round(mass,2),movable=g.get('movable',True),face_count=len(fs))
   if g['behavior']=='curtain':
    row.update(curtain_meta)
   if g['behavior']=='chandelier':row['pivot_cm']=cm(Vector((origin.x,origin.y,hi.z))-origin)
   records.append(row);my.append(row);bpy.data.objects.remove(intact,do_unlink=True)
  remrow=None
  if rem:
   fs={f for p in rem for f in p['faces']};allocated.extend(fs);stem='HI_Remainder_'+re.sub('[^A-Za-z0-9_]','_',label)[:48];ob,slots=submesh(o,fs,stem)
   remrow=dict(source_actor_label=label,source_mesh=path,fbx=export(ob,stem),asset_name=stem,material_source_indices=slots);remainders.append(remrow);bpy.data.objects.remove(ob,do_unlink=True)
  if len(allocated)!=len(set(allocated)) or set(allocated)!=set(range(len(o.data.polygons))):raise RuntimeError('Geometry conservation failed '+label)
  coverage.append(dict(actor=label,kind=kind,faces=len(o.data.polygons),pieces=len(my),retained_faces=sum(len(p['faces']) for p in rem),conservation_pass=True))
  cache[key]={'records':my,'remainder':remrow}
  json.dump(dict(pieces=records,remainders=remainders,remove_colliders=remove_colliders,coverage=coverage),open(os.path.join(ROOT,'extraction-progress.json'),'w'),indent=2)
 result=dict(pieces=records,remainders=remainders,remove_colliders=remove_colliders,coverage=coverage)
 json.dump(result,open(os.path.join(ROOT,'extraction-manifest.json'),'w'),indent=2)
 print('HOUSE_EXTRACTION_COMPLETE',len(records),len(remainders),len(coverage))
if __name__=='__main__':main()
