import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
import unreal,pathlib,json
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseJoinRepair';S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert w.get_name()=='Chapter01_House'
before=json.loads((B/'evidence/actors_after.json').read_text());assert len(S.get_all_level_actors())==len(before)==1954
# Saved repairs remain in the level after the twenty unrelated mesh metadata
# experiments were reverted; no room or interactable is deleted.
assert set(a.get_actor_label() for a in S.get_all_level_actors())==set(a['label'] for a in before)
A=unreal.EditorAssetLibrary;restored=json.loads((B/'evidence/performance-selection.json').read_text())['restored_original_mesh_settings']
for name in restored:assert not A.load_asset(name).get_editor_property('nanite_settings').enabled
report=dict(status='PASS',map=w.get_path_name(),actor_count=1954,actors_preserved=True,experiments_reverted=True)
(B/'evidence/saved-map-check.json').write_text(json.dumps(report,indent=2));unreal.log('HOUSE_JOIN_SAVED_MAP_PASS')

import unreal,pathlib,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='Chapter01_House'
s=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);k=s.get_active_viewport_config_key()
p=unreal.Vector(480,270,185);target=unreal.Vector(-430,-450,220)
s.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,target),k);s.editor_set_game_view(True,k)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
report=dict(status='PASS',map=world.get_path_name(),actor_count=len(actors),house_repairs=[a.get_actor_label()for a in actors if a.get_actor_label().startswith('HouseJoin_')])
(pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseJoinRepair/evidence/reopened-map.json').write_text(json.dumps(report,indent=2));unreal.log('HOUSE_JOIN_FINISH_VIEW_PASS')
