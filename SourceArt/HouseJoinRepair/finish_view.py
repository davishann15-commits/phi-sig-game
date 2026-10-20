import unreal,pathlib,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='Chapter01_House'
s=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);k=s.get_active_viewport_config_key()
p=unreal.Vector(480,270,185);target=unreal.Vector(-430,-450,220)
s.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,target),k);s.editor_set_game_view(True,k)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
report=dict(status='PASS',map=world.get_path_name(),actor_count=len(actors),house_repairs=[a.get_actor_label()for a in actors if a.get_actor_label().startswith('HouseJoin_')])
(pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseJoinRepair/evidence/reopened-map.json').write_text(json.dumps(report,indent=2));unreal.log('HOUSE_JOIN_FINISH_VIEW_PASS')
