import unreal,pathlib,json,hashlib
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
R=pathlib.Path(unreal.Paths.project_dir());B=R/'SourceArt/DoorSwing'
W=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
a=S.get_all_level_actors();previous=json.loads((R/'SourceArt/HouseCleanup2/evidence/actors_after.json').read_text())
assert W.get_name()=='Chapter01_House'
assert len(a)==len(previous)==1955
assert {x.get_actor_label()for x in a}=={x['label']for x in previous}
assert (R/'Content/Story/Maps/Chapter01_House.umap').read_bytes()==(B/'before/Chapter01_House.umap').read_bytes()
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);k=L.get_active_viewport_config_key()
p=unreal.Vector(-30,-1270,170);t=unreal.Vector(-36.65,-989,130)
L.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t),k);L.editor_set_game_view(True,k)
(B/'evidence/reopened.json').write_text(json.dumps(dict(status='PASS',map=W.get_path_name(),actor_count=len(a),all_actor_labels_preserved=True,map_bytes_unchanged=True),indent=2))
unreal.log('DOOR_SWING_EDITOR_READY')
