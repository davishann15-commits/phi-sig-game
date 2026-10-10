"""Trim two upstairs wall ends that enter the great room's interior.

The original architectural FBX remains untouched. The great-room walls and
the remaining upstairs architecture retain their original geometry.
"""

import bmesh
import bpy
from pathlib import Path
from mathutils import Vector


source = Path(__file__).resolve().parents[1] / (
    "ShelfFireplaceRefinement/exports/SM_Upper_p0_p0_Solid.fbx")
output = Path(__file__).resolve().parents[1] / (
    "GreatRoomCorner/SM_Upper_p0_p0_CornerCleared.fbx")
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source))
obj = next(item for item in bpy.context.scene.objects if item.type == "MESH")
bm = bmesh.new()
bm.from_mesh(obj.data)
bm.verts.ensure_lookup_table()
bm.faces.ensure_lookup_table()

targets = [(bm.faces[19205], (0, -4.585, 0), (0, 1, 0), False),
           (bm.faces[19193], (7.095, 0, 0), (1, 0, 0), True)]
for seed, plane_point, plane_normal, keep_positive in targets:
    material = seed.material_index
    if obj.material_slots[material].name != "MI_SSO_S05RoomWarm":
        raise RuntimeError("The target is no longer an upstairs room wall")
    island = {seed}
    pending = [seed]
    while pending:
        face = pending.pop()
        for edge in face.edges:
            for linked in edge.link_faces:
                if linked not in island:
                    island.add(linked)
                    pending.append(linked)
    if len(island) != 12:
        raise RuntimeError(f"Expected a twelve-face wall box, got {len(island)}")
    verts = {vertex for face in island for vertex in face.verts}
    edges = {edge for face in island for edge in face.edges}
    normal = Vector(plane_normal)
    point = Vector(plane_point)
    cut = bmesh.ops.bisect_plane(
        bm, geom=list(verts | edges | island),
        plane_co=obj.matrix_world.inverted() @ point,
        plane_no=(obj.matrix_world.to_3x3().transposed() @ normal).normalized(),
        dist=0.0001, clear_inner=keep_positive, clear_outer=not keep_positive)
    boundary = [edge for edge in cut["geom_cut"]
                if isinstance(edge, bmesh.types.BMEdge) and edge.is_boundary]
    if boundary:
        caps = bmesh.ops.holes_fill(bm, edges=boundary, sides=0)["faces"]
        for face in caps:
            face.material_index = material
        bmesh.ops.recalc_face_normals(bm, faces=list(caps))
    signed = [((obj.matrix_world @ v.co) - point).dot(normal)
              for v in verts if v.is_valid]
    if (keep_positive and min(signed) < -.0001) or (
            not keep_positive and max(signed) > .0001):
        raise RuntimeError("Upstairs wall still enters the great-room interior")
    print("CORNER_WALL_TRIM", plane_point, "keep_positive", keep_positive,
          "original_faces", len(island), flush=True)

bmesh.ops.dissolve_degenerate(bm, dist=0.0001, edges=list(bm.edges))

bm.to_mesh(obj.data)
bm.free()
obj.data.update()
output.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(
    filepath=str(output), use_selection=True, apply_unit_scale=True,
    axis_forward="-Z", axis_up="Y", bake_space_transform=False,
    add_leaf_bones=False, mesh_smooth_type="FACE", use_tspace=True)
print("CORNER_TRIM_COMPLETE", output, "faces", len(obj.data.polygons), flush=True)
