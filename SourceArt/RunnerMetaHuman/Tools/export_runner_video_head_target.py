"""Export the video-fitted face as a head-only Unreal conform target.

The transform maps FaceBuilder's arbitrary units into the existing Runner
MetaHuman head position. The approved body/jersey is not edited.
"""

from pathlib import Path

import bpy


ROOT = Path("/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman")
head = bpy.data.objects["Runner_SamVideo_FaceBuilderHead"]
mesh = head.data.copy()
target = bpy.data.objects.new("SM_Runner_SamVideoHeadTarget", mesh)
bpy.context.scene.collection.objects.link(target)
for vertex in mesh.vertices:
    x, y, z = vertex.co
    # Unreal's static-mesh importer treats FBX coordinates as centimetres.
    # Avoid importing a tiny mesh whose triangles are culled as degenerates.
    vertex.co = (x * 18.0, y * 11.5 + 0.2, z * 16.0 + 169.4)
mesh.materials.clear()

bpy.ops.object.select_all(action="DESELECT")
target.select_set(True)
bpy.context.view_layer.objects.active = target
bpy.ops.export_scene.fbx(
    filepath=str(ROOT / "SM_Runner_SamVideoHeadTargetCM.fbx"),
    use_selection=True,
    object_types={"MESH"},
    bake_anim=False,
    apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_ALL",
    axis_forward="-Y",
    axis_up="Z",
    mesh_smooth_type="FACE",
)
print("HEAD_TARGET_EXPORTED", len(mesh.vertices), len(mesh.polygons))
