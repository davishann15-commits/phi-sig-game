import unreal,sys,json,traceback
from pathlib import Path
B=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HallFloorsPorch');sys.path.insert(0,str(B));import materials
try:
 mats=materials.build();w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 a=next(a for a in sub.get_all_level_actors()if a.get_actor_label()=='HallEnd_SM_HallEnd_ThreeRoomsFloors');c=a.static_mesh_component
 assert c.get_material(0).get_name()=='MI_SSO_FloorWood'
 c.set_material(1,mats['HFP_HallCoating'])
 assert c.get_material(0).get_name()=='MI_SSO_FloorWood'
 assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 json.dump(dict(status='PASS',extended_hall_material='HFP_HallCoating',room_floor_material_unchanged='MI_SSO_FloorWood'),open(B/'evidence/final_finish.json','w'),indent=2)
 unreal.log('HFP_FINAL_FINISH_PASS')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
