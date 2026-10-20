"""Inspect acquired FBX donor anatomy in Blender without changing the sources."""
import json
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parent
report = {}
for species in ["pig", "doe", "fox", "crow"]:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(ROOT / (species + ".fbx")))
    meshes = []
    bones = {}
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            points = [obj.matrix_world @ v.co for v in obj.data.vertices]
            meshes.append({
                "name": obj.name, "vertices": len(points), "polygons": len(obj.data.polygons),
                "min": [min(v[i] for v in points) for i in range(3)],
                "max": [max(v[i] for v in points) for i in range(3)],
                "groups": [g.name for g in obj.vertex_groups],
                "materials": [s.material.name if s.material else None for s in obj.material_slots],
            })
        elif obj.type == "ARMATURE":
            for bone in obj.data.bones:
                bones[bone.name] = {"head": list(obj.matrix_world @ bone.head_local),
                                    "tail": list(obj.matrix_world @ bone.tail_local),
                                    "parent": bone.parent.name if bone.parent else None}
    report[species] = {"meshes": meshes, "bones": bones}
target = ROOT / "animal_anatomy.json"
target.unlink(missing_ok=True)
target.write_text(json.dumps(report, indent=2))
print("CALDER_ANATOMY PASS", target)
