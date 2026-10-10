"""Read-only inventory of architectural meshes near the Chapter 1 great room."""

import unreal


unreal.EditorLoadingAndSavingUtils.load_map("/Game/Story/Maps/Chapter01_House")

for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    mesh = actor.static_mesh_component.static_mesh
    if not mesh:
        continue
    origin, extent = actor.get_actor_bounds(False)
    lo = [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z]
    hi = [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z]
    if hi[0] < -1500 or lo[0] > 1500 or hi[1] < -1000 or lo[1] > 1000:
        continue
    if hi[2] < 170 or lo[2] > 800:
        continue
    name = actor.get_actor_label()
    if name.startswith(("COL", "HI_", "SM_GR", "SM_GR2", "SM_GR3")):
        continue
    materials = []
    for index in range(len(mesh.get_editor_property("static_materials"))):
        material = mesh.get_material(index)
        materials.append(material.get_name() if material else "None")
    unreal.log("GRCORNER actor={} mesh={} low={} high={} materials={}".format(
        name, mesh.get_path_name(), [round(value) for value in lo],
        [round(value) for value in hi], ",".join(sorted(set(materials)))))

unreal.log("GRCORNER_COMPLETE")
