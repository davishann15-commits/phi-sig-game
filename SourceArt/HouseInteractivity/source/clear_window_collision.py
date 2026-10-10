"""Remove hidden glass proxy collision; preserve adjacent wall/sill/frame solids."""
import bpy,json,os,runpy,collections
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));d=json.load(open(R+'/extraction-manifest.json'));rem={x['source_actor_label']:x for x in d['remainders']};windows=collections.defaultdict(list)
for p in d['pieces']:
 if p['behavior']=='window':windows[p['source_actor_label'].split('_')[1]].append(p)
def volume(lo,hi):return max(0,hi.x-lo.x)*max(0,hi.y-lo.y)*max(0,hi.z-lo.z)
def subtract(lo,hi,cl,ch):
 il=Vector([max(lo[i],cl[i])for i in range(3)]);ih=Vector([min(hi[i],ch[i])for i in range(3)])
 if any(ih[i]-il[i]<.000005 for i in range(3)):return[(lo,hi)],0
 parts=[];a=lo.copy();b=hi.copy()
 for i in range(3):
  if il[i]>a[i]+.000005:
   h=b.copy();h[i]=il[i];parts.append((a.copy(),h));a[i]=il[i]
  if ih[i]<b[i]-.000005:
   l=a.copy();l[i]=ih[i];parts.append((l,b.copy()));b[i]=ih[i]
 return parts,volume(il,ih)
audit=[];skipped=[]
for actor in n['actors']:
 label=actor['label'];floor=label.split('_')[1] if label.startswith('COL_') else ''
 if floor not in windows or actor['class']!='StaticMeshActor':continue
 c=actor['components'][0];info=n['meshes'][c['mesh']];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=rem[label]['fbx'] if label in rem else info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
 if not len(o.data.materials):o.data.materials.append(bpy.data.materials.new('WorldGridMaterial'))
 ps=s['components'](o);removefaces=set();newboxes=[];entries=[]
 for q in ps:
  boxes=[(q['lo'],q['hi'])];hits=[];removedvol=0
  for p in windows[floor]:
   assert all(abs(c['transform'][k][i]-p['source_transform'][k][i])<1e-4 for k in ['location','rotation','scale']for i in range(3)),label
   lo,hi=map(Vector,p['bounds_blender_metres']);normal=0 if hi.x-lo.x<hi.y-lo.y else 1
   if q['size'][normal]>.80:continue
   cl=lo.copy();ch=hi.copy();cl[normal]-=.35;ch[normal]+=.35
   if any(q['hi'][i]<cl[i] or q['lo'][i]>ch[i]for i in range(3)):continue
   # Only existing axis-aligned hidden collision boxes can be sliced exactly.
   coords=[o.matrix_world@o.data.vertices[v].co for f in q['faces']for v in o.data.polygons[f].vertices]
   aligned=all(all(min(abs(v[i]-q['lo'][i]),abs(v[i]-q['hi'][i]))<.00005 for i in range(3))for v in coords)
   if not aligned:
    skipped.append(dict(actor=label,window=p['id'],bounds=[s['raw'](q['lo']),s['raw'](q['hi'])],reason='non-axis-aligned source solid'));continue
   nxt=[];cut=0
   for bl,bh in boxes:
    parts,vol=subtract(bl,bh,cl,ch);nxt.extend(parts);cut+=vol
   if cut>0:boxes=nxt;removedvol+=cut;hits.append(p['id'])
  if hits:
   removefaces.update(q['faces']);newboxes.extend(boxes);entries.append(dict(windows=hits,original_bounds=[s['raw'](q['lo']),s['raw'](q['hi'])],remaining_boxes=[[s['raw'](a),s['raw'](b)]for a,b in boxes],removed_volume_m3=removedvol))
 if not entries:continue
 kept=set(range(len(o.data.polygons)))-removefaces;verts=[];faces=[]
 for f in sorted(kept):
  ids=[]
  for v in o.data.polygons[f].vertices:ids.append(len(verts));verts.append(o.matrix_world@o.data.vertices[v].co)
  faces.append(ids)
 for lo,hi in newboxes:
  base=len(verts);verts.extend([(lo.x,lo.y,lo.z),(hi.x,lo.y,lo.z),(hi.x,hi.y,lo.z),(lo.x,hi.y,lo.z),(lo.x,lo.y,hi.z),(hi.x,lo.y,hi.z),(hi.x,hi.y,hi.z),(lo.x,hi.y,hi.z)])
  faces.extend([tuple(base+i for i in f)for f in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]])
 stem='HI_WindowPassage_'+label;me=bpy.data.meshes.new(stem);me.from_pydata(verts,[],faces);me.update();me.materials.append(bpy.data.materials.new('WorldGridMaterial'));ob=bpy.data.objects.new(stem,me);bpy.context.collection.objects.link(ob)
 rem[label]=dict(source_actor_label=label,source_mesh=c['mesh'],fbx=s['export'](ob,stem),asset_name=stem,material_slots=['/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial'],collision_remainder=True)
 audit.append(dict(actor=label,original_components=len(ps),modified_components=len(entries),original_faces=len(o.data.polygons),retained_original_faces=len(kept),new_fragment_boxes=len(newboxes),cuts=entries))
d['remainders']=list(rem.values());json.dump(d,open(R+'/extraction-manifest.json','w'),indent=2);json.dump(dict(modified=audit,skipped=skipped),open(R+'/inventory/window-collision-clearance-audit.json','w'),indent=2);print('WINDOW_COLLISION_CLEARANCE_DONE',len(audit),sum(x['modified_components']for x in audit),'skipped',len(skipped))
