"""Render the fitted mesh for UE's local face-contour tracker."""

from math import radians
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path("/Users/Stewart/Documents/Unreal Projects/SeniorSendoff")
OUTPUT = ROOT / "Saved/RunnerVideoHeadReview/TargetPortrait.png"
OUTPUT.parent.mkdir(parents=True, exist_ok=True)

head = bpy.data.objects["Runner_SamVideo_FaceBuilderHead"]
for vertex in head.data.vertices:
    x, y, z = vertex.co
    vertex.co = (x * 18.0, y * 11.5 + 0.2, z * 16.0 + 169.4)

# Render the baked video texture without shading so landmark detection sees the
# eyes and lip contours instead of the temporary review lighting.
texture = None
for material in head.data.materials:
    for node in material.node_tree.nodes:
        if node.type == "TEX_IMAGE" and node.image:
            texture = node.image
            break
if texture is None:
    raise RuntimeError("Video texture was not baked")
material = bpy.data.materials.new("TrackerPortraitUnlit")
material.use_nodes = True
nodes = material.node_tree.nodes
nodes.clear()
image = nodes.new("ShaderNodeTexImage")
image.image = texture
emission = nodes.new("ShaderNodeEmission")
emission.inputs["Strength"].default_value = 3.0
output = nodes.new("ShaderNodeOutputMaterial")
material.node_tree.links.new(image.outputs["Color"], emission.inputs["Color"])
material.node_tree.links.new(emission.outputs["Emission"], output.inputs["Surface"])
head.data.materials.clear()
head.data.materials.append(material)

for object_ in list(bpy.data.objects):
    if object_.type in {"CAMERA", "LIGHT"}:
        bpy.data.objects.remove(object_, do_unlink=True)
camera_data = bpy.data.cameras.new("TrackerCamera")
camera = bpy.data.objects.new("TrackerCamera", camera_data)
bpy.context.scene.collection.objects.link(camera)
camera.location = (0, -68, 172)
camera.rotation_euler = (Vector((0, 0, 172)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera_data.type = "PERSP"
camera_data.angle = radians(40)

scene = bpy.context.scene
scene.camera = camera
scene.world.color = (0.3, 0.3, 0.3)
scene.view_settings.view_transform = "Standard"
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 900
scene.render.resolution_y = 1100
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(OUTPUT)
bpy.ops.render.render(write_still=True)
print("TRACKER_PORTRAIT", OUTPUT)
