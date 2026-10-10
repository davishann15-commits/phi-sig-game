import bpy,json,os,re,runpy,ast,math,hashlib
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));d=json.load(open(R+'/extraction-manifest.json'));rows={a['label']:a for a in n['actors']};pathmap={}
for info in n['meshes'].values():
 for mat in info['materials']:
  p=mat['material']
  if p:pathmap[p.split('/')[-1].split('.')[0]]=p
t=ast.parse(open(R+'/source/polish_manifest.py').read());exec(compile(ast.Module(body=[next(x for x in t.body if isinstance(x,ast.FunctionDef) and x.name=='stage')],type_ignores=[]),'stage','exec'))
audit=[]
for label in ['SM_Room102_Sign','BR_BedDetail_TennesseeDoorPlaque_14','PIG_OutsideDoorPlaque','SM_SpaRoom_ArchitecturalDetails']:
 a=rows[label];c=a['components'][0];info=n['meshes'][c['mesh']];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'));ps=s['components'](o)
 groups=[ps] if label!='SM_SpaRoom_ArchitecturalDetails' else [[p for p in ps if -28.8<p['center'].x<-26.4 and p['center'].y>3.1],[p for p in ps if p['center'].x<-29.8 and -5.5<p['center'].y<-3.8]]
 used=set()
 for i,g in enumerate(groups):
  lo,hi=s['bounds'](g);origin=(lo+hi)*.5;sz=hi-lo;fs={f for p in g for f in p['faces']};assert not used&fs;used|=fs;stem='HI_WallDecor_'+label+'_'+str(i+1);ob,slots=s['submesh'](o,fs,stem,origin)
  row=dict(id=label+'__walldecor_'+str(i+1),asset_name=stem,source_actor_label=label,source_actor_labels=[label],source_mesh=c['mesh'],source_transform=c['transform'],intact_fbx=s['export'](ob,stem),material_slots=[pathmap[re.sub(r'\.\d+$','',m.name)] for m in ob.data.materials],local_origin_cm=s['cm'](origin),bounds_blender_metres=[s['raw'](lo),s['raw'](hi)],behavior='furniture',name=('Spa framed artwork' if i==0 else 'Spa wall plaque') if label=='SM_SpaRoom_ArchitecturalDetails' else 'Wall plaque '+label,wall_mounted=True,mass_kg=max(1,min(25,sz.x*sz.y*sz.z*110)),movable=True,face_count=len(fs))
  br=s['wreck'](ob,stem+'_Broken','furniture');row['broken_fbx']=s['export'](br,stem+'_Broken');bpy.data.objects.remove(br,do_unlink=True)
  for lev,suf,key in[(1,'_Damaged','damaged_fbx'),(2,'_HeavilyDamaged','heavily_damaged_fbx')]:
   br=stage(ob,stem+suf,lev);row[key]=s['export'](br,stem+suf);bpy.data.objects.remove(br,do_unlink=True)
  d['pieces'].append(row);bpy.data.objects.remove(ob,do_unlink=True)
 kept=set(range(len(o.data.polygons)))-used
 if kept:
  stem='HI_Retained_'+label;ob,slots=s['submesh'](o,kept,stem);d['remainders'].append(dict(source_actor_label=label,source_mesh=c['mesh'],fbx=s['export'](ob,stem),asset_name=stem,material_slots=[pathmap[re.sub(r'\.\d+$','',m.name)] for m in ob.data.materials]))
 audit.append(dict(actor=label,original_faces=len(o.data.polygons),interactive_faces=len(used),retained_faces=len(kept),conservation_pass=len(used)+len(kept)==len(o.data.polygons)))
json.dump(d,open(R+'/extraction-manifest.json','w'),indent=2);json.dump(audit,open(R+'/inventory/wall-decor-audit.json','w'),indent=2);print('FINAL_WALL_DECOR_DONE',len(d['pieces']))
