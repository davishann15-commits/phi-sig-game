"""Import the Calder rigid pig parts into their own folder; no map writes."""
import json
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = ROOT / "SourceArt/CalderLandscape/phase2/pig_parts"
DEST = "/Game/SeniorSendOff/Calder/Pig"
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
plan = json.loads((SOURCE / "parts.json").read_text())
report = []
ASSETS.make_directory(DEST)
for part, row in plan["parts"].items():
    name = "SM_CalderPig_" + part
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / row["file"])
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.static_mesh_import_data.auto_generate_collision = False
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.generate_lightmap_u_vs = False
    ui.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    ui.static_mesh_import_data.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE
    task.options = ui
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(DEST + "/" + name)
    if not mesh:
        raise RuntimeError("Import failed: " + name)
    mesh.set_editor_property("allow_cpu_access", True)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", False)
    mesh.set_editor_property("nanite_settings", nanite)
    for index, slot in enumerate(mesh.static_materials):
        mat_name = str(slot.material_slot_name).split(".")[0]
        material = unreal.load_asset("/Game/AnimalVarietyPack/Pig/Materials/" + mat_name)
        if not material:
            raise RuntimeError("Missing acquired material: " + mat_name)
        mesh.set_material(index, material)
    # Automatic static LODs retain the texture UVs and never enable Nanite.
    options = unreal.StaticMeshReductionOptions()
    reductions = []
    for fraction, screen in [(1.0, 1.0), (.5, .35), (.22, .12)]:
        reduction = unreal.StaticMeshReductionSettings()
        reduction.percent_triangles = fraction
        reduction.screen_size = screen
        reductions.append(reduction)
    options.reduction_settings = reductions
    options.auto_compute_lod_screen_size = False
    lods = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_lods(mesh, options)
    if lods < 3:
        raise RuntimeError("Expected three static LODs: " + name)
    ASSETS.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    lo, hi = b.origin - b.box_extent, b.origin + b.box_extent
    actual = [[lo.x, lo.y, lo.z], [hi.x, hi.y, hi.z]]
    expected = row["bounds_cm"]
    if max(abs(actual[side][axis] - expected[side][axis]) for side in range(2) for axis in range(3)) > .12:
        raise RuntimeError("Imported axes/scale differ from authored part: " + name + " " + str(actual))
    report.append({"part": part, "asset": mesh.get_path_name(), "lods": lods,
                   "bounds_cm": [[lo.x, lo.y, lo.z], [hi.x, hi.y, hi.z]],
                   "material_slots": [str(s.material_slot_name) for s in mesh.static_materials]})
parameters = {}
for name in ["M_Pig", "M_PigDirty", "M_PigHair"]:
    material = unreal.load_asset("/Game/AnimalVarietyPack/Pig/Materials/" + name)
    parameters[name] = {"vectors": [str(v) for v in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)],
                        "textures": [str(v) for v in unreal.MaterialEditingLibrary.get_texture_parameter_names(material)]}
target = SOURCE / "installed.json"
target.unlink(missing_ok=True)
target.write_text(json.dumps({"parts": report, "material_parameters": parameters}, indent=2))
unreal.log("CALDER_PIG_IMPORT PASS parts=%d lods=3 runtimeSkeletons=0 nanite=0" % len(report))
# -ExecutePythonScript exits after the script. Do not request editor shutdown
# from inside a Python frame while asset compilation callbacks are still live.
