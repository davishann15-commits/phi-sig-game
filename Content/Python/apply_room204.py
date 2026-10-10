"""Save the photo matched room 204 and preserve all unrelated map actors."""
import os,json,re,unreal
HELPER='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Content/Python/apply_bedroom.py'
exec(open(HELPER).read().split('textures={}')[0].replace("if name.startswith('SM'):","if True:"))
BASE='/Users/Stewart/Projects/Senior sendoff/outputs/bathroom-opposite-fridge-room'
SOURCE=os.path.join(unreal.Paths.project_dir(),'SourceArt/Room204')
DEST='/Game/SeniorSendOff/House/Room204'
PLAN=json.load(open(SOURCE+'/import_manifest.json'))
textures={}
for filename in PLAN.get('textures',[]):
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
   key=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0]);mat=materials.get(key)
   if not mat:
    old=key if key.startswith('MI_SSO_') else 'MI_SSO_'+key
    mat=A.load_asset('/Game/SeniorSendOff/House/Materials/'+old)
   if not mat:raise RuntimeError('Missing material '+key+' on '+row['name'])
   m.set_material(i,mat)
 A.save_loaded_asset(m);meshes[row['name']]=m
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()};before={k:state(a) for k,a in actors.items()}
removed=[];changed=[];new=[]
for label,a in actors.items():
 if label.startswith('R204_'):unreal.EditorLevelLibrary.destroy_actor(a)
for label in PLAN.get('remove_actors',[]):
 if label in actors:unreal.EditorLevelLibrary.destroy_actor(actors[label]);removed.append(label)
for row in PLAN.get('replacements',[]):
 a=actors[row['actor']];c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_editor_property('override_materials',[])
 collision=row.get('collision') in (True,'BlockAll');c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
 c.set_visibility(not collision);changed.append(row['actor'])
for row in PLAN.get('door_updates',[]):
 a=actors[row['actor']]
 for key,val in row['properties'].items():
  if key in ('doorway_center','panel_center_world','panel_half_extent','added_handle_world_position'):val=unreal.Vector(*val)
  elif key in ('original_leaf','original_glass','added_handle_mesh'):val=meshes[val] if val else None
  a.set_editor_property(key,val)
 if row.get('location'):a.set_actor_location(unreal.Vector(*row['location']),False,False)
 p=a.get_actor_location();a.set_actor_location(p+unreal.Vector(.01,0,0),False,False);a.set_actor_location(p,False,False)
 changed.append(row['actor'])
for row in PLAN.get('actors',[]):
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*row.get('location',[0,0,0])),unreal.Rotator(pitch=0,yaw=row.get('yaw',0),roll=0))
 a.set_actor_label(row['label']);a.set_folder_path('Second floor/Photo matched Room 204');a.set_actor_scale3d(unreal.Vector(*row.get('scale',[1,1,1])))
 c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_collision_profile_name(row.get('collision','NoCollision'));c.set_editor_property('generate_overlap_events',False)
 if row.get('hidden'):c.set_visibility(False)
 new.append(row['label'])
for row in PLAN.get('lights',[]):
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*row['location']));a.set_actor_label(row['label']);a.set_folder_path('Second floor/Photo matched Room 204')
 c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
 c.set_intensity(row.get('intensity',950));c.set_attenuation_radius(row.get('radius',row.get('attenuation_radius',500)));c.set_light_color(unreal.LinearColor(*row.get('color',[1,.93,.8]),1))
 c.set_editor_property('cast_shadows',row.get('cast_shadows',False));c.set_editor_property('source_radius',row.get('source_radius',15));c.set_editor_property('soft_source_radius',row.get('soft_source_radius',20));new.append(row['label'])
registry_path=os.path.join(unreal.Paths.project_content_dir(),'HouseReview/original_house_doors.json');registry=json.load(open(registry_path))
for update in PLAN.get('door_updates',[]):
 matches=[r for r in registry if r['id']==update['id']]
 if len(matches)!=1:raise RuntimeError('Door registry mismatch '+update['id'])
 row=matches[0];props=update['properties']
 for key,newkey in [('closed_yaw','closedYaw'),('open_yaw','openYaw'),('doorway_center','doorwayCenter'),('locked','locked'),('panel_center_world','panelCenterWorld'),('panel_half_extent','panelHalfExtent')]:
  if key in props:row[newkey]=props[key]
 if update.get('location'):row['hinge']=update['location']
 if props.get('original_leaf'):row['leafAsset']=DEST+'/'+props['original_leaf']
after={a.get_actor_label():state(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected=[n for n in before if n not in changed and n not in removed and not n.startswith('R204_')];errors=[n for n in protected if before[n]!=after.get(n)]
if errors:raise RuntimeError('Protected actors changed '+str(errors))
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Level save failed')
json.dump(registry,open(registry_path,'w'),indent=2)
json.dump({'removed':removed,'changed':changed,'new':new,'protected_actors':len(protected),'all_protected_unchanged':True,'actor_count':len(after),'door_updates':PLAN.get('door_updates',[])},open(BASE+'/applied.json','w'),indent=2)
unreal.log('ROOM204_APPLY_PASSED protected=%d changed=%d new=%d'%(len(protected),len(changed),len(new)))
