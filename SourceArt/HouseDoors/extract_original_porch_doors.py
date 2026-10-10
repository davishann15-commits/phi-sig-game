"""Split the eight existing porch glass leaves from the imported house meshes.

First export the original Unreal house meshes with
Content/Python/export_original_house_meshes.py. This keeps the fixed jambs,
brick, and upper glass transoms in the room assets while making the original
white frame, brass fittings, and pane move together.
"""
import bpy
import bmesh
import collections
import json
import os
from mathutils import Vector

project = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
source_dir = '/tmp/sso_house_original_meshes'
out_dir = os.path.join(project, 'SourceArt/HouseDoors')
manifest = []


def load_mesh(path):
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=path)
    objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    assert len(objects) == 1, (path, len(objects))
    return objects[0]


def parts(obj):
    mesh = obj.data
    parents = list(range(len(mesh.vertices)))

    def find(v):
        while parents[v] != v:
            parents[v] = parents[parents[v]]
            v = parents[v]
        return v

    for face in mesh.polygons:
        for v in face.vertices[1:]:
            parents[find(v)] = find(face.vertices[0])
    groups = collections.defaultdict(list)
    for face in mesh.polygons:
        groups[find(face.vertices[0])].append(face.index)
    result = []
    for faces in groups.values():
        verts = {v for i in faces for v in mesh.polygons[i].vertices}
        coords = [obj.matrix_world @ mesh.vertices[v].co for v in verts]
        lo = [min(p[k] for p in coords) for k in range(3)]
        hi = [max(p[k] for p in coords) for k in range(3)]
        material = mesh.materials[mesh.polygons[faces[0]].material_index].name.split('.')[0]
        result.append({'faces': set(faces), 'lo': lo, 'hi': hi, 'material': material})
    return result


def export_faces(obj, faces, name, cut=None):
    duplicate = obj.data.copy()
    duplicate.name = name + 'Mesh'
    moving = bpy.data.objects.new(name, duplicate)
    bpy.context.collection.objects.link(moving)
    moving.matrix_world = obj.matrix_world.copy()
    bm = bmesh.new()
    bm.from_mesh(duplicate)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in faces], context='FACES')
    if cut:
        mid, side = cut
        # Bisect the original continuous top rail at the centre of each pair.
        local_point = moving.matrix_world.inverted() @ Vector((mid, 0, 0))
        local_normal = (moving.matrix_world.inverted().transposed().to_3x3() @ Vector((1, 0, 0))).normalized()
        bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
                               dist=0.00001, plane_co=local_point, plane_no=local_normal,
                               clear_outer=side == 'left', clear_inner=side == 'right')
    bm.to_mesh(duplicate)
    bm.free()
    duplicate.update()
    bpy.ops.object.select_all(action='DESELECT')
    moving.select_set(True)
    bpy.context.view_layer.objects.active = moving
    bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, name + '.fbx'), use_selection=True,
                             apply_unit_scale=True, axis_forward='-Z', axis_up='Y',
                             bake_space_transform=False)
    bpy.data.objects.remove(moving, do_unlink=True)


for chunk in ('p0_m1', 'p0_p0'):
    glass = load_mesh(os.path.join(source_dir, 'SM_Entrance_' + chunk + '_Glass.fbx'))
    glass_parts = parts(glass)
    panes = sorted((p for p in glass_parts if 1.8 < p['hi'][2] < 2.1 and p['lo'][2] < .1),
                   key=lambda p: p['lo'][0])
    assert len(panes) == 4, (chunk, len(panes))
    for index, pane in enumerate(panes):
        name = 'SM_OriginalPorchGlass_' + chunk + '_' + str(index + 1).zfill(2)
        export_faces(glass, pane['faces'], name)
    export_faces(glass, set(range(len(glass.data.polygons))) - set.union(*(p['faces'] for p in panes)),
                 'SM_OriginalRoom_Entrance_' + chunk + '_Glass')

    solid_source = (os.path.join(out_dir, 'Baselines/SM_OriginalRoom_Entrance_p0_p0_Solid.fbx')
                    if chunk == 'p0_p0' else
                    os.path.join(source_dir, 'SM_Entrance_p0_m1_Solid.fbx'))
    solid = load_mesh(solid_source)
    solid_parts = parts(solid)
    used = set()
    for index, pane in enumerate(panes):
        lo_x, hi_x = pane['lo'][0], pane['hi'][0]
        pair = index // 2
        pair_lo = panes[pair * 2]['lo'][0]
        pair_hi = panes[pair * 2 + 1]['hi'][0]
        midpoint = (panes[pair * 2]['hi'][0] + panes[pair * 2 + 1]['lo'][0]) / 2
        selected = set()
        stripped_handles = set()
        recovered_stiles = 0
        for part in solid_parts:
            lo, hi = part['lo'], part['hi']
            white_or_brass = part['material'] in ('MI_SSO_PaintWhite', 'MI_SSO_MetalBrass')
            near_leaf = -4.77 < lo[1] and hi[1] < -4.59 and -.01 <= lo[2] and hi[2] <= 2.18
            inside_panel = lo_x - .070 <= lo[0] and hi[0] <= hi_x + .070
            was_inside_panel = lo_x - .055 <= lo[0] and hi[0] <= hi_x + .055
            shared_top_rail = (part['material'] == 'MI_SSO_PaintWhite' and
                               pair_lo - .1 <= lo[0] and hi[0] <= pair_hi + .1 and
                               hi[0] - lo[0] > 1.3 and lo[2] > 1.85)
            if white_or_brass and near_leaf and (inside_panel or shared_top_rail):
                # The source has one pull at the left edge of *each* glass
                # panel, even on the right-hand leaves. Replacing just these
                # pulls with paired central levers avoids duplicate handles.
                if part['material'] == 'MI_SSO_MetalBrass' and .85 < lo[2] < 1.2 and hi[2] < 1.2:
                    stripped_handles.update(part['faces'])
                else:
                    selected.update(part['faces'])
                    if inside_panel and not was_inside_panel and not shared_top_rail:
                        assert part['material'] == 'MI_SSO_PaintWhite' and hi[2] - lo[2] > 1.8
                        recovered_stiles += 1
        assert selected and recovered_stiles == 2, (chunk, index, recovered_stiles)
        used.update(selected | stripped_handles)
        frame_name = 'SM_OriginalPorchFrame_' + chunk + '_' + str(index + 1).zfill(2)
        export_faces(solid, selected, frame_name,
                     (midpoint, 'left' if index % 2 == 0 else 'right'))
        manifest.append({'id': 'Porch_' + chunk + '_' + str(index + 1).zfill(2),
                         'frame': frame_name, 'glass': 'SM_OriginalPorchGlass_' + chunk + '_' + str(index + 1).zfill(2),
                         'bounds': [pane['lo'], pane['hi']], 'group': 'Porch_' + chunk + '_' + str(pair + 1)})
    export_faces(solid, set(range(len(solid.data.polygons))) - used,
                 'SM_OriginalRoom_Entrance_' + chunk + '_Solid')
    print('PORCH_EXTRACTED', chunk, 'leaves=4', 'solid_faces_removed=' + str(len(used)))

with open(os.path.join(project, 'Content/HouseReview/original_porch_doors.json'), 'w') as handle:
    json.dump(manifest, handle, indent=2)
