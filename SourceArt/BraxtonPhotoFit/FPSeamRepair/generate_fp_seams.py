"""Idempotent FP-only seam continuation; never replaces active or third-person files.
Run with native Blender --background --threads 4 --python this_file.
The active FBX supplies the unchanged rig, distal hands/weights and existing UVs. The intact
source supplies shoulder cross-section dimensions. New proximal continuations
bridge the weight-threshold holes and end behind the camera, not at their jagged
visible edge. Native Unreal import/rendering remains a separate integration step.
"""
import bpy, json, hashlib, math, collections
from pathlib import Path
from mathutils import Vector, Matrix
HERE=Path(__file__).resolve().parent;SOURCE=HERE.parent
ROOT=Path('/home/davis/Documents/ChatGPT/Senior Sendoff')
EVIDENCE=ROOT/'ArtSource/HouseRebuild/Analysis/FirstPersonSleeveAudit20260928'
BASE=SOURCE/'SK_C01_Arms.fbx';INTACT=SOURCE/'Braxton_Playable.blend'
OUTPUT=HERE/'SK_C01_Arms_SeamRepair.fbx'
EXPECTED='2ade780574240e2f5f0e5842a8f28061df944b9d218cc213bbff78f60b69400c'
NAMES=['C01_Body.male_casualsuit02_Arms','C01_Body_Arms']
ARM_PREFIX=('upperarm_','lowerarm_','hand_','index_','middle_','ring_','pinky_','thumb_')
STEPS=(.06,.12,.20,.30,.40,.50,.60,.70,.80,.90,1.)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def weights(o):
 names={g.index:g.name for g in o.vertex_groups}
 return [{names[g.group]:g.weight for g in v.groups}for v in o.data.vertices]
def loop_components(mesh):
 edges={};adj=collections.defaultdict(list)
 for p in mesh.polygons:
  ls=list(p.loop_indices)
  for i,li in enumerate(ls):
   lj=ls[(i+1)%len(ls)];a=mesh.loops[li].vertex_index;b=mesh.loops[lj].vertex_index
   edges.setdefault(tuple(sorted((a,b))),[]).append((a,b,li,lj))
 boundary={e:r[0] for e,r in edges.items()if len(r)==1}
 for a,b in boundary:adj[a].append(b);adj[b].append(a)
 remaining=set(adj);components=[]
 while remaining:
  first=min(remaining);ordered=[first];previous=None;current=first
  while True:
   assert len(adj[current])==2,('Non-loop boundary',current)
   following=next((n for n in adj[current]if n!=previous),None)
   if following==first:break
   assert following not in ordered,'Self-intersecting boundary order'
   ordered.append(following);previous,current=current,following
  remaining.difference_update(ordered)
  # Orient so existing face uses boundary a->b; appended face must use b->a.
  if boundary[tuple(sorted(ordered[:2]))][:2] != tuple(ordered[:2]):ordered.reverse()
  components.append(ordered)
 return components,boundary

def snap_mesh(o):
 m=o.data;m.calc_loop_triangles();w=weights(o)
 return dict(name=o.name,positions=[list(v.co)for v in m.vertices],weights=w,
  polygons=[list(p.vertices)for p in m.polygons],triangles=[dict(polygonIndex=t.polygon_index,vertices=list(t.vertices),uv=[list(m.uv_layers[0].data[i].uv)for i in t.loops],normals=[list(m.corner_normals[i].vector)for i in t.loops],material=m.materials[t.material_index].name)for t in m.loop_triangles],
  materials=[x.name for x in m.materials],groups=[g.name for g in o.vertex_groups])
def snap_rig(rig):
 return dict(name=rig.name,matrix=[list(r)for r in rig.matrix_world],bones=[dict(name=b.name,parent=b.parent.name if b.parent else None,matrix=[list(r)for r in b.matrix_local],deform=b.use_deform)for b in rig.data.bones])
def blend_weights(start,clavicle,t):
 d={k:v*(1-t)for k,v in start.items()};d[clavicle]=d.get(clavicle,0)+t
 total=sum(d.values());return {k:v/total for k,v in d.items()if v>1e-8}
def donor_profile(points,center,sign):
 # Cross-section of the intact anatomical shoulder, not the later face/hood.
 selected=[p for p in points if abs(p.x-center.x)<.012 and abs(p.z-center.z)<.125 and abs(p.y-center.y)<.15]
 assert len(selected)>=8,('No intact shoulder cross-section',len(selected))
 ry=(max(p.y for p in selected)-min(p.y for p in selected))*.5
 rz=(max(p.z for p in selected)-min(p.z for p in selected))*.5
 return dict(samples=len(selected),radiusY=ry,radiusZ=rz)

def repair(o,donor):
 old=o.data;old.update();before=snap_mesh(o);loops,boundary=loop_components(old)
 oldnorm=[tuple(n.vector)for n in old.corner_normals]
 verts=[v.co.copy()for v in old.vertices];faces=[tuple(p.vertices)for p in old.polygons]
 mats=[p.material_index for p in old.polygons];smooth=[p.use_smooth for p in old.polygons]
 uvnames=[u.name for u in old.uv_layers];uvs={u.name:[[tuple(u.data[i].uv)for i in p.loop_indices]for p in old.polygons]for u in old.uv_layers}
 ws=[dict(w) for w in before['weights']];added=[];caps=[];repairs=[];mutable=set()
 for ring in loops:
  sums=[sum(v for n,v in ws[i].items()if n.startswith(ARM_PREFIX))for i in ring]
  if min(sums)>.7:continue # Preserve the real distal cuff openings.
  center=sum((verts[i]for i in ring),Vector())/len(ring);sign=1 if center.x>0 else -1
  side='l'if sign>0 else'r';clavicle='clavicle_'+side
  assert o.vertex_groups.get(clavicle),clavicle
  profile=donor_profile(donor,center,sign)
  # Flatten only the four original ragged crop loops to smooth shoulder
  # sections. Existing distal arm/hand vertices and their original weights remain.
  # Place the section just proximal to every retained vertex; flattening it
  # at the old average X would invert adjacent ragged faces into visible folds.
  center.x=sign*(min(abs(verts[i].x)for i in ring)-.015)
  center.y=(min(verts[i].y for i in ring)+max(verts[i].y for i in ring))*.5
  center.z=(min(verts[i].z for i in ring)+max(verts[i].z for i in ring))*.5
  ry=(max(verts[i].y for i in ring)-min(verts[i].y for i in ring))*.5
  rz=(max(verts[i].z for i in ring)-min(verts[i].z for i in ring))*.5
  angles=[math.atan2((verts[i].z-center.z)/rz,(verts[i].y-center.y)/ry)for i in ring]
  deltas=[math.atan2(math.sin(angles[(j+1)%len(ring)]-angles[j]),math.cos(angles[(j+1)%len(ring)]-angles[j]))for j in range(len(ring))]
  direction=1 if sum(deltas)>0 else -1;travel=[0.]
  for d in deltas[:-1]:travel.append(travel[-1]+abs(d))
  total=sum(abs(d)for d in deltas)
  # Hidden skin roots taper within the outer sleeve so the near plane cannot
  # expose a second flesh-coloured shoulder surface inside the sleeve volume.
  if 'casualsuit' not in o.name:center.x=sign*.18;ry*=.25;rz*=.25
  # Monotonic angles eliminate backtracking teeth in the threshold-cropped loop.
  radial=[Vector((0,ry*math.cos(angles[0]+direction*2*math.pi*d/total),rz*math.sin(angles[0]+direction*2*math.pi*d/total)))for d in travel]
  for j,i in enumerate(ring):verts[i]=center+radial[j]
  mutable.update(ring)
  # The threshold crop crosses several different torso/arm weight gradients.
  # A common, source-derived shoulder-ring blend removes that ragged transform
  # boundary without changing any distal vertex's skinning.
  names=sorted(set(k for i in ring for k in before['weights'][i]));mean={k:sum(before['weights'][i].get(k,0)for i in ring)/len(ring)for k in names}
  total=sum(mean.values());mean={k:v/total for k,v in mean.items()if v>1e-8}
  for i in ring:ws[i]=dict(mean)
  base=[verts[i]-center for i in ring]
  rY=max(abs(v.y)for v in base);rZ=max(abs(v.z)for v in base)
  # Only the new continuation adopts intact-source shoulder radii; never refit
  # existing forearm/hand vertices to the differently refined full-body mesh.
  sy=max(.85,min(1.15,profile['radiusY']/rY));sz=max(.85,min(1.15,profile['radiusZ']/rZ))
  vectors=[Vector((0,p.y*sy,p.z*sz))for p in base]
  for _ in range(3):vectors=[(vectors[(i-1)%len(ring)]+vectors[i]*2+vectors[(i+1)%len(ring)])/4 for i in range(len(ring))]
  prev=ring;newrings=[]
  p0=center;p1=center+Vector((-sign*.015,.04,0));p2=center+Vector((sign*.10,.28,.01));p3=center+Vector((sign*.15,.52,.02))
  for t in STEPS:
   eased=t*t*(3-2*t);c=p0*(1-t)**3+p1*3*(1-t)**2*t+p2*3*(1-t)*t*t+p3*t**3
   rotation=Matrix.Rotation(-sign*math.pi/2*eased,3,'Z');new=[]
   for j,original in enumerate(ring):
    radial=base[j].lerp(vectors[j],min(1.,t/.3));point=c+rotation@radial
    new.append(len(verts));verts.append(point);ws.append(blend_weights(ws[original],clavicle,eased));added.append(len(verts)-1)
   for j,a in enumerate(prev):
    k=(j+1)%len(ring);b=prev[k];faces.append((b,a,new[j],new[k]));mats.append(0);smooth.append(True)
    src=boundary[tuple(sorted((ring[j],ring[k])))];_,_,la,lb=src
    if src[0]!=ring[j]:la,lb=lb,la
    for name in uvnames:
     ua=Vector(old.uv_layers[name].data[la].uv);ub=Vector(old.uv_layers[name].data[lb].uv)
     old_t=0 if prev is ring else STEPS[len(newrings)-1]
     delta=Vector((0,t*.5));old_delta=Vector((0,old_t*.5))
     uvs[name].append([tuple(ub+old_delta),tuple(ua+old_delta),tuple(ua+delta),tuple(ub+delta)])
   newrings.append(new);prev=new
  cap_center=sum((verts[i]for i in prev),Vector())/len(prev);ci=len(verts);verts.append(cap_center);ws.append({clavicle:1.});added.append(ci)
  for j,a in enumerate(prev):
   b=prev[(j+1)%len(prev)];faces.append((b,a,ci));mats.append(0);smooth.append(True)
   for name in uvnames:uvs[name].append([(0,0),(1,0),(.5,.5)])
  caps.extend(prev+[ci]);repairs.append(dict(side=side,oldBoundaryVertices=ring,profile=profile,profileRadiusScale=[sy,sz],newRingCount=len(STEPS),capVertices=prev+[ci],oldBoundaryNowInterior=True))
 assert len(repairs)==2,(o.name,len(repairs))
 mesh=bpy.data.meshes.new(old.name+'_FPSeamRepair');mesh.from_pydata(verts,[],faces);mesh.update()
 for mat in old.materials:mesh.materials.append(mat)
 for i,p in enumerate(mesh.polygons):p.material_index=mats[i];p.use_smooth=smooth[i]
 for name in uvnames:
  uv=mesh.uv_layers.new(name=name)
  for i,p in enumerate(mesh.polygons):
   for li,v in zip(p.loop_indices,uvs[name][i]):uv.data[li].uv=v
 normal_mutable={i for p in old.polygons if any(v in mutable for v in p.vertices)for i in p.vertices}
 before['normalMutableVertexIds']=sorted(normal_mutable)
 mesh.update();norms=[tuple(n.vector)for n in mesh.corner_normals];
 for li,n in enumerate(oldnorm):
  if old.loops[li].vertex_index not in normal_mutable:norms[li]=n
 mesh.normals_split_custom_set(norms);o.data=mesh
 o.vertex_groups.clear()
 for group_name in before['groups']:o.vertex_groups.new(name=group_name)
 for i,w in enumerate(ws):
  for name,value in w.items():o.vertex_groups[name].add([i],value,'REPLACE')
 # Preserve distal positions/weights, every original polygon and existing UV.
 after=snap_mesh(o)
 assert all(p==after['positions'][i]for i,p in enumerate(before['positions'])if i not in mutable)
 before['mutableVertexIds']=sorted(mutable)
 before['mutablePolygonIds']=[p.index for p in old.polygons if any(i in mutable for i in p.vertices)]
 assert all(w==after['weights'][i]for i,w in enumerate(before['weights'])if i not in mutable)
 original_faces=len(old.polygons)
 for i,p in enumerate(old.polygons):
  assert tuple(mesh.polygons[i].vertices)==tuple(p.vertices)
  for name in uvnames:
   assert [tuple(old.uv_layers[name].data[j].uv)for j in p.loop_indices]==[tuple(mesh.uv_layers[name].data[j].uv)for j in mesh.polygons[i].loop_indices]
 remaining,_=loop_components(mesh)
 assert len(remaining)==(2 if 'casualsuit' in o.name else 0),(o.name,len(remaining))
 assert all((mesh.vertices[t.vertices[1]].co-mesh.vertices[t.vertices[0]].co).cross(mesh.vertices[t.vertices[2]].co-mesh.vertices[t.vertices[0]].co).length>1e-10 for t in mesh.loop_triangles),'Degenerate triangle'
 assert all(abs(sum(ws[i].values())-1)<2e-5 for i in set(added)|mutable),'Unnormalized repaired weights'
 edge_faces=collections.Counter(tuple(sorted((p.vertices[i],p.vertices[(i+1)%len(p.vertices)])))for p in mesh.polygons for i in range(len(p.vertices)))
 assert max(edge_faces.values())==2,'Non-manifold edge created'
 assert all(all(math.isfinite(x)for x in v.co)for v in mesh.vertices),'Non-finite vertex'
 assert all(set(w)<=set(before['groups'])and all(math.isfinite(v)and v>=0 for v in w.values())for w in ws),'Invalid bone influence'
 return dict(name=o.name,originalVertexCount=len(before['positions']),originalTriangleCount=len(before['triangles']),resultVertexCount=len(mesh.vertices),resultTriangleCount=len(mesh.loop_triangles),addedVertexIndices=added,capVertices=caps,repairs=repairs,remainingBoundaryLoops=[len(r)for r in remaining],distalGeometryWeightsAndAllExistingUvExact=True,modifiedProximalVertices=sorted(mutable),maxProximalDisplacementM=max((Vector(before['positions'][i])-mesh.vertices[i].co).length for i in mutable),maxProximalWeightChange=max(abs(before['weights'][i].get(k,0)-ws[i].get(k,0))for i in mutable for k in set(before['weights'][i])|set(ws[i])),maximumIncidentFacesPerEdge=max(edge_faces.values()),addedVerticesWeightSumRange=[min(sum(ws[i].values())for i in added),max(sum(ws[i].values())for i in added)],maximumAddedBoneInfluences=max(len(ws[i])for i in added),originalWeightSumRange=[min(map(lambda w:sum(w.values()),before['weights'])),max(map(lambda w:sum(w.values()),before['weights']))]),before

def preserve_original_fbx_records(candidate,baselines):
 """Keep all original bones/models/bind transforms/materials/connections exactly.
 Blender's normal FBX rewrite introduces small bone-matrix rounding changes.
 Only replace geometry arrays and cluster vertex index/weight lists in the
 original parsed document; no skeleton or animation record is regenerated.
 """
 import copy
 from io_scene_fbx import parse_fbx,encode_bin
 old,version=parse_fbx.parse(str(BASE));new,nversion=parse_fbx.parse(str(candidate));assert version==nversion
 def label(e):return e.props[1].split(b'\0')[0].decode()
 def records(tree):
  objects=next(e for e in tree.elems if e.id==b'Objects');byid={e.props[0]:e for e in objects.elems}
  connections=next(e for e in tree.elems if e.id==b'Connections')
  parents=collections.defaultdict(list)
  for c in connections.elems:
   if c.props[0]==b'OO':parents[c.props[1]].append(c.props[2])
  geom={}
  for e in objects.elems:
   if e.id==b'Geometry':
    model=next(byid[i]for i in parents[e.props[0]]if byid[i].id==b'Model');geom[label(model)]=e
  clusters={}
  for e in objects.elems:
   if e.id==b'Deformer'and e.props[2]==b'Cluster':
    skin=next(byid[i]for i in parents[e.props[0]]if byid[i].id==b'Deformer')
    geometry=next(byid[i]for i in parents[skin.props[0]]if byid[i].id==b'Geometry')
    model=next(byid[i]for i in parents[geometry.props[0]]if byid[i].id==b'Model')
    clusters[(label(model),label(e))]=e
  return objects,geom,clusters
 objects,og,oc=records(old);_,ng,nc=records(new)
 def canonical(e):
  data=hashlib.sha256(e.id+bytes(e.props_type))
  for value in e.props:
   if hasattr(value,'tobytes'):value=value.tobytes()
   data.update(repr(value).encode())
  for child in e.elems:data.update(canonical(child).encode())
  return data.hexdigest()
 protected={e.props[0]:canonical(e)for e in objects.elems if e.id not in [b'Geometry',b'Deformer']}
 def protected_deformer(e):
  result=copy.deepcopy(e)
  if e.props[2]==b'Cluster':result.elems[:]=[child for child in result.elems if child.id not in [b'Indexes',b'Weights']]
  return canonical(result)
 deformer_hashes={e.props[0]:protected_deformer(e)for e in objects.elems if e.id==b'Deformer'}
 # Blender's encoder stamps FileId/CreationTime only; every substantive top-level
 # FBX section, including unit axes, bind pose definitions and connections stays.
 section_hashes={e.id.decode():canonical(e)for e in old.elems if e.id not in [b'Objects',b'FileId',b'CreationTime']}
 changed=[]
 for name,g in og.items():
  n=ng[name]
  for prop in [b'Vertices',b'PolygonVertexIndex']:
   original=next(c.props[0]for c in g.elems if c.id==prop);updated=next(c.props[0]for c in n.elems if c.id==prop)
   if prop==b'Vertices':
    mutable=set(next(b['mutableVertexIds']for b in baselines if b['name']==name))
    assert all(v==updated[i]for i,v in enumerate(original)if i//3 not in mutable),('Distal geometry array changed',name)
   else:assert original==updated[:len(original)],('Original geometry topology changed',name,prop)
  # Preserve the original raw normal values for unchanged vertices, rather
  # than accepting Blender's custom-normal re-encoding of them.
  old_normal=next(e for e in g.elems if e.id==b'LayerElementNormal')
  new_normal=next(e for e in n.elems if e.id==b'LayerElementNormal')
  def child(layer,key):return next(e for e in layer.elems if e.id==key)
  assert child(old_normal,b'MappingInformationType').props[0]==b'ByVertice'
  assert child(new_normal,b'MappingInformationType').props[0]==b'ByPolygonVertex'
  ov=child(old_normal,b'Normals').props[0];oi=child(old_normal,b'NormalsIndex').props[0]
  nv=child(new_normal,b'Normals').props[0];ni=child(new_normal,b'NormalsIndex').props[0]
  pvi=next(e.props[0]for e in g.elems if e.id==b'PolygonVertexIndex');normal_map={}
  mutable=set(next(b['normalMutableVertexIds']for b in baselines if b['name']==name))
  for loop,i in enumerate(pvi):
   vi=i if i>=0 else -i-1
   if vi in mutable:continue
   old_index=oi[vi]
   if old_index not in normal_map:normal_map[old_index]=len(nv)//3;nv.extend(ov[old_index*3:old_index*3+3])
   ni[loop]=normal_map[old_index]
   assert nv[ni[loop]*3:ni[loop]*3+3]==ov[old_index*3:old_index*3+3]
  g.elems[:]=copy.deepcopy(n.elems);changed.append(name)
 for key,c in oc.items():
  n=nc[key]
  old_arrays={e.id:e for e in c.elems if e.id in [b'Indexes',b'Weights']}
  new_arrays={e.id:e for e in n.elems if e.id in [b'Indexes',b'Weights']}
  old_pairs=dict(zip(old_arrays[b'Indexes'].props[0],old_arrays[b'Weights'].props[0]))if old_arrays else{}
  mutable=set(next(b['mutableVertexIds']for b in baselines if b['name']==key[0]))
  if new_arrays:
   new_pairs=dict(zip(new_arrays[b'Indexes'].props[0],new_arrays[b'Weights'].props[0]))
   assert all(i in new_pairs and abs(new_pairs[i]-w)<2e-6 for i,w in old_pairs.items()if i not in mutable),('Distal weight lost',key)
   # Preserve raw distal weights, including any original tiny normalization error.
   for j,i in enumerate(new_arrays[b'Indexes'].props[0]):
    if i in old_pairs and i not in mutable:new_arrays[b'Weights'].props[0][j]=old_pairs[i]
  c.elems[:]=[e for e in c.elems if e.id not in [b'Indexes',b'Weights']]+copy.deepcopy(list(new_arrays.values()))
 assert all(canonical(e)==protected[e.props[0]]for e in objects.elems if e.props[0]in protected)
 methods={'Y':'add_int16','C':'add_char','B':'add_bool','Z':'add_int8','I':'add_int32','F':'add_float32','D':'add_float64','L':'add_int64','R':'add_bytes','S':'add_string','b':'add_bool_array','c':'add_byte_array','i':'add_int32_array','l':'add_int64_array','f':'add_float32_array','d':'add_float64_array'}
 def encode(e):
  result=encode_bin.FBXElem(e.id)
  for code,value in zip(e.props_type,e.props):getattr(result,methods[chr(code)])(value)
  result.elems=[encode(child)for child in e.elems];return result
 encode_bin.write(str(OUTPUT),encode(old),version)
 checked,_=parse_fbx.parse(str(OUTPUT));checked_objects,_,_=records(checked)
 assert all(canonical(e)==protected[e.props[0]]for e in checked_objects.elems if e.props[0]in protected),[(e.id,e.props[:2])for e in checked_objects.elems if e.props[0]in protected and canonical(e)!=protected[e.props[0]]][:4]
 assert all(protected_deformer(e)==deformer_hashes[e.props[0]]for e in checked_objects.elems if e.id==b'Deformer'),'Original skin/bind transforms changed'
 assert all(canonical(e)==section_hashes[e.id.decode()]for e in checked.elems if e.id.decode()in section_hashes),'Original FBX metadata/connections changed'
 return dict(originalNonGeometryObjectRecordsPreserved=len(protected),originalSkinAndClusterRecordsPreservedExceptVertexIndexWeights=len(deformer_hashes),preservedSectionSha256=section_hashes,geometryModelNames=changed,originalModelBoneMaterialTextureAndConnectionRecordsPreserved=True,distalRawNormalValuesPreserved=True)

def compare_roundtrip(baselines,rig_before):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(OUTPUT),use_anim=False)
 rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');now=snap_rig(rig)
 assert [(b['name'],b['parent'])for b in now['bones']]==[(b['name'],b['parent'])for b in rig_before['bones']],'Skeleton names/order/parents changed'
 bone_error=max(abs(x-y)for a,b in zip(rig_before['bones'],now['bones'])for r,s in zip(a['matrix'],b['matrix'])for x,y in zip(r,s))
 root_error=max(abs(x-y)for r,s in zip(rig_before['matrix'],now['matrix'])for x,y in zip(r,s));assert bone_error<3e-6 and root_error<3e-6,(bone_error,root_error)
 results=[]
 for base in baselines:
  o=bpy.data.objects[base['name']];m=o.data;m.calc_loop_triangles();ws=weights(o)
  mapping=[];pe=we=0.
  mutable=set(base['mutableVertexIds']);proximal_error=0.
  # Original vertices keep their FBX indices; independently verify all unchanged
  # distal coordinates and weights, excluding the four repaired shoulder loops.
  assert len(m.vertices)>=len(base['positions'])
  for i,(p,w)in enumerate(zip(base['positions'],base['weights'])):
   mapping.append(i);dist=(m.vertices[i].co-Vector(p)).length
   if i in mutable:proximal_error=max(proximal_error,dist)
   else:pe=max(pe,dist)
   if i not in mutable:we=max(we,max(abs(w.get(k,0)-ws[i].get(k,0))for k in set(w)|set(ws[i])))
  assert pe<2e-6 and we<2e-6,(pe,we)
  assert base['polygons']==[list(p.vertices)for p in m.polygons][:len(base['polygons'])],'Original polygon topology changed'
  ue=ne=0.
  for t in base['triangles']:
   ids=[mapping[i]for i in t['vertices']];rt=m.polygons[t['polygonIndex']];assert set(ids)<=set(rt.vertices),'Missing original polygon'
   assert m.materials[rt.material_index].name==t['material']
   loops={v:l for v,l in zip(rt.vertices,rt.loop_indices)}
   for j,v in enumerate(ids):
    li=loops[v];ue=max(ue,(m.uv_layers[0].data[li].uv-Vector(t['uv'][j])).length)
    if v not in base['normalMutableVertexIds']:ne=max(ne,(m.corner_normals[li].vector-Vector(t['normals'][j])).length)
  assert ue<2e-6 and ne<1e-3,(ue,ne) # Native custom-normal encoding precision; raw normals are preserved above.
  remaining,_=loop_components(m);assert len(remaining)==(2 if 'casualsuit' in o.name else 0)
  results.append(dict(name=o.name,preservedDistalVertices=len(mapping)-len(mutable),modifiedProximalVertices=len(mutable),maxProximalDisplacementM=proximal_error,preservedPolygonTopology=len(base['polygons']),originalTriangleCount=len(base['triangles']),maxPositionErrorM=pe,maxWeightError=we,maxUvError=ue,maxCornerNormalError=ne,remainingBoundaryLoops=[len(r)for r in remaining]))
 return dict(skeletonBoneCount=len(now['bones']),maxBoneMatrixElementError=bone_error,maxRootMatrixElementError=root_error,meshes=results)

def main():
 EVIDENCE.mkdir(parents=True,exist_ok=True)
 protected=[BASE,INTACT,SOURCE/'SK_C01.fbx',SOURCE/'AN_C01_ArmsIdle.fbx',SOURCE/'AN_C01_Idle.fbx',SOURCE/'AN_C01_Walk.fbx']
 before={str(p):sha(p)for p in protected};assert before[str(BASE)]==EXPECTED,'Active baseline changed; audit before rebuilding'
 bpy.ops.wm.open_mainfile(filepath=str(INTACT),load_ui=False)
 donors={name:[v.co.copy()for v in bpy.data.objects[name[:-5]].data.vertices]for name in NAMES}
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(BASE),use_anim=False)
 rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');rig_before=snap_rig(rig);rig.animation_data_clear()
 for p in rig.pose.bones:p.matrix_basis=Matrix.Identity(4)
 results=[];baselines=[]
 for name in NAMES:
  result,baseline=repair(bpy.data.objects[name],donors[name]);results.append(result);baselines.append(baseline)
 # Stable semantic input hash makes repeated invocations byte-idempotent while
 # still rebuilding/validating the candidate in memory and reopening the FBX.
 key=hashlib.sha256((sha(Path(__file__))+json.dumps(before,sort_keys=True)).encode()).hexdigest()
 prior_path=HERE/'repair_export.json';prior=json.loads(prior_path.read_text())if prior_path.exists()else{}
 reuse=prior.get('inputKey')==key and OUTPUT.exists()and prior.get('outputSha256')==sha(OUTPUT)
 if not reuse:
  bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
  for name in NAMES:bpy.data.objects[name].select_set(True)
  bpy.context.view_layer.objects.active=rig
  candidate=HERE/'SK_C01_Arms_SeamRepair.working.fbx'
  bpy.ops.export_scene.fbx(filepath=str(candidate),use_selection=True,object_types={'ARMATURE','MESH'},global_scale=1.,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=False,use_mesh_modifiers=False,mesh_smooth_type='FACE',path_mode='RELATIVE',embed_textures=False,bake_anim=False)
  raw_preservation=preserve_original_fbx_records(candidate,baselines);candidate.unlink()
 else:raw_preservation=prior['rawFbxRecordPreservation']
 roundtrip=compare_roundtrip(baselines,rig_before)
 after={str(p):sha(p)for p in protected};assert before==after,'Protected input changed'
 report=dict(status='PASS_FP_SEAM_REPAIR_SOURCE_VALIDATION',scope='FP-only geometry derivative and native Blender topology/FBX roundtrip validation; Unreal import, rendering and networking not run.',inputKey=key,output=str(OUTPUT),outputSha256=sha(OUTPUT),reusedByteIdenticalOutput=reuse,protectedInputsBefore=before,protectedInputsAfter=after,protectedInputsUnchanged=True,sourceProfileNote='Later intact garment/body have different refinements from cropped FP copies. Only donor shoulder radii guide the new proximal continuation; four proximal loop positions/weights/normals are smoothed; all distal geometry/weights and every existing UV remain unchanged.',meshes=results,roundtrip=roundtrip,rawFbxRecordPreservation=raw_preservation)
 prior_path.write_text(json.dumps(report,indent=2)+'\n');(EVIDENCE/'repair_validation.json').write_text(json.dumps(report,indent=2)+'\n')
 (EVIDENCE/'pre_repair_geometry_snapshot.json').write_text(json.dumps(dict(rig=rig_before,meshes=baselines))+'\n')
 print('SSO_FP_SEAM_REPAIR_PASS');print(json.dumps({k:report[k]for k in ['status','output','outputSha256','reusedByteIdenticalOutput','roundtrip']},indent=2))
if __name__=='__main__':main()
