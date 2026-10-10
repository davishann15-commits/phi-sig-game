import bpy,json,os,re,runpy
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));d=json.load(open(R+'/extraction-manifest.json'));out=[]
for a in n['actors']:
 if not a['label'].startswith('OriginalPorchDoor'):continue
 c=next(c for c in a['components'] if c['name']=='OriginalDoorLeaf');info=n['meshes'][c['mesh']];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'));ps=s['components'](o)
 # Explicit authored profile: 22mm muntins and 200mm lower kickrail.
 remove=[p for p in ps if (p['size'].x<.03 and p['size'].z>1.8) or (p['size'].x>.6 and p['size'].z<.025) or(p['size'].x>.6 and .19<p['size'].z<.21 and p['lo'].z<.05)]
 assert len(remove)==6,(a['label'],len(remove));fs={f for p in ps if p not in remove for f in p['faces']};suffix=info['name'].replace('SM_OriginalPorchFrame_','');stem='HI_BrokenPorchFrame_'+suffix;ob,slots=s['submesh'](o,fs,stem)
 # Preserve complete original FBX material-slot order including unused slots.
 originalmats=list(o.data.materials);ob.data.materials.clear()
 for m in originalmats:ob.data.materials.append(m)
 for p in ob.data.polygons:p.material_index=slots[p.material_index]
 pathmap={v['material'].split('/')[-1].split('.')[0]:v['material'] for v in info['materials']};mats=[pathmap[re.sub(r'\.\d+$','',m.name)]for m in ob.data.materials]
 fbx=s['export'](ob,stem);row=dict(actor_label=a['label'],original_mesh=c['mesh'],asset_name=stem,fbx=fbx,material_slots=mats,original_faces=len(o.data.polygons),remaining_faces=len(fs),removed_components=len(remove),preserves_source_coordinates=True)
 out.append(row);d.setdefault('shared_meshes',[]).append({k:row[k]for k in ['asset_name','fbx','material_slots']})
d['porch_broken_frames']=out;json.dump(d,open(R+'/extraction-manifest.json','w'),indent=2);json.dump(out,open(R+'/inventory/shattered-porch-frame-audit.json','w'),indent=2);print('SHATTERED_PORCH_FRAMES_DONE',len(out))
