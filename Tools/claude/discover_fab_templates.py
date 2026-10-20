"""Read-only: records how Fab's "Add to project" built the Megascans assets under /Game/Environment/Megascans (the Beech
Fern for the lady fern, the Nordic Forest Cliff for the mossy boulder) and the Animal Variety Pack pig material, so
import_fab_extras.py can match them. Never saves. Writes $FAB_TEMPLATES_OUT/fab_templates.json."""
import json
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def safe(fn):
    try:
        return fn()
    except Exception as e:  # noqa: BLE001
        return "ERR: %s" % e


def name(v):
    return getattr(v, "name", str(v))


def props(obj, keys):
    return {k: safe(lambda k=k: str(obj.get_editor_property(k))) for k in keys}


def texture(t):
    d = props(t, ["compression_settings", "srgb", "lod_group", "max_texture_size", "mip_gen_settings",
                  "virtual_texture_streaming", "never_stream", "flip_green_channel"])
    d["size"] = safe(lambda: [t.blueprint_get_size_x(), t.blueprint_get_size_y()])
    d["source"] = safe(lambda: [f.get_editor_property("relative_filename") for f in
                                 t.get_editor_property("asset_import_data").get_editor_property("source_data").get_editor_property("source_files")])
    return d


def material(m):
    d = {"class": m.get_class().get_name()}
    d["texture_params"] = safe(lambda: [str(n) for n in mel.get_texture_parameter_names(m)])
    d["scalar_params"] = safe(lambda: [str(n) for n in mel.get_scalar_parameter_names(m)])
    d["vector_params"] = safe(lambda: [str(n) for n in mel.get_vector_parameter_names(m)])
    d["used_textures"] = safe(lambda: [t.get_path_name() if t else None for t in mel.get_used_textures(m)])
    if isinstance(m, unreal.Material):
        d.update(props(m, ["blend_mode", "shading_model", "two_sided", "opacity_mask_clip_value",
                           "used_with_instanced_static_meshes", "used_with_nanite", "used_with_skeletal_mesh"]))
        d["num_expressions"] = safe(lambda: mel.get_num_material_expressions(m))
    else:
        d["parent"] = safe(lambda: m.get_editor_property("parent").get_path_name())
    return d


def mesh(sm):
    d = {"lods": safe(lambda: sm.get_num_lods()), "bounds": safe(lambda: str(sm.get_bounds().box_extent))}
    d["nanite"] = safe(lambda: str(sm.get_editor_property("nanite_settings")))
    d["materials"] = safe(lambda: [(str(s.material_slot_name), s.material_interface.get_path_name() if s.material_interface else None)
                                    for s in sm.get_editor_property("static_materials")])
    d["collision"] = safe(lambda: [str(sm.get_editor_property("body_setup").get_editor_property("collision_trace_flag")),
                                   sms.get_simple_collision_count(sm)])
    d["collision_profile"] = safe(lambda: str(sm.get_editor_property("body_setup").get_editor_property("default_instance").get_editor_property("collision_profile_name")))
    d["lightmap"] = safe(lambda: [sm.get_editor_property("light_map_resolution"), sm.get_editor_property("light_map_coordinate_index")])
    d["lod_screen_sizes"] = safe(lambda: [str(x) for x in sms.get_lod_screen_sizes(sm)])
    d["source"] = safe(lambda: [f.get_editor_property("relative_filename") for f in
                                 sm.get_editor_property("asset_import_data").get_editor_property("source_data").get_editor_property("source_files")])
    d["import_data_class"] = safe(lambda: sm.get_editor_property("asset_import_data").get_class().get_name())
    return d


def folder(path):
    out = {}
    for p in sorted(eal.list_assets(path, recursive=True, include_folder=False)):
        a = eal.load_asset(p)
        if a is None:
            out[p] = "LOAD FAILED"
        elif isinstance(a, unreal.Texture2D):
            out[p] = texture(a)
        elif isinstance(a, unreal.MaterialInterface):
            out[p] = material(a)
        elif isinstance(a, unreal.StaticMesh):
            out[p] = mesh(a)
        else:
            out[p] = a.get_class().get_name()
    return out


report = {
    "mel_api": [n for n in dir(mel) if "expression" in n or "texture" in n.lower()],
    "sms_api": [n for n in dir(sms) if "lod" in n.lower() or "nanite" in n.lower() or "collision" in n.lower()],
    "beech_fern": folder("/Game/Environment/Megascans/BeechFern_vmkpdbeia"),
    "cliff": folder("/Game/Environment/Megascans/NordicForestCliffLarge_xibsff1"),
    "pig_material": {p: material(eal.load_asset(p)) for p in eal.list_assets("/Game/AnimalVarietyPack/Pig/Materials", recursive=True)
                     if isinstance(eal.load_asset(p), unreal.MaterialInterface)},
    "interchange": safe(lambda: str(unreal.SystemLibrary.get_console_variable_int_value("Interchange.FeatureFlags.Import.FBX"))),
}
out_dir = os.environ.get("FAB_TEMPLATES_OUT", "/tmp")
path = os.path.join(out_dir, "fab_templates.json")
if os.path.exists(path):
    os.remove(path)
with open(path, "w") as f:
    json.dump(report, f, indent=1, default=str)
unreal.log("FAB_TEMPLATES written %s" % path)
