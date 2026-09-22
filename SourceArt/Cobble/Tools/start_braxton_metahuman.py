"""Create a separate, non-gameplay MetaHuman work asset; never replace C01."""
import unreal
import json
from pathlib import Path

path='/Game/Characters/MetaHumans/Braxton/MH_Braxton_Working'
assets=unreal.EditorAssetLibrary
character=assets.load_asset(path) if assets.does_asset_exist(path) else None
if character is None:
    character=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name='MH_Braxton_Working', package_path='/Game/Characters/MetaHumans/Braxton',
        asset_class=unreal.MetaHumanCharacter,
        factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))
if character is None:
    raise RuntimeError('MetaHuman asset creation failed')
assets.set_metadata_tag(character,'DisplayName','Braxton Hungate')
assets.set_metadata_tag(character,'ReferenceDirectory','/Users/Stewart/Downloads/Braxton')
assets.set_metadata_tag(character,'TargetHeightCm','175.26')
assets.set_metadata_tag(character,'ReferenceWeightLb','155')
assets.set_metadata_tag(character,'ProductionStatus','Work in progress; not a finished likeness or gameplay replacement')
if not assets.save_loaded_asset(character):
    raise RuntimeError('Could not save Braxton work asset')
unreal.log('BRAXTON_METAHUMAN_ASSET_CREATED: '+path)
subsystem=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
reference='/Users/Stewart/Downloads/Braxton/IMG_2006.PNG'
try:
    size,pixels=unreal.PromotedFrameUtils.get_promoted_frame_as_pixel_array_from_disk(reference)
    contours=subsystem.track_face_landmarks_from_image(pixels,size.x,size.y)
    if isinstance(contours,tuple) and len(contours)==1: contours=contours[0]
    if contours and hasattr(contours,'items'):
        result={'source':reference,'width':size.x,'height':size.y,
                'contours':{str(k):str(v) for k,v in contours.items()}}
        Path('/Users/Stewart/Documents/Codex/2026-09-11/c/work/braxton_metahuman_contours.json').write_text(json.dumps(result,indent=2))
        unreal.log('BRAXTON_REFERENCE_TRACKED: '+str(len(contours))+' contours')
    else:
        unreal.log_warning('BRAXTON_REFERENCE_TRACKING_UNAVAILABLE')
except Exception as error:
    unreal.log_warning('BRAXTON_REFERENCE_TRACKING_UNAVAILABLE: '+str(error))
if not subsystem.try_add_object_to_edit(character):
    unreal.log_warning('BRAXTON_METAHUMAN_EDIT_BLOCKED: MetaHuman editing initialization failed')
else:
    try:
        landmarks=subsystem.get_face_landmarks(character=character)
        unreal.log('BRAXTON_METAHUMAN_EDIT_READY: '+str(len(landmarks))+' face landmarks')
        Path('/Users/Stewart/Documents/Codex/2026-09-11/c/work/braxton_metahuman_landmarks.json').write_text(json.dumps([[v.x,v.y,v.z] for v in landmarks],indent=2))
        assets.save_loaded_asset(character)
    finally:
        subsystem.remove_object_to_edit(character)
