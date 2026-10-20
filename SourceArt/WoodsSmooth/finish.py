import unreal,json,pathlib,hashlib,traceback
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
R=pathlib.Path(unreal.Paths.project_dir());B=R/'SourceArt/WoodsSmooth'
try:
 before=json.loads((B/'evidence/before.json').read_text())
 actual=hashlib.sha256((R/'Content/Story/Maps/Chapter01_House.umap').read_bytes()).hexdigest()
 assert actual==before['map_sha256'],'Saved house map changed'
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);a=S.get_all_level_actors()
 expected=json.loads((R/'SourceArt/NightClub/hall_conversion/evidence/actors_after.json').read_text())
 assert len(a)==len(expected)==1957
 assert {x.get_name() for x in a}=={x['name'] for x in expected}
 L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);k=L.get_active_viewport_config_key();p=unreal.Vector(-1317,-752,503);t=unreal.Vector(-817,-752,495)
 L.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t),k);L.editor_set_game_view(True,k)
 (B/'evidence/reopened.json').write_text(json.dumps(dict(status='PASS',actors=len(a),map_sha256=actual,saved_house_unchanged=True),indent=2))
 unreal.log('WOODS_EDITOR_READY map=Chapter01_House saved_house_unchanged=1 actors='+str(len(a)))
except:unreal.log_error('WOODS_EDITOR_FAILED '+traceback.format_exc())
