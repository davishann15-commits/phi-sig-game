"""Cover the rear projection seam beneath Sam's bound hairstyle."""

from math import cos, pi, sin, sqrt
from pathlib import Path
import random

import bpy
from mathutils import Vector


root = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
random_ = random.Random(31996)
vertices = []
faces = []


def vertex(p):
    vertices.append(tuple(p))
    return len(vertices) - 1


def face(items):
    faces.append(tuple(items))


def y_at(x, z):
    width = 14.0 - .08 * abs(z - 171.0)
    depth = sqrt(max(0.0, 1.0 - (x / max(1.0, width)) ** 2))
    return (9.4 if z < 169 else 10.5) * depth + 1.0


# Rounded, irregular taper behind the ears. The video shows the nape tapering
# toward the neck, not a rigid straight helmet edge.
rows = 14
cols = 54
for row in range(rows):
    t = row / (rows - 1)
    for col in range(cols):
        a = col / (cols - 1) * 2 - 1
        z = 158.8 + 4.2 * abs(a) + t * (22.0 - 3.0 * abs(a))
        x = (13.0 - 1.0 * abs(a) * (1 - t)) * a
        y = y_at(x, z) + .8
        vertex((x, y, z))
for row in range(rows - 1):
    for col in range(cols - 1):
        a = row * cols + col
        b = a + 1
        c = (row + 1) * cols + col + 1
        d = c - 1
        face((a, b, c, d))


for strand in range(620):
    x = random_.uniform(-12.2, 12.2)
    root_z = random_.uniform(171, 183)
    end_z = random_.uniform(158.5 + 4 * abs(x) / 12.2, 169)
    if end_z >= root_z - 2:
        end_z = root_z - 3
    bend = random_.uniform(-1.2, 1.2)
    width = random_.uniform(.14, .41)
    previous = None
    for step in range(5):
        t = step / 4
        z = root_z + (end_z - root_z) * t
        xx = x + bend * t
        y = y_at(xx, z) + 1.1 + .4 * sin(pi * t)
        w = width * max(.05, (1 - t) ** .7)
        one = vertex((xx - w, y, z))
        two = vertex((xx + w, y, z))
        if previous:
            face((*previous, two, one))
        previous = (one, two)


mesh = bpy.data.meshes.new('RunnerSamVideoNapeMesh')
mesh.from_pydata(vertices, [], faces)
mesh.update()
for poly in mesh.polygons:
    poly.use_smooth = True
material = bpy.data.materials.new('RunnerSamVideoNapeBrown')
material.diffuse_color = (.08, .053, .039, 1)
mesh.materials.append(material)
obj = bpy.data.objects.new('SM_Runner_SamVideoNape', mesh)
bpy.context.scene.collection.objects.link(obj)
bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(
    filepath=str(root / 'SM_Runner_SamVideoNape.fbx'),
    use_selection=True,
    object_types={'MESH'},
    bake_anim=False,
    apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_ALL',
    axis_forward='-Y',
    axis_up='Z',
    mesh_smooth_type='FACE',
)
print('RUNNER_SAM_NAPE', len(mesh.vertices), len(mesh.polygons))
