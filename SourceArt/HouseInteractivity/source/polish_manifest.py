"""Material-name reconciliation and incremental visible damage stages."""
import bpy,bmesh,json,os,re,runpy,hashlib,random
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');d=json.load(open(R+'/extraction-manifest.json'));n=json.load(open(R+'/context/actors.json'));pathmap={};warnings=[]
for info in n['meshes'].values():
 for slot in info['materials']:
  p=slot['material']
  if p:pathmap.setdefault(p.split('/')[-1].split('.')[0],set()).add(p)
def material_paths(o,row):
 native=[]
 for m in o.data.materials:
  name=re.sub(r'\.\d+$','',m.name) if m else ''
  hits=list(pathmap.get(name,[]))
  if len(hits)==1:native.append(hits[0]);continue
  source=[p for p in row.get('material_slots',[]) if p and p.split('/')[-1].split('.')[0]==name]
  if source:native.append(source[0]);continue
  if not hits:raise RuntimeError('Unmapped exported material '+name+' '+row.get('id','remainder'))
  native.append(sorted(hits)[0]);warnings.append([row.get('id'),name,hits])
 return native

def stage(intact,name,level):
 m=intact.data;vs=m.vertices;lo=Vector(tuple(min(v.co[k] for v in vs) for k in range(3)));hi=Vector(tuple(max(v.co[k] for v in vs) for k in range(3)));sz=hi-lo
 verts=[];faces=[];uvsrc=[];mi=[];smooth=[];lookup={}
 damage=.035 if level==1 else .28
 for poly in m.polygons:
  c=sum((vs[v].co for v in poly.vertices),Vector())/len(poly.vertices)
  fault=(c.x-lo.x)/max(sz.x,.01)+.23*(c.y-lo.y)/max(sz.y,.01);piece=0 if fault<.39 else 1 if fault<.79 else 2
  # Chipped rims expose sparse gaps at outer upper edges, progressing to
  # missing corners, bent frames and separated upholstery at stage two.
  edge=abs((c.x-(lo.x+hi.x)*.5)/max(sz.x*.5,.01))
  token=int(hashlib.md5((name+str(poly.index)).encode()).hexdigest()[:8],16)
  chip=(c.z>lo.z+sz.z*.55 and edge>.7 and token%100<(2 if level==1 else 8))
  if chip:continue
  inds=[]
  for v in poly.vertices:
   key=(piece,v)
   if key not in lookup:
    p=vs[v].co.copy();side=piece-1
    p.x+=side*min(.11,sz.x*.11)*damage;p.y+=side*min(.08,sz.y*.09)*damage
    p.z=lo.z+(p.z-lo.z)*(1-damage)
    p.z+=abs(side)*damage*.07*(p.x-lo.x)
    if edge>.65:p.z+=damage*.014*__import__('math').sin(p.x*21+p.y*13)
    lookup[key]=len(verts);verts.append(p)
   inds.append(lookup[key])
  faces.append(inds);uvsrc.append(poly.loop_indices);mi.append(poly.material_index);smooth.append(poly.use_smooth)
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
 for mat in m.materials:mesh.materials.append(mat)
 for poly,mat,sm in zip(mesh.polygons,mi,smooth):poly.material_index=mat;poly.use_smooth=sm
 for layer in m.uv_layers:
  uv=mesh.uv_layers.new(name=layer.name)
  for new,old in zip(mesh.polygons,uvsrc):
   for dst,src in zip(new.loop_indices,old):uv.data[dst].uv=layer.data[src].uv
 ob=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(ob);return ob

assets={};audit=[]
for row in d['pieces']:
 stem=row['asset_name']
 if stem in assets:row.update(assets[stem]);continue
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=row['intact_fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH')
 mats=material_paths(o,row);changes={'material_slots':mats,'material_names':[m.name for m in o.data.materials]}
 if row['id'] in ('SMF_Upper_ALL__040','SMF_Upper_ALL__042'):
  row['behavior']='chandelier';row['name']='Great room wrought iron ring chandelier';lo,hi=row['bounds_blender_metres'];row['pivot_cm']=[0,0,(hi[2]-lo[2])*50];row['broken_fbx']=None
 if row['behavior']=='furniture':
  for lev,suffix,key in [(1,'_Damaged','damaged_fbx'),(2,'_HeavilyDamaged','heavily_damaged_fbx')]:
   ob=stage(o,stem+suffix,lev);changes[key]=s['export'](ob,stem+suffix);bpy.data.objects.remove(ob,do_unlink=True)
  label=row['source_actor_label'];text=(label+' '+row['name']+' '+' '.join(mats)).lower();lo,hi=row['bounds_blender_metres'];sz=[hi[i]-lo[i] for i in range(3)]
  named_wall=any(t in text for t in ('poster','banner','plaque','_art_','flag','walltelevision','framedcollage','wingedwreath','collegiatet','coronaoval','volshelmet','_crest','part ytime','photo composite','photocomposite','hanging','_hook','entrycoatrack','checkeredt','window schedule','windowschedule','fishposter','santabass','alarm','keep shut','keepshut'))
  floor=3.023 if 'Upper' in label else -3.2906 if 'Basement' in label else 0
  mounted_shape=label.startswith('SMF_') and lo[2]>floor+1.15 and min(sz[:2])<.45 and max(sz[:2])>.3 and sz[2]>.3
  if named_wall or mounted_shape:changes.update(wall_mounted=True,movable=True)
  if 'rug' in text and sz[2]<.12:changes.update(movable=False,wall_mounted=False)
 row.update(changes);assets[stem]=changes
 audit.append({'id':row['id'],'materials':len(mats),'faces':len(o.data.polygons),'behavior':row['behavior']})
 if len(audit)%30==0:print('POLISH_PROGRESS',len(audit),flush=True)
for row in d['remainders']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=row['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH');row['material_slots']=material_paths(o,row)
json.dump(d,open(R+'/extraction-manifest.json','w'),indent=2)
json.dump({'assets':audit,'ambiguous_materials':warnings,'passed':True},open(R+'/inventory/material-stage-audit.json','w'),indent=2)
print('POLISH_DONE',len(audit),len(warnings))
