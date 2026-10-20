import unreal,pathlib,json
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseCleanup2'
S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);W=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert W.get_name()=='Chapter01_House'
a=S.get_all_level_actors();saved=json.loads((B/'evidence/actors_after.json').read_text());assert len(a)==len(saved)==1955;assert {x.get_actor_label()for x in a}=={x['label']for x in saved}
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);k=L.get_active_viewport_config_key();p=unreal.Vector(-1790,720,-190);t=unreal.Vector(-1430,690,-230);L.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t),k);L.editor_set_game_view(True,k)
(B/'evidence/reopened.json').write_text(json.dumps(dict(status='PASS',map=W.get_path_name(),actor_count=len(a),all_actor_labels_preserved=True),indent=2));unreal.log('HOUSE_CLEANUP_REOPEN_PASS')
