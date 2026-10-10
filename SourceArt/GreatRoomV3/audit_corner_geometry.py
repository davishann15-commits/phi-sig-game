"""List compact white architectural mesh islands crossing the great-room wall."""

import bpy
from collections import defaultdict
from pathlib import Path
from mathutils import Vector
import math


root = Path(__file__).resolve().parents[1]
paths = [
    root / "ShelfFireplaceRefinement/exports/SM_Upper_p0_m1_Solid.fbx",
    root / "ShelfFireplaceRefinement/exports/SM_Upper_p0_p0_Solid.fbx",
    root / "ShelfFireplaceRefinement/models/SM_FPR_Retained_SM_Entrance_p0_m1_Solid.fbx",
    root / "ShelfFireplaceRefinement/models/SM_FPR_Retained_SM_Entrance_p0_p0_Solid.fbx",
]


def to_blender(point):
    return Vector((point[0] / 1.8, -point[1] / 1.8, point[2]))


def ray(view_position, view_target, pixel):
    camera = Vector(view_position)
    target = Vector(view_target)
    forward = (target - camera).normalized()
    # Unreal's camera-right axis is +Y when forward is +X.
    right = Vector((0, 0, 1)).cross(forward).normalized()
    up = forward.cross(right).normalized()
    vertical_tangent = math.tan(math.radians(75 / 2))
    horizontal_tangent = vertical_tangent * 1280 / 720
    x = (pixel[0] / 640 - 1) * horizontal_tangent
    y = (1 - pixel[1] / 360) * vertical_tangent
    direction = (forward + right * x + up * y).normalized()
    return to_blender(camera), to_blender(camera + direction) - to_blender(camera)


def components(mesh):
    parent = list(range(len(mesh.vertices)))

    def find(value):
        while parent[value] != value:
            parent[value] = parent[parent[value]]
            value = parent[value]
        return value

    for edge in mesh.edges:
        left, right = edge.vertices
        a, b = find(left), find(right)
        if a != b:
            parent[a] = b
    groups = defaultdict(list)
    for index, vertex in enumerate(mesh.vertices):
        groups[find(index)].append(vertex.co)
    return groups.values()


def describe_face_island(obj, face_index):
    mesh = obj.data
    incident = defaultdict(set)
    for edge in mesh.edges:
        left, right = edge.vertices
        incident[left].add(right)
        incident[right].add(left)
    found = set(mesh.polygons[face_index].vertices)
    pending = list(found)
    while pending:
        vertex = pending.pop()
        for neighbor in incident[vertex]:
            if neighbor not in found:
                found.add(neighbor)
                pending.append(neighbor)
    points = [obj.matrix_world @ mesh.vertices[index].co for index in found]
    low = tuple(round(min(point[axis] for point in points), 4) for axis in range(3))
    high = tuple(round(max(point[axis] for point in points), 4) for axis in range(3))
    faces = [polygon for polygon in mesh.polygons
             if any(index in found for index in polygon.vertices)]
    materials = {obj.material_slots[polygon.material_index].name for polygon in faces}
    print("FACE_ISLAND", obj.name, face_index, "verts", len(found),
          "faces", len(faces), "bounds", low, high,
          "materials", sorted(materials), flush=True)


casts = [
    ("east_near_center", (7, 1, 1.65), (13, 6, 3.3), (640, 360)),
    ("east_near", (7, 1, 1.65), (13, 6, 3.3), (707, 312)),
    ("east_far_center", (8.5, -2.5, 1.65), (13, 7.5, 3.4), (640, 360)),
    ("east_far", (8.5, -2.5, 1.65), (13, 7.5, 3.4), (654, 335)),
    ("east_around_center", (11, 2, 1.65), (13, 9, 3.4), (640, 360)),
    ("east_around", (11, 2, 1.65), (13, 9, 3.4), (632, 268)),
]
for px in (620, 650, 680, 695, 705, 715, 725, 735, 745, 760):
    casts.append((f"east_scan_{px}", (7, 1, 1.65), (13, 6, 3.3), (px, 310)))
hits = defaultdict(list)

for path in paths:
    if not path.is_file():
        print("MISSING", path, flush=True)
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path))
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH" or obj.name.startswith("UCX_"):
            continue
        if path.name == "SM_Upper_p0_p0_Solid.fbx":
            describe_face_island(obj, 16949)
            describe_face_island(obj, 19205)
            describe_face_island(obj, 19193)
            describe_face_island(obj, 16409)
        print("MESH", path.name, obj.name, "verts", len(obj.data.vertices),
              "materials", [slot.name for slot in obj.material_slots],
              "scale", tuple(round(value, 3) for value in obj.matrix_world.to_scale()),
              "bounds", [tuple(round(value, 3) for value in corner)
                         for corner in (min((obj.matrix_world @ v.co for v in obj.data.vertices),
                                            key=lambda v: v.x),
                                        max((obj.matrix_world @ v.co for v in obj.data.vertices),
                                            key=lambda v: v.x))], flush=True)
        for points in components(obj.data):
            if len(points) < 12:
                continue
            world = [obj.matrix_world @ point for point in points]
            low = [min(point[index] for point in world) for index in range(3)]
            high = [max(point[index] for point in world) for index in range(3)]
            width = high[0] - low[0]
            depth = high[1] - low[1]
            height = high[2] - low[2]
            if height > 1.1 and width < 2.0 and depth < 2.0:
                print("ISLAND", path.name, obj.name, len(points),
                      tuple(round(v, 3) for v in low),
                      tuple(round(v, 3) for v in high), flush=True)
        for label, view_position, view_target, pixel in casts:
            origin, direction = ray(view_position, view_target, pixel)
            transform = obj.matrix_world.inverted()
            local_origin = transform @ origin
            local_direction = (transform.to_3x3() @ direction).normalized()
            found, local_hit, normal, face_index = obj.ray_cast(
                local_origin, local_direction, distance=5000)
            if found:
                location = obj.matrix_world @ local_hit
                polygon = obj.data.polygons[face_index]
                material = obj.material_slots[polygon.material_index].name
                hits[label].append((round((location - origin).length, 3), path.name,
                                    tuple(round(value, 3) for value in location), material,
                                    face_index))

for label, rows in hits.items():
    for row in sorted(rows):
        print("RAY_HIT", label, row, flush=True)
