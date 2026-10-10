"""Remove only original static leaf blockers from selected house collision chunks.

Run with Blender in background mode. Baseline FBXs are exported from the
unmodified Unreal COL_* static meshes and live next to this script in Baselines.
The output keeps every other connected component and face.
"""

import bpy
import bmesh
import collections
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SPECS = [
    {
        "source": "COL_Upper_m1_p1_Solid",
        "output": "COL_Upper_m1_p1_DoorClear",
        "remove": [
            # The two original static proxies for room leaves 04 and 05.
            ((11.9537058, 1.2813451, 3.1353173),
             (11.9917059, 2.1413453, 5.1923175)),
            ((10.4397068, 1.2363452, 3.1287968),
             (10.4777060, 2.0863452, 5.1857972)),
        ],
    },
    {
        "source": "COL_Basement_m1_p1_Solid",
        "output": "COL_Basement_m1_p1_DoorClear",
        "remove": [
            # Original static proxy for the Black Hall service alcove leaf.
            ((8.8024988, 2.1750002, -3.2905998),
             (8.8495007, 3.0150003, -1.2306001)),
        ],
    },
    {
        "source": "COL_Basement_p0_m1_Solid",
        "output": "COL_Basement_p0_m1_DoorClear",
        "remove": [
            # Unframed alcove slab that would otherwise be an invisible blocker.
            ((-6.9039831, -0.6746809, -3.2905998),
             (-5.9680228, -0.4755605, -1.2106001)),
        ],
    },
]


def load_mesh(path):
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(path))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    assert len(meshes) == 1, (path, len(meshes))
    return meshes[0]


def connected_components(obj):
    mesh = obj.data
    parent = list(range(len(mesh.vertices)))

    def find(index):
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    for face in mesh.polygons:
        for index in face.vertices[1:]:
            parent[find(index)] = find(face.vertices[0])

    groups = collections.defaultdict(list)
    for face in mesh.polygons:
        groups[find(face.vertices[0])].append(face.index)

    result = []
    for faces in groups.values():
        vertices = {index for face_id in faces
                    for index in mesh.polygons[face_id].vertices}
        points = [obj.matrix_world @ mesh.vertices[index].co
                  for index in vertices]
        lo = tuple(min(p[axis] for p in points) for axis in range(3))
        hi = tuple(max(p[axis] for p in points) for axis in range(3))
        result.append((faces, vertices, lo, hi))
    return result


def face_signatures(obj):
    """World-space face geometry, independent of FBX vertex ordering."""
    mesh = obj.data
    points = [obj.matrix_world @ v.co for v in mesh.vertices]
    signatures = collections.Counter()
    for face in mesh.polygons:
        corners = tuple(sorted(tuple(round(points[i][axis], 4)
                                     for axis in range(3))
                               for i in face.vertices))
        signatures[corners] += 1
    return signatures


for spec in SPECS:
    original = ROOT / "Baselines" / (spec["source"] + ".fbx")
    output = ROOT / (spec["output"] + ".fbx")
    assert original.is_file(), original
    obj = load_mesh(original)
    mesh = obj.data
    groups = connected_components(obj)
    selected_faces = set()
    selected_vertices = set()
    for expected_lo, expected_hi in spec["remove"]:
        matches = [group for group in groups
                   if all(abs(group[2][axis] - expected_lo[axis]) < .0002 and
                          abs(group[3][axis] - expected_hi[axis]) < .0002
                          for axis in range(3))]
        assert len(matches) == 1, (spec["source"], expected_lo, len(matches))
        faces, vertices, _, _ = matches[0]
        assert len(faces) == 12, (spec["source"], len(faces))
        selected_faces.update(faces)
        selected_vertices.update(vertices)

    before_faces = len(mesh.polygons)
    before_vertices = len(mesh.vertices)
    kept_signatures = face_signatures(obj)
    for face_id in selected_faces:
        face = mesh.polygons[face_id]
        points = [obj.matrix_world @ mesh.vertices[i].co for i in face.vertices]
        corners = tuple(sorted(tuple(round(p[axis], 4) for axis in range(3))
                               for p in points))
        kept_signatures[corners] -= 1
        if kept_signatures[corners] == 0:
            del kept_signatures[corners]

    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm,
                     geom=[bm.verts[index] for index in selected_vertices],
                     context="VERTS")
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    assert len(mesh.polygons) == before_faces - len(selected_faces)
    assert len(mesh.vertices) == before_vertices - len(selected_vertices)
    assert face_signatures(obj) == kept_signatures

    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(output), use_selection=True,
                             apply_unit_scale=True, axis_forward="-Z",
                             axis_up="Y", bake_space_transform=False)

    reimported = load_mesh(output)
    exported_signatures = face_signatures(reimported)
    assert exported_signatures == kept_signatures, spec["output"]
    print("VERIFIED_DOOR_CLEAR", spec["output"],
          "removed_components", len(spec["remove"]),
          "removed_faces", len(selected_faces),
          "removed_vertices", len(selected_vertices),
          "kept_faces", len(reimported.data.polygons), flush=True)
