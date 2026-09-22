import unreal
asset='/Game/Characters/MetaHumans/Braxton/MH_Braxton_Working'
character=unreal.load_asset(asset)
sub=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character): raise RuntimeError('Cannot edit Braxton')
try:
    if not character.has_high_resolution_textures:
        params=unreal.MetaHumanCharacterTextureRequestParams()
        params.blocking=True
        params.report_progress=False
        sub.request_texture_sources(character,params)
    unreal.EditorAssetLibrary.save_loaded_asset(character)
    if not sub.can_build_meta_human(character):
        raise RuntimeError('MetaHuman assembly prerequisites are not satisfied')
    build=unreal.MetaHumanCharacterEditorBuildParameters()
    build.pipeline_type=unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    build.pipeline_quality=unreal.MetaHumanQualityLevel.MEDIUM
    build.absolute_build_path='/Game/MetaHumans/BraxtonReview'
    build.common_folder_path='/Game/MetaHumans/Common'
    build.enable_wardrobe_item_validation=False
    sub.build_meta_human(character,build)
    for folder in ('/Game/MetaHumans/Common','/Game/MetaHumans/BraxtonReview'):
        if not unreal.EditorAssetLibrary.save_directory(folder,only_if_is_dirty=False,recursive=True):
            raise RuntimeError('Failed to persist assembled packages: '+folder)
    unreal.EditorAssetLibrary.save_loaded_asset(character)
    results=unreal.EditorAssetLibrary.list_assets('/Game/MetaHumans/BraxtonReview',recursive=True)
    unreal.log('BRAXTON_ASSEMBLY_ASSETS: '+str(results))
    if not results: raise RuntimeError('Assembly did not produce saved assets')
    unreal.log('BRAXTON_ASSEMBLY_COMPLETE')
finally:
    sub.remove_object_to_edit(character)
