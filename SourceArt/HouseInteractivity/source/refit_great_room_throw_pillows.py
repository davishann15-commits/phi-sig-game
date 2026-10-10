"""Lay the four great-room pillows on their seats with soft inset collision."""

import bmesh
import bpy
import json
import math
from pathlib import Path

from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = json.loads((ROOT / "extraction-manifest.json").read_text())
PIECES = {row["id"]: row for row in MANIFEST["pieces"]}
DEST = ROOT / "GreatRoomCushionFix"
ORIGINALS = Path("/Users/Stewart/Projects/Senior sendoff/outputs/house-interactivity/models")
IDS = tuple(f"SM_GR3_NearSeating__{number:03d}" for number in (4, 5, 6, 7))
LAYOUT = {
    4: ("SM_GR3_NearSeating__003", -9),
    5: ("SM_GR3_NearSeating__003", 14),
    6: ("SM_GR3_NearSeating__001", 12),
    7: ("SM_GR3_NearSeating__001", -9),
}


def blender_origin(row):
    x, y, z = row["local_origin_cm"]
    return Vector((x / 100, -y / 100, z / 100))


def seat_surface(sofa):
    source = DEST / (sofa["asset_name"] + ".fbx")
    bpy.ops.import_scene.fbx(filepath=str(source))
    couch = next(obj for obj in bpy.context.selected_objects
                 if obj.type == "MESH" and not obj.name.startswith("UCX_"))
    origin = blender_origin(sofa)
    points = [couch.matrix_world @ vertex.co + origin for vertex in couch.data.vertices]
    faces = []
    for polygon in couch.data.polygons:
        corners = [points[index] for index in polygon.vertices]
        if max(point.z for point in corners) > .72 or min(point.z for point in corners) < .38:
            continue
        normal = (corners[1] - corners[0]).cross(corners[2] - corners[0])
        if normal.length < 1e-7 or abs(normal.normalized().z) < .65:
            continue
        faces.append(tuple(polygon.vertices))
    if len(faces) < 30:
        raise RuntimeError("Could not isolate sofa seat surfaces: " + sofa["id"])
    return BVHTree.FromPolygons(points, faces)


def surface_z(bvh, x, y):
    hit, _, _, _ = bvh.ray_cast(Vector((x, y, 1.5)), Vector((0, 0, -1)), 1.5)
    return hit.z if hit is not None else None


def fit_damage_stage(row, field, rotation, displacement):
    source = Path(row[field])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if len(meshes) < 2 or not any(obj.name.startswith("UCX_") for obj in meshes):
        raise RuntimeError("Damage stage lacks its visual/collision parts: " + str(source))
    for obj in meshes:
        inverse = obj.matrix_world.inverted()
        for vertex in obj.data.vertices:
            vertex.co = inverse @ (rotation @ (obj.matrix_world @ vertex.co) + displacement)
        obj.data.update()
    output = DEST / source.name
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = next(obj for obj in meshes if not obj.name.startswith("UCX_"))
    bpy.ops.export_scene.fbx(filepath=str(output), use_selection=True, apply_unit_scale=True,
                             axis_forward="-Z", axis_up="Y", bake_space_transform=False,
                             add_leaf_bones=False)
    print("GREAT_ROOM_PILLOW_STAGE=" + json.dumps({
        "id": row["id"], "stage": field, "output": str(output),
        "mesh_parts": len(meshes),
    }), flush=True)


def refit(row):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    number = int(row["suffix"])
    sofa_id, yaw = LAYOUT[number]
    seat = seat_surface(PIECES[sofa_id])
    bpy.ops.import_scene.fbx(filepath=str(ORIGINALS / (row["asset_name"] + ".fbx")))
    visible = next(obj for obj in bpy.context.selected_objects
                   if obj.type == "MESH" and not obj.name.startswith("UCX_"))
    for obj in list(bpy.context.scene.objects):
        if obj.type == "MESH" and obj.name.startswith("UCX_"):
            bpy.data.objects.remove(obj, do_unlink=True)

    # The layout generated these pillows at an 85-degree tilt. Turn that
    # vertical plane down to an eight-degree rest, keeping the original yaw.
    axis = Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0))
    rotation = Matrix.Rotation(math.radians(8 - 85), 3, axis)
    inverse = visible.matrix_world.inverted()
    # The near green pillow originally intersected the left arm. Nudge its
    # soft body inward before fitting it to the seat rather than lifting the
    # entire pillow above the armrest.
    center_shift = Vector((.13, 0, 0)) if number == 4 else Vector()
    for vertex in visible.data.vertices:
        vertex.co = inverse @ (rotation @ (visible.matrix_world @ vertex.co) + center_shift)
    visible.data.update()

    origin = blender_origin(row)
    world_points = [visible.matrix_world @ vertex.co + origin for vertex in visible.data.vertices]
    lowest = min(point.z for point in world_points)
    clearance = []
    for point in world_points:
        if point.z > lowest + .09:
            continue
        top = surface_z(seat, point.x, point.y)
        if top is not None:
            clearance.append(point.z - top)
    if len(clearance) < 20:
        raise RuntimeError("Not enough pillow-to-seat contact samples: " + row["id"])
    shift_z = .008 - min(clearance)
    for vertex in visible.data.vertices:
        vertex.co = inverse @ (visible.matrix_world @ vertex.co + Vector((0, 0, shift_z)))
    visible.data.update()

    points = [vertex.co.copy() for vertex in visible.data.vertices]
    low = Vector(min(p[axis] for p in points) for axis in range(3))
    high = Vector(max(p[axis] for p in points) for axis in range(3))
    center = (low + high) * .5
    # Most of the pillow is soft cloth and foam, not hard collision. The
    # visible cover may compress into the seat/back cushion on activation.
    inset = Vector((.72, .72, .70))
    directions = [Vector((x, y, z)).normalized() for x in (-1, 0, 1)
                  for y in (-1, 0, 1) for z in (-1, 0, 1) if x or y or z]
    chosen = {max(range(len(points)), key=lambda index: points[index].dot(direction))
              for direction in directions}
    bm = bmesh.new()
    verts = [bm.verts.new(center + (points[index] - center) * inset) for index in chosen]
    result = bmesh.ops.convex_hull(bm, input=verts, use_existing_faces=False)
    unused = result.get("geom_interior", []) + result.get("geom_unused", [])
    if unused:
        bmesh.ops.delete(bm, geom=[v for v in unused if isinstance(v, bmesh.types.BMVert)], context="VERTS")
    if len(bm.faces) < 4:
        raise RuntimeError("Pillow collision core could not be made: " + row["id"])
    hull_mesh = bpy.data.meshes.new(row["asset_name"] + "_collision")
    bm.to_mesh(hull_mesh)
    bm.free()
    hull = bpy.data.objects.new("UCX_" + row["asset_name"] + "_00", hull_mesh)
    bpy.context.collection.objects.link(hull)
    hull.matrix_world = visible.matrix_world.copy()

    DEST.mkdir(parents=True, exist_ok=True)
    output = DEST / (row["asset_name"] + ".fbx")
    bpy.ops.object.select_all(action="DESELECT")
    visible.select_set(True)
    hull.select_set(True)
    bpy.context.view_layer.objects.active = visible
    bpy.ops.export_scene.fbx(filepath=str(output), use_selection=True, apply_unit_scale=True,
                             axis_forward="-Z", axis_up="Y", bake_space_transform=False,
                             add_leaf_bones=False)
    print("GREAT_ROOM_PILLOW_CORE=" + json.dumps({
        "id": row["id"], "output": str(output), "visible_vertices": len(points),
        "collision_vertices": len(hull_mesh.vertices),
        "rest_angle_degrees": 8, "seat_samples": len(clearance),
        "lowest_seat_gap_m": round(min(clearance) + shift_z, 4),
        "shift_z_m": round(shift_z, 4),
        "visible_world_height_m": round(high.z - low.z, 4),
        "visible_world_bounds_m": [
            [round(low[index] + origin[index], 4) for index in range(3)],
            [round(high[index] + origin[index], 4) for index in range(3)],
        ],
    }), flush=True)
    for field in ("damaged_fbx", "heavily_damaged_fbx", "broken_fbx"):
        fit_damage_stage(row, field, rotation, center_shift + Vector((0, 0, shift_z)))


for identifier in IDS:
    refit(PIECES[identifier])
