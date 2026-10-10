import bpy, json, os, re, runpy
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));d=json.load(open(R+'/extraction-manifest.json'));actors={a['label']:a for a in n['actors']};converted={p['source_actor_label'] for p in d['pieces']};remove=set(d['remove_colliders']);audit=[]
for label in actors:
 if label=='COL_GR3_Furniture' or label in ('PIG_MattressCollision','PIG_TroughCollision','PIG_BucketCollision'):remove.add(label)
 if label.startswith('BR_COL_Bedroom_') and label.replace('BR_COL_','BR_SM_') in converted:remove.add(label)
 if label.startswith('R204_COL_') and label.replace('R204_COL_','R204_') in converted:remove.add(label)
 if label.startswith('UpBath_COL_Bath_') and label.replace('UpBath_COL_','UpBath_SM_') in converted:remove.add(label)
 if label.startswith('UpBath_Bath_') and 'blocker_' in label:
  visual=label.replace('stall_blocker','open_toilet_stall').replace('toilet_blocker','ceramic_toilet').replace('shower_blocker','orange_shower_frame')
  if visual in converted:remove.add(label)

def load(label):
 bpy.ops.wm.read_factory_settings(use_empty=True);a=actors[label];c=a['components'][0];info=n['meshes'][c['mesh']];bpy.ops.import_scene.fbx(filepath=info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_')))
 if not len(o.data.materials):o.data.materials.append(bpy.data.materials.new('WorldGridMaterial'))
 return c,o,s['components'](o)
# Mixed original spa collision: remove only boxes authored for now-separated furniture.
label='COL_SpaRoom';c,o,ps=load(label)
centers=[(-29.62,-1.68,.29),(-28.61,.80,.23),(-28.61,1.74,.68),(-26.97,.80,.23),(-26.97,1.74,.68),(-29.69,2.55,.77),(-24.12,1.04,.535),(-25.11,.44,.35),(-25.11,1.61,.35)]
removed=[p for p in ps if any((p['center']-Vector(v)).length<.05 for v in centers)]
assert len(removed)==9,(len(removed),[(tuple(p['center']),tuple(p['size'])) for p in ps])
fs={f for p in ps if p not in removed for f in p['faces']};stem='HI_CollisionRemainder_SpaRoom';out,slots=s['submesh'](o,fs,stem)
d['remainders'].append(dict(source_actor_label=label,source_mesh=c['mesh'],fbx=s['export'](out,stem),asset_name=stem,material_slots=['/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial'],collision_remainder=True))
audit.append(dict(actor=label,original_components=len(ps),removed_components=len(removed),retained_components=len(ps)-len(removed),removed_bounds=[[s['raw'](p['lo']),s['raw'](p['hi'])] for p in removed],conservation_pass=True))
# Inspect architectural tile collision for only fully-contained furnishing boxes.
for p in d['pieces']:
 if p.get('name')!='Bathroom cabinet' or '__embedded_' not in p['id']:continue
 label=p['source_actor_label'].replace('SM_','COL_',1)
 if label not in actors:continue
 c,o,ps=load(label);lo,hi=map(Vector,p['bounds_blender_metres']);removed=[q for q in ps if all(q['lo'][k]>=lo[k]-.10 and q['hi'][k]<=hi[k]+.10 for k in range(3))]
 if not removed:
  audit.append(dict(actor=label,removed_components=0,note='No original collision box fully inside cabinet bounds'));continue
 fs={f for q in ps if q not in removed for f in q['faces']};stem='HI_CollisionRemainder_'+label;out,slots=s['submesh'](o,fs,stem)
 d['remainders'].append(dict(source_actor_label=label,source_mesh=c['mesh'],fbx=s['export'](out,stem),asset_name=stem,material_slots=['/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial'],collision_remainder=True))
 audit.append(dict(actor=label,original_components=len(ps),removed_components=len(removed),retained_components=len(ps)-len(removed),conservation_pass=True))
# Centered triangular glass shard, Blender meter dimensions 0.20 x 0.12 x 0.002.
bpy.ops.wm.read_factory_settings(use_empty=True);verts=[(x,y,z) for z in (-.001,.001) for x,y in [(-.1,-.06),(.1,-.06),(-.02,.06)]];faces=[(2,1,0),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)];me=bpy.data.meshes.new('HI_GlassShard');me.from_pydata(verts,[],faces);me.update();mat=bpy.data.materials.new('Glass');me.materials.append(mat);o=bpy.data.objects.new('HI_GlassShard',me);bpy.context.collection.objects.link(o)
glasspath=next(p['material_slots'][0] for p in d['pieces'] if p['behavior']=='window');d['shared_meshes']=[dict(asset_name='HI_GlassShard',fbx=s['export'](o,'HI_GlassShard'),material_slots=[glasspath],dimensions_cm=[20,12,.2])]
d['remove_colliders']=sorted(remove);json.dump(d,open(R+'/extraction-manifest.json','w'),indent=2);json.dump(audit,open(R+'/inventory/collision-cleanup-audit.json','w'),indent=2);print('COLLISION_FINALIZE_DONE',len(remove),len(audit))
