import unreal,json,re,traceback,sys
from pathlib import Path
B=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HallFloorsPorch');D='/Game/SeniorSendOff/House/HallFloorsPorch'
sys.path.insert(0,str(B));import materials
ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
try:
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
 before=json.load(open(B/'context/actors.json'))
 floor_protected={r['label']:r for r in before if r['class_name']=='StaticMeshActor' and any(s in r['label'].lower()for s in ['floor','rug']) and not any(s in r['label']for s in ['UpBath_SM_UpBath_GrimyFloor'])}
 mats=materials.build();cache={}
 def imp(row,shell=False):
  name=row['name'];t=unreal.AssetImportTask();t.filename=row['file'];t.destination_name=name;t.destination_path=D;t.automated=True;t.replace_existing=True;t.save=False;t.factory=unreal.FbxFactory()
  opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_as_skeletal=False;opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False;opt.static_mesh_import_data.one_convex_hull_per_ucx=True;t.options=opt
  at.import_asset_tasks([t]);m=unreal.load_asset(D+'/'+name);assert m,name
  byname={p.split('.')[-1]:unreal.load_asset(p)for p in row['materials']}if shell else mats.copy();byname.update(mats)
  slots=list(m.static_materials)
  for s in slots:
   key=re.sub(r'[._]\d+$','',str(s.get_editor_property('imported_material_slot_name')));assert key in byname,(name,key);s.set_editor_property('material_interface',byname[key])
  m.static_materials=slots;m.set_editor_property('support_ray_tracing',False)
  bs=m.get_editor_property('body_setup');bs.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if row.get('collision')else unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  if row.get('collision'):
   
   if not row.get('convex_collision'):me.remove_collisions(m);me.add_simple_collisions(m,unreal.ScriptCollisionShapeType.BOX)
   assert me.get_simple_collision_count(m)+me.get_convex_collision_count(m)>0
  if shell:m.set_editor_property('nanite_settings',unreal.load_asset(row['old_mesh']).get_editor_property('nanite_settings'))
  for attempt in range(3):
   if ed.save_loaded_asset(m,False):break
  else:raise RuntimeError('Cannot save '+name)
  cache[name]=m;return m
 for r in json.load(open(B/'shell_plan.json')):
  a=actors[r['label']];c=a.static_mesh_component;old=c.static_mesh.get_path_name();assert old==r['old_mesh'] or old.startswith(D+'/'),(a.get_actor_label(),old)
  c.set_static_mesh(imp(r,True));c.set_editor_property('override_materials',[])
 for r in json.load(open(B/'detail_plan.json')):imp(r)
 hall_end=actors['HallEnd_SM_HallEnd_ThreeRoomsFloors'];hall_end.static_mesh_component.set_material(1,mats['HFP_HallCoating'])
 bath=actors['UpBath_SM_UpBath_GrimyFloor'];bath.static_mesh_component.set_material(0,mats['HFP_BathCoating'])
 # Preserve furniture actor identities, loose cushion and native damage/handling.
 for label,stem,p,mass,hp,title in [('Interactive_SMF_Upper_ALL__015','Bench',(-375,-985,308.5),52,68,'weathered porch bench'),('Interactive_SMF_Upper_ALL__016','Chair',(340,-1008,308.5),64,82,'brown leather porch armchair'),('Interactive_SMF_Upper_ALL__007','SeatCushion',(340,-1031,373.5),4,18,'loose leather seat cushion')]:
  a=actors[label]
  for k,v in [('intact_mesh',cache['SM_HFP_'+stem]),('damaged_mesh',cache['SM_HFP_'+stem+'_Damaged']),('heavily_damaged_mesh',cache['SM_HFP_'+stem+'_Heavy']),('broken_mesh',cache['SM_HFP_'+stem+'_Broken']),('material_overrides',[]),('prop_name',title),('mass_kg',mass),('break_health',hp),('movable',True),('breakable',True)]:a.set_editor_property(k,v)
  a.set_actor_location(unreal.Vector(*p),False,True);a.set_actor_rotation(unreal.Rotator(),True);a.set_actor_scale3d(unreal.Vector(1,1,1));a.refresh_prop()
 def static(label,mesh,p,coll=False,parent=None):
  a=actors.get(label)or sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p));a.set_actor_label(label);a.set_folder_path('House details/Hall floors and porch');a.tags=[unreal.Name('HallFloorsPorch')];a.set_actor_location(unreal.Vector(*p),False,True);c=a.static_mesh_component;c.set_static_mesh(cache[mesh]);c.set_collision_profile_name('BlockAll'if coll else'NoCollision');c.set_mobility(unreal.ComponentMobility.MOVABLE if parent else unreal.ComponentMobility.STATIC)
  if parent:a.attach_to_component(parent,'',unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,False);a.set_actor_relative_location(unreal.Vector(*p),False,True)
  return a
 # Remove only the old door pane, whose fixed source pose no longer belongs to the moving leaf.
 oldpane=actors.get('Interactive_SM_Upper_m1_p0_Glass__001')
 if oldpane:sub.destroy_actor(oldpane)
 door=actors['OriginalDoor_Upper_m1_p0_Solid_01']
 for k,v in [('original_leaf',cache['SM_HFP_PorchDoor']),('original_glass',cache['SM_HFP_PorchDoorGlass']),('broken_frame_mesh',cache['SM_HFP_PorchDoor']),('added_handle_mesh',None),('local_leaf_space',True),('closed_yaw',0.0),('open_yaw',90.0),('panel_center_world',unreal.Vector(-79.2,0,110)),('panel_half_extent',unreal.Vector(79.2,3,110)),('locked',False)]:door.set_editor_property(k,v)
 door.set_actor_rotation(unreal.Rotator(),True)
 pivot=next(c for c in door.get_components_by_class(unreal.SceneComponent)if c.get_name()=='LeafPivot')
 leaf=next(c for c in door.get_components_by_class(unreal.StaticMeshComponent)if c.get_name()=='OriginalDoorLeaf');leaf.set_static_mesh(cache['SM_HFP_PorchDoor']);leaf.set_relative_transform(unreal.Transform(),False,True)
 static('Porch door curtain rod','SM_HFP_CurtainRod',(0,0,0),False,pivot)
 curtain=actors.get('Porch navy door curtain')or sub.spawn_actor_from_class(unreal.HouseReactiveFixture,door.get_actor_location());curtain.set_actor_label('Porch navy door curtain');curtain.set_folder_path('House details/Hall floors and porch');curtain.tags=[unreal.Name('HallFloorsPorch')]
 for k,v in [('fixture_kind',unreal.HouseReactiveFixtureKind.CURTAIN),('fixture_id',unreal.Name('FrontPorchNavyDoorCurtain')),('original_mesh',cache['SM_HFP_DoorCurtain']),('mesh_transform',unreal.Transform()),('motion_pivot_local',unreal.Vector(-151,7.5,207)),('curtain_axis',0),('curtain_gathered_scale',.14)]:curtain.set_editor_property(k,v)
 curtain.attach_to_component(pivot,'',unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,False);curtain.set_actor_relative_transform(unreal.Transform(),False,True);curtain.refresh_fixture()
 static('Landing white sash window','SM_HFP_LandingWindowFrame',(-2210,1233.9,340),False)
 static('Landing black window guard','SM_HFP_LandingGuard',(-2210,1233.9,340),True)
 pane=actors.get('Landing sash glass')or sub.spawn_actor_from_class(unreal.HouseReactiveFixture,unreal.Vector(-2210,1233.9,340));pane.set_actor_label('Landing sash glass');pane.set_folder_path('House details/Hall floors and porch');pane.tags=[unreal.Name('HallFloorsPorch')]
 for k,v in [('fixture_kind',unreal.HouseReactiveFixtureKind.WINDOW),('fixture_id',unreal.Name('UpperLandingSashWindow')),('original_mesh',cache['SM_HFP_LandingWindowGlass']),('mesh_transform',unreal.Transform(location=unreal.Vector(0,-8,0))),('glass_breaking_damage',24.0)]:pane.set_editor_property(k,v)
 pane.refresh_fixture()
 for label,mesh,p in [('Ground hallway floor drain','SM_HFP_FloorGrate',(-2188,-280,.35)),('Upper hallway floor drain','SM_HFP_FloorGrate',(-2190,165,302.6)),('West hall wood transition','SM_HFP_WoodThreshold',(-1294.2,-691.5,307)),('Upper hall rubber doormat','SM_HFP_RubberMat',(-2190,-1020,302.6))]:static(label,mesh,p)
 # Protected room/rug actors retain their exact original floor assets and transforms.
 preserved=[]
 for label,row in floor_protected.items():
  a=actors[label];assert sum((v-row['location'][i])**2 for i,v in enumerate([a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z]))<.0001,label
  c=a.get_components_by_class(unreal.StaticMeshComponent)
  assert [x.static_mesh.get_path_name()for x in c if x.static_mesh]==[x['mesh']for x in row['components']],label
  preserved.append(label)
 assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 json.dump(dict(status='PASS',imported_meshes=len(cache),protected_room_floors=preserved,porch_door=door.get_actor_label(),new_window_center=[-2210,1233.9,447.5]),open(B/'evidence/install.json','w'),indent=2)
 (B/'evidence/install-error.txt').unlink(missing_ok=True)
 unreal.log('HFP_INSTALL_PASS meshes='+str(len(cache)))
except:
 error=traceback.format_exc();(B/'evidence/install-error.txt').write_text(error);unreal.log_error(error)
finally:unreal.SystemLibrary.quit_editor()
