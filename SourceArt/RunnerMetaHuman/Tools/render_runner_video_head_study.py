"""Render a repeatable front/three-quarter/profile review of the new head."""

from math import cos, radians, sin
from pathlib import Path

import bpy
from mathutils import Vector


OUTPUT_DIR = Path("/tmp/runner_samvideo_review")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
head = bpy.data.objects["Runner_SamVideo_FaceBuilderHead"]
head.hide_render = False
if "--neutral" in __import__("sys").argv:
    neutral = bpy.data.materials.new("HeadStudyNeutral")
    neutral.diffuse_color = (0.55, 0.4, 0.34, 1)
    head.data.materials.clear()
    head.data.materials.append(neutral)

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 900
scene.render.resolution_y = 1100
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.world.color = (0.12, 0.12, 0.12)

for object_ in list(bpy.data.objects):
    if object_.type in {"CAMERA", "LIGHT"}:
        bpy.data.objects.remove(object_, do_unlink=True)

camera_data = bpy.data.cameras.new("ReviewCamera")
camera = bpy.data.objects.new("ReviewCamera", camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
camera_data.type = "ORTHO"
camera_data.ortho_scale = 3.8

for name, location, power, size in (
    ("Key", (-3, -4, 5), 600, 4),
    ("Fill", (4, -3, 1), 270, 4),
    ("Rim", (1, 3, 3), 320, 3),
):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = power
    data.shape = "DISK"
    data.size = size
    light = bpy.data.objects.new(name, data)
    scene.collection.objects.link(light)
    light.location = location
    light.rotation_euler = (Vector((0, 0, 0)) - light.location).to_track_quat("-Z", "Y").to_euler()

for angle in (0, 45, 90, 180):
    a = radians(angle)
    camera.location = (6 * sin(a), -6 * cos(a), 0)
    camera.rotation_euler = (Vector((0, 0, 0)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    suffix = "-neutral" if "--neutral" in __import__("sys").argv else ""
    scene.render.filepath = str(OUTPUT_DIR / f"face-{angle:03d}{suffix}.png")
    bpy.ops.render.render(write_still=True)
    print("RENDERED", scene.render.filepath)
