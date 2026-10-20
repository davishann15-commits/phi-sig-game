"""Read-only probe of the Fab Norway spruce under /Game/Megaplant_Library (never saves, never opens a map).

Loads every asset under /Game/Megaplant_Library and reports, per asset: registry class vs loaded class; for material
instances the full parent chain (and whether it resolves to a UMaterial) plus texture parameters; for skeletal and static
meshes their material slots, LOD count / screen sizes / vertex counts, bounds (height in cm), Nanite settings, Nanite
assembly parts and node count, asset user data (DynamicWind skeletal wind data); for textures size/compression/sRGB; the
PVE wind settings and the PVE master material; the external package dependencies; and the Nanite / DynamicWind cvars.

Run it twice (headless, one Unreal process at a time) and compare the two reports:
  nice -n 19 /Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd "<uproject>" -run=pythonscript -script="<this file>"
      -unattended -nosplash -nopause -NoTraceServer -NullRHI -stdout -FullStdOutLogOutput -UserDir=<scratch user dir>
  ... the same with  -EnablePlugins=ProceduralVegetationEditor,DynamicWind  (enables them for that process only).
Output: $SPRUCE_PROBE_OUT (default <project>/Saved/ClaudeDump)/probe_spruce_<mode>.json and .txt, where <mode> is
with_plugins / without_plugins, plus _naniteFoliage when r.Nanite.Foliage=1 in that process.
"""
import json
import os

import unreal

ROOT = "/Game/Megaplant_Library"
PVE_ASSETS = [
    "/ProceduralVegetationEditor/SampleAssets/WindSettings/DefaultTreeWindSettings",
    "/ProceduralVegetationEditor/SampleAssets/WindSettings/DefaultSaplingWindSettings",
    "/ProceduralVegetationEditor/SampleAssets/WindSettings/DefaultGroundScatterWindSettings",
    "/ProceduralVegetationEditor/SampleAssets/Materials/MasterMaterials/MA_Foliage_Trees",
]
CVARS = ["r.Nanite", "r.Nanite.Foliage", "r.Nanite.AllowAssemblies", "r.Nanite.AllowVoxels",
         "r.Nanite.AllowSkinnedMeshes", "r.Nanite.ProjectEnabled", "DynamicWind.Enable", "r.Shadow.Virtual.Enable",
         "r.SkinCache.CompileShaders"]
USAGE_FLAGS = ["used_with_skeletal_mesh", "used_with_instanced_static_meshes", "used_with_nanite", "used_with_voxels",
               "used_with_instanced_skinned_mesh", "used_with_static_mesh", "used_with_static_lighting",
               "automatically_set_usage_in_editor"]

registry = unreal.AssetRegistryHelpers.get_asset_registry()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("SPRUCE_PROBE " + msg)


def warn(msg):
    unreal.log_warning("SPRUCE_PROBE_WARN " + msg)


def path_of(obj):
    return obj.get_path_name() if obj else None


def safe(fn, default=None):
    try:
        return fn()
    except Exception as e:  # noqa: BLE001
        return default if default is not None else "ERR: %s" % e


def vec(v):
    return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]


def bounds_info(b):
    if b is None or isinstance(b, str):
        return b
    o, e = b.origin, b.box_extent
    return {"origin": vec(o), "extent": vec(e), "sphere_radius": round(b.sphere_radius, 2),
            "min_z_cm": round(o.z - e.z, 2), "max_z_cm": round(o.z + e.z, 2), "height_cm": round(2 * e.z, 2),
            "width_x_cm": round(2 * e.x, 2), "width_y_cm": round(2 * e.y, 2)}


def enum_name(v):
    return getattr(v, "name", str(v))


def nanite_info(owner):
    ns = safe(lambda: owner.get_editor_property("nanite_settings"))
    if ns is None or isinstance(ns, str):
        return {"error": ns}
    out = {}
    for k in ("enabled", "shape_preservation", "voxel_ndf", "voxel_opacity", "keep_percent_triangles",
              "trim_relative_error", "generate_fallback", "fallback_target", "fallback_percent_triangles",
              "fallback_relative_error", "max_edge_length_factor", "voxel_level", "num_rays"):
        val = safe(lambda k=k: ns.get_editor_property(k))
        out[k] = enum_name(val) if hasattr(val, "name") else (round(val, 4) if isinstance(val, float) else val)
    asm = safe(lambda: ns.get_editor_property("nanite_assembly_data"))
    if asm is not None and not isinstance(asm, str):
        parts = safe(lambda: list(asm.get_editor_property("parts")), [])
        out["assembly_parts"] = [str(p.get_editor_property("mesh_object_path")) for p in parts] if isinstance(parts, list) else parts
        text = safe(lambda: asm.export_text(), "")
        out["assembly_node_count"] = text.count("PartIndex=") if isinstance(text, str) else None
        out["assembly_bone_relative_nodes"] = text.count("BoneRelative") if isinstance(text, str) else None
    return out


def user_data_info(owner):
    items = safe(lambda: list(owner.get_editor_property("asset_user_data")), [])
    if isinstance(items, str):
        return [items]
    out = []
    for ud in items:
        if ud is None:
            out.append({"class": None, "note": "null entry (class could not be loaded)"})
            continue
        entry = {"class": ud.get_class().get_path_name()}
        for k in ("is_enabled", "is_ground_cover", "gust_attenuation"):
            val = safe(lambda k=k: ud.get_editor_property(k), "n/a")
            entry[k] = round(val, 4) if isinstance(val, float) else val
        groups = safe(lambda: list(ud.get_editor_property("simulation_groups")), None)
        if isinstance(groups, list):
            entry["simulation_groups"] = [
                {k: (round(g.get_editor_property(k), 3) if isinstance(g.get_editor_property(k), float)
                     else g.get_editor_property(k))
                 for k in ("use_dual_influence", "influence", "min_influence", "max_influence", "shift_top",
                           "is_trunk_group")} for g in groups]
        out.append(entry)
    return out


def material_chain(mi):
    chain = []
    cur = mi
    seen = set()
    while cur is not None and cur.get_path_name() not in seen:
        seen.add(cur.get_path_name())
        chain.append({"path": cur.get_path_name(), "class": cur.get_class().get_name()})
        if isinstance(cur, unreal.MaterialInstance):
            cur = safe(lambda c=cur: c.get_editor_property("parent"))
            if isinstance(cur, str):
                chain.append({"error": cur})
                cur = None
        else:
            cur = None
    base = chain[-1] if chain else None
    resolved = bool(base and base.get("class") in ("Material",))
    return chain, resolved


def base_material_info(mat):
    out = {"blend_mode": enum_name(safe(lambda: mat.get_editor_property("blend_mode"))),
           "shading_model": enum_name(safe(lambda: mat.get_editor_property("shading_model"))),
           "two_sided": safe(lambda: mat.get_editor_property("two_sided"))}
    for f in USAGE_FLAGS:
        out[f] = safe(lambda f=f: mat.get_editor_property(f), "n/a")
    out["scalar_params"] = safe(lambda: [str(n) for n in mel.get_scalar_parameter_names(mat)], [])
    out["vector_params"] = safe(lambda: [str(n) for n in mel.get_vector_parameter_names(mat)], [])
    out["texture_params"] = safe(lambda: [str(n) for n in mel.get_texture_parameter_names(mat)], [])
    out["switch_params"] = safe(lambda: [str(n) for n in mel.get_static_switch_parameter_names(mat)], [])
    return out


def describe_mi(mi):
    chain, resolved = material_chain(mi)
    info = {"parent_chain": chain, "resolves_to_material": resolved}
    info["texture_values"] = safe(lambda: [
        [str(p.get_editor_property("parameter_info").get_editor_property("name")),
         path_of(p.get_editor_property("parameter_value"))] for p in mi.get_editor_property("texture_parameter_values")], [])
    info["scalar_values"] = safe(lambda: [
        [str(p.get_editor_property("parameter_info").get_editor_property("name")),
         round(p.get_editor_property("parameter_value"), 4)] for p in mi.get_editor_property("scalar_parameter_values")], [])
    info["switch_overrides"] = safe(lambda: [str(n) for n in mel.get_static_switch_parameter_names(mi)], [])
    if resolved:
        base = unreal.load_asset(chain[-1]["path"].split(":")[0])
        info["base_material"] = base_material_info(base) if base else None
    return info


def bone_count(mesh):
    comp = safe(lambda: unreal.SkeletalMeshComponent())
    if comp is None or isinstance(comp, str):
        return comp
    for setter in ("set_skinned_asset_and_update", "set_skeletal_mesh_asset", "set_skeletal_mesh"):
        if hasattr(comp, setter):
            r = safe(lambda s=setter: getattr(comp, s)(mesh))
            if not isinstance(r, str):
                break
    return safe(lambda: comp.get_num_bones())


def describe_skeletal(mesh):
    sk = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    info = {}
    mats = safe(lambda: list(mesh.get_editor_property("materials")), [])
    info["materials"] = [[str(m.get_editor_property("material_slot_name")), path_of(m.get_editor_property("material_interface"))]
                         for m in mats] if isinstance(mats, list) else mats
    lods = safe(lambda: list(mesh.get_editor_property("lod_info")), [])
    info["lod_count"] = safe(lambda: sk.get_lod_count(mesh), len(lods) if isinstance(lods, list) else None)
    info["lod_screen_sizes"] = safe(lambda: [round(l.get_editor_property("screen_size").get_editor_property("default"), 4)
                                             for l in lods], [])
    info["lod_vertices"] = safe(lambda: [sk.get_num_verts(mesh, i) for i in range(int(info["lod_count"]))], [])
    info["bounds"] = bounds_info(safe(lambda: mesh.get_bounds()))
    info["imported_bounds"] = bounds_info(safe(lambda: mesh.get_imported_bounds()))
    info["skeleton"] = path_of(safe(lambda: mesh.get_editor_property("skeleton")))
    info["physics_asset"] = path_of(safe(lambda: mesh.get_editor_property("physics_asset")))
    info["bones"] = bone_count(mesh)
    info["nanite"] = nanite_info(mesh)
    info["asset_user_data"] = user_data_info(mesh)
    return info


def describe_static(mesh):
    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    info = {}
    mats = safe(lambda: list(mesh.get_editor_property("static_materials")), [])
    info["materials"] = [[str(m.get_editor_property("material_slot_name")), path_of(m.get_editor_property("material_interface"))]
                         for m in mats] if isinstance(mats, list) else mats
    info["lod_count"] = safe(lambda: mesh.get_num_lods())
    info["lod_vertices"] = safe(lambda: [sm.get_number_verts(mesh, i) for i in range(mesh.get_num_lods())], [])
    info["lod0_triangles"] = safe(lambda: mesh.get_num_triangles(0))
    info["bounds"] = bounds_info(safe(lambda: mesh.get_bounds()))
    info["nanite"] = nanite_info(mesh)
    info["asset_user_data"] = user_data_info(mesh)
    return info


def describe_texture(tex):
    return {"size": [safe(lambda: tex.blueprint_get_size_x()), safe(lambda: tex.blueprint_get_size_y())],
            "compression": enum_name(safe(lambda: tex.get_editor_property("compression_settings"))),
            "srgb": safe(lambda: tex.get_editor_property("srgb")),
            "lod_group": enum_name(safe(lambda: tex.get_editor_property("lod_group"))),
            "max_texture_size": safe(lambda: tex.get_editor_property("max_texture_size"))}


def describe_generic(obj):
    info = {}
    for k in ("simulation_group_data", "overwrite_existing", "graph"):
        val = safe(lambda k=k: obj.get_editor_property(k), "n/a")
        if val == "n/a" or isinstance(val, str):
            continue
        if k == "simulation_group_data":
            info[k] = [{kk: (round(g.get_editor_property(kk), 3) if isinstance(g.get_editor_property(kk), float)
                             else g.get_editor_property(kk))
                        for kk in ("use_dual_influence", "influence", "min_influence", "max_influence", "shift_top",
                                   "is_trunk_group")} for g in val]
        else:
            info[k] = path_of(val) if hasattr(val, "get_path_name") else val
    return info


def describe(obj):
    if isinstance(obj, unreal.MaterialInstance):
        return describe_mi(obj)
    if isinstance(obj, unreal.Material):
        return base_material_info(obj)
    if isinstance(obj, unreal.SkeletalMesh):
        return describe_skeletal(obj)
    if isinstance(obj, unreal.StaticMesh):
        return describe_static(obj)
    if isinstance(obj, unreal.Texture2D):
        return describe_texture(obj)
    return describe_generic(obj)


def external_deps(package_name):
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                 include_hard_package_references=True,
                                                 include_searchable_names=False,
                                                 include_soft_management_references=False,
                                                 include_hard_management_references=False)
    deps = safe(lambda: registry.get_dependencies(package_name, opts), []) or []
    return sorted(str(d) for d in deps if not str(d).startswith(ROOT))


def cvar_values():
    out = {}
    for name in CVARS:
        out[name] = safe(lambda n=name: unreal.SystemLibrary.get_console_variable_int_value(n))
    return out


def main():
    plugins = hasattr(unreal, "DynamicWindSkeletalData") or hasattr(unreal, "PVWindSettings")
    cvars = cvar_values()
    mode = "with_plugins" if plugins else "without_plugins"
    if cvars.get("r.Nanite.Foliage") == 1:
        mode += "_naniteFoliage"
    out_dir = os.environ.get("SPRUCE_PROBE_OUT") or os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "ClaudeDump")
    os.makedirs(out_dir, exist_ok=True)
    report = {"mode": mode, "command_line": unreal.SystemLibrary.get_command_line(), "cvars": cvars,
              "python_classes": {n: hasattr(unreal, n) for n in (
                  "DynamicWindSkeletalData", "DynamicWindData", "DynamicWindSubsystem", "PVWindSettings",
                  "ProceduralVegetation", "InstancedSkinnedMeshComponent")},
              "assets": {}, "pve_assets": {}, "external_dependencies": {}, "load_failures": []}
    log("mode=%s cvars=%s" % (mode, cvars))

    registry.scan_paths_synchronous([ROOT], True)
    ads = sorted(registry.get_assets_by_path(ROOT, recursive=True), key=lambda a: str(a.package_name))
    log("registry assets under %s: %d" % (ROOT, len(ads)))
    all_ext = {}
    for ad in ads:
        pkg = str(ad.package_name)
        obj_path = "%s.%s" % (pkg, ad.asset_name)
        cp = ad.asset_class_path
        reg_class = "%s.%s" % (cp.get_editor_property("package_name"), cp.get_editor_property("asset_name"))
        entry = {"registry_class": reg_class}
        obj = safe(lambda: unreal.load_asset(obj_path))
        if obj is None or isinstance(obj, str):
            entry["loaded"] = False
            entry["load_error"] = obj
            report["load_failures"].append(obj_path)
            warn("could not load %s (%s)" % (obj_path, reg_class))
        else:
            entry["loaded"] = True
            entry["loaded_class"] = obj.get_class().get_path_name()
            entry.update(describe(obj))
        ext = external_deps(pkg)
        entry["external_deps"] = ext
        for d in ext:
            all_ext.setdefault(d, []).append(pkg.split("/")[-1])
        report["assets"][obj_path] = entry
        log("%s %s loaded=%s" % (obj_path, reg_class, entry["loaded"]))
    report["external_dependencies"] = {k: sorted(set(v)) for k, v in sorted(all_ext.items())}

    for p in PVE_ASSETS:
        obj = safe(lambda p=p: unreal.load_asset(p))
        if obj is None or isinstance(obj, str):
            report["pve_assets"][p] = {"loaded": False, "load_error": obj}
        else:
            report["pve_assets"][p] = dict({"loaded": True, "class": obj.get_class().get_path_name()}, **describe(obj))

    dirty = safe(lambda: [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()], [])
    report["dirty_packages_not_saved"] = dirty

    jpath = os.path.join(out_dir, "probe_spruce_%s.json" % mode)
    for p in (jpath, jpath[:-5] + ".txt"):
        if os.path.exists(p):
            os.remove(p)   # iCloud-safe rewrite (delete first)
    with open(jpath, "w") as f:
        json.dump(report, f, indent=1, default=str)
    with open(jpath[:-5] + ".txt", "w") as f:
        f.write(summary(report))
    log("wrote %s" % jpath)
    log("DONE mode=%s assets=%d load_failures=%d dirty(not saved)=%d" % (
        mode, len(report["assets"]), len(report["load_failures"]), len(dirty) if isinstance(dirty, list) else -1))


def summary(r):
    lines = ["mode: %s" % r["mode"], "cvars: %s" % r["cvars"], "python classes: %s" % r["python_classes"],
             "load failures: %s" % r["load_failures"], ""]
    for path, e in r["assets"].items():
        name = path.split(".")[-1]
        cls = e.get("loaded_class", "NOT LOADED (%s)" % e["registry_class"]).split(".")[-1]
        if "parent_chain" in e:
            chain = " -> ".join((c.get("path") or c.get("error", "?")).split(".")[0] for c in e["parent_chain"][1:]) or "NONE"
            lines.append("%-44s %-26s parent: %s resolved=%s" % (name, cls, chain, e["resolves_to_material"]))
        elif cls in ("SkeletalMesh", "StaticMesh"):
            b = e.get("bounds") or {}
            nan = e.get("nanite") or {}
            ud = ",".join(str(u.get("class")).split(".")[-1] for u in e.get("asset_user_data") or []) or "-"
            lines.append("%-44s %-26s h=%scm lods=%s nanite=%s/%s parts=%s nodes=%s bones=%s userdata=%s mats=%s" % (
                name, cls, b.get("height_cm") if isinstance(b, dict) else b, e.get("lod_count"), nan.get("enabled"),
                nan.get("shape_preservation"), len(nan.get("assembly_parts") or []), nan.get("assembly_node_count"),
                e.get("bones", "-"), ud, [m[1].split(".")[-1] if m[1] else None for m in e.get("materials") or []]))
        else:
            lines.append("%-44s %-26s" % (name, cls))
    lines += ["", "external dependencies:"] + ["  %s <- %s" % (k, ", ".join(v[:6]) + (" ..." if len(v) > 6 else ""))
                                              for k, v in r["external_dependencies"].items()]
    lines += ["", "PVE assets:"] + ["  %s loaded=%s %s" % (k, v.get("loaded"), v.get("class", v.get("load_error")))
                                    for k, v in r["pve_assets"].items()]
    lines += ["", "dirty packages (NOT saved): %s" % r["dirty_packages_not_saved"]]
    return "\n".join(lines) + "\n"


main()
