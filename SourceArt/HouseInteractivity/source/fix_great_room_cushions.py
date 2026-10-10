"""Fit the great-room sofa cushions and keep their backrests aligned on hits.

Only the five sofa meshes are changed. Their loose throw pillows remain separate
actors. Intact collision is copied from the corrected first-damage hulls so
the playable body has individual support for pads, frame, arms, and seat.
"""

import bpy
import bmesh
import json
import math
import sys
from pathlib import Path

from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from extract_interactivity import components  # noqa: E402


ROOT = Path("/Users/Stewart/Projects/Senior sendoff/outputs/house-interactivity/models")
DEST = Path(__file__).resolve().parents[1] / "GreatRoomCushionFix"
SOFAS = {
    "HI_SM_GR3_NearSeating_001": 5,
    "HI_SM_GR3_NearSeating_003": 3,
    "HI_SM_GR3_FarSeating_001": 5,
    "HI_SM_GR3_FarSeating_002": 3,
    "HI_SM_GR3_FarSeating_003": 3,
}


def bounds(obj):
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector(min(v[axis] for v in points) for axis in range(3))
    hi = Vector(max(v[axis] for v in points) for axis in range(3))
    return lo, hi


def hull_bounds(obj):
    points = [obj.matrix_world @ Vector(v) for v in obj.bound_box]
    lo = Vector(min(v[axis] for v in points) for axis in range(3))
    hi = Vector(max(v[axis] for v in points) for axis in range(3))
    return lo, hi


def is_back_pad(piece):
    size = piece["size"]
    return (len(piece["faces"]) >= 650 and 0.55 < size.z < 0.95
            and 0.8 < max(size.x, size.y) < 2.0
            and "Leather" in " ".join(piece["materials"]))


def is_seat_pad(piece):
    size = piece["size"]
    return (400 <= len(piece["faces"]) <= 500 and 0.15 < size.z < 0.26
            and min(size.x, size.y) > 0.75 and "Leather" in " ".join(piece["materials"]))


def is_back_seam(piece, pads):
    if "Stitch" not in " ".join(piece["materials"]) or piece["size"].z < 0.45:
        return False
    return any((piece["center"] - pad["center"]).length < 0.48 for pad in pads)


def is_back_frame(piece):
    size = piece["size"]
    return (size.z > 0.7 and max(size.x, size.y) > 2.5
            and len(piece["faces"]) >= 300
            and "Leather" in " ".join(piece["materials"]))


def transformed_bounds(obj, piece, pivot, axis, angle):
    rotation = Matrix.Rotation(angle, 3, axis)
    ids = {int(index) for face in piece["faces"] for index in obj.data.polygons[face].vertices}
    points = [rotation @ (obj.matrix_world @ obj.data.vertices[index].co - pivot) + pivot for index in ids]
    return (Vector(min(v[k] for v in points) for k in range(3)),
            Vector(max(v[k] for v in points) for k in range(3)))


def transform_component(obj, piece, rotation=None, pivot=None, delta=None):
    ids = {int(index) for face in piece["faces"] for index in obj.data.polygons[face].vertices}
    inverse = obj.matrix_world.inverted()
    for index in ids:
        world = obj.matrix_world @ obj.data.vertices[index].co
        if rotation is not None:
            world = rotation @ (world - pivot) + pivot
        if delta is not None:
            world += delta
        obj.data.vertices[index].co = inverse @ world
    obj.data.update()


def transform_hull(obj, rotation=None, pivot=None, delta=None):
    inverse = obj.matrix_world.inverted()
    for vertex in obj.data.vertices:
        world = obj.matrix_world @ vertex.co
        if rotation is not None:
            world = rotation @ (world - pivot) + pivot
        if delta is not None:
            world += delta
        vertex.co = inverse @ world
    obj.data.update()


def baseline_frame(source):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    mesh = next(obj for obj in bpy.context.scene.objects if obj.type == "MESH" and not obj.name.startswith("UCX_"))
    matches = [piece for piece in components(mesh) if is_back_frame(piece)]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one original sofa back frame in {source}, got {len(matches)}")
    frame = matches[0]
    return frame["lo"].copy(), frame["hi"].copy(), frame["center"].copy(), frame["size"].copy()


def collision_piece(piece):
    size = piece["size"]
    if len(piece["faces"]) >= 280 and size.z > 0.12 and max(size.x, size.y) > 0.24:
        return True
    return ("Walnut" in " ".join(piece["materials"]) and len(piece["faces"]) >= 12
            and size.z > 0.05 and max(size.x, size.y) > 0.1)


def make_hull(mesh, piece, name):
    ids = {int(index) for face in piece["faces"] for index in mesh.data.polygons[face].vertices}
    points = [mesh.data.vertices[index].co.copy() for index in ids]
    directions = [Vector((x, y, z)).normalized() for x in (-1, 0, 1)
                  for y in (-1, 0, 1) for z in (-1, 0, 1) if x or y or z]
    chosen = {max(range(len(points)), key=lambda i: points[i].dot(direction)) for direction in directions}
    bm = bmesh.new()
    vertices = [bm.verts.new(points[index]) for index in chosen]
    result = bmesh.ops.convex_hull(bm, input=vertices, use_existing_faces=False)
    interior = result.get("geom_interior", []) + result.get("geom_unused", [])
    if interior:
        bmesh.ops.delete(bm, geom=[v for v in interior if isinstance(v, bmesh.types.BMVert)], context="VERTS")
    if len(bm.faces) < 4:
        bm.free()
        raise RuntimeError("Cannot build fitted sofa collision hull: " + name)
    geometry = bpy.data.meshes.new(name)
    bm.to_mesh(geometry)
    bm.free()
    geometry.update()
    hull = bpy.data.objects.new(name, geometry)
    bpy.context.collection.objects.link(hull)
    hull.matrix_world = mesh.matrix_world.copy()
    return hull


def fix(source, stem, count, frame_reference):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    mesh = next(obj for obj in bpy.context.scene.objects if obj.type == "MESH" and not obj.name.startswith("UCX_"))
    pieces = components(mesh)
    pads = [piece for piece in pieces if is_back_pad(piece)]
    seats = [piece for piece in pieces if is_seat_pad(piece)]
    seams = [piece for piece in pieces if is_back_seam(piece, pads)]
    if len(pads) != count or len(seats) != count or len(seams) != count:
        raise RuntimeError(f"Unexpected sofa topology {source}: back={len(pads)} seat={len(seats)} seams={len(seams)}")
    seat_center = sum((piece["center"] for piece in seats), Vector()) / len(seats)
    back_center = sum((piece["center"] for piece in pads), Vector()) / len(pads)
    toward_seat = seat_center - back_center
    toward_seat.z = 0.0
    toward_seat.normalize()
    if toward_seat.length < 0.99:
        raise RuntimeError("Could not resolve sofa facing direction: " + stem)
    cushion_delta = toward_seat * 0.055 + Vector((0, 0, -0.035))
    imported_hulls = [obj for obj in bpy.context.scene.objects if obj.type == "MESH" and obj.name.startswith("UCX_")]

    # The old heavy mesh bent just the back frame by seven degrees, while its
    # cushions stayed in place. Straighten that frame before fitting the pads.
    straightened = False
    if stem.endswith("_HeavyV2"):
        frames = [piece for piece in pieces if is_back_frame(piece)]
        if len(frames) != 1:
            raise RuntimeError("Missing heavy sofa back frame: " + stem)
        frame = frames[0]
        ref_lo, ref_hi, ref_center, ref_size = frame_reference
        pivot = Vector((ref_center.x, ref_center.y, ref_lo.z))
        axis = "X" if ref_size.y < ref_size.x else "Y"
        options = []
        for degrees in (-7, 7):
            lo, hi = transformed_bounds(mesh, frame, pivot, axis, math.radians(degrees))
            error = (lo - ref_lo).length_squared + (hi - ref_hi).length_squared
            options.append((error, degrees))
        original_error = (frame["lo"] - ref_lo).length_squared + (frame["hi"] - ref_hi).length_squared
        error, degrees = min(options)
        if error < original_error * 0.35:
            rotation = Matrix.Rotation(math.radians(degrees), 3, axis)
            # Connected frame + its thin visible seams, not the soft pads.
            frame_parts = [piece for piece in pieces if piece is frame or
                           (len(piece["faces"]) < 300 and piece["size"].z > 0.45
                            and (piece["center"] - frame["center"]).dot(toward_seat) < 0.10)]
            for piece in frame_parts:
                transform_component(mesh, piece, rotation=rotation, pivot=pivot)
            straightened = True

    # Fit all pads and their stitched fronts as one authored set.
    for piece in (*pads, *seams):
        transform_component(mesh, piece, delta=cushion_delta)

    # Replace old wide merged hulls with one fitted hull per padded part. In
    # particular, a seat and back pad must never share a single convex hull.
    for obj in imported_hulls:
        bpy.data.objects.remove(obj, do_unlink=True)
    fitted = [piece for piece in components(mesh) if collision_piece(piece)]
    if not (count * 2 + 4 <= len(fitted) <= 26):
        raise RuntimeError(f"Unexpected sofa collision part count {len(fitted)} in {stem}")
    hulls = [make_hull(mesh, piece, f"UCX_{stem}_{index:02d}") for index, piece in enumerate(fitted)]

    DEST.mkdir(parents=True, exist_ok=True)
    output = DEST / source.name
    bpy.ops.object.select_all(action="DESELECT")
    for obj in (mesh, *hulls):
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh
    bpy.ops.export_scene.fbx(filepath=str(output), use_selection=True, apply_unit_scale=True,
                             axis_forward="-Z", axis_up="Y", bake_space_transform=False,
                             add_leaf_bones=False)
    print("GREAT_ROOM_CUSHIONS=" + json.dumps({
        "asset": stem, "output": str(output), "back_pads": len(pads),
        "all_hulls": len(hulls),
        "straightened_back_frame": straightened,
        "toward_seat": [round(float(v), 4) for v in toward_seat],
    }, separators=(",", ":")), flush=True)


if __name__ == "__main__":
    for asset, count in SOFAS.items():
        intact = ROOT / (asset + ".fbx")
        reference = baseline_frame(intact)
        for stage in ("_DamageV2", "_HeavyV2", ""):
            source = (ROOT / "damage-v2" if stage else ROOT) / (asset + stage + ".fbx")
            fix(source, asset + stage, count, reference)
