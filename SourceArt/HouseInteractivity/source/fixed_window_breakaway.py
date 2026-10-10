import bpy,json,os,re,runpy,collections
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));d=json.load(open(R+'/extraction-manifest.json'));aa={a['label']:a for a in n['actors']};rem={v['source_actor_label']:v for v in d['remainders']};pathmap={}
for info in n['meshes'].values():
 for mat in info['materials']:
  if mat['material']:pathmap[mat['material'].split('/')[-1].split('.')[0]]=mat['material']
windows=collections.defaultdict(list)
for p in d['pieces']:
 if p['behavior']!='window':continue
 lo,hi=map(Vector,p['bounds_blender_metres']);sz=hi-lo;scale=p['source_transform']['scale']
 if max(sz.x*scale[0],sz.y*scale[1])>=.64 and sz.z*scale[2]>=.85:windows[p['source_actor_label'].replace('_Glass','_Solid')].append(p)
audit=[];col_audit=[]
def load(label):
 c=aa[label]['components'][0];info=n['meshes'][c['mesh']];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=rem[label]['fbx'] if label in rem else info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_')))
 if not len(o.data.materials):o.data.materials.append(bpy.data.materials.new('WorldGridMaterial'))
 return c,o,s['components'](o)
def interior(q,p,collision=False):
 lo,hi=map(Vector,p['bounds_blender_metres']);sz=hi-lo;axis=0 if sz.x>sz.y else 1;normal=1-axis
 if not(lo[normal]-.20<q['center'][normal]<hi[normal]+.20) or q['size'][normal]>.19:return False
 if q['lo'][axis]<lo[axis]-.085 or q['hi'][axis]>hi[axis]+.085:return False
 if not collision and not any('PaintWhite' in m for m in q['materials']):return False
 vertical=q['size'][axis]<.081 and q['size'].z>.35 and lo[axis]+.12<q['center'][axis]<hi[axis]-.12 and q['lo'].z>=lo.z-.085 and q['hi'].z<=hi.z+.085
 horizontal=q['size'][axis]>.32 and q['size'].z<.061 and lo.z+.08<q['center'].z<hi.z-.08
 kick=q['size'][axis]>.32 and .17<q['size'].z<.23 and abs(q['lo'].z-(lo.z-.03))<.035
 return vertical or horizontal or kick
for label,panes in windows.items():
 c,o,parts=load(label);removed=set();rows=[]
 for p in panes:
  assert c['transform']==p['source_transform'],(label,'transform mismatch')
  chosen=[q for q in parts if interior(q,p)];fs={f for q in chosen for f in q['faces']};fs-=removed
  if not fs:rows.append(dict(window_id=p['id'],components=0));continue
  removed|=fs;origin=Vector((p['local_origin_cm'][0]/100,-p['local_origin_cm'][1]/100,p['local_origin_cm'][2]/100));stem='HI_Breakaway_'+p['id'];ob,slots=s['submesh'](o,fs,stem,origin);mats=[pathmap[re.sub(r'\.\d+$','',m.name)]for m in ob.data.materials]
  p.update(breakaway_frame_asset_name=stem,breakaway_frame_fbx=s['export'](ob,stem),breakaway_frame_material_slots=mats)
  rows.append(dict(window_id=p['id'],components=len(chosen),faces=len(fs),bounds=[[s['raw'](q['lo']),s['raw'](q['hi'])]for q in chosen]));bpy.data.objects.remove(ob,do_unlink=True)
 if removed:
  fs=set(range(len(o.data.polygons)))-removed;stem='HI_WindowCleared_'+label;ob,slots=s['submesh'](o,fs,stem);row=dict(source_actor_label=label,source_mesh=c['mesh'],fbx=s['export'](ob,stem),asset_name=stem,material_slots=[pathmap[re.sub(r'\.\d+$','',m.name)]for m in ob.data.materials]);rem[label]=row
 audit.append(dict(actor=label,original_faces=len(o.data.polygons),extracted_faces=len(removed),retained_faces=len(o.data.polygons)-len(removed),windows=rows,conservation_pass=True))
 # Any invisible collision bars wholly matching an opening are removed too.
 cl=label.replace('SM_','COL_',1)
 if cl not in aa:continue
 c,o,parts=load(cl);chosen=[q for q in parts if any(interior(q,p,True)for p in panes)]
 fs={f for q in chosen for f in q['faces']}
 if fs:
  kept=set(range(len(o.data.polygons)))-fs;stem='HI_WindowCleared_'+cl;ob,slots=s['submesh'](o,kept,stem);rem[cl]=dict(source_actor_label=cl,source_mesh=c['mesh'],fbx=s['export'](ob,stem),asset_name=stem,material_slots=['/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial'],collision_remainder=True)
 col_audit.append(dict(actor=cl,removed_components=len(chosen),original_components=len(parts),removed_faces=len(fs),bounds=[[s['raw'](q['lo']),s['raw'](q['hi'])]for q in chosen]))
d['remainders']=list(rem.values());json.dump(d,open(R+'/extraction-manifest.json','w'),indent=2);json.dump(dict(visual=audit,collision=col_audit),open(R+'/inventory/fixed-window-breakaway-audit.json','w'),indent=2);print('FIXED_WINDOW_BREAKAWAY_DONE',sum(len([x for x in a['windows']if x['components']])for a in audit),sum(a['removed_components']for a in col_audit))
