"""Export the Runner photo-study flip-flops in MetaHuman component space."""
from pathlib import Path

import bpy

ROOT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt')
bpy.ops.wm.open_mainfile(filepath=str(ROOT / 'RunnerV2' / 'RunnerV2.blend'))
for side, source_name in [('L', 'Flip flop l'), ('R', 'Flip flop r')]:
    source = bpy.data.objects[source_name]
    mesh = source.data.copy()
    obj = bpy.data.objects.new('SM_RunnerSandal' + side, mesh)
    bpy.context.collection.objects.link(obj)
    for vertex in mesh.vertices:
        vertex.co = (source.matrix_world @ vertex.co) * (1.75 / 1.92)
    obj.parent = None
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(ROOT / 'RunnerMetaHuman' / (obj.name + '.fbx')),
        use_selection=True, object_types={'MESH'},
        apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE',
        axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    print('RUNNER_SANDAL_EXPORTED', side, len(mesh.vertices), len(mesh.polygons))
