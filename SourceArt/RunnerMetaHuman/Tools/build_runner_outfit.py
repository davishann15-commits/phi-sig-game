"""Fit the Runner photo wardrobe from the C02 study onto his MetaHuman rig.

The MetaHuman body/face/groom remain native Unreal assets. Blender is used for
the custom sleeveless jersey, shorts, lettering, sandals, and glasses just as
it was used for Braxton's custom hoodie and footwear.
"""
import json
from pathlib import Path

import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

ROOT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt')
SOURCE = ROOT / 'RunnerV2' / 'RunnerV2.blend'
OUT = ROOT / 'RunnerMetaHuman'

bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
source_names = [
    'C02_Body.toigo_wool_pants',
    'C02_Body.toigo_basic_tucked_t-shirt',
    'Black jersey edge', 'Black jersey edge.001',
    'Garment print', 'Garment print.001', 'Garment print.002', 'Garment print.003',
    'RunnerV2_GoldOutline_Garment_print',
    'RunnerV2_GoldOutline_Garment_print.001',
    'RunnerV2_GoldOutline_Garment_print.003',
    'Flip flop l', 'Flip flop r',
    'Worn rectangular sunglasses',
]
missing = [name for name in source_names if name not in bpy.data.objects]
if missing:
    raise RuntimeError('Runner source pieces missing: ' + str(missing))

prior = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(OUT / 'RunnerNativeOutfit.fbx'), use_anim=False)
added = [o for o in bpy.data.objects if o not in prior]
rig = next(o for o in added if o.type == 'ARMATURE')
native_matrix = rig.matrix_world.copy()
# Keep the exporter-provided armature node name. Renaming it introduces a new
# root bone on FBX import and breaks MetaHuman hierarchy/pose copying.
prior_body = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(OUT / 'RunnerNativeBody.fbx'), use_anim=False)
body = next(o for o in bpy.data.objects if o not in prior_body and o.type == 'MESH' and len(o.vertex_groups) > 100)

# Both studies use the same forward axis. Fit 1.92 m C02 proportions to the
# 1.75 m native MetaHuman bind pose; final in-game display scale sets 1.90 m.
scale = 1.747 / 1.92
tree = KDTree(len(body.data.vertices))
for vertex in body.data.vertices:
    tree.insert(rig.matrix_world.inverted() @ body.matrix_world @ vertex.co, vertex.index)
tree.balance()
groups_by_index = {g.index: g.name for g in body.vertex_groups}
pieces = []
report = {'scale_to_native': scale, 'source': str(SOURCE), 'pieces': {}}

for source_name in source_names:
    src = bpy.data.objects[source_name]
    data = src.data.copy()
    piece = bpy.data.objects.new('MH_' + source_name.replace(' ', '_'), data)
    bpy.context.collection.objects.link(piece)
    is_glasses = source_name == 'Worn rectangular sunglasses'
    is_jersey = 't-shirt' in source_name
    is_shorts = 'wool_pants' in source_name
    is_logo = 'Garment_print' in source_name or 'Garment print' in source_name
    is_footwear = 'Flip flop' in source_name
    for vertex in data.vertices:
        point = (src.matrix_world @ vertex.co) * scale
        if not is_glasses and not is_footwear:
            point.x *= 1.04
            point.y *= 1.04
        # Eyewear sits on the MetaHuman bridge; the original face is taller.
        if is_glasses:
            point.z += .02
        # The native outfit armature is exported in centimeters (world scale
        # 0.01), whereas the C02 study is authored in meters. Express every
        # garment vertex in the native rig's local centimeter coordinates.
        vertex.co = rig.matrix_world.inverted() @ point
    piece.parent = rig
    piece.matrix_parent_inverse = Matrix.Identity(4)
    piece.matrix_basis = Matrix.Identity(4)
    piece.matrix_world = rig.matrix_world.copy()
    piece.vertex_groups.clear()
    if is_glasses:
        piece.vertex_groups.new(name='head').add(
            list(range(len(data.vertices))), 1.0, 'REPLACE')
    else:
        output_groups = {}
        for vertex in data.vertices:
            nearest = tree.find_n(vertex.co, 4)
            weights = {}
            total = 0.0
            for _, index, distance in nearest:
                influence = 1.0 / max(distance, .015) ** 2
                total += influence
                for group in body.data.vertices[index].groups:
                    name = groups_by_index[group.group]
                    weights[name] = weights.get(name, 0.0) + group.weight * influence
            for name, weight in weights.items():
                value = weight / total
                if value < .002:
                    continue
                if name not in output_groups:
                    output_groups[name] = piece.vertex_groups.new(name=name)
                output_groups[name].add([vertex.index], value, 'REPLACE')
    piece.modifiers.clear()
    modifier = piece.modifiers.new('MetaHuman native skeleton', 'ARMATURE')
    modifier.object = rig
    for face in data.polygons:
        face.use_smooth = is_jersey or is_shorts or is_footwear
    report['pieces'][source_name] = {
        'vertices': len(data.vertices),
        'materials': [material.name if material else None for material in data.materials],
        'groups': len(piece.vertex_groups),
    }
    pieces.append(piece)

for obj in bpy.data.objects:
    if obj not in pieces and obj != rig:
        bpy.data.objects.remove(obj, do_unlink=True)

# Importing the source wardrobe as one skinned mesh preserves the relative
# placement of embroidered numerals, ribbing, and sandals around the body.
bpy.ops.object.select_all(action='DESELECT')
for piece in pieces:
    piece.select_set(True)
bpy.context.view_layer.objects.active = pieces[0]
bpy.ops.object.join()
outfit = pieces[0]
outfit.name = 'SK_Runner_MetaOutfit'
outfit.data.name = outfit.name
outfit.matrix_world = native_matrix
bpy.context.view_layer.update()
print('RUNNER_NATIVE_MATRICES', native_matrix[0][0], rig.matrix_world[0][0], outfit.matrix_world[0][0])
if abs(outfit.matrix_world[0][0] - .01) > .001:
    raise RuntimeError('Runner native outfit import scale was not preserved')
for face in outfit.data.polygons:
    face.use_smooth = True

# Keep shorts and jersey as the first two sections; subsequent sections are
# rigid trim/accessories and can be left out of cloth simulation.
materials = list(outfit.data.materials)
priority = {'C02_Shorts': 0, 'C02_Shirt': 1}
order = sorted(range(len(materials)), key=lambda i: (priority.get(materials[i].name, 2), i))
remap = {old: new for new, old in enumerate(order)}
indices = [remap[poly.material_index] for poly in outfit.data.polygons]
outfit.data.materials.clear()
for index in order:
    outfit.data.materials.append(materials[index])
for poly, index in zip(outfit.data.polygons, indices):
    poly.material_index = index

report['combined_vertices'] = len(outfit.data.vertices)
report['combined_materials'] = [m.name for m in outfit.data.materials]
bpy.ops.object.select_all(action='DESELECT')
outfit.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = outfit
bpy.ops.export_scene.fbx(
    filepath=str(OUT / 'SK_Runner_MetaOutfit.fbx'),
    use_selection=True, object_types={'ARMATURE', 'MESH'},
    add_leaf_bones=False, armature_nodetype='NULL',
    use_armature_deform_only=False, bake_anim=False,
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
    axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'Runner_MetaOutfit.blend'))
(OUT / 'outfit_build.json').write_text(json.dumps(report, indent=2))
print('RUNNER_META_OUTFIT_EXPORTED', report['combined_vertices'], report['combined_materials'])
