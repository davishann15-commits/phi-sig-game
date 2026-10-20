"""Brings the user's extra Megascans downloads into the project the way Fab's "Add to project" builds them, and re-links
the Animal Variety Pack materials. Idempotent. Run headless:
  nice -n 19 UnrealEditor-Cmd "<uproject>" -run=pythonscript -script="<this file>" -unattended -nosplash -nopause
      -NoTraceServer -NullRHI -nosound -UserDir=<scratch>
Env: FAB_EXTRAS_OUT (report folder), FAB_EXTRAS_RAW (staged downloads, default ~/SeniorSendoffFabRaw).

Megascans (matching the Fab-added siblings under /Game/Environment/Megascans, read by discover_fab_templates.py):
  every map as T_<Name>_<Map> with the sibling's texture settings; one material M_<Name> duplicated from the sibling's
  material with its texture samplers swapped; one StaticMesh per variant from the LOD0 FBX (one LOD, Nanite off, no
  collision, like Fab's). Lady Fern copies the Beech Fern; Mossy Forest Boulder copies the Nordic Forest Cliff Large.
Animal Variety Pack: its 17 materials import their textures through /Game/<Animal>/Textures/..., which do not exist
  (the textures live under /Game/AnimalVarietyPack/<Animal>/Textures). Run with temporary [CoreRedirects] package
  redirects in DefaultEngine.ini so they load resolved, then re-save them so the redirects are no longer needed. A
  material is saved only if every texture it samples resolved under /Game/AnimalVarietyPack."""
import glob
import json
import os
import re

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
RAW = os.environ.get("FAB_EXTRAS_RAW", os.path.expanduser("~/SeniorSendoffFabRaw"))
OUT = os.environ.get("FAB_EXTRAS_OUT", "/tmp")
MEGASCANS = "/Game/Environment/Megascans"
ITEMS = [
    {"name": "LadyFern", "folder": "LadyFern_wdvlditia", "raw": "lady_fern_wdvlditia_mid",
     "template": ("BeechFern_vmkpdbeia", "BeechFern"), "mesh_glob": "*_Var*_LOD0.fbx"},
    {"name": "MossyForestBoulder", "folder": "MossyForestBoulder_wfstcdnaw", "raw": "mossy_forest_boulder_wfstcdnaw_high",
     "template": ("NordicForestCliffLarge_xibsff1", "NordicForestCliffLarge"), "mesh_glob": "*.fbx"},
]
ANIMAL_MATERIALS = [
    "/Game/AnimalVarietyPack/Crow/Materials/M_Crow",
    "/Game/AnimalVarietyPack/DeerStagAndDoe/Materials/M_DeerDoe",
    "/Game/AnimalVarietyPack/DeerStagAndDoe/Materials/M_DeerDoeFur",
    "/Game/AnimalVarietyPack/DeerStagAndDoe/Materials/M_DeerStag",
    "/Game/AnimalVarietyPack/DeerStagAndDoe/Materials/M_DeerStagFur",
    "/Game/AnimalVarietyPack/Fox/Materials/M_Fox",
    "/Game/AnimalVarietyPack/Fox/Materials/M_FoxFur",
    "/Game/AnimalVarietyPack/Pig/Materials/M_Pig",
    "/Game/AnimalVarietyPack/Pig/Materials/M_PigDirty",
    "/Game/AnimalVarietyPack/Pig/Materials/M_PigHair",
    "/Game/AnimalVarietyPack/Pig/Materials/M_PigHairDirty",
    "/Game/AnimalVarietyPack/Wolf/Materials/M_Wolf",
    "/Game/AnimalVarietyPack/Wolf/Materials/M_WolfArctic",
    "/Game/AnimalVarietyPack/Wolf/Materials/M_WolfDark",
    "/Game/AnimalVarietyPack/Wolf/Materials/M_WolfFur",
    "/Game/AnimalVarietyPack/Wolf/Materials/M_WolfFurArctic",
    "/Game/AnimalVarietyPack/Wolf/Materials/M_WolfFurDark",
]
report = {"items": {}, "animals": {}, "errors": []}


def safe(fn, what):
    try:
        return fn()
    except Exception as e:  # noqa: BLE001
        report["errors"].append("%s: %s" % (what, e))
        return None


def import_file(src, dest_dir, dest_name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", dest_dir)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    return [str(p) for p in task.get_editor_property("imported_object_paths")]


def sampler_textures(material):
    out = []
    for expr in mel.get_material_expressions(material):
        try:
            out.append((expr, expr.get_editor_property("texture")))
        except Exception:  # noqa: BLE001  (not a texture sampler)
            pass
    return out


def build_item(item):
    root = "%s/%s" % (MEGASCANS, item["folder"])
    src_dir = os.path.join(RAW, item["raw"])
    tpl_folder, tpl_short = item["template"]
    tpl_root = "%s/%s" % (MEGASCANS, tpl_folder)
    rec = {"textures": {}, "meshes": {}, "material": None}

    # Textures: every map, settings copied from the sibling's map of the same kind.
    for jpg in sorted(glob.glob(os.path.join(src_dir, "*.jpg"))):
        m = re.search(r"_\d+K_(.+)\.jpg$", os.path.basename(jpg))
        if not m:
            continue
        kind = m.group(1)
        tex_name = "T_%s_%s" % (item["name"], kind)
        safe(lambda: import_file(jpg, root + "/Textures", tex_name), "import " + tex_name)
        tex = eal.load_asset("%s/Textures/%s" % (root, tex_name))
        if tex is None:
            report["errors"].append("texture missing after import: " + tex_name)
            continue
        tpl = eal.load_asset("%s/Textures/T_%s_%s" % (tpl_root, tpl_short, kind)) if eal.does_asset_exist(
            "%s/Textures/T_%s_%s" % (tpl_root, tpl_short, kind)) else None
        if tpl is not None:
            for prop in ("srgb", "compression_settings", "lod_group", "flip_green_channel"):
                safe(lambda prop=prop: tex.set_editor_property(prop, tpl.get_editor_property(prop)), "tex prop " + prop)
        else:
            normal = kind.endswith("Normal")
            safe(lambda: tex.set_editor_property("srgb", kind in ("BaseColor", "Translucency", "Billboard_BaseColor",
                                                                   "Billboard_Translucency")), "srgb")
            if normal:
                safe(lambda: tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP), "nm")
        rec["textures"][tex_name] = {"template": tpl is not None, "srgb": str(tex.get_editor_property("srgb")),
                                     "compression": str(tex.get_editor_property("compression_settings")),
                                     "size": [tex.blueprint_get_size_x(), tex.blueprint_get_size_y()]}

    # Material: duplicate the sibling's, swap each sampler's texture to ours by map kind.
    mat_path = "%s/Materials/M_%s" % (root, item["name"])
    if eal.does_asset_exist(mat_path):
        eal.delete_asset(mat_path)
    mat = eal.duplicate_asset("%s/Materials/M_%s" % (tpl_root, tpl_short), mat_path)
    swaps = []
    for expr, tex in sampler_textures(mat):
        if tex is None:
            swaps.append("sampler with no texture")
            continue
        kind = tex.get_name().split("T_%s_" % tpl_short, 1)[-1]
        new = "%s/Textures/T_%s_%s" % (root, item["name"], kind)
        if eal.does_asset_exist(new):
            expr.set_editor_property("texture", eal.load_asset(new))
            swaps.append("%s -> %s" % (tex.get_name(), new.split("/")[-1]))
        else:
            swaps.append("NO MATCH for %s" % tex.get_name())
            report["errors"].append("%s: no texture for sampler %s" % (item["name"], tex.get_name()))
    safe(lambda: mat.set_editor_property("used_with_instanced_static_meshes", True), "ism usage")
    mel.recompile_material(mat)
    rec["material"] = {"path": mat_path, "swaps": swaps}

    # Meshes: LOD0 of each variant, one LOD, Nanite off, no collision, our material.
    for fbx in sorted(glob.glob(os.path.join(src_dir, item["mesh_glob"]))):
        base = os.path.basename(fbx)
        var = re.search(r"_(Var[A-Z0-9]+)_LOD0\.fbx$", base)
        mesh_name = "SM_%s_%s" % (item["name"], var.group(1)) if var else "SM_%s" % item["name"]
        tmp = "%s/_import_%s" % (root, mesh_name)
        if eal.does_directory_exist(tmp):
            eal.delete_directory(tmp)
        paths = safe(lambda: import_file(fbx, tmp, mesh_name), "import " + base) or []
        meshes = [p for p in paths if isinstance(eal.load_asset(p), unreal.StaticMesh)]
        if not meshes:
            report["errors"].append("no static mesh from " + base + " " + str(paths))
            continue
        final = "%s/Meshes/%s" % (root, mesh_name)
        if eal.does_asset_exist(final):
            eal.delete_asset(final)
        eal.rename_asset(meshes[0].split(".")[0], final)
        eal.delete_directory(tmp)
        sm = eal.load_asset(final)
        ns = sm.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)
        sm.set_editor_property("nanite_settings", ns)
        safe(lambda: unreal.EditorStaticMeshLibrary.remove_collisions(sm), "remove collisions")
        for i in range(len(sm.get_editor_property("static_materials"))):
            sm.set_material(i, mat)
        b = sm.get_bounds().box_extent
        rec["meshes"][mesh_name] = {"from": base, "lods": sm.get_num_lods(), "extent_cm": [round(b.x, 1), round(b.y, 1), round(b.z, 1)],
                                    "slots": len(sm.get_editor_property("static_materials")),
                                    "imported": [p.split("/")[-1] for p in paths]}
    eal.save_directory(root, only_if_is_dirty=False, recursive=True)
    report["items"][item["name"]] = rec


def relink_animals():
    for path in ANIMAL_MATERIALS:
        m = eal.load_asset(path)
        texs = [t.get_path_name() if t else None for _, t in sampler_textures(m)] if m else None
        ok = bool(texs) and all(t and t.startswith("/Game/AnimalVarietyPack/") for t in texs)
        if ok:
            eal.save_asset(path, only_if_is_dirty=False)
        report["animals"][path] = {"textures": texs, "saved": ok}


for it in ITEMS:
    safe(lambda it=it: build_item(it), "build " + it["name"])
safe(relink_animals, "animals")
dst = os.path.join(OUT, "import_fab_extras.json")
if os.path.exists(dst):
    os.remove(dst)
with open(dst, "w") as f:
    json.dump(report, f, indent=1, default=str)
unreal.log("FAB_EXTRAS done errors=%d -> %s" % (len(report["errors"]), dst))
