"""Join native furniture tiles before splitting items at tile boundaries."""
import bpy,json,os,sys,hashlib
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
source=sys.argv[sys.argv.index('--')+1];native=json.load(open(source));rows=native['actors'];meshmap=native['meshes'];remove=set();new=[];audit=[]
def merge(label,source_rows):
 bpy.ops.wm.read_factory_settings(use_empty=True);objs=[];mat_paths={};source_names=[];faces=0
 for a in source_rows:
  c=a['components'][0];t=c['transform'];assert all(abs(x)<.0001 for x in t['location']+t['rotation']), (a['label'],t);assert all(abs(x-1)<.0001 for x in t['scale']),t
  info=meshmap[c['mesh']];before=set(bpy.context.scene.objects);bpy.ops.import_scene.fbx(filepath=info['fbx'])
  obs=[o for o in bpy.context.scene.objects if o not in before and o.type=='MESH'];assert len(obs)==1
  o=obs[0];faces+=len(o.data.polygons)
  for i,m in enumerate(o.data.materials):
   if m:mat_paths[m.name]=c['materials'][i] if i<len(c['materials']) else None
  objs.append(o);source_names.append(a['label'])
 bpy.ops.object.select_all(action='DESELECT')
 for o in objs:o.select_set(True)
 bpy.context.view_layer.objects.active=objs[0]
 if len(objs)>1:bpy.ops.object.join()
 o=bpy.context.object;o.name=label
 mats=[mat_paths.get(m.name) if m else None for m in o.data.materials]
 assert len(o.data.polygons)==faces
 out=os.path.join(ROOT,'context','fbx',label+'.fbx');bpy.ops.export_scene.fbx(filepath=out,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y')
 meshpath='/HouseInteractivityMerged/'+label;transform={'location':[0,0,0],'rotation':[0,0,0],'scale':[1,1,1]}
 meshmap[meshpath]={'fbx':out,'materials':mats}
 new.append({'label':label,'class':'StaticMeshActor','source_actor_labels':source_names,'components':[{'mesh':meshpath,'materials':mats,'transform':transform}]})
 remove.update(source_names);audit.append({'merged':label,'source_labels':source_names,'source_faces':faces,'exported_faces':len(o.data.polygons),'materials':len(mats)})
for floor in ('Basement','Entrance','Upper'):
 for prefix in ('SMF','COLF'):
  group=[a for a in rows if a['label'].startswith(prefix+'_'+floor+'_') and a.get('class')=='StaticMeshActor' and len(a.get('components',[]))==1]
  if group:merge(prefix+'_'+floor+'_ALL',group)
native['actors']=[a for a in rows if a['label'] not in remove]+new
json.dump(native,open(os.path.join(ROOT,'context','prepared-actors.json'),'w'),indent=2)
json.dump(audit,open(os.path.join(ROOT,'inventory','floor-merge-audit.json'),'w'),indent=2)
print('FLOOR_MERGE_COMPLETE',len(remove),len(new))
