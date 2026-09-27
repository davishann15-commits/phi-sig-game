"""Read-only comparison of known masculine Braxton body and Runner body."""
import unreal
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
for name, path in [('BRAXTON','/Game/Characters/MetaHumans/Braxton/MH_Braxton_Rebuild'),
                   ('RUNNER','/Game/Characters/MetaHumans/Runner/MH_Runner_Working')]:
    char = unreal.load_asset(path)
    if not sub.try_add_object_to_edit(char):
        continue
    try:
        data = [(str(c.name),c.is_active,c.target_measurement) for c in sub.get_body_constraints(char)
                if str(c.name) in ('Height','Masculine/Feminine','Fat','Muscularity','Chest','Waist')]
        unreal.log(name + '_BODY_COMPARE ' + str(data))
    finally:
        sub.remove_object_to_edit(char)
