import unreal,json,pathlib,re,traceback
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
W=B/'hall_conversion'
try:
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 after={a.get_name():snapshot(a)for a in S.get_all_level_actors()};expected={r['name']:r for r in json.loads((W/'evidence/actors_after.json').read_text())}
 def norm(r):return re.sub(r'0x[0-9a-fA-F]+','runtime-address',json.dumps(r,sort_keys=True))
 drift=[r['label']for n,r in expected.items()if n not in after or norm(r)!=norm(after[n])];assert not drift,drift
 assert len(after)==1957
 L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);k=L.get_active_viewport_config_key();p=unreal.Vector(-1510,-2075,487);t=unreal.Vector(-2390,-1770,455);L.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t),k);L.editor_set_game_view(True,k)
 (W/'evidence/reopened.json').write_text(json.dumps(dict(status='PASS',actors=len(after),saved_actor_settings_match=True),indent=2))
 unreal.log('CLUB_HALL_EDITOR_READY saved=1 actors='+str(len(after)))
except:unreal.log_error('CLUB_HALL_EDITOR_FAILED '+traceback.format_exc())
