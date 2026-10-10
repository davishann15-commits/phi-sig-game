import unreal,json
from pathlib import Path
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/hot-tub-water')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors=unreal.EditorLevelLibrary.get_all_level_actors()
waters=[a for a in actors if isinstance(a,unreal.HouseHotTubWater)]
assert len(waters)==1
w=waters[0]
camera=unreal.Vector(-2523,646,205)
aim=w.get_actor_location()-camera
unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera,unreal.Rotator(pitch=-20.63,yaw=-140.91,roll=0))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).clear_actor_selection_set()
dirty=[x.get_path_name()for x in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
report={'map':'Chapter01_House','dirtyMaps':dirty,'actorCount':len(actors),'dropletPool':w.splash_droplets.get_instance_count(),'foamPool':w.splash_foam.get_instance_count(),'waterMaterial':w.water_material.get_path_name(),'splashMaterial':w.splash_material.get_path_name(),'foamMaterial':w.foam_material.get_path_name(),'waveHeightCm':w.maximum_wave_height}
assert not dirty and report['dropletPool']==96 and report['foamPool']==64
json.dump(report,open(root/'editor-ready.json','w'),indent=2)
unreal.log('SPA_WATER_EDITOR_READY '+json.dumps(report))
