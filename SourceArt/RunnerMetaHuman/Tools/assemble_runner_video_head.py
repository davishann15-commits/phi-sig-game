"""Assemble the locally conformed video-head study without cloud requests."""

import unreal


assets = unreal.EditorAssetLibrary
variant = "V2" if "-RunnerVideoHeadV2" in unreal.SystemLibrary.get_command_line() else "Study"
source = "/Game/Characters/MetaHumans/Runner/MH_Runner_SamVideoHead" + variant
destination = "/Game/MetaHumans/RunnerSamVideoHead" + variant
character = assets.load_asset(source)
if not character:
    raise RuntimeError("Conformed Runner source is missing")
if assets.does_directory_exist(destination):
    raise RuntimeError("Video head assembly already exists; refusing to replace")
editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not editor.try_add_object_to_edit(character):
    raise RuntimeError("Could not edit Runner video head source")
try:
    if variant == "V2":
        collection = editor.get_preview_collection(character)
        fringe = assets.load_asset('/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_S_SideSweptFringe')
        if not fringe:
            raise RuntimeError('Fringe hair asset is missing')
        selection = collection.try_add_item_from_wardrobe_item('Hair', fringe)
        collection.default_instance.set_single_slot_selection('Hair', selection)
        editor.on_edit_preview_collection(character)
        assets.save_loaded_asset(character)
    if not editor.can_build_meta_human(character):
        raise RuntimeError("Video head source is not locally buildable")
    parameters = unreal.MetaHumanCharacterEditorBuildParameters()
    parameters.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    parameters.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    parameters.absolute_build_path = destination
    parameters.common_folder_path = destination + "/Common"
    parameters.enable_wardrobe_item_validation = False
    unreal.log("RUNNER_VIDEO_HEAD_ASSEMBLY_BEGIN")
    editor.build_meta_human(character, parameters)
    if not assets.save_directory(destination, only_if_is_dirty=False, recursive=True):
        raise RuntimeError("Could not save assembled video head")
    unreal.log("RUNNER_VIDEO_HEAD_ASSEMBLY_COMPLETE " + str(len(assets.list_assets(destination, recursive=True))))
finally:
    editor.remove_object_to_edit(character)
