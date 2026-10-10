import unreal,json,traceback
try:
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 doors={str(a.get_editor_property('door_id')):a for a in sub.get_all_level_actors() if a.get_class().get_name()=='ExistingHouseDoor'}
 a=doors['SM_OriginalLeaf_Upper_m1_m1_Solid_01']
 a.set_editor_property('closed_yaw',-90.0)
 a.set_editor_property('open_yaw',0.0)
 a.set_editor_property('doorway_center',unreal.Vector(-1294.2,-694.1,410.3))
 a.set_actor_location(unreal.Vector(-1294.2,-776.9,306.8),False,False)
 sub.destroy_actor(doors['SM_OriginalLeaf_Upper_m1_p0_Solid_02'])
 unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 unreal.log('PORCH_ACCESS_FIX_SAVED')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
