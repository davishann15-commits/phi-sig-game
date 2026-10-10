import unreal,json
from pathlib import Path
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/impossible-hallway')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
cam=unreal.Vector(-2177,-1640,459)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(cam,unreal.Rotator(pitch=0,yaw=-90,roll=0))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
actors=unreal.EditorLevelLibrary.get_all_level_actors()
halls=[a for a in actors if isinstance(a,unreal.HouseImpossibleHallway)]
pigs=[a for a in actors if isinstance(a,unreal.HouseRoamingPig)]
doors=[a for a in actors if isinstance(a,unreal.ExistingHouseDoor) and str(a.get_editor_property('door_id'))=='HallEnd_End']
dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
info={'map':'Chapter01_House','hallCount':len(halls),'doorCount':len(doors),'dirtyMaps':[p.get_path_name()for p in dirty],'pigs':[{k:p.get_editor_property(k)for k in ('pig_name','visual_scale','max_health','walk_speed','run_speed')}for p in pigs]}
if len(halls)!=1 or len(doors)!=1 or len(pigs)!=2:raise RuntimeError('Unexpected hallway or preserved pig configuration')
info['hall']={k:str(halls[0].get_editor_property(k))for k in ('entrance_door_id','module_length','loop_depth','return_min_depth')}
json.dump(info,open(root/'editor-ready.json','w'),indent=2)
unreal.log('IMPOSSIBLE_HALL_EDITOR_READY saved=%d hall=%d pigs=%d'%(not dirty,len(halls),len(pigs)))
