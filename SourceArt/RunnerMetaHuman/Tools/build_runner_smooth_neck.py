"""Build a gently flared collarbone insert below Runner's fitted head."""

from math import cos, pi, sin
from pathlib import Path
import bpy


root = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
name = 'SM_Runner_SamVideoCollarBlend'
radial = 64
# Preserve the existing fit directly under the jaw. Below that, widen quickly
# into a shallow chest/collarbone surface instead of making a longer tube. The
# lower rings sit inside the existing jersey and cover its hollow dark opening.
rings = (
    (122.0, 20.0, 11.5, -2.3),
    (128.0, 18.0, 11.0, -2.2),
    (134.0, 16.0, 10.3, -2.0),
    (139.0, 13.2, 8.8, -1.6),
    (143.0, 10.5, 7.6, -1.2),
    (146.0, 8.9, 7.0, -1.0),
    (152.0, 8.4, 6.8, -1.1),
    (158.0, 8.5, 7.2, -1.3),
    (164.0, 8.8, 7.0, -1.2),
)
vertices = []
faces = []
for z, rx, ry, yoff in rings:
    for j in range(radial):
        phi = 2 * pi * j / radial
        vertices.append((rx * cos(phi), yoff + ry * sin(phi), z))
for row in range(len(rings) - 1):
    for j in range(radial):
        following = (j + 1) % radial
        faces.append((row * radial + j, row * radial + following,
                      (row + 1) * radial + following,
                      (row + 1) * radial + j))
mesh = bpy.data.meshes.new(name + 'Mesh')
mesh.from_pydata(vertices, [], faces)
mesh.update()
for polygon in mesh.polygons:
    polygon.use_smooth = True
obj = bpy.data.objects.new(name, mesh)
bpy.context.scene.collection.objects.link(obj)
bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(
    filepath=str(root / (name + '.fbx')),
    use_selection=True, object_types={'MESH'}, bake_anim=False,
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
    axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
print('RUNNER_SMOOTH_NECK_EXPORTED', len(mesh.vertices), len(mesh.polygons))
