import unreal,json,traceback
from pathlib import Path
B=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HallFloorsPorch');D='/Game/SeniorSendOff/House/HallFloorsPorch'
try:
 ed=unreal.EditorAssetLibrary;ml=unreal.MaterialEditingLibrary
 for path in ed.list_assets(D,recursive=True):
  m=unreal.load_asset(path)
  if isinstance(m,unreal.Material):
   ml.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_NANITE);ml.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES);ml.recompile_material(m);assert ed.save_loaded_asset(m,False)
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
 a=actors.get('Upper short hall worn floor')or sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-1660.25,-687.7,305.2))
 a.set_actor_label('Upper short hall worn floor');a.set_folder_path('House details/Hall floors and porch');a.tags=[unreal.Name('HallFloorsPorch')];a.set_actor_scale3d(unreal.Vector(7.325,3.058,.04));c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));c.set_material(0,unreal.load_asset(D+'/HFP_HallCoating'));c.set_collision_profile_name('BlockAll');c.set_editor_property('cast_shadow',False)
 assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 unreal.log('HFP_TRANSITION_AND_MATERIAL_FLAGS_PASS')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
