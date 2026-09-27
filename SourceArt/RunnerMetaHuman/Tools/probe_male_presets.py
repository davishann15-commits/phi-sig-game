"""Read-only inspection of local MetaHuman preset types and editor operations."""
import unreal

root = '/MetaHumanCharacter/Optional/Presets/'
assets = unreal.EditorAssetLibrary
for name in ['Cameron', 'Victor', 'Dominic', 'Mikel', 'Braxton']:
    obj = assets.load_asset(root + name) if name != 'Braxton' else assets.load_asset(
        '/Game/Characters/MetaHumans/Braxton/MH_Braxton_Rebuild')
    if not obj:
        continue
    unreal.log('RUNNER_PRESET ' + name + ' class=' + str(obj.get_class().get_name()) +
               ' body=' + str(obj.get_editor_property('fixed_body_type')) +
               ' texture=' + str(obj.get_editor_property('has_high_resolution_textures')))
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
unreal.log('RUNNER_PRESET_METHODS ' + str([n for n in dir(sub) if any(word in n.lower()
           for word in ('preset', 'copy', 'face', 'wardrobe', 'body'))]))
