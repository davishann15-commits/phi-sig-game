"""Offline inventory of exported evergreen geometry; no project asset writes."""
import collections
import hashlib
import json
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'Saved/CalderWork/evergreen_screening'
rows = []
for source in sorted(WORK.glob('*.fbx')):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source), use_anim=False)
    objects = []
    for obj in bpy.data.objects:
        if obj.type != 'MESH':
            continue
        mesh = obj.data
        mesh.calc_loop_triangles()
        points = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
        bounds = [[min(p[a] for p in points) for a in range(3)],
                  [max(p[a] for p in points) for a in range(3)]]
        objects.append({'name': obj.name, 'vertices': len(mesh.vertices),
                        'triangles': len(mesh.loop_triangles), 'world_bounds_m': bounds,
                        'uv_layers': [layer.name for layer in mesh.uv_layers],
                        'colour_attributes': [layer.name for layer in mesh.color_attributes],
                        'materials': [slot.material.name if slot.material else None for slot in obj.material_slots],
                        'triangles_per_slot': dict(collections.Counter(t.material_index for t in mesh.loop_triangles)),
                        'vertex_groups': len(obj.vertex_groups)})
    rows.append({'source': source.name, 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'objects': objects})
(WORK/'fbx_geometry.json').write_text(json.dumps({'scope': 'Read-only rest geometry screening; no runtime tree/species acceptance', 'donors': rows}, indent=2)+'\n')
print('CALDER_EVERGREEN_FBX '+json.dumps(rows))
