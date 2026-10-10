"""Build the striped-room door collision variant from the baseline mesh.

Run in Blender background mode. Removes only the connected source-pose door
proxy. All other collision faces, including the wall and floor, stay exact.
"""

import bpy
import bmesh
import collections
from pathlib import Path

root = Path(__file__).resolve().parent
source = root / 'Baselines' / 'COL_Upper_m1_m2_Solid.fbx'
output = root / 'COL_Upper_m1_m2_DoorClear.fbx'
expected_lo = (-13.1806345, 0.5034193, 3.0274539)
expected_hi = (-13.0573664, 1.4830022, 5.0844541)


def load(path):
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(path))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
    assert len(meshes) == 1, (path, len(meshes))
    return meshes[0]


def signatures(obj):
    mesh = obj.data
    points = [obj.matrix_world @ vertex.co for vertex in mesh.vertices]
    return collections.Counter(
        tuple(sorted(tuple(round(points[index][axis], 3)
                           for axis in range(3))
                     for index in face.vertices))
        for face in mesh.polygons)


assert source.is_file(), source
obj = load(source)
mesh = obj.data
assert len(mesh.polygons) == 460, len(mesh.polygons)
parent = list(range(len(mesh.vertices)))


def find(index):
    while parent[index] != index:
        parent[index] = parent[parent[index]]
        index = parent[index]
    return index


for face in mesh.polygons:
    for index in face.vertices[1:]:
        parent[find(index)] = find(face.vertices[0])

parts = collections.defaultdict(list)
for face in mesh.polygons:
    parts[find(face.vertices[0])].append(face.index)
assert len(parts) == 20, len(parts)

target = []
for faces in parts.values():
    vertices = {vertex for face_index in faces
                for vertex in mesh.polygons[face_index].vertices}
    points = [obj.matrix_world @ mesh.vertices[index].co
              for index in vertices]
    lo = tuple(min(point[axis] for point in points) for axis in range(3))
    hi = tuple(max(point[axis] for point in points) for axis in range(3))
    if all(abs(lo[axis] - expected_lo[axis]) < .0002 and
           abs(hi[axis] - expected_hi[axis]) < .0002
           for axis in range(3)):
        target.append((faces, vertices))
assert len(target) == 1, len(target)
removed_faces, removed_vertices = target[0]
assert len(removed_faces) == 12, len(removed_faces)
assert len(removed_vertices) == 8, len(removed_vertices)

kept = signatures(obj)
all_points = [obj.matrix_world @ vertex.co for vertex in mesh.vertices]
for face_index in removed_faces:
    face = mesh.polygons[face_index]
    signature = tuple(sorted(tuple(round(all_points[index][axis], 3)
                                   for axis in range(3))
                             for index in face.vertices))
    kept[signature] -= 1
    if kept[signature] == 0:
        del kept[signature]

before_vertices = len(mesh.vertices)
bm = bmesh.new()
bm.from_mesh(mesh)
bm.verts.ensure_lookup_table()
bmesh.ops.delete(bm, geom=[bm.verts[index] for index in removed_vertices],
                 context='VERTS')
bm.to_mesh(mesh)
bm.free()
mesh.update()
assert len(mesh.polygons) == 448
assert len(mesh.vertices) == before_vertices - 8
assert signatures(obj) == kept

bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(filepath=str(output), use_selection=True,
                         apply_unit_scale=True, axis_forward='-Z',
                         axis_up='Y', bake_space_transform=False)

reimported = load(output)
assert signatures(reimported) == kept, output
kept_points = [point for index, point in enumerate(all_points)
               if index not in removed_vertices]
exported_points = [reimported.matrix_world @ vertex.co
                   for vertex in reimported.data.vertices]
assert len(exported_points) == len(kept_points)
max_vertex_delta = max(min((point - original).length
                           for original in kept_points)
                       for point in exported_points)
assert max_vertex_delta < .00002, max_vertex_delta
print('VERIFIED_SOUTHSTRIPED_DOOR_CLEAR', output,
      'removed_faces', len(removed_faces),
      'removed_vertices', len(removed_vertices),
      'kept_faces', len(reimported.data.polygons),
      'max_vertex_delta_m', max_vertex_delta, flush=True)
