"""Measure sofa throw-pillow clearance against the fitted seat surfaces."""

import bpy
import json
import sys
from pathlib import Path

from mathutils import Vector
from mathutils.bvhtree import BVHTree


ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / "extraction-manifest.json").read_text())
pieces = {row["id"]: row for row in manifest["pieces"]}


def object_at(path, origin):
    bpy.ops.import_scene.fbx(filepath=str(path))
    obj = next(o for o in bpy.context.selected_objects if o.type == "MESH" and not o.name.startswith("UCX_"))
    return obj, [obj.matrix_world @ v.co + origin for v in obj.data.vertices]


for pillow_id, sofa_id in (
    ("SM_GR3_NearSeating__004", "SM_GR3_NearSeating__003"),
    ("SM_GR3_NearSeating__005", "SM_GR3_NearSeating__003"),
    ("SM_GR3_NearSeating__006", "SM_GR3_NearSeating__001"),
    ("SM_GR3_NearSeating__007", "SM_GR3_NearSeating__001"),
):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    pillow = pieces[pillow_id]
    sofa = pieces[sofa_id]
    sofa_path = ROOT / "GreatRoomCushionFix" / (sofa["asset_name"] + ".fbx")
    pillow_path = Path(pillow["intact_fbx"])
    so = sofa["local_origin_cm"]
    po = pillow["local_origin_cm"]
    sofa_origin = Vector((so[0] / 100, -so[1] / 100, so[2] / 100))
    pillow_origin = Vector((po[0] / 100, -po[1] / 100, po[2] / 100))
    couch_obj, couch_verts = object_at(sofa_path, sofa_origin)
    pillow_obj, pillow_verts = object_at(pillow_path, pillow_origin)
    bvh = BVHTree.FromPolygons(couch_verts, [list(face.vertices) for face in couch_obj.data.polygons])
    bottom = min(v.z for v in pillow_verts)
    gaps = []
    for vertex in pillow_verts:
        if vertex.z > bottom + 0.12:
            continue
        hit, _, _, _ = bvh.ray_cast(vertex + Vector((0, 0, 0.2)), Vector((0, 0, -1)), 1.0)
        if hit is not None:
            gaps.append(round(vertex.z - hit.z, 4))
    gaps.sort()
    print("PILLOW_SUPPORT=" + json.dumps({
        "pillow": pillow_id, "sofa": sofa_id, "sampled": len(gaps),
        "bottom_m": round(bottom, 4),
        "gap_m": {"min": gaps[0], "p10": gaps[len(gaps) // 10],
                  "median": gaps[len(gaps) // 2], "p90": gaps[9 * len(gaps) // 10],
                  "max": gaps[-1]} if gaps else None,
    }, separators=(",", ":")), flush=True)
