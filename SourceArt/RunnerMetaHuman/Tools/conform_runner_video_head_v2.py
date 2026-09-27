"""Fresh local head conform using both the 360 mesh and tracked face contours."""

import time

import unreal


assets = unreal.EditorAssetLibrary
source = "/Game/Characters/MetaHumans/Runner/MH_Runner_MaleFace"
study = "/Game/Characters/MetaHumans/Runner/MH_Runner_SamVideoHeadV2"
head_path = "/Game/Characters/MetaHumans/Runner/SM_Runner_SamVideoHeadTargetCM"
portrait = "/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/RunnerVideoHeadReview/TargetPortrait.png"
if assets.does_asset_exist(study):
    raise RuntimeError("Video head V2 already exists; refusing to replace")
mesh = assets.load_asset(head_path)
if not mesh:
    raise RuntimeError("Video head target is unavailable")

editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
image_size, pixels = unreal.PromotedFrameUtils.get_promoted_frame_as_pixel_array_from_disk(portrait)
curves = editor.track_face_landmarks_from_image(pixels, image_size.x, image_size.y)
if isinstance(curves, tuple) and len(curves) == 1:
    curves = curves[0]
if not curves or len(curves) < 12:
    raise RuntimeError("Could not track the fitted video face")
unreal.log(f"RUNNER_VIDEO_V2_TRACKED contours={len(curves)}")

character = assets.duplicate_asset(source, study)
if not character:
    raise RuntimeError("Could not create independent V2 Runner source")
if not editor.try_add_object_to_edit(character):
    raise RuntimeError("Could not edit independent V2 Runner source")

try:
    vertices, triangles, *_ = editor.get_mesh_data_for_conforming(mesh)
    params = unreal.ConformTargetParams()
    params.conform_target_mesh.target_parts_type = unreal.TargetPartsType.HEAD_ONLY
    params.conform_target_mesh.head_vertices = vertices
    params.conform_target_mesh.head_vertex_indices = triangles
    params.auto_solve = True
    params.body_conform_solve_settings.pipeline_name = "head_only"
    params.curve_tracking_points = curves
    view = unreal.MinimalViewInfo()
    view.location = unreal.Vector(0.0, 68.0, 172.0)
    view.rotation = unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0)
    view.fov = 40.0
    view.aspect_ratio = image_size.x / image_size.y
    view.projection_mode = unreal.CameraProjectionMode.PERSPECTIVE
    params.camera_view_info = view
    params.image_size = image_size
    key = unreal.MetaHumanCharacterTargetMeshKey()
    key.head_mesh = mesh
    unreal.log("RUNNER_VIDEO_V2_SOLVE_BEGIN")
    started = time.monotonic()
    solved = editor.conform_to_target_meshes(character, key, params)
    unreal.log(f"RUNNER_VIDEO_V2_SOLVE_END success={solved} seconds={time.monotonic()-started:.1f}")
    if not solved:
        raise RuntimeError("V2 contour-guided head conform failed")
    editor.commit_posed_state_as_a_pose(character, key)
    editor.commit_face_state(character)
    assets.set_metadata_tag(character, "RunnerHeadReference", "Sammcball.MOV 360-degree video; local mesh and contour fitting")
    if not assets.save_loaded_asset(character):
        raise RuntimeError("Could not save contour-fitted V2 head")
    unreal.log(f"RUNNER_VIDEO_V2_SAVED buildable={editor.can_build_meta_human(character)}")
finally:
    editor.remove_object_to_edit(character)
