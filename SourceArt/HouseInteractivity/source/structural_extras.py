"""Extract furnishings embedded in structural tiles without touching walls."""
import bpy,json,os,re,ast,hashlib,math
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));import runpy
s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));d=json.load(open(R+'/extraction-manifest.json'));pathmap={}
for info in n['meshes'].values():
 for mat in info['materials']:
  p=mat['material']
  if p:pathmap[p.split('/')[-1].split('.')[0]]=p
# Reuse stage authoring function without executing its batch driver.
t=ast.parse(open(R+'/source/polish_manifest.py').read());exec(compile(ast.Module(body=[next(x for x in t.body if isinstance(x,ast.FunctionDef) and x.name=='stage')],type_ignores=[]),'stage','exec'))
rows={a['label']:a for a in n['actors']};audit=[]
for label,a in rows.items():
 if not(label.startswith('SM_') and '_Solid' in label and a['components']):continue
 c=a['components'][0];info=n['meshes'][c['mesh']];materialstring=' '.join(c['materials'])
 if not any(k in materialstring for k in ('Fabric','Print','BlueShower','S02Chest')):continue
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'));ps=s['components'](o)
 selected=[];items=[]
 # Build whole framed artwork around each authored art sheet.
 art=[p for p in ps if (any('Print' in m for m in p['materials']) or(any('FabricCream' in m for m in p['materials']) and p['size'].z>.4 and min(p['size'].x,p['size'].y)<.045)) and p['lo'].z>.8]
 for group in s['merge_near'](art,.055):
  lo,hi=s['bounds'](group);extra=[p for p in ps if p not in selected and p not in group and all(p['lo'][k]>=lo[k]-.08 and p['hi'][k]<=hi[k]+.08 for k in range(3))]
  full=[p for p in group+extra if p not in selected]
  if full:items.append(dict(parts=full,behavior='furniture',name='Framed wall picture',wall_mounted=True));selected.extend(full)
 cloth=[p for p in ps if p not in selected and any('BlueShower' in m for m in p['materials']) and p['size'].z>.8 and max(p['size'].x,p['size'].y)>.25 and min(p['size'].x,p['size'].y)<.2]
 for g in s['merge_near'](cloth,.055):items.append(dict(parts=g,behavior='curtain',name='Blue shower curtain'));selected.extend(g)
 storage=[p for p in ps if p not in selected and any('Fabric' in m for m in p['materials']) and label=='SM_Basement_p0_m1_Solid']
 for g in s['merge_near'](storage,.035):items.append(dict(parts=g,behavior='furniture',name='Stored fabric cushions'));selected.extend(g)
 cabinets=[p for p in ps if p not in selected and any('S02Chest' in m for m in p['materials'])]
 for g in s['merge_near'](cabinets,.07):
  lo,hi=s['bounds'](g);extra=[p for p in ps if p not in selected and p not in g and all(p['lo'][k]>=lo[k]-.05 and p['hi'][k]<=hi[k]+.05 for k in range(3))]
  full=[p for p in g+extra if p not in selected];items.append(dict(parts=full,behavior='furniture',name='Bathroom cabinet'));selected.extend(full)
 if not items:continue
 removed=set()
 for i,g in enumerate(items):
  lo,hi=s['bounds'](g['parts']);origin=(lo+hi)*.5;sz=hi-lo;fs={f for p in g['parts'] for f in p['faces']};assert not(removed&fs);removed|=fs
  stem='HI_Embedded_'+label+'_'+str(i+1).zfill(2);ob,slots=s['submesh'](o,fs,stem,origin)
  mats=[pathmap[re.sub(r'\.\d+$','',m.name)] for m in ob.data.materials]
  row=dict(id=label+'__embedded_'+str(i+1),asset_name=stem,source_actor_label=label,source_actor_labels=[label],source_mesh=c['mesh'],source_transform=c['transform'],intact_fbx=s['export'](ob,stem),material_slots=mats,local_origin_cm=s['cm'](origin),bounds_blender_metres=[s['raw'](lo),s['raw'](hi)],behavior=g['behavior'],name=g['name'],wall_mounted=g.get('wall_mounted',False),mass_kg=max(1,min(140,sz.x*sz.y*sz.z*110)),movable=True,face_count=len(fs))
  if g['behavior']=='furniture':
   br=s['wreck'](ob,stem+'_Broken','furniture');row['broken_fbx']=s['export'](br,stem+'_Broken');bpy.data.objects.remove(br,do_unlink=True)
   for lev,suf,key in[(1,'_Damaged','damaged_fbx'),(2,'_HeavilyDamaged','heavily_damaged_fbx')]:
    br=stage(ob,stem+suf,lev);row[key]=s['export'](br,stem+suf);bpy.data.objects.remove(br,do_unlink=True)
  else:
   axis=0 if sz.x>sz.y else 1;pivot=origin.copy();pivot[axis]=lo[axis];pivot.z=hi.z;row.update(pivot_cm=s['cm'](pivot-origin),curtain_axis=axis,curtain_gathered_scale=.14,initially_open=False)
  d['pieces'].append(row);bpy.data.objects.remove(ob,do_unlink=True)
 kept=set(range(len(o.data.polygons)))-removed;stem='HI_ArchitecturalRemainder_'+label;ob,slots=s['submesh'](o,kept,stem)
 d['remainders'].append(dict(source_actor_label=label,source_mesh=c['mesh'],fbx=s['export'](ob,stem),asset_name=stem,material_slots=[pathmap[re.sub(r'\.\d+$','',m.name)] for m in ob.data.materials],material_source_indices=slots))
 audit.append(dict(actor=label,original_faces=len(o.data.polygons),extracted_faces=len(removed),retained_faces=len(kept),pieces=len(items),conservation_pass=len(removed)+len(kept)==len(o.data.polygons)))
json.dump(d,open(R+'/extraction-manifest.json','w'),indent=2);json.dump(audit,open(R+'/inventory/structural-extraction-audit.json','w'),indent=2);print('STRUCTURAL_EXTRAS_DONE',sum(a['pieces'] for a in audit),len(audit))
