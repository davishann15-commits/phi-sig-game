"""Inspect connected sofa mesh pieces and their vertical clearance in Blender."""

import bpy
import json
import sys
from pathlib import Path
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from extract_interactivity import components  # noqa: E402


def audit(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH" and not obj.name.startswith("UCX_")]
    report = {"path": str(path), "meshes": [], "collision_hulls": []}
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH" or not obj.name.startswith("UCX_"):
            continue
        corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
        report["collision_hulls"].append({
            "name": obj.name,
            "lo": [round(min(c[axis] for c in corners), 4) for axis in range(3)],
            "hi": [round(max(c[axis] for c in corners), 4) for axis in range(3)],
        })
    for obj in meshes:
        pieces = components(obj)
        rows = []
        for piece in pieces:
            if len(piece["faces"]) < 4:
                continue
            rows.append({
                "faces": len(piece["faces"]),
                "lo": [round(v, 4) for v in piece["lo"]],
                "hi": [round(v, 4) for v in piece["hi"]],
                "size": [round(v, 4) for v in piece["size"]],
                "materials": piece["materials"],
            })
        rows.sort(key=lambda p: (-p["faces"], p["lo"][2]))
        report["meshes"].append({"name": obj.name, "pieces": rows})
    print("SOFA_AUDIT=" + json.dumps(report, separators=(",", ":")))


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    for source in args:
        audit(Path(source))
