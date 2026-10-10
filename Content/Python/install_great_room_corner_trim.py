"""Replace only the protruding upstairs wall mesh in Chapter 1."""

from pathlib import Path
import unreal


project = Path(unreal.Paths.project_dir())
source = project / "SourceArt/GreatRoomCorner/SM_Upper_p0_p0_CornerCleared.fbx"
package = "/Game/SeniorSendOff/House/GreatRoomCorner"
name = "SM_Upper_p0_p0_CornerCleared"
original_path = "/Game/SeniorSendOff/House/Meshes/SM_Upper_p0_p0_Solid"
original = unreal.load_asset(original_path)
if not isinstance(original, unreal.StaticMesh):
    raise RuntimeError("Original upstairs wall mesh is unavailable")
if not source.is_file():
    raise RuntimeError("Trimmed upstairs source is unavailable")

original_slots = original.get_editor_property("static_materials")
materials = {
    str(slot.get_editor_property("imported_material_slot_name")): original.get_material(index)
    for index, slot in enumerate(original_slots)
}
unreal.EditorAssetLibrary.make_directory(package)
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
ui = unreal.FbxImportUI()
ui.import_mesh = True
ui.import_as_skeletal = False
ui.import_materials = False
ui.import_textures = False
ui.static_mesh_import_data.auto_generate_collision = False
ui.static_mesh_import_data.combine_meshes = True
ui.static_mesh_import_data.reorder_material_to_fbx_order = True
ui.static_mesh_import_data.normal_import_method = (
    unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
ui.static_mesh_import_data.normal_generation_method = (
    unreal.FBXNormalGenerationMethod.BUILT_IN)
task = unreal.AssetImportTask()
task.factory = unreal.FbxFactory()
task.filename = str(source)
task.destination_path = package
task.destination_name = name
task.automated = True
task.replace_existing = True
task.save = False
task.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(package + "/" + name)
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError("Corner-cleared mesh import failed")
new_slots = mesh.get_editor_property("static_materials")
new_names = [str(slot.get_editor_property("imported_material_slot_name"))
             for slot in new_slots]
source_names = [name.removeprefix("MI_") for name in new_names]
if not set(source_names).issubset(materials):
    raise RuntimeError("Corner-cleared material slots differ: " +
                       repr((new_names, list(materials))))
for index, slot_name in enumerate(source_names):
    mesh.set_material(index, materials[slot_name])
old_bounds = original.get_bounds()
new_bounds = mesh.get_bounds()
for axis in ("x", "y", "z"):
    if abs(getattr(old_bounds.box_extent, axis) - getattr(new_bounds.box_extent, axis)) > 40:
        raise RuntimeError("Unexpected architectural bounds change on " + axis)
mesh.get_editor_property("body_setup").set_editor_property(
    "collision_trace_flag", original.get_editor_property("body_setup").get_editor_property(
        "collision_trace_flag"))
if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
    raise RuntimeError("Could not save corner-cleared mesh")
unreal.log("GREAT_ROOM_CORNER_ASSET imported={} materials={}".format(name, new_names))

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Story/Maps/Chapter01_House")
actors = unreal.EditorLevelLibrary.get_all_level_actors()
matching = [actor for actor in actors if actor.get_actor_label() == "SM_Upper_p0_p0_Solid"]
if len(matching) != 1 or not isinstance(matching[0], unreal.StaticMeshActor):
    raise RuntimeError("Expected one original upstairs wall actor")
actor = matching[0]
current = actor.static_mesh_component.static_mesh
overrides = actor.static_mesh_component.get_editor_property("override_materials")
unreal.log("GREAT_ROOM_CORNER_OVERRIDES {}".format(
    [item.get_path_name() if item else "None" for item in overrides]))
if current.get_path_name().split(".")[0] not in (original_path, package + "/" + name):
    raise RuntimeError("Upstairs wall actor has an unexpected existing mesh")
before = actor.get_actor_transform()
actor.static_mesh_component.set_static_mesh(mesh)
after = actor.get_actor_transform()
if not before.equals(after):
    raise RuntimeError("Upstairs wall actor transform changed")
if len(unreal.EditorLevelLibrary.get_all_level_actors()) != len(actors):
    raise RuntimeError("Corner trim changed the level actor count")
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save Chapter 1 with the corner trim")
unreal.log("GREAT_ROOM_CORNER_TRIM_SAVED actor={} mesh={} actors={}".format(
    actor.get_actor_label(), mesh.get_path_name(), len(actors)))
