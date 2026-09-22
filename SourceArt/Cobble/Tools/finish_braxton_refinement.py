"""Rebuild the approved cloud rig and isolated review assets, then groom finish."""
import runpy, unreal
ROOT='/Users/Stewart/Documents/Codex/2026-09-11/c/work/'
runpy.run_path(ROOT+'rig_braxton_metahuman.py',run_name='__main__')
runpy.run_path(ROOT+'assemble_braxton_metahuman.py',run_name='__main__')
assets=unreal.EditorAssetLibrary
materials=unreal.MaterialEditingLibrary
changed=[]
for path in assets.list_assets('/Game/MetaHumans/BraxtonReview/MH_Braxton_Working/Grooms',recursive=True):
    if '/MI_' not in path: continue
    obj=assets.load_asset(path)
    if not isinstance(obj,unreal.MaterialInstanceConstant): continue
    names={str(n) for n in materials.get_scalar_parameter_names(obj)}
    melanin=.68 if 'WI_Hair_' in path or 'Eyebrows' in path else .45
    edits={'hairMelanin':melanin,'hairRedness':.02,'HairRoughness':.42,'WhiteAmount':0.0,'LightAmount':0.0}
    for name,value in edits.items():
        if name in names:
            materials.set_material_instance_scalar_parameter_value(obj,name,value)
    materials.update_material_instance(obj)
    if not assets.save_loaded_asset(obj): raise RuntimeError('Material save failed '+path)
    changed.append(path)
unreal.log('BRAXTON_GROOM_FINISH_SAVED '+str(changed))
