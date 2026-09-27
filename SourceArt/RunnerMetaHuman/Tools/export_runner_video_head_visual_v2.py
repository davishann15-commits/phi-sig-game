"""Export fitted face while omitting unobserved rear-neck projection pixels."""

from pathlib import Path
import sys
import bmesh
import bpy


root = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
source = bpy.data.objects['Runner_SamVideo_FaceBuilderHead']
mesh = source.data.copy()
scalp_cut = '--scalp-cut' in sys.argv
scalp_contour = '--scalp-contour' in sys.argv
scalp_contour_v5 = '--scalp-contour-v5' in sys.argv
rear_only = '--rear-only' in sys.argv
material_zones = '--material-zones' in sys.argv
visual = bpy.data.objects.new(
    'SM_Runner_SamVideoRearFill' if rear_only else
    'SM_Runner_SamVideoHeadVisualV6' if material_zones else
    'SM_Runner_SamVideoHeadVisualV5' if scalp_contour_v5 else
    'SM_Runner_SamVideoHeadVisualV4' if scalp_contour else
    'SM_Runner_SamVideoHeadVisualV3' if scalp_cut else
    'SM_Runner_SamVideoHeadVisualV2', mesh)
bpy.context.scene.collection.objects.link(visual)
for vertex in mesh.vertices:
    x, y, z = vertex.co
    vertex.co = (x * 18.0, y * 11.5 + .2, z * 16.0 + 169.4)

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
if scalp_cut or scalp_contour or scalp_contour_v5 or material_zones:
    # The enlarged fitted skull otherwise occludes every strand of the bound
    # hair groom. For the contoured revision, keep the ear/temple area while
    # hiding the crown. The curved seam sits under the groom instead of making
    # a horizontal wedge above each ear.
    if scalp_contour:
        for vertex in bm.verts:
            vertex.co.z -= 8.0 * min(1.0, abs(vertex.co.x) / 14.0) ** 4
    if scalp_contour_v5 or material_zones:
        # Lower the scalp seam at the temples. The previous curve raised its
        # sides, leaving two conspicuous spikes outside the bound hair groom.
        for vertex in bm.verts:
            vertex.co.z += 6.0 * min(1.0, abs(vertex.co.x) / 14.0) ** 4
    bmesh.ops.bisect_plane(
        bm,
        geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
        plane_co=(0, 0, 178.0),
        plane_no=(0, 0, 1),
        clear_inner=False,
        clear_outer=True,
    )
    if scalp_contour:
        for vertex in bm.verts:
            vertex.co.z += 8.0 * min(1.0, abs(vertex.co.x) / 14.0) ** 4
    if scalp_contour_v5 or material_zones:
        for vertex in bm.verts:
            vertex.co.z -= 6.0 * min(1.0, abs(vertex.co.x) / 14.0) ** 4
bad_rear = [
    face for face in bm.faces
    if (sum(v.co.y for v in face.verts) / len(face.verts) > 1.5 and
        sum(v.co.z for v in face.verts) / len(face.verts) < 167.0)
]
if rear_only:
    bmesh.ops.delete(bm, geom=[face for face in bm.faces if face not in bad_rear], context='FACES')
else:
    bmesh.ops.delete(bm, geom=bad_rear, context='FACES')
bm.to_mesh(mesh)
bm.free()
mesh.update()
mesh.materials.clear()
if material_zones:
    for name in ('FittedFace', 'UnderHair', 'RearSkin'):
        mesh.materials.append(bpy.data.materials.new(name))
    for polygon in mesh.polygons:
        center = polygon.center
        if center.y > 1.5:
            polygon.material_index = 1 if center.z >= 170.0 else 2
        else:
            polygon.material_index = 0
bpy.ops.object.select_all(action='DESELECT')
visual.select_set(True)
bpy.context.view_layer.objects.active = visual
bpy.ops.export_scene.fbx(
    filepath=str(root / (visual.name + '.fbx')),
    use_selection=True,
    object_types={'MESH'},
    bake_anim=False,
    apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_ALL',
    axis_forward='-Y',
    axis_up='Z',
    mesh_smooth_type='FACE',
)
print('RUNNER_VIDEO_HEAD_V2_EXPORTED', len(mesh.vertices), len(mesh.polygons),
      'BAD_REAR_FACES', len(bad_rear), 'SCALP_CUT', scalp_cut,
      'SCALP_CONTOUR', scalp_contour, 'SCALP_CONTOUR_V5', scalp_contour_v5,
      'REAR_ONLY', rear_only, 'MATERIAL_ZONES', material_zones)
