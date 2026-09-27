"""Export the photo-study glasses in MetaHuman component-space centimeters."""
from pathlib import Path

import bpy

ROOT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt')
bpy.ops.wm.open_mainfile(filepath=str(ROOT / 'RunnerV2' / 'RunnerV2.blend'))
source = bpy.data.objects['Worn rectangular sunglasses']
mesh = source.data.copy()
glasses = bpy.data.objects.new('SM_RunnerSunglasses', mesh)
bpy.context.collection.objects.link(glasses)
for vertex in mesh.vertices:
    # The reference body is approximately 1.92 m in the Blender study. The
    # assembled MetaHuman is 1.75 m and is scaled to 1.90 m at runtime.
    point = source.matrix_world @ vertex.co
    # FBX static-mesh import interprets Blender coordinates as meters and
    # converts them to UE centimeters, unlike the skeletal wardrobe import.
    vertex.co = point * (1.75 / 1.92)
glasses.parent = None
bpy.ops.object.select_all(action='DESELECT')
glasses.select_set(True)
bpy.context.view_layer.objects.active = glasses
bpy.ops.export_scene.fbx(
    filepath=str(ROOT / 'RunnerMetaHuman' / 'SM_RunnerSunglasses.fbx'),
    use_selection=True, object_types={'MESH'},
    apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE',
    axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
print('RUNNER_GLASSES_EXPORTED', len(mesh.vertices), len(mesh.polygons))
