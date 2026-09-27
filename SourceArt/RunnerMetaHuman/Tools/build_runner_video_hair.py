"""Build a local, video-inspired shaggy hair mesh for Sam's fitted head.

The face texture came from the user's own 360-degree reference.  This script
only creates geometry and does not send images or likeness data to a service.
All coordinates below are centimetres in the existing FaceBuilder head space.
"""

from math import cos, pi, sin, sqrt
from pathlib import Path
import random

import bpy
from mathutils import Vector


ROOT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
RANDOM = random.Random(71023)


def mat(name, rgb, roughness=0.92):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*rgb, 1)
    material.use_nodes = True
    bsdf = material.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*rgb, 1)
    bsdf.inputs['Roughness'].default_value = roughness
    material.use_backface_culling = False
    return material


base = mat('RunnerSam_HairRoot', (.075, .050, .036))
colors = [
    mat('RunnerSam_HairBrown', (.105, .071, .050)),
    mat('RunnerSam_HairSoftBrown', (.132, .092, .063)),
    mat('RunnerSam_HairHighlight', (.175, .130, .090)),
]
skin = mat('RunnerSam_NeckSkin', (.47, .312, .265), .82)

vertices = []
faces = []
face_materials = []


def add_vertex(pos):
    vertices.append(tuple(pos))
    return len(vertices) - 1


def add_face(indices, material=0):
    faces.append(tuple(indices))
    face_materials.append(material)


def rim_height(phi):
    # In this head's coordinates -Y is the face, +Y the nape.
    toward_face = -sin(phi)
    if toward_face > 0:
        return 176.9 + 1.2 * toward_face
    return 176.9 + 8.0 * toward_face


def shell(phi, theta, lift=0.0):
    sr = sin(theta)
    return Vector((
        13.0 * sr * cos(phi),
        -1.1 + 10.0 * sr * sin(phi),
        rim_height(phi) * sr * sr + 196.2 * cos(theta) * cos(theta) + lift,
    ))


# A continuous scalp underlayer prevents the video-photo scalp from showing
# through in motion.  Its hem follows the low nape and higher front hairline.
ring_count = 23
around = 96
for ring in range(ring_count):
    theta = .018 + (pi / 2 - .018) * ring / (ring_count - 1)
    for point in range(around):
        phi = 2 * pi * point / around
        irregular = .25 * sin(7 * phi + .6) + .18 * sin(13 * phi - .3)
        add_vertex(shell(phi, theta, irregular * (ring / (ring_count - 1)) ** 2))
for ring in range(ring_count - 1):
    for point in range(around):
        nxt = (point + 1) % around
        add_face((ring * around + point, ring * around + nxt,
                  (ring + 1) * around + nxt, (ring + 1) * around + point))


def ribbon(points, widths, material):
    """A smoothly tapered, double-sided strand; no straight spike endpoints."""
    ids = []
    for index, point in enumerate(points):
        tangent = points[min(index + 1, len(points) - 1)] - points[max(index - 1, 0)]
        tangent.normalize()
        sideways = tangent.cross(Vector((0, 0, 1)))
        if sideways.length < .001:
            sideways = Vector((1, 0, 0))
        sideways.normalize()
        w = widths[index] / 2
        ids.append((add_vertex(point + sideways * w), add_vertex(point - sideways * w)))
    for index in range(len(ids) - 1):
        a, b = ids[index]
        c, d = ids[index + 1]
        add_face((a, b, d, c), material)
        add_face((c, d, b, a), material)


# Clumped top strands sweep toward the forehead and sides. Each starts just
# within the shell and its fine end follows the skull instead of floating.
for strand in range(1260):
    phi = RANDOM.uniform(-pi, pi)
    start = RANDOM.uniform(.10, 1.14)
    stop = min(pi / 2 + RANDOM.uniform(-.08, .24), start + RANDOM.uniform(.34, .82))
    sweep = RANDOM.uniform(-.25, .22) + .09 * cos(phi)
    rise = RANDOM.uniform(.25, 1.35)
    width = RANDOM.uniform(.22, .66)
    points = []
    widths = []
    for step in range(6):
        t = step / 5
        angle = phi + sweep * t + .025 * sin(2 * pi * t + strand)
        theta = start + (stop - start) * t
        p = shell(angle, theta, 1.0 + rise * sin(pi * t) + .2 * t)
        p.x += .25 * sin(t * pi + phi)
        points.append(p)
        widths.append(width * max(.04, (1 - t) ** .7))
    choice = RANDOM.choices((1, 2, 3), (.50, .38, .12))[0]
    ribbon(points, widths, choice)


# The forward, slightly broken fringe is the subject's most recognizable
# hair silhouette. Keep it clear of his eyes and vary length across the brow.
for strand in range(160):
    x = RANDOM.uniform(-10.9, 10.9)
    root = Vector((x * .72, -7.2 + RANDOM.uniform(-1.0, 1.0),
                   188.5 + RANDOM.uniform(-1.5, 3.0)))
    tip_z = RANDOM.uniform(175.3, 181.5)
    if abs(x) < 3.8:
        tip_z += 1.9
    tip = Vector((x + RANDOM.uniform(-1.8, 1.8), -12.0 + RANDOM.uniform(-1.8, .4), tip_z))
    mid = (root + tip) / 2
    mid.y -= RANDOM.uniform(.3, 1.4)
    mid.z += RANDOM.uniform(.8, 2.2)
    points = [root, root.lerp(mid, .5), mid, mid.lerp(tip, .5), tip]
    w = RANDOM.uniform(.35, 1.10)
    ribbon(points, [w, w * .93, w * .72, w * .36, .03],
           RANDOM.choices((1, 2, 3), (.55, .35, .10))[0])


mesh = bpy.data.meshes.new('RunnerSamVideoHairMesh')
mesh.from_pydata(vertices, [], faces)
mesh.update()
hair = bpy.data.objects.new('SM_Runner_SamVideoHair', mesh)
bpy.context.scene.collection.objects.link(hair)
for material in (base, *colors):
    mesh.materials.append(material)
for face, material in zip(mesh.polygons, face_materials):
    face.material_index = material
    face.use_smooth = True

# A short neck continuation intersects the approved jersey collar. Its top is
# sunk into the fitted photo head; it is not a replacement torso or shoulders.
neck_vertices = []
neck_faces = []
radial = 32
for z, rx, ry, yoff in ((146.0, 8.9, 7.0, -1.0),
                        (152.0, 8.4, 6.8, -1.1),
                        (158.0, 8.5, 7.2, -1.3),
                        (164.0, 8.8, 7.0, -1.2)):
    for j in range(radial):
        phi = 2 * pi * j / radial
        neck_vertices.append((rx * cos(phi), yoff + ry * sin(phi), z))
for ring in range(3):
    for j in range(radial):
        nxt = (j + 1) % radial
        neck_faces.append((ring * radial + j, ring * radial + nxt,
                           (ring + 1) * radial + nxt, (ring + 1) * radial + j))
neck_mesh = bpy.data.meshes.new('RunnerSamVideoNeckMesh')
neck_mesh.from_pydata(neck_vertices, [], neck_faces)
neck_mesh.materials.append(skin)
neck_mesh.update()
for face in neck_mesh.polygons:
    face.use_smooth = True
neck = bpy.data.objects.new('SM_Runner_SamVideoNeck', neck_mesh)
bpy.context.scene.collection.objects.link(neck)

# Show hair over the original local fitted photo head in the source .blend.
for obj in bpy.data.objects:
    if obj.type == 'MESH' and obj.name.startswith('Runner_SamVideo_FaceBuilderHead'):
        obj.hide_render = True

for obj in (hair, neck):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(ROOT / f'{obj.name}.fbx'),
        use_selection=True,
        object_types={'MESH'},
        bake_anim=False,
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Y',
        axis_up='Z',
        mesh_smooth_type='FACE',
    )

bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / 'Runner_SamVideo_HairStudy.blend'))
print('RUNNER_SAM_HAIR', len(mesh.vertices), len(mesh.polygons),
      'NECK', len(neck_mesh.vertices), len(neck_mesh.polygons))
