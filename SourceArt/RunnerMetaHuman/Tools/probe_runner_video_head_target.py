import unreal

asset = unreal.load_asset('/Game/Characters/MetaHumans/Runner/SM_Runner_SamVideoHeadTargetCM')
if not asset:
    raise RuntimeError('Missing video head target')
unreal.log('RUNNER_VIDEO_TARGET_BOUNDS ' + str(asset.get_bounds()))
editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
result = editor.get_mesh_data_for_conforming(asset)
unreal.log('RUNNER_VIDEO_TARGET_RESULT_SHAPE ' + str([(type(v).__name__, len(v) if hasattr(v, '__len__') else None) for v in result]))
verts, tris, *_ = result
unreal.log(f'RUNNER_VIDEO_TARGET_TOPOLOGY verts={len(verts)} tris={len(tris)//3} vertex0={verts[0]} tri0={tris[0]}')
