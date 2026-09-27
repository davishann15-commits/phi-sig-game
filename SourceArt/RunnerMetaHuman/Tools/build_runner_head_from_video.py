"""Fit a fresh, local FaceBuilder head to the user's 360-degree video stills.

This script is deliberately separate from the existing Runner MetaHuman assets.
It never uploads reference images and writes a Blender study for review.
"""

from pathlib import Path

import bpy

from keentools.facebuilder.settings import fb_settings
from keentools.facebuilder.fbloader import FBLoader
from keentools.utils.images import load_rgba
from keentools.utils.focal_length import configure_focal_mode_and_fixes
from keentools.utils.coords import update_head_mesh_non_neutral
from keentools.utils.materials import bake_tex, show_texture_in_mat, assign_material_to_object


PROJECT = Path("/Users/Stewart/Documents/Unreal Projects/SeniorSendoff")
FRAMES = PROJECT / "SourceArt/RunnerMetaHuman/ReferenceFrames"
OUTPUT = PROJECT / "SourceArt/RunnerMetaHuman/Runner_SamVideo_HeadStudy.blend"
# The middle of the clip is the rear of the head, which FaceBuilder's face
# detector cannot use. Fit both visible facial profiles and front angles.
FRAME_NUMBERS = (0, 1, 2, 3, 4, 5, 10, 11, 12, 13, 14, 15)


def main():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    print("ADD_HEAD", bpy.ops.keentools_fb.add_head())
    settings = fb_settings()
    head = settings.get_head(0)
    head.headobj.name = "Runner_SamVideo_FaceBuilderHead"
    builder = FBLoader.get_builder()
    used = []
    for frame in FRAME_NUMBERS:
        image_path = FRAMES / f"frame-{frame:02d}.png"
        camera = FBLoader.add_new_camera_with_image(0, str(image_path))
        camnum = len(head.cameras) - 1
        kid = camera.get_keyframe()
        image = load_rgba(camera)
        builder.set_use_emotions(head.should_use_emotions())
        configure_focal_mode_and_fixes(builder, head)
        faces = builder.detect_faces(image, builder.pixel_aspect_ratio(kid))
        print("FRAME", frame, "DETECTED", len(faces))
        if not faces:
            continue
        # The largest detected face should be the subject, not the background.
        face = max(faces, key=lambda f: getattr(f, "width", 0) * getattr(f, "height", 0))
        ok = builder.detect_face_pose(kid, face)
        print("FRAME", frame, "POSE", ok)
        if not ok:
            continue
        builder.remove_pins(kid)
        builder.add_preset_pins_and_solve(kid)
        FBLoader.update_camera_pins_count(0, camnum)
        update_head_mesh_non_neutral(builder, head)
        FBLoader.update_all_camera_positions(0)
        FBLoader.update_all_camera_focals(0)
        used.append(frame)
    FBLoader.save_fb_serial_and_image_pathes(0)
    update_head_mesh_non_neutral(builder, head)
    settings.tex_width = 2048
    settings.tex_height = 2048
    settings.tex_fill_gaps = True
    texture_status = bake_tex(0, head.preview_texture_name())
    print("TEXTURE_BAKE", texture_status.success, texture_status.error_message)
    if texture_status.success:
        material = show_texture_in_mat(head.preview_texture_name(), head.preview_material_name())
        assign_material_to_object(head.headobj, material)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT))
    print("FITTED_FRAMES", used, "HEAD_VERTICES", len(head.headobj.data.vertices), "SAVED", OUTPUT)


if __name__ == "__main__":
    main()
