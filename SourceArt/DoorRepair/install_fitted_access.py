import unreal,re,traceback
base='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/DoorRepair/'
dest='/Game/SeniorSendOff/House/DoorRepair'
def imp(name):
 t=unreal.AssetImportTask();t.filename=base+name+'.fbx';t.destination_path=dest;t.destination_name=name+"_v2";t.automated=True;t.replace_existing=True;t.save=True
 opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False;t.options=opt
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0');unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);return unreal.load_asset(dest+'/'+name+'_v2')
try:
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
 leaf=imp('SM_PorchAccessFittedLeaf')
 for i,s in enumerate(leaf.static_materials):
  n=re.sub(r'[._]\d+$','',str(s.material_slot_name));m=unreal.load_asset('/Game/SeniorSendOff/House/Materials/MI_SSO_'+n);assert m,n;leaf.set_material(i,m)
 unreal.EditorAssetLibrary.save_loaded_asset(leaf)
 for label,tag in [('SM_Upper_m1_m2_Solid','westhall'),('SM_Upper_m1_p1_Solid','easthall')]:
  a=actors[label];old=a.static_mesh_component.static_mesh
  mapping={re.sub(r'[._]\d+$','',str(s.material_slot_name)):s.material_interface for s in old.static_materials}
  mesh=imp('SM_Clean_current_'+tag)
  for i,s in enumerate(mesh.static_materials):
   n=re.sub(r'[._]\d+$','',str(s.material_slot_name));m=mapping.get(n) or unreal.load_asset('/Game/SeniorSendOff/House/Materials/'+n);assert m,n;mesh.set_material(i,m)
  unreal.EditorAssetLibrary.save_loaded_asset(mesh);a.static_mesh_component.set_static_mesh(mesh)
 for label,pos,rot in [('OriginalDoor_Upper_m1_m1_Solid_01',(-1294.2,-778.5,307.5),0),('OriginalDoor_Upper_m1_p0_Solid_03',(1310.2,-606,310.8),180)]:
  a=actors[label]
  for p,v in dict(original_leaf=leaf,original_glass=None,added_handle_mesh=None,local_leaf_space=True,closed_yaw=0.,open_yaw=90.,panel_center_world=unreal.Vector(0,87,103),panel_half_extent=unreal.Vector(2.8,87,103),doorway_center=unreal.Vector(pos[0],pos[1]+(87 if rot==0 else -87),pos[2]+103)).items():a.set_editor_property(p,v)
  a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=rot,roll=0),False);a.set_actor_location(unreal.Vector(*pos),False,False)
 unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House');unreal.log('FITTED_ACCESS_SAVED')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
