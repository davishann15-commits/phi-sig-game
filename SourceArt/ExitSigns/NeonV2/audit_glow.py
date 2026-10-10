import unreal,json,traceback
try:
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 for x in a:
  if isinstance(x,unreal.PostProcessVolume):
   s=x.get_editor_property('settings')
   unreal.log('GLOW_AUDIT '+json.dumps(dict(label=x.get_actor_label(),priority=x.get_editor_property('priority'),unbound=x.get_editor_property('unbound'),bloom=s.get_editor_property('bloom_intensity'),override=s.get_editor_property('override_bloom_intensity'),threshold=s.get_editor_property('bloom_threshold'))))
 m=unreal.load_asset('/Game/SeniorSendOff/House/ExitSigns/NEX_RedLens')
 unreal.log('GLOW_AUDIT shader='+str(m.get_editor_property('shading_model')))
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
