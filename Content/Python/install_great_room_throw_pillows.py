"""Reimport the four great-room throw pillows with inset soft collision."""

import unreal
from pathlib import Path


source = Path(unreal.Paths.project_dir()) / "SourceArt/HouseInteractivity/GreatRoomCushionFix"
package = "/Game/SeniorSendOff/House/Interactivity"
names = tuple(f"HI_SM_GR3_NearSeating_{number:03d}" for number in (4, 5, 6, 7))
editor = (unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
          or unreal.EditorStaticMeshLibrary)
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")


for base in names:
  for suffix in ("", "_DamageV2", "_HeavyV2", "_BrokenV2"):
    name = base + suffix
    mesh = unreal.load_asset(package + "/" + name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Missing throw pillow: " + name)
    slots = mesh.get_editor_property("static_materials")
    materials = [mesh.get_material(i) for i in range(len(slots))]
    slot_names = [str(slot.get_editor_property("imported_material_slot_name")) for slot in slots]
    fbx = source / (name + ".fbx")
    if not fbx.is_file():
        raise RuntimeError("Missing fitted throw pillow FBX: " + str(fbx))

    editor.remove_collisions(mesh)
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.static_mesh_import_data.auto_generate_collision = False
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.reorder_material_to_fbx_order = True
    ui.static_mesh_import_data.one_convex_hull_per_ucx = True
    task = unreal.AssetImportTask()
    task.factory = unreal.FbxFactory()
    task.filename = str(fbx)
    task.destination_path = package
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = ui
    tools.import_asset_tasks([task])

    mesh = unreal.load_asset(package + "/" + name)
    new_slots = mesh.get_editor_property("static_materials")
    new_names = [str(slot.get_editor_property("imported_material_slot_name")) for slot in new_slots]
    if new_names != slot_names:
        raise RuntimeError("Throw pillow materials changed: " + name + " " + repr(new_names))
    for index, material in enumerate(materials):
        if material:
            mesh.set_material(index, material)
    mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    hulls = len(mesh.get_editor_property("body_setup").get_editor_property(
        "agg_geom").get_editor_property("convex_elems"))
    if (not suffix and hulls != 1) or (suffix and hulls < 1):
        raise RuntimeError(f"Throw pillow {name} has invalid hull count {hulls}")
    if not assets.save_loaded_asset(mesh):
        raise RuntimeError("Could not save throw pillow: " + name)
    unreal.log(f"GREAT_ROOM_PILLOW_FIX imported={name} hulls={hulls}")

unreal.log("GREAT_ROOM_PILLOW_FIX_COMPLETE")
