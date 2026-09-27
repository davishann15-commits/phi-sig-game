"""Read-only geometry probe for Runner's body and native shirt."""
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

root = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
for file in ('RunnerNativeBody.fbx', 'RunnerJerseyBody.fbx',
             'RunnerJerseyFace.fbx', 'RunnerNativeOutfit.fbx'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root / file), use_anim=False)
    print('RUNNER_PROBE', file)
    for obj in bpy.data.objects:
        if obj.type != 'MESH':
            continue
        world = [obj.matrix_world @ v.co for v in obj.data.vertices]
        bbox = [(round(min(p[i] for p in world), 3),
                 round(max(p[i] for p in world), 3)) for i in range(3)]
        mats = [(i, m.name if m else None) for i, m in enumerate(obj.data.materials)]
        print('RUNNER_MESH', obj.name, len(obj.data.vertices), len(obj.data.polygons),
              'bbox', bbox, 'materials', mats,
              'object_scale', list(obj.scale), 'matrix', str(obj.matrix_world))
        if file in ('RunnerJerseyBody.fbx', 'RunnerJerseyFace.fbx'):
            tree = BVHTree.FromPolygons(world, [list(face.vertices) for face in obj.data.polygons])
            for x, z in ((0, 1.536), (0, 1.55), (0, 1.57), (.04, 1.552),
                         (.08, 1.593), (.10, 1.61)):
                hit, _, _, _ = tree.ray_cast(Vector((x, -1, z)), Vector((0, 1, 0)), 2)
                print('RUNNER_SKIN_NECK_FRONT', file, x, z,
                      round(hit.y, 3) if hit else None)
        for i in range(len(obj.data.materials)):
            indices = {vi for poly in obj.data.polygons if poly.material_index == i for vi in poly.vertices}
            if not indices:
                continue
            pts = [world[j] for j in indices]
            sb = [(round(min(p[k] for p in pts), 3), round(max(p[k] for p in pts), 3)) for k in range(3)]
            print('RUNNER_SECTION', i, 'verts', len(indices), 'bbox', sb)
            if file == 'RunnerNativeOutfit.fbx' and i == 1:
                parent = {j: j for j in indices}
                def find(j):
                    while parent[j] != j:
                        parent[j] = parent[parent[j]]
                        j = parent[j]
                    return j
                for face in obj.data.polygons:
                    if face.material_index == 1:
                        root = find(face.vertices[0])
                        for j in face.vertices[1:]:
                            parent[find(j)] = root
                components = {}
                for j in indices:
                    components.setdefault(find(j), []).append(world[j])
                for part in sorted(components.values(), key=len, reverse=True)[:15]:
                    print('RUNNER_SHIRT_COMPONENT', len(part),
                          [(round(min(p[k] for p in part), 3),
                            round(max(p[k] for p in part), 3)) for k in range(3)])
                for key, part in components.items():
                    if len(part) not in (3222, 2510):
                        continue
                    uv = obj.data.uv_layers.active.data
                    coords = [uv[loop_index].uv for face in obj.data.polygons
                              if face.material_index == 1 and find(face.vertices[0]) == key
                              for loop_index in face.loop_indices]
                    print('RUNNER_SHIRT_UV_ISLAND', len(part),
                          [(round(min(p[k] for p in coords), 3),
                            round(max(p[k] for p in coords), 3)) for k in (0, 1)])
                for key, part in components.items():
                    if len(part) != 840:
                        continue
                    members = {j for j in indices if find(j) == key}
                    edge_count = {}
                    for face in obj.data.polygons:
                        if face.material_index != 1 or face.vertices[0] not in members:
                            continue
                        verts = list(face.vertices)
                        for a, b in zip(verts, verts[1:] + verts[:1]):
                            edge = tuple(sorted((a, b)))
                            edge_count[edge] = edge_count.get(edge, 0) + 1
                    adj = {}
                    for (a, b), count in edge_count.items():
                        if count == 1:
                            adj.setdefault(a, []).append(b)
                            adj.setdefault(b, []).append(a)
                    loops = []
                    visited = set()
                    for start in adj:
                        if start in visited:
                            continue
                        stack = [start]
                        loop = []
                        visited.add(start)
                        while stack:
                            a = stack.pop()
                            loop.append(a)
                            for b in adj[a]:
                                if b not in visited:
                                    visited.add(b)
                                    stack.append(b)
                        loops.append(loop)
                    for loop in sorted(loops, key=len, reverse=True):
                        print('RUNNER_SLEEVE_BOUNDARY', len(loop),
                              'mean_abs_x', round(sum(abs(world[j].x) for j in loop) / len(loop), 3),
                              'bbox', [(round(min(world[j][k] for j in loop), 3),
                                        round(max(world[j][k] for j in loop), 3)) for k in range(3)])
                for low, high in ((1.02, 1.20), (1.20, 1.35), (1.35, 1.40),
                                  (1.40, 1.45), (1.45, 1.50), (1.50, 1.55),
                                  (1.55, 1.60), (1.60, 1.65)):
                    band = [p for p in pts if low <= p.z < high]
                    if band:
                        print('RUNNER_JERSEY_BAND', low, high, len(band),
                              'max_x', round(max(abs(p.x) for p in band), 3),
                              'outer_count', sum(abs(p.x) > .22 for p in band))
