"""Create an isolated Runner MetaHuman source asset from the local UE 5.8 creator.

The three reference photographs remain on disk. Nothing here replaces C02.
"""
import unreal

PATH = '/Game/Characters/MetaHumans/Runner/MH_Runner_Working'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(PATH) if assets.does_asset_exist(PATH) else None
if character is None:
    character = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name='MH_Runner_Working',
        package_path='/Game/Characters/MetaHumans/Runner',
        asset_class=unreal.MetaHumanCharacter,
        factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))
if character is None:
    raise RuntimeError('Could not create Runner MetaHuman character')

for key, value in {
    'DisplayName': 'Runner (Character 02)',
    'ReferenceDirectory': '/Users/Stewart/Downloads/COBBLE',
    'ReferenceFront': 'IMG_1982.HEIC',
    'ReferenceSide': 'IMG_1983.HEIC',
    'ReferenceBack': 'IMG_1984.HEIC',
    'HeightDirection': 'Significantly taller than Braxton; exact height not provided',
    'ProductionStatus': 'Isolated MetaHuman work asset; original C02 unchanged',
}.items():
    assets.set_metadata_tag(character, key, value)

sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character):
    raise RuntimeError('MetaHuman edit subsystem unavailable')
try:
    points = sub.get_face_landmarks(character)
    unreal.log('RUNNER_META_CREATED points=' + str(len(points)) + ' has_hi_res=' + str(character.has_high_resolution_textures))
    unreal.log('RUNNER_BODY_METHODS ' + str([n for n in dir(sub) if 'body' in n.lower() or 'height' in n.lower()]))
    unreal.log('RUNNER_CHARACTER_PROPERTIES ' + str([n for n in dir(character) if 'body' in n.lower() or 'height' in n.lower() or 'face' in n.lower()]))
    if not assets.save_loaded_asset(character):
        raise RuntimeError('Could not save Runner character')
finally:
    sub.remove_object_to_edit(character)

unreal.log('RUNNER_META_ASSET_SAVED ' + PATH)
