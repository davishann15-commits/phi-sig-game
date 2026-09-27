"""Locally conform a fresh Runner MetaHuman head to the 360-video mesh.

This uses UE's built-in local conform solver. It duplicates an existing rigged
Runner source, does not change the playable character, and makes no cloud
texture/auto-rig request.
"""

import time

import unreal


assets = unreal.EditorAssetLibrary
source = "/Game/Characters/MetaHumans/Runner/MH_Runner_MaleFace"
study = "/Game/Characters/MetaHumans/Runner/MH_Runner_SamVideoHeadStudy"
head_path = "/Game/Characters/MetaHumans/Runner/SM_Runner_SamVideoHeadTargetCM"
if assets.does_asset_exist(study):
    raise RuntimeError("Video head study already exists; refusing to replace")
head_mesh = assets.load_asset(head_path)
if not head_mesh:
    raise RuntimeError("Video head conform mesh not imported")

character = assets.duplicate_asset(source, study)
if not character:
    raise RuntimeError("Could not create an independent Runner head study")

editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not editor.try_add_object_to_edit(character):
    raise RuntimeError("Could not open separate Runner head study")

try:
    vertices, triangles, *_ = editor.get_mesh_data_for_conforming(head_mesh)
    if len(vertices) < 10000 or len(triangles) < 30000:
        raise RuntimeError("Imported video head mesh lost geometry")
    params = unreal.ConformTargetParams()
    params.conform_target_mesh.target_parts_type = unreal.TargetPartsType.HEAD_ONLY
    params.conform_target_mesh.head_vertices = vertices
    params.conform_target_mesh.head_vertex_indices = triangles
    params.auto_solve = True
    params.body_conform_solve_settings.pipeline_name = "head_only"
    key = unreal.MetaHumanCharacterTargetMeshKey()
    key.head_mesh = head_mesh
    unreal.log(f"RUNNER_VIDEO_HEAD_SOLVE_BEGIN vertices={len(vertices)} faces={len(triangles)//3}")
    started = time.monotonic()
    solved = editor.conform_to_target_meshes(character, key, params)
    unreal.log(f"RUNNER_VIDEO_HEAD_SOLVE_END success={solved} seconds={time.monotonic()-started:.1f}")
    if not solved:
        raise RuntimeError("Local video-head conform solve failed")
    editor.commit_posed_state_as_a_pose(character, key)
    editor.commit_face_state(character)
    assets.set_metadata_tag(character, "RunnerHeadReference", "Sammcball.MOV 360-degree video; local FaceBuilder fitting")
    if not assets.save_loaded_asset(character):
        raise RuntimeError("Could not save video head study")
    unreal.log(f"RUNNER_VIDEO_HEAD_STUDY_SAVED buildable={editor.can_build_meta_human(character)}")
finally:
    editor.remove_object_to_edit(character)
