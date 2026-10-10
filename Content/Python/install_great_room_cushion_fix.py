"""Reimport only the five fitted great-room sofa meshes and damage stages."""

import unreal
from pathlib import Path


source = Path(unreal.Paths.project_dir()) / "SourceArt/HouseInteractivity/GreatRoomCushionFix"
package = "/Game/SeniorSendOff/House/Interactivity"
sofas = {
    "HI_SM_GR3_NearSeating_001": 20,
    "HI_SM_GR3_NearSeating_003": 16,
    "HI_SM_GR3_FarSeating_001": 20,
    "HI_SM_GR3_FarSeating_002": 16,
    "HI_SM_GR3_FarSeating_003": 16,
}
editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
# UE 5.8's Interchange reimport drops UCX collision on existing damage meshes.
# The FBX factory path used to create them retains their per-cushion hulls.
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")


for base, expected_hulls in sofas.items():
    for suffix in ("", "_DamageV2", "_HeavyV2"):
        name = base + suffix
        old = unreal.load_asset(package + "/" + name)
        if not isinstance(old, unreal.StaticMesh):
            raise RuntimeError("Missing current great-room sofa: " + name)
        slots = old.get_editor_property("static_materials")
        materials = [old.get_material(i) for i in range(len(slots))]
        slot_names = [str(slot.get_editor_property("imported_material_slot_name")) for slot in slots]
        fbx = source / (name + ".fbx")
        if not fbx.is_file():
            raise RuntimeError("Missing fitted sofa source: " + str(fbx))

        # The previous imported convex hulls must not stack with the new ones.
        editor.remove_collisions(old)
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
            raise RuntimeError("Great-room sofa import failed: " + name)
        new_slots = mesh.get_editor_property("static_materials")
        new_names = [str(slot.get_editor_property("imported_material_slot_name")) for slot in new_slots]
        if new_names != slot_names:
            raise RuntimeError("Great-room sofa material slots changed: " + name + " " + repr(new_names))
        for index, material in enumerate(materials):
            if material:
                mesh.set_material(index, material)
        setup = mesh.get_editor_property("body_setup")
        setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
        hulls = editor.get_convex_collision_count(mesh)
        if hulls != expected_hulls:
            raise RuntimeError(f"Great-room sofa {name} has {hulls} hulls, expected {expected_hulls}")
        if not assets.save_loaded_asset(mesh):
            raise RuntimeError("Could not save great-room sofa: " + name)
        unreal.log(f"GREAT_ROOM_CUSHION_FIX imported={name} hulls={hulls} materials={len(new_slots)}")

unreal.log("GREAT_ROOM_CUSHION_FIX_COMPLETE")
