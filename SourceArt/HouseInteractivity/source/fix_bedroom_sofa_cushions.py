"""Seat the bedroom sofa's back pads without changing its frame or break debris.

The original Blender sofa has six disconnected upholstered back/lumbar pads.
Their lower edges hover above the seat. Move all six pads and their stitched
front seams as a unit in intact, damaged, and heavy meshes; carry their three
matching UCX hulls with them so hits meet the visible surface.
"""

import bpy
import json
import sys
from pathlib import Path

from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from extract_interactivity import components  # noqa: E402


SOURCE_ROOT = Path("/Users/Stewart/Projects/Senior sendoff/outputs/house-interactivity/models")
DEST_ROOT = Path(__file__).resolve().parents[1] / "CouchCushionFix"
ASSET = "HI_BR_SM_Bedroom_BrownReclinerSofa_001"
DELTA = Vector((0.0, -0.055, -0.075))


def is_back_piece(piece):
    size, lo, center = piece["size"], piece["lo"], piece["center"]
    names = " ".join(piece["materials"])
    if "BrownMicrofiber" in names:
        # Three main cushions and the three low lumbar bolsters.
        main = size.x > 0.55 and size.z > 0.5 and center.y > 0.12
        lumbar = size.x > 0.55 and 0.12 < size.z < 0.25 and lo.z > 0.0 and center.y > 0.04
        return main or lumbar
    if "DarkBrownSeams" in names:
        # Six vertical stitch lengths run down the front of the back pads.
        return size.x < 0.02 and size.z > 0.3 and center.y > 0.1
    return False


def move_vertices(obj, indices):
    local_delta = obj.matrix_world.inverted().to_3x3() @ DELTA
    for index in indices:
        obj.data.vertices[index].co += local_delta
    obj.data.update()


def fix(source, collision_template=None):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    mesh = next(obj for obj in bpy.context.scene.objects if obj.type == "MESH" and not obj.name.startswith("UCX_"))
    selected = [part for part in components(mesh) if is_back_piece(part)]
    if len(selected) != 12:
        raise RuntimeError(f"Expected 3 back pads + 3 lumbar pads + 6 seams, found {len(selected)} in {source}")
    vertex_indices = {int(index) for part in selected for face in part["faces"] for index in mesh.data.polygons[face].vertices}
    move_vertices(mesh, vertex_indices)

    hulls = [obj for obj in bpy.context.scene.objects if obj.type == "MESH" and obj.name.startswith("UCX_")]
    moved_hulls = []
    for hull in hulls:
        corners = [hull.matrix_world @ Vector(v) for v in hull.bound_box]
        lo = Vector(min(c[axis] for c in corners) for axis in range(3))
        hi = Vector(max(c[axis] for c in corners) for axis in range(3))
        size = hi - lo
        center = (lo + hi) * 0.5
        if size.x > 0.55 and size.z > 0.55 and center.y > 0.1:
            move_vertices(hull, range(len(hull.data.vertices)))
            moved_hulls.append(hull.name)
    if hulls and len(moved_hulls) != 3:
        raise RuntimeError(f"Expected three matching back-pad collision hulls, found {len(moved_hulls)} in {source}")
    if collision_template:
        if hulls:
            raise RuntimeError("Intact sofa already has collision hulls")
        present = set(bpy.context.scene.objects)
        bpy.ops.import_scene.fbx(filepath=str(collision_template))
        imported = [obj for obj in bpy.context.scene.objects if obj not in present]
        hulls = sorted((obj for obj in imported if obj.type == "MESH" and obj.name.startswith("UCX_")), key=lambda obj: obj.name)
        if len(hulls) != 12:
            raise RuntimeError(f"Expected 12 fitted sofa collision hulls, found {len(hulls)}")
        for obj in imported:
            if obj not in hulls:
                bpy.data.objects.remove(obj, do_unlink=True)
        for index, hull in enumerate(hulls):
            hull.name = f"UCX_{ASSET}_{index:02d}"
        moved_hulls = [hull.name for hull in hulls]

    DEST_ROOT.mkdir(parents=True, exist_ok=True)
    dest = DEST_ROOT / source.name
    bpy.ops.object.select_all(action="DESELECT")
    for obj in (mesh, *hulls):
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh
    bpy.ops.export_scene.fbx(
        filepath=str(dest), use_selection=True, apply_unit_scale=True,
        axis_forward="-Z", axis_up="Y", bake_space_transform=False,
        add_leaf_bones=False,
    )
    print("SOFA_FIX=" + json.dumps({"source": str(source), "output": str(dest),
                                    "pieces": len(selected), "vertices": len(vertex_indices),
                                    "hulls": moved_hulls}, separators=(",", ":")))


if __name__ == "__main__":
    for name in (ASSET + "_DamageV2", ASSET + "_HeavyV2", ASSET):
        folder = SOURCE_ROOT if name == ASSET else SOURCE_ROOT / "damage-v2"
        template = DEST_ROOT / (ASSET + "_DamageV2.fbx") if name == ASSET else None
        fix(folder / (name + ".fbx"), template)
