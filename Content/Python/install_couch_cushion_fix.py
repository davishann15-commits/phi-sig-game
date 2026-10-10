"""Reimport only the corrected bedroom sofa meshes; never resave the map."""

import unreal
from pathlib import Path


project = Path(unreal.Paths.project_dir())
source = project / "SourceArt/HouseInteractivity/CouchCushionFix"
package = "/Game/SeniorSendOff/House/Interactivity"
base = "HI_BR_SM_Bedroom_BrownReclinerSofa_001"
names = (base, base + "_DamageV2", base + "_HeavyV2")
editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")


for name in names:
    old = unreal.load_asset(package + "/" + name)
    if not isinstance(old, unreal.StaticMesh):
        raise RuntimeError("Missing current sofa asset: " + name)
    old_slots = old.get_editor_property("static_materials")
    old_materials = [old.get_material(i) for i in range(len(old_slots))]
    old_names = [str(slot.get_editor_property("imported_material_slot_name")) for slot in old_slots]
    # Reimport otherwise stacks imported UCX hulls on top of the old solver
    # shapes, leaving a duplicate broad hull around the corrected cushions.
    editor.remove_collisions(old)
    fbx = source / (name + ".fbx")
    if not fbx.is_file():
        raise RuntimeError("Missing corrected sofa FBX: " + str(fbx))

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
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Sofa reimport failed: " + name)
    slots = mesh.get_editor_property("static_materials")
    actual_names = [str(slot.get_editor_property("imported_material_slot_name")) for slot in slots]
    if len(slots) != len(old_slots) or actual_names != old_names:
        raise RuntimeError("Sofa material slots changed during reimport: " + name + " " + repr(actual_names))
    for index, material in enumerate(old_materials):
        if material:
            mesh.set_material(index, material)
    setup = mesh.get_editor_property("body_setup")
    setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    hulls = editor.get_convex_collision_count(mesh) + editor.get_simple_collision_count(mesh)
    if name != base and editor.get_convex_collision_count(mesh) == 0:
        raise RuntimeError("Corrected damaged sofa lost its UCX collision: " + name)
    if name == base and hulls == 0:
        editor.set_convex_decomposition_collisions(mesh, 8, 20, 30000)
        hulls = editor.get_convex_collision_count(mesh) + editor.get_simple_collision_count(mesh)
    if hulls == 0:
        raise RuntimeError("Corrected sofa has no simple collision: " + name)
    if not assets.save_loaded_asset(mesh):
        raise RuntimeError("Could not save corrected sofa: " + name)
    unreal.log(f"COUCH_CUSHION_FIX imported={name} hulls={hulls} materials={len(slots)}")

unreal.log("COUCH_CUSHION_FIX_COMPLETE")
