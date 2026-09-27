"""Create an independent, male-looking MetaHuman study for Runner.

The first blank-character study assembled with feminine facial structure. This
copies Epic's Cameron preset as a more appropriate *starting mesh*, then fits
the proportions visible in the three local reference photographs. It neither
modifies the preset nor the existing C02/Braxton characters.
"""
import unreal

SOURCE = '/MetaHumanCharacter/Optional/Presets/Cameron'
DEST = '/Game/Characters/MetaHumans/Runner/MH_Runner_CameronStudy'
assets = unreal.EditorAssetLibrary
character = assets.load_asset(DEST)
if not character:
    character = assets.duplicate_asset(SOURCE, DEST)
if not character:
    raise RuntimeError('Could not copy Cameron MetaHuman preset')

sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character):
    raise RuntimeError('Could not edit independent Runner study')
try:
    if assets.get_metadata_tag(character, 'RunnerCameronFit') != '1':
        constraints = sub.get_body_constraints(character)
        targets = {
            'Height': 190.0,  # Art direction; the reference has no ruler.
            'Inseam': 83.0,
            'Shoulder Height': 163.0,
            'Chest': 96.0,
            'Waist': 77.0,
            'Hip': 94.0,
            'Across Shoulder': 39.0,
        }
        for item in constraints:
            if item.name in targets:
                item.is_active = True
                item.target_measurement = targets[item.name]
        sub.set_body_constraints(character, constraints)
        sub.commit_body_state(character)
        collection = sub.get_preview_collection(character)
        item = assets.load_asset('/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_M_SideSweptFringe')
        if not item:
            raise RuntimeError('Local side-swept hair wardrobe unavailable')
        key = collection.try_add_item_from_wardrobe_item('Hair', item)
        collection.default_instance.set_single_slot_selection('Hair', key)
        outfit = assets.load_asset('/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment')
        key = collection.try_add_item_from_wardrobe_item('Outfits', outfit)
        collection.default_instance.set_single_slot_selection('Outfits', key)
        sub.on_edit_preview_collection(character)
        for key, value in {
            'RunnerCameronFit': '1',
            'ReferenceFront': 'IMG_1982.HEIC',
            'ReferenceSide': 'IMG_1983.HEIC',
            'ReferenceBack': 'IMG_1984.HEIC',
            'HeightArtDirectionCm': '190',
            'ProductionStatus': 'Unreleased independent Runner visual study',
        }.items():
            assets.set_metadata_tag(character, key, value)
        if not assets.save_loaded_asset(character):
            raise RuntimeError('Could not save Runner study')
    unreal.log('RUNNER_CAMERON_STUDY_READY ' + DEST)
finally:
    sub.remove_object_to_edit(character)
