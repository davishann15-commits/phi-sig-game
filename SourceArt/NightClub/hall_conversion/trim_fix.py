import pathlib,json,re,unreal,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
W=B/'hall_conversion';D='/Game/SeniorSendOff/House/NightClub/HallConversion'
try:
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0');unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 before={a.get_name():snapshot(a)for a in S.get_all_level_actors()};art=json.loads((W/'geometry/manifest.json').read_text());row=next(r for r in art['assets']if r['name']=='SM_NCH_RetainedTrim')
 mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
 for slot in slots:
  key=re.sub(r'[.]\d+$','',str(slot.get_editor_property('imported_material_slot_name')));slot.material_interface=A.load_asset(art['materials'][key]['reuse_asset']);assert slot.material_interface
 mesh.static_materials=slots;collision(mesh);mesh.set_editor_property('allow_cpu_access',True);assert A.save_loaded_asset(mesh,False)
 after={a.get_name():snapshot(a)for a in S.get_all_level_actors()};assert before==after
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 unreal.log('CLUB_HALL_TRIM_PASS protected='+str(len(after)))
except:unreal.log_error('CLUB_HALL_TRIM_FAIL '+traceback.format_exc())
