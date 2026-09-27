"""Read-only landmark comparison of local Runner face studies."""
import unreal

assets = unreal.EditorAssetLibrary
editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
for name in ('CasualHairStudy', 'FaceStudyV2', 'FaceStudyV3'):
    character = assets.load_asset('/Game/Characters/MetaHumans/Runner/MH_Runner_' + name)
    if not character or not editor.try_add_object_to_edit(character):
        raise RuntimeError('Could not inspect face study ' + name)
    try:
        landmarks = editor.get_face_landmarks(character)
        print('RUNNER_FACE_FEATURES', name, [(i, round(landmarks[i].x, 3),
             round(landmarks[i].y, 3), round(landmarks[i].z, 3))
             for i in (3, 4, 9, 12, 16, 30, 43, 62, 63, 64, 65)])
    finally:
        editor.remove_object_to_edit(character)
