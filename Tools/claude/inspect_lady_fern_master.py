"""Read-only: inspects the Megascans foliage master /Game/MSPresets/MS_Foliage_Material/MasterMaterials/MA_Foliage (and
how the user's Fab tree packs instance it) so the lady-fern import can match its parameters and sampler types.
Never saves anything. Run headless (one Unreal process at a time):
  nice -n 19 UnrealEditor-Cmd "<uproject>" -run=pythonscript -script="<this file>" -unattended -nosplash -nopause
      -NoTraceServer -NullRHI -UserDir=<scratch user dir>
Writes $LADY_FERN_OUT (default <project>/Saved/ClaudeDump)/inspect_lady_fern_master.json."""
import json
import os

import unreal

MASTER = "/Game/MSPresets/MS_Foliage_Material/MasterMaterials/MA_Foliage"
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def safe(fn, default=None):
    try:
        return fn()
    except Exception as e:  # noqa: BLE001
        return default if default is not None else "ERR: %s" % e


def enum_name(v):
    return getattr(v, "name", str(v))


def val(v):
    if isinstance(v, float):
        return round(v, 4)
    if isinstance(v, unreal.LinearColor):
        return [round(v.r, 4), round(v.g, 4), round(v.b, 4), round(v.a, 4)]
    if hasattr(v, "get_path_name"):
        return v.get_path_name()
    if hasattr(v, "name"):
        return v.name
    return v


def expressions_of(owner_path):
    """All loaded material expressions whose outer chain contains owner_path (material or function)."""
    out = []
    for e in unreal.ObjectIterator(unreal.MaterialExpression):
        p = e.get_path_name()
        if p.startswith(owner_path + ".") or p.startswith(owner_path + ":"):
            out.append(e)
    return out


def param_nodes(owner_path):
    rows = []
    for e in expressions_of(owner_path):
        cls = e.get_class().get_name()
        name = safe(lambda: e.get_editor_property("parameter_name"), None)
        if name is None or isinstance(name, str) and name.startswith("ERR"):
            continue
        row = {"class": cls, "name": str(name), "group": str(safe(lambda: e.get_editor_property("group"), "")),
               "desc": str(safe(lambda: e.get_editor_property("desc"), ""))[:160]}
        for k in ("default_value", "texture", "sampler_type", "sampler_source", "channel_names"):
            v = safe(lambda k=k: e.get_editor_property(k), "n/a")
            if v != "n/a" and not (isinstance(v, str) and v.startswith("ERR")):
                row[k] = val(v) if k != "channel_names" else str(v)[:200]
        if "texture" in row and row["texture"]:
            tex = unreal.load_asset(row["texture"].split(".")[0])
            if tex:
                row["texture_compression"] = enum_name(tex.get_editor_property("compression_settings"))
                row["texture_srgb"] = tex.get_editor_property("srgb")
        rows.append(row)
    return sorted(rows, key=lambda r: (r["class"], r["group"], r["name"]))


def function_calls(owner_path):
    rows = []
    for e in expressions_of(owner_path):
        if e.get_class().get_name() == "MaterialExpressionMaterialFunctionCall":
            f = safe(lambda: e.get_editor_property("material_function"))
            rows.append(val(f))
    return sorted(set(str(r) for r in rows if r))


def function_tree(owner_path, seen=None):
    """Recursively: {function path: [param nodes]} for every material function reachable from owner_path."""
    seen = {} if seen is None else seen
    for f in function_calls(owner_path):
        fpath = f.split(".")[0]
        if fpath in seen:
            continue
        unreal.load_asset(fpath)
        seen[fpath] = param_nodes(fpath)
        function_tree(fpath, seen)
    return seen


def main():
    out_dir = os.environ.get("LADY_FERN_OUT") or os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "ClaudeDump")
    report = {}
    mat = unreal.load_asset(MASTER)
    report["master_loaded"] = mat is not None
    if mat:
        info = {}
        for k in ("blend_mode", "shading_model", "two_sided", "opacity_mask_clip_value", "dithered_lod_transition",
                  "used_with_instanced_static_meshes", "used_with_static_mesh", "used_with_nanite", "used_with_skeletal_mesh",
                  "used_with_static_lighting", "automatically_set_usage_in_editor", "tangent_space_normal",
                  "material_domain", "translucency_lighting_mode", "is_thin_surface", "always_evaluate_world_position_offset",
                  "max_world_position_offset_displacement"):
            info[k] = val(safe(lambda k=k: mat.get_editor_property(k), "n/a"))
        info["scalar_params"] = safe(lambda: [str(n) for n in mel.get_scalar_parameter_names(mat)], [])
        info["vector_params"] = safe(lambda: [str(n) for n in mel.get_vector_parameter_names(mat)], [])
        info["texture_params"] = safe(lambda: [str(n) for n in mel.get_texture_parameter_names(mat)], [])
        info["switch_params"] = safe(lambda: [str(n) for n in mel.get_static_switch_parameter_names(mat)], [])
        info["param_nodes"] = param_nodes(mat.get_path_name().split(".")[0])
        info["function_calls"] = function_calls(mat.get_path_name().split(".")[0])
        info["function_params"] = {k: v for k, v in function_tree(mat.get_path_name().split(".")[0]).items() if v}
        stats = safe(lambda: mel.get_statistics(mat))
        info["statistics"] = (None if stats is None or isinstance(stats, str) else
                              {k: safe(lambda k=k: stats.get_editor_property(k)) for k in (
                                  "num_pixel_shader_instructions", "num_vertex_shader_instructions", "num_samplers",
                                  "num_pixel_texture_samples")})
        report["master"] = info
    # Every MaterialInstanceConstant in the project that derives from MA_Foliage, with its overrides.
    children = []
    for ad in registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "MaterialInstanceConstant"), True):
        pkg = str(ad.package_name)
        if not pkg.startswith("/Game/") or pkg.startswith("/Game/MSPresets/MS_Foliage_Material/GlobalFoliageActor"):
            continue
        tags = safe(lambda: str(ad.get_tag_value("Parent")), "")
        if "MA_Foliage" not in (tags or ""):
            continue
        mi = unreal.load_asset(pkg)
        if not mi:
            continue
        row = {"path": pkg, "parent": val(mi.get_editor_property("parent"))}
        row["textures"] = [[str(p.get_editor_property("parameter_info").get_editor_property("name")),
                            val(p.get_editor_property("parameter_value"))] for p in mi.get_editor_property("texture_parameter_values")]
        for t in row["textures"]:
            tex = unreal.load_asset(str(t[1]).split(".")[0]) if t[1] else None
            if tex:
                t.append("%s srgb=%s %dx%d flipG=%s" % (enum_name(tex.get_editor_property("compression_settings")),
                                                       tex.get_editor_property("srgb"), tex.blueprint_get_size_x(),
                                                       tex.blueprint_get_size_y(), tex.get_editor_property("flip_green_channel")))
        row["scalars"] = [[str(p.get_editor_property("parameter_info").get_editor_property("name")),
                           val(p.get_editor_property("parameter_value"))] for p in mi.get_editor_property("scalar_parameter_values")]
        row["vectors"] = [[str(p.get_editor_property("parameter_info").get_editor_property("name")),
                           val(p.get_editor_property("parameter_value"))] for p in mi.get_editor_property("vector_parameter_values")]
        sp = safe(lambda: mi.get_editor_property("static_parameters"), None)
        row["static_switches"] = []
        if sp is not None and not isinstance(sp, str):
            for s in safe(lambda: list(sp.get_editor_property("static_switch_parameters")), []):
                row["static_switches"].append([str(s.get_editor_property("parameter_info").get_editor_property("name")),
                                               s.get_editor_property("value"), s.get_editor_property("override")])
        children.append(row)
    report["children"] = children
    # Nanite / LOD settings of a few existing Fab foliage meshes for consistency.
    meshes = []
    for root in ("/Game/EuropeanBeech", "/Game/NorwayMaple", "/Game/Environment/Megascans"):
        for ad in registry.get_assets_by_path(root, recursive=True):
            if str(ad.asset_class_path.get_editor_property("asset_name")) != "StaticMesh":
                continue
            m = unreal.load_asset(str(ad.package_name))
            ns = m.get_editor_property("nanite_settings")
            meshes.append({"path": str(ad.package_name), "lods": m.get_num_lods(),
                           "nanite": ns.get_editor_property("enabled"),
                           "shape": enum_name(ns.get_editor_property("shape_preservation")),
                           "mats": [val(s.get_editor_property("material_interface")) for s in m.get_editor_property("static_materials")]})
            if len(meshes) > 40:
                break
    report["fab_meshes"] = meshes
    path = os.path.join(out_dir, "inspect_lady_fern_master.json")
    if os.path.exists(path):
        os.remove(path)
    with open(path, "w") as f:
        json.dump(report, f, indent=1, default=str)
    unreal.log("LADY_FERN_INSPECT wrote %s (master_loaded=%s children=%d meshes=%d)" % (
        path, report["master_loaded"], len(children), len(meshes)))


main()
