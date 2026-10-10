import pathlib,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing'
exec((B/'install.py').read_text().split('def run():')[0],globals())
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
art=json.loads((B/'art/manifest.json').read_text());mats={n:A.load_asset(D+'/'+n)for n in art['materials']}
for r in art['assets']:
 if r['name'] not in ['SM_NW_HallWalls','SM_NW_StairHardware','SM_NW_StairNorthernPractical','SM_NW_FordPlaque','SM_NW_SportsPennant','SM_NW_UtilityCable','SM_NW_FireExtinguisher']:continue
 me=imp(r['fbx'],r['name']);slots=list(me.static_materials)
 for sl in slots:
  key=re.sub(r'[._]\d+$','',str(sl.get_editor_property('imported_material_slot_name')));assert mats[key],key;sl.material_interface=mats[key]
 me.static_materials=slots;setupcol(me,r['kind']);assert A.save_loaded_asset(me,False)
unreal.log('NOODLE_REFRESH_ART_PASS')
# Continue into the scoped map installation in this same editor session.
exec((B/'install.py').read_text(),globals())
