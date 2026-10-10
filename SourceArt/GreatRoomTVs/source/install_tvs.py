import unreal,json,re,traceback,shutil,datetime
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs');DEST='/Game/SeniorSendOff/House/GreatRoomTVs';doc=json.loads((R/'manifest.json').read_text());shells=json.loads((R/'shell-manifest.json').read_text());ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary;me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
try:
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 if not world or world.get_name()!='Chapter01_House':world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 assert world.get_name()=='Chapter01_House'
 prop_before=sum(isinstance(a,unreal.HouseInteractiveProp)and 'SSOKitchenRefit'in[str(t)for t in a.tags]for a in actors.get_all_level_actors());assert prop_before==137
 backup=R/'before-map.umap'
 if not backup.exists():shutil.copy2(Path(unreal.Paths.project_dir())/'Content/Story/Maps/Chapter01_House.umap',backup)
 def connect_const(m,value,prop):
  n=ml.create_material_expression(m,unreal.MaterialExpressionConstant);n.r=value;ml.connect_material_property(n,'',prop)
 for name,d in doc['materials'].items():
  key=DEST+'/'+name;m=unreal.load_asset(key)if ed.does_asset_exist(key)else at.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew());ml.delete_all_material_expressions(m)
  if d.get('texture'):
   t=unreal.AssetImportTask();t.filename=d['texture'];t.destination_path=DEST+'/Textures';t.destination_name='T_GRTV_'+Path(t.filename).stem;t.automated=True;t.replace_existing=True;t.save=True;at.import_asset_tasks([t]);tex=unreal.load_asset(t.destination_path+'/'+t.destination_name);tex.max_texture_size=1024;ed.save_loaded_asset(tex);n=ml.create_material_expression(m,unreal.MaterialExpressionTextureSample);n.texture=tex
  else:
   n=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*d['base_color'],1)
  ml.connect_material_property(n,'RGB'if d.get('texture')else '',unreal.MaterialProperty.MP_BASE_COLOR)
  connect_const(m,d['roughness'],unreal.MaterialProperty.MP_ROUGHNESS);connect_const(m,d['metallic'],unreal.MaterialProperty.MP_METALLIC)
  if d.get('emission'):
   mul=ml.create_material_expression(m,unreal.MaterialExpressionMultiply);factor=ml.create_material_expression(m,unreal.MaterialExpressionConstant);factor.r=d['emission'];ml.connect_material_expressions(n,'RGB'if d.get('texture')else '',mul,'A');ml.connect_material_expressions(factor,'',mul,'B');ml.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  ml.recompile_material(m);assert ed.save_loaded_asset(m,False)
 def import_mesh(a):
  name=a.get('name',a.get('asset_name'));ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.one_convex_hull_per_ucx=True;ui.static_mesh_import_data.reorder_material_to_fbx_order=True
  t=unreal.AssetImportTask();t.factory=unreal.FbxFactory();t.filename=a['fbx'];t.destination_path=DEST;t.destination_name=name;t.options=ui;t.automated=True;t.replace_existing=True;t.save=False;at.import_asset_tasks([t]);mesh=unreal.load_asset(DEST+'/'+name);assert isinstance(mesh,unreal.StaticMesh),name
  byname={p.rsplit('/',1)[-1].split('.')[0]:p for p in a['materials']};slots=list(mesh.static_materials)
  for slot in slots:
   n=str(slot.get_editor_property('imported_material_slot_name'));n=re.sub(r'[._][0-9]{3}$','',n);path=byname.get(n);assert path,(name,n,byname);slot.set_editor_property('material_interface',unreal.load_asset(path));assert slot.get_editor_property('material_interface'),path
  mesh.set_editor_property('static_materials',slots);mesh.set_editor_property('support_ray_tracing',False);bs=mesh.get_editor_property('body_setup');bs.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if a.get('collision')else unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  if a.get('collision') and me:assert me.get_convex_collision_count(mesh)>0
  saved=ed.save_loaded_asset(mesh,False)
  if not saved:saved=ed.save_loaded_asset(mesh,False)
  assert saved,name;return mesh
 cache={a['name']:import_mesh(a)for a in doc['assets']}
 # Only the two old LCDs and their rear casings have been removed offline.
 existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
 for a in shells:
  target=existing[a['label']];expected=a['original_mesh'];current=target.static_mesh_component.static_mesh.get_path_name();assert current==expected or current.startswith(DEST+'/'),(a['label'],current)
  mesh=import_mesh(a);target.static_mesh_component.set_static_mesh(mesh);target.static_mesh_component.set_collision_profile_name('NoCollision');target.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);target.static_mesh_component.set_editor_property('override_materials',[])
 for a in list(actors.get_all_level_actors()):
  if 'GreatRoomTVUpgrade'in[str(t)for t in a.tags]:actors.destroy_actor(a)
 installed=[]
 for p in doc['placements']:
  loc=unreal.Vector(*p['location']);rot=unreal.Rotator(pitch=0,yaw=p['yaw'],roll=0);a=actors.spawn_actor_from_class(unreal.HouseInteractiveProp,loc,rot);a.set_actor_label(p['id']);a.set_folder_path('GreatRoom/Televisions');a.tags=[unreal.Name('GreatRoomTVUpgrade'),unreal.Name('HouseWallTelevision')]
  for k,v in [('prop_id',unreal.Name(p['id'])),('prop_name','great room television'),('intact_mesh',cache['SM_GRTV_Television_0']),('damaged_mesh',cache['SM_GRTV_Television_1']),('heavily_damaged_mesh',cache['SM_GRTV_Television_2']),('broken_mesh',cache['SM_GRTV_Television_3']),('wall_mounted',True),('movable',True),('breakable',True),('broken_version_movable',True),('mass_kg',23.5),('break_health',60.0),('detach_health',36.0),('impact_break_speed',520.0),('damage_category',unreal.Name('Televisions'))]:a.set_editor_property(k,v)
  a.set_actor_scale3d(unreal.Vector(*p.get('scale',[1,1,1])));a.refresh_prop()
  away=unreal.Vector(1 if p['yaw']<0 else -1,0,0);mount=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p['mount_location']) if 'mount_location' in p else loc-away*7,rot);mount.set_actor_scale3d(unreal.Vector(*p.get('mount_scale',[1,1,1])));mount.set_actor_label(p['id']+'_Bracket');mount.set_folder_path('GreatRoom/Televisions');mount.tags=[unreal.Name('GreatRoomTVUpgrade')];c=mount.static_mesh_component;c.set_static_mesh(cache['SM_GRTV_WallBracket']);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
  installed.append(dict(id=p['id'],location=p['location'],mass=23.5,health=60,attachment_health=36))
 prop_after=sum(isinstance(a,unreal.HouseInteractiveProp)and 'SSOKitchenRefit'in[str(t)for t in a.tags]for a in actors.get_all_level_actors());assert prop_after==prop_before
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House')
 if (R/'install-error.txt').exists():(R/'install-error.txt').unlink()
 (R/'installed.json').write_text(json.dumps(dict(passed=True,tvs=installed,kitchen_props_preserved=prop_after,shell_face_conservation=shells),indent=2));unreal.log('GREAT_ROOM_TVS_INSTALLED')
except Exception:
 error=traceback.format_exc();(R/'install-error.txt').write_text(error);unreal.log_error(error)
