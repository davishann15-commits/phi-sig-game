"""Import the terminal three-room hall; audit protected actors before saving."""
import os,json,re,unreal
HELPER='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Content/Python/apply_bedroom.py'
exec(open(HELPER).read().split('textures={}')[0])
BASE='/Users/Stewart/Projects/Senior sendoff/outputs/hallway-end-three-rooms'
SOURCE=os.path.join(unreal.Paths.project_dir(),'SourceArt/HallwayEnd')
DEST='/Game/SeniorSendOff/House/HallwayEnd'
PLAN=json.load(open(SOURCE+'/import_manifest.json'))
textures={}
for filename in PLAN['textures']:
 tex=imp(SOURCE+'/'+filename,os.path.splitext(filename)[0]);lower=filename.lower()
 if 'normal' in lower:
  tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('srgb',False)
 elif any(s in lower for s in ('roughness','mask')):tex.set_editor_property('srgb',False)
 A.save_loaded_asset(tex);textures[os.path.basename(filename)]=tex
materials={}
for name,info in PLAN['materials'].items():
 if info.get('existing_asset'):
  m=A.load_asset(info['existing_asset'])
  if not m:raise RuntimeError('Missing retained material '+info['existing_asset'])
  materials[name]=m
 else:materials[name]=make_material(name,info,textures)
meshes={}
for row in PLAN['meshes']:
 m=imp(SOURCE+'/'+row['file'],row['name'])
 if row.get('complex_collision'):
  m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 else:
  for i,s in enumerate(m.static_materials):
   key=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0])
   mat=materials.get(key)
   if not mat:
    old=key if key.startswith('MI_SSO_') else 'MI_SSO_'+key
    mat=A.load_asset('/Game/SeniorSendOff/House/Materials/'+old)
   if not mat:raise RuntimeError('Missing material '+key+' on '+row['name'])
   m.set_material(i,mat)
 A.save_loaded_asset(m);meshes[row['name']]=m
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
before={k:state(a) for k,a in actors.items()}
removed=[];changed=[];new=[]
for label,a in actors.items():
 if label.startswith('HallEnd_'):unreal.EditorLevelLibrary.destroy_actor(a)
for label in PLAN['remove_actors']:
 if label in actors:unreal.EditorLevelLibrary.destroy_actor(actors[label]);removed.append(label)
for row in PLAN['replacements']:
 a=actors[row['actor']];c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_editor_property('override_materials',[])
 collision=row.get('collision') in (True,'BlockAll')
 c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
 if collision:c.set_visibility(False)
 changed.append(row['actor'])
for row in PLAN['actors']:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*row.get('location',[0,0,0])),unreal.Rotator(pitch=0,yaw=row.get('yaw',0),roll=0))
 a.set_actor_label(row['label']);a.set_folder_path('Second floor/Hallway end')
 a.set_actor_scale3d(unreal.Vector(*row.get('scale',[1,1,1])))
 c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_collision_profile_name(row.get('collision','NoCollision'));c.set_editor_property('generate_overlap_events',False)
 if row.get('hidden'):c.set_visibility(False)
 new.append(row['label'])
for row in PLAN['doors']:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.ExistingHouseDoor,unreal.Vector(*row['hinge']))
 a.set_actor_label(row['label']);a.set_folder_path('Second floor/Hallway end')
 props={'door_id':row['id'],'door_group':'None','locked':False,'closed_yaw':0.0,'open_yaw':row['open_yaw'],
  'doorway_center':unreal.Vector(*row['center']),'panel_center_world':unreal.Vector(*row['panel_center']),
  'panel_half_extent':unreal.Vector(*row['panel_half_extent']),'original_leaf':meshes[row['leaf']],
  'original_glass':None,'added_handle_mesh':None}
 for key,v in props.items():a.set_editor_property(key,v)
 p=a.get_actor_location();a.set_actor_location(p+unreal.Vector(.01,0,0),False,False);a.set_actor_location(p,False,False)
 new.append(row['label'])
for row in PLAN['lights']:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*row['location']))
 a.set_actor_label(row['label']);a.set_folder_path('Second floor/Hallway end')
 c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE)
 c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(row.get('intensity',900));c.set_attenuation_radius(row.get('radius',row.get('attenuation_radius',450)))
 c.set_light_color(unreal.LinearColor(*row.get('color',[1,.92,.79]),1));c.set_editor_property('cast_shadows',row.get('cast_shadows',False));c.set_editor_property('source_radius',row.get('source_radius',12));new.append(row['label'])
registry_path=os.path.join(unreal.Paths.project_content_dir(),'HouseReview/original_house_doors.json')
registry=json.load(open(registry_path));retire=set(PLAN['remove_door_ids'])|{r['id'] for r in PLAN['doors']}
registry=[r for r in registry if r['id'] not in retire]
for row in PLAN['doors']:
 registry.append({'id':row['id'],'kind':'Solid','doorwayCenter':row['center'],'hinge':row['hinge'],'closedYaw':0,'openYaw':row['open_yaw'],'locked':False,'leafAsset':DEST+'/'+row['leaf'],'glassAsset':None,'group':None,'panelCenterWorld':row['panel_center'],'panelHalfExtent':row['panel_half_extent']})
after={a.get_actor_label():state(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected=[n for n in before if n not in changed and n not in removed and not n.startswith('HallEnd_')]
errors=[n for n in protected if before[n]!=after.get(n)]
if errors:raise RuntimeError('Protected actors changed '+str(errors))
for label in PLAN['remove_actors']:
 if label in after:raise RuntimeError('Retired actor remains '+label)
if len(PLAN['doors'])!=3 or len({r['id'] for r in PLAN['doors']})!=3:raise RuntimeError('Expected three independent doors')
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Level save failed')
json.dump(registry,open(registry_path,'w'),indent=2)
json.dump({'removed':removed,'changed':changed,'new':new,'protected_actors':len(protected),'all_protected_unchanged':True,'actor_count':len(after),'doors':PLAN['doors']},open(BASE+'/applied.json','w'),indent=2)
unreal.log('HALLWAY_END_APPLY_PASSED protected=%d removed=%d new=%d'%(len(protected),len(removed),len(new)))
exec(open(BASE+'/task/source/inspect_hallway_import.py').read())
