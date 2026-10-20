"""Read-only (master job, step 2b): usage flags of the basement library's materials and Nanite on its meshes, to choose how
the campus may draw them (instanced or not). Writes Saved/ClaudeDump/campus_flags.json; saves nothing."""
import json, os, unreal
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeDump', 'campus_flags.json')
A = unreal.EditorAssetLibrary
LIB = '/Game/SeniorSendOff/House/BasementLibrary'
res = {'materials': {}, 'meshes': {}}
for path in A.list_assets(LIB, recursive=False, include_folder=False):
    name = path.split('/')[-1].split('.')[0]
    if name.startswith('M_Lib_'):
        m = unreal.load_asset(path)
        if isinstance(m, unreal.Material):
            d = {}
            for f in ('used_with_instanced_static_meshes', 'used_with_nanite', 'used_with_static_lighting', 'blend_mode', 'shading_model'):
                try: d[f] = str(m.get_editor_property(f))
                except Exception as e: d[f] = 'n/a'
            res['materials'][name] = d
    elif name.startswith('SM_Lib_') and ('Loose_BookShelfContents' in name or name in ('SM_Lib_SixShelfBookcase', 'SM_Lib_LongRug', 'SM_Lib_StudyPendant')) and not any(s in name for s in ('V2', 'V3')):
        m = unreal.load_asset(path)
        try:
            ns = m.get_editor_property('nanite_settings')
            res['meshes'][name] = {'nanite': bool(ns.get_editor_property('enabled'))}
        except Exception as e:
            res['meshes'][name] = {'nanite': 'n/a ' + str(e)}
with open(OUT, 'w') as f:
    json.dump(res, f, indent=1, sort_keys=True)
unreal.log('CAMPUS_FLAGS materials=%d meshes=%d' % (len(res['materials']), len(res['meshes'])))
