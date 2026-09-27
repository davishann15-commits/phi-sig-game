"""Export an independent textured head overlay for visual comparison in UE."""

from pathlib import Path

import bmesh
import bpy


ROOT = Path("/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman")
original = bpy.data.objects["Runner_SamVideo_FaceBuilderHead"]
mesh = original.data.copy()
visual = bpy.data.objects.new("SM_Runner_SamVideoHeadVisual", mesh)
bpy.context.scene.collection.objects.link(visual)
for vertex in mesh.vertices:
    x, y, z = vertex.co
    vertex.co = (x * 18.0, y * 11.5 + 0.2, z * 16.0 + 169.4)

# Trim the video subject's shirt/shoulders out of the head model. The existing
# approved body and basketball jersey remain responsible for the neckline.
bm = bmesh.new()
bm.from_mesh(mesh)
bmesh.ops.bisect_plane(
    bm,
    geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
    plane_co=(0, 0, 156.0),
    plane_no=(0, 0, 1),
    clear_inner=True,
    clear_outer=False,
)
bm.to_mesh(mesh)
bm.free()
mesh.update()
mesh.materials.clear()

bpy.ops.object.select_all(action="DESELECT")
visual.select_set(True)
bpy.context.view_layer.objects.active = visual
bpy.ops.export_scene.fbx(
    filepath=str(ROOT / "SM_Runner_SamVideoHeadVisual.fbx"),
    use_selection=True,
    object_types={"MESH"},
    bake_anim=False,
    apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_ALL",
    axis_forward="-Y",
    axis_up="Z",
    mesh_smooth_type="FACE",
)
print("RUNNER_VIDEO_HEAD_VISUAL_EXPORTED", len(mesh.vertices), len(mesh.polygons))
