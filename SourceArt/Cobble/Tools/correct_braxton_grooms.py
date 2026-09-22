import unreal,runpy
assets=unreal.EditorAssetLibrary
character=assets.load_asset('/Game/Characters/MetaHumans/Braxton/MH_Braxton_Working')
sub=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
editor.open_editor_for_assets([character])
collection=sub.get_preview_collection(character)
for slot,relative in [('Hair','Hair/WI_Hair_S_SideSweptFringe'),('Eyebrows','Eyebrows/WI_Eyebrows_M_SlightArch'),('Beard','Beards/WI_Beard_S_Stubble')]:
    item=assets.load_asset('/MetaHumanCharacter/Optional/Grooms/Bindings/'+relative)
    if not item: raise RuntimeError('Missing '+relative)
    key=collection.try_add_item_from_wardrobe_item(slot,item)
    collection.default_instance.set_single_slot_selection(slot,key)
sub.on_edit_preview_collection(character)
assets.save_loaded_asset(character)
editor.close_all_editors_for_asset(character)
runpy.run_path('/Users/Stewart/Documents/Codex/2026-09-11/c/work/assemble_braxton_metahuman.py',run_name='__main__')
materials=unreal.MaterialEditingLibrary
for path in assets.list_assets('/Game/MetaHumans/BraxtonReview/MH_Braxton_Working/Grooms',recursive=True):
    if '/MI_' not in path: continue
    obj=assets.load_asset(path)
    if not isinstance(obj,unreal.MaterialInstanceConstant): continue
    names={str(n) for n in materials.get_scalar_parameter_names(obj)}
    melanin=.68 if 'WI_Hair_' in path or 'Eyebrows' in path else .45
    for name,value in {'hairMelanin':melanin,'hairRedness':.02,'HairRoughness':.42,'WhiteAmount':0.0,'LightAmount':0.0}.items():
        if name in names: materials.set_material_instance_scalar_parameter_value(obj,name,value)
    materials.update_material_instance(obj)
    assets.save_loaded_asset(obj)
unreal.log('BRAXTON_SHORT_HAIR_CORRECTION_SAVED')
