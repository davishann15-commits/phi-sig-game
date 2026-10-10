"""Install only the two photo-matched door leaves at the verified 101/102 corner."""
import unreal,json,pathlib,re,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/Room101Entry'
D='/Game/SeniorSendOff/House/Room101Entry'
E=unreal.MaterialEditingLibrary;A=unreal.EditorAssetLibrary
T=unreal.AssetToolsHelpers.get_asset_tools();S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def xyz(v):return [v.x,v.y,v.z]
def state(a):
 r=a.get_actor_rotation()
 return dict(label=a.get_actor_label(),location=xyz(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=xyz(a.get_actor_scale3d()),meshes=[c.static_mesh.get_path_name() for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh])
def imp(path,name,fbx=False):
 t=unreal.AssetImportTask();t.filename=str(path);t.destination_path=D;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=False
 if fbx:
  t.factory=unreal.FbxFactory();o=unreal.FbxImportUI();o.import_mesh=True;o.import_as_skeletal=False;o.import_materials=False;o.import_textures=False
  o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False
  o.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
  t.options=o
 T.import_asset_tasks([t]);a=A.load_asset(D+'/'+name);assert a,'Import failed '+name
 return a
def constant(m,v):
 if isinstance(v,list):
  n=E.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*v,1)
 else:n=E.create_material_expression(m,unreal.MaterialExpressionConstant);n.r=v
 return n
def material(name,row,textures):
 m=A.load_asset(D+'/'+name) or T.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew())
 E.delete_all_material_expressions(m)
 for prop,key,default in [(unreal.MaterialProperty.MP_BASE_COLOR,'color',[.7,.7,.7]),(unreal.MaterialProperty.MP_ROUGHNESS,'roughness',.6),(unreal.MaterialProperty.MP_METALLIC,'metallic',0)]:
  texkey='base_texture' if key=='color' else 'roughness_texture' if key=='roughness' else None
  if texkey and row.get(texkey):
   n=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);n.texture=textures[pathlib.Path(row[texkey]).stem]
   if key=='roughness':n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
   output='RGB' if key=='color' else 'R'
  else:n=constant(m,row.get(key,default));output=''
  E.connect_material_property(n,output,prop)
 E.recompile_material(m);assert A.save_loaded_asset(m,False),name
 return m
def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 actors={a.get_actor_label():a for a in S.get_all_level_actors()};before={a.get_name():state(a) for a in actors.values()}
 manifest=json.load(open(B/'door_manifest.json'));textures={}
 for p in manifest['textures']:
  path=B/'textures'/p;tex=imp(path,path.stem)
  if 'Roughness' in path.stem:tex.srgb=False
  A.save_loaded_asset(tex,False);textures[path.stem]=tex
 mats={n:material(n,r,textures) for n,r in manifest['materials'].items()};meshes={}
 for row in manifest['assets']:
  if 'Door101Leaf' not in row['name'] and 'Door102Leaf' not in row['name'] and 'NamePlaque' not in row['name']:continue
  mesh=imp(row['fbx'],row['name'],True);slots=list(mesh.static_materials)
  for slot in slots:
   key=re.sub(r'[._]\d+$','',str(slot.get_editor_property('imported_material_slot_name')))
   assert key in mats,(row['name'],key)
   slot.material_interface=mats[key]
  mesh.static_materials=slots;mesh.set_editor_property('support_ray_tracing',False)
  unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
  assert A.save_loaded_asset(mesh,False),row['name'];meshes[row['name']]=mesh
 changed=set();details=[]
 for number,label in [('101','OriginalDoor_Entrance_m1_m2_Solid_01'),('102','OriginalDoor_Entrance_m1_m2_Solid_03')]:
  a=actors[label];assert isinstance(a,unreal.ExistingHouseDoor),label
  changed.add(a.get_name());row=manifest['doors'][number]
  assert (a.get_actor_scale3d()-unreal.Vector(1.1,1.1,1.1)).length()<.001,'House scale drift'
  for key,val in [('original_leaf',meshes[row['leaf']]),('original_glass',None),('added_handle_mesh',None),('local_leaf_space',True),('panel_center_world',unreal.Vector(*row['panelCenterLocalCm'])),('panel_half_extent',unreal.Vector(*row['panelHalfExtentLocalCm'])),('closed_yaw',float(row['closedYaw'])),('open_yaw',float(row['openYaw'])),('locked',False)]:a.set_editor_property(key,val)
  p=a.get_actor_location();a.set_actor_rotation(unreal.Rotator(),True);a.set_actor_location(p+unreal.Vector(.001,0,0),False,True);a.set_actor_location(p,False,True)
  pivot=next(c for c in a.get_components_by_class(unreal.SceneComponent) if c.get_name()=='LeafPivot')
  pivot.set_relative_rotation(unreal.Rotator(yaw=row['closedYaw']),False,True)
  leaf=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OriginalDoorLeaf')
  leaf.set_static_mesh(meshes[row['leaf']]);leaf.set_editor_property('override_materials',[]);leaf.set_relative_transform(unreal.Transform(),False,True)
  panel=next(c for c in a.get_components_by_class(unreal.BoxComponent) if c.get_name()=='MovingPanelCollision')
  panel.set_box_extent(unreal.Vector(*row['panelHalfExtentLocalCm']),False);panel.set_relative_location(unreal.Vector(*row['panelCenterLocalCm']),False,True)
  details.append(dict(number=number,actor=label,id=str(a.get_editor_property('door_id')),hinge=xyz(p),doorway=xyz(a.get_editor_property('doorway_center')),leaf=leaf.static_mesh.get_path_name()))
 # The number now moves with the leaf. Remove the older separate 102 header placard.
 removed=[]
 old=actors.get('Interactive_SM_Room102_Sign__walldecor_1')
 if old:changed.add(old.get_name());removed.append(old.get_actor_label());S.destroy_actor(old)
 plaque=actors.get('Room101 wood name plaque') or S.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-2338.5*1.1,-1323*1.1,139*1.1))
 plaque.set_actor_label('Room101 wood name plaque');plaque.set_folder_path('First floor/Rooms 101 and 102')
 plaque.set_actor_transform(unreal.Transform(location=unreal.Vector(-2338.5*1.1,-1323*1.1,139*1.1),rotation=unreal.Rotator(yaw=-90),scale=unreal.Vector(1.1,1.1,1.1)),False,True)
 plaque.static_mesh_component.set_static_mesh(meshes['SM_R101Entry_BlankNamePlaque']);plaque.static_mesh_component.set_collision_profile_name('NoCollision')
 after={a.get_name():a for a in S.get_all_level_actors()}
 errors=[r['label'] for n,r in before.items() if n not in changed and n in after and state(after[n])!=r]
 missing=[r['label'] for n,r in before.items() if n not in changed and n not in after]
 assert not errors and not missing,dict(changed=errors,missing=missing)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level(),'Map save failed'
 json.dump(dict(status='PASS',doors=details,protected_actors=len(before)-len(changed),protected_unchanged=True,removed_placards=removed,room101_usable_bounds_cm=[[-3316.5,-1444.41,.88],[-2605.68,-520.74,288.464]],room101_shell_and_furniture_preserved=True),open(B/'evidence/install.json','w'),indent=2)
 unreal.log('ROOM101_ENTRY_INSTALL_PASS protected='+str(len(before)-len(changed)))
try:run()
except:unreal.log_error('ROOM101_ENTRY_INSTALL_FAIL '+traceback.format_exc())
