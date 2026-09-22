import unreal
character=unreal.load_asset('/Game/Characters/MetaHumans/Braxton/MH_Braxton_Working')
sub=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(character): raise RuntimeError('Cannot edit Braxton')
try:
    params=unreal.MetaHumanCharacterAutoRiggingRequestParams()
    params.blocking=True
    params.report_progress=False
    params.rig_type=unreal.MetaHumanRigType.JOINTS_ONLY
    unreal.log('BRAXTON_AUTHORIZED_RIG_REQUEST_START')
    sub.request_auto_rigging(character,params)
    unreal.EditorAssetLibrary.save_loaded_asset(character)
    unreal.log('BRAXTON_RIG_REQUEST_RETURNED')
finally:
    sub.remove_object_to_edit(character)
