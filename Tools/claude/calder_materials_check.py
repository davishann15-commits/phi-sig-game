"""Read-only check of the Calder landscape assets that Tools/claude/calder_materials.py builds (Calder mountains job).

Never saves, never opens a map, creates no asset (the two compile-baseline materials are transient, in /Engine/Transient).
Checks, against SourceArt/CalderLandscape/ASSETS.md:
  1. every texture row loads as a Texture2D with the settings its kind needs (A/S sRGB TC_Default; N TC_Normalmap, linear,
     no green flip; ARM TC_Masks linear; R/AO/Op TC_Grayscale linear), LOD group World, max size 2048 (4096 for the two
     big sets), built size = source size, source image dims = the raw file's, the import came from the raw file named in
     the row, the raw file / zip matches the manifest sha256 (and the unzipped member the zip CRC), page + licence match;
     no other asset under /Game/SeniorSendOff/Calder;
  2. M_CalderGround / M_CalderRock / M_CalderWater load, carry the documented flags, have every Custom input, sample UV,
     texture-object input and used output connected, use the shared wrap sampler with the texture's sampler type;
  3. every parameter in ASSETS.md exists with its type, group and default, and the material has no other parameter;
  4. the texture samples of M_CalderGround (graph sample nodes + reads in Custom HLSL) are <= 12, and the engine's own
     estimate minus the engine-added baseline (measured on a transient empty material) agrees;
  5. each material is force-recompiled (MaterialEditingLibrary.recompile_material -> new DDC key, so a fresh compile) for
     the running RHI's shader platform with r.ShowShaderCompilerWarnings 1, and the engine log is scanned for errors and
     warnings about it (needs -AllowCommandletRendering; under NullRHI it reports NOT COMPILED);
  6. no file under Content outside SeniorSendOff/Calder changed since the CalderWork baseline, and Config/DefaultGame.ini
     differs from the baseline copy only by the one cook line, inside [/Script/UnrealEd.ProjectPackagingSettings].
Results: log lines prefixed CALDERCHECK and Saved/ClaudeDump/calder_materials_check.json.
Run: the ue_py.sh command line plus -AllowCommandletRendering (see Saved/CalderWork/plan/materials_verify.md).
"""
import csv
import datetime
import difflib
import hashlib
import json
import os
import re
import struct
import time
import zipfile
import zlib

import unreal

PROJECT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
RAW = "/Users/Stewart/SeniorSendoffCC0Raw"
UNZIP = "/tmp/calder_unzip"
ROOT = "/Game/SeniorSendOff/Calder"
MAT_DIR = ROOT + "/Materials"
ASSETS_MD = os.path.join(PROJECT, "SourceArt", "CalderLandscape", "ASSETS.md")
GAME_INI = os.path.join(PROJECT, "Config", "DefaultGame.ini")
BASELINE = os.path.join(PROJECT, "Saved", "CalderWork", "baseline")
OUT_JSON = os.path.join(PROJECT, "Saved", "ClaudeDump", "calder_materials_check.json")
COOK_LINE = '+DirectoriesToAlwaysCook=(Path="/Game/SeniorSendOff/Calder")'
MATERIALS = ("M_CalderGround", "M_CalderRock", "M_CalderWater")
EXPECTED_EXPRESSIONS = {"M_CalderGround": 54, "M_CalderRock": 26, "M_CalderWater": 33}
OUTPUTS = {"M_CalderGround": ("BASE_COLOR", "NORMAL", "ROUGHNESS", "AMBIENT_OCCLUSION"),
           "M_CalderRock": ("BASE_COLOR", "NORMAL", "ROUGHNESS", "AMBIENT_OCCLUSION"),
           "M_CalderWater": ("BASE_COLOR", "NORMAL", "ROUGHNESS", "SPECULAR")}
UNUSED_OUTPUTS = ("EMISSIVE_COLOR", "METALLIC", "OPACITY", "OPACITY_MASK", "WORLD_POSITION_OFFSET")
SAMPLE_BUDGET = 12

assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
TC = unreal.TextureCompressionSettings
ST = unreal.MaterialSamplerType
R = {"fail": [], "warn": [], "info": [], "textures": {}, "materials": {}, "content": {}, "ini": {}}


def log(msg):
    unreal.log("CALDERCHECK " + msg)


def fail(msg):
    R["fail"].append(msg)
    unreal.log_warning("CALDERCHECK FAIL " + msg)


def warn(msg):
    R["warn"].append(msg)
    unreal.log("CALDERCHECK WARN " + msg)


def info(msg):
    R["info"].append(msg)
    log("INFO " + msg)


def pkg(obj):
    return obj.get_path_name().split(".")[0] if obj else None


# ======================================================================================================== ASSETS.md parse
def parse_md():
    text = open(ASSETS_MD).read()
    tex_rows = []
    for m in re.finditer(r"^\| `(/Game/SeniorSendOff/Calder/Textures/[^`]+)` \| (\w+) \| `([^`]+)` \| (\S+) \| ([^|]+) \|$",
                         text, re.M):
        tex_rows.append({"path": m.group(1), "kind": m.group(2), "raw": m.group(3), "page": m.group(4),
                         "licence": m.group(5).strip()})
    big = set()
    m = re.search(r"Max size 2048, except ([^(]+) \(4096", text)
    if m:
        big = {s.strip() for s in m.group(1).replace(" and ", ",").split(",") if s.strip()}
    params = {}
    for sec in re.split(r"^### ", text, flags=re.M)[1:]:
        name = sec.splitlines()[0].strip().rsplit("/", 1)[-1]
        rows = []
        for m in re.finditer(r"^\| `(\w+)` \| ([^|]+) \| `([^`]*)` \| (\w+) \| ([^|]*) \|$", sec, re.M):
            rows.append({"name": m.group(1), "type": m.group(2).strip(), "default": m.group(3), "group": m.group(4),
                         "meaning": m.group(5).strip()})
        params[name] = rows
    head = re.search(r"## Textures \((\d+) assets, (\d+) sets\)", text)
    return tex_rows, big, params, (int(head.group(1)), int(head.group(2))) if head else None


def read_manifest():
    rows = {}
    with open(os.path.join(RAW, "CC0_SOURCES.csv"), newline="") as f:
        for row in csv.DictReader(f):
            rows[row["file"]] = row
    return rows


# ======================================================================================================== raw files
def image_dims(path):
    with open(path, "rb") as f:
        head = f.read(32)
        if head[:8] == b"\x89PNG\r\n\x1a\n":
            return struct.unpack(">II", head[16:24])
        if head[:2] != b"\xff\xd8":
            return None
        f.seek(2)
        while True:
            b = f.read(1)
            while b and b != b"\xff":
                b = f.read(1)
            while b == b"\xff":
                b = f.read(1)
            if not b:
                return None
            marker = b[0]
            if marker in (0xD8, 0x01) or 0xD0 <= marker <= 0xD7:
                continue
            seg = struct.unpack(">H", f.read(2))[0]
            if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
                data = f.read(5)
                h, w = struct.unpack(">HH", data[1:5])
                return w, h
            f.seek(seg - 2, 1)


_SHA = {}


def sha256(path):
    if path not in _SHA:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 22), b""):
                h.update(chunk)
        _SHA[path] = h.hexdigest()
    return _SHA[path]


_ZIPINFO = {}


def zip_member(zpath, member):
    if zpath not in _ZIPINFO:
        with zipfile.ZipFile(zpath) as z:
            _ZIPINFO[zpath] = {i.filename: (i.CRC, i.file_size) for i in z.infolist()}
    return _ZIPINFO[zpath].get(member)


def crc32(path):
    c = 0
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 22), b""):
            c = zlib.crc32(chunk, c)
    return c & 0xFFFFFFFF


# ======================================================================================================== 1. textures
KIND_RULES = {"A": (True, "TC_DEFAULT"), "S": (True, "TC_DEFAULT"), "N": (False, "TC_NORMALMAP"),
              "ARM": (False, "TC_MASKS"), "R": (False, "TC_GRAYSCALE"), "AO": (False, "TC_GRAYSCALE"),
              "Op": (False, "TC_GRAYSCALE")}


def check_textures(rows, big, manifest):
    seen = set()
    per_kind = {}
    batch = {}
    for n, row in enumerate(rows):
        if n % 16 == 0:
            # Load 16, then finish their (async, with rendering on) builds: until then a texture reports the 32x32
            # placeholder's size.
            batch = {r["path"]: unreal.load_asset(r["path"]) for r in rows[n:n + 16]}
            unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncTextureCompilationFinishAll")
        path, kind = row["path"], row["kind"]
        problems = []
        set_id = path.split("/")[-2]
        seen.add(path)
        per_kind[kind] = per_kind.get(kind, 0) + 1
        # raw source on disk
        if " : " in row["raw"]:
            zrel, member = row["raw"].split(" : ")
            src = os.path.join(UNZIP, set_id, member)
            man = manifest.get(zrel)
            zabs = os.path.join(RAW, zrel)
            if not os.path.isfile(zabs):
                problems.append("zip missing " + zabs)
            else:
                zi = zip_member(zabs, member)
                if zi is None:
                    problems.append("member %s not in %s" % (member, zrel))
                elif not os.path.isfile(src):
                    problems.append("unzipped file missing " + src)
                elif crc32(src) != zi[0] or os.path.getsize(src) != zi[1]:
                    problems.append("unzipped %s differs from the zip member" % src)
                if man and sha256(zabs) != man["sha256"]:
                    problems.append("zip sha256 != manifest")
        else:
            zrel = row["raw"]
            src = os.path.join(RAW, zrel)
            man = manifest.get(zrel)
            if not os.path.isfile(src):
                problems.append("raw missing " + src)
            elif man and sha256(src) != man["sha256"]:
                problems.append("raw sha256 != manifest")
        if not man:
            problems.append("raw %s not in CC0_SOURCES.csv" % zrel)
        else:
            if man["page"] != row["page"]:
                problems.append("page %s != manifest %s" % (row["page"], man["page"]))
            if man["licence"].strip() != row["licence"]:
                problems.append("licence differs from manifest")
            if "cc0" not in man["licence"].lower():
                problems.append("manifest licence is not CC0: " + man["licence"])
        # expected kind from the raw name (catches NormalGL / disp / wrong slot)
        rawname = os.path.basename(src)
        expect_kind = None
        for pat, k in ((r"_diff(use)?_\dk\.", "A"), (r"_nor_dx_\dk\.", "N"), (r"_arm_\dk\.", "ARM"), (r"_Color\.", "A"),
                       (r"_NormalDX\.", "N"), (r"_Roughness\.", "R"), (r"_AmbientOcclusion\.", "AO"),
                       (r"_Opacity\.", "Op"), (r"_Scattering\.", "S")):
            if re.search(pat, rawname):
                expect_kind = k
                break
        if expect_kind != kind:
            problems.append("raw file %s looks like kind %s, row says %s" % (rawname, expect_kind, kind))
        if not path.endswith("/T_%s_%s" % (set_id, kind)):
            problems.append("asset name does not follow T_<set>_<kind>")
        src_dims = image_dims(src) if os.path.isfile(src) else None
        # the asset
        tex = batch.get(path)
        rec = {"kind": kind, "raw": row["raw"], "src_dims": src_dims}
        if not isinstance(tex, unreal.Texture2D):
            problems.append("does not load as Texture2D (%s)" % tex)
        else:
            srgb = tex.get_editor_property("srgb")
            comp = tex.get_editor_property("compression_settings").name
            lod = tex.get_editor_property("lod_group").name
            mx = tex.get_editor_property("max_texture_size")
            flip = tex.get_editor_property("flip_green_channel")
            vt = tex.get_editor_property("virtual_texture_streaming")
            want_srgb, want_comp = KIND_RULES[kind]
            want_max = 4096 if set_id in big else 2048
            built = (tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
            tag = None
            try:
                v = assets.find_asset_data(path).get_tag_value("Dimensions")
                if isinstance(v, tuple):
                    v = v[1] if v[0] else None
                tag = str(v) if v else None
            except Exception:  # noqa: BLE001
                pass
            try:
                imported_from = tex.get_editor_property("asset_import_data").get_first_filename()
            except Exception as e:  # noqa: BLE001
                imported_from = "?(%s)" % e
            rec.update(srgb=srgb, comp=comp, lod=lod, max=mx, flip=flip, vt=vt, built=built, source_tag=tag,
                       imported_from=imported_from)
            if srgb != want_srgb:
                problems.append("srgb=%s, want %s" % (srgb, want_srgb))
            if comp != want_comp:
                problems.append("compression %s, want %s" % (comp, want_comp))
            if kind == "N" and flip:
                problems.append("normal has flip_green_channel on (DirectX normal must not be flipped)")
            if lod != "TEXTUREGROUP_WORLD":
                problems.append("LOD group %s, want TEXTUREGROUP_WORLD" % lod)
            if mx != want_max:
                problems.append("max_texture_size %d, want %d" % (mx, want_max))
            if vt:
                problems.append("virtual texture streaming on (the masters use non-VT samplers)")
            if src_dims:
                # the cap applies to the longer side and keeps the aspect
                f = min(1.0, float(mx) / max(src_dims))
                want_built = (int(src_dims[0] * f), int(src_dims[1] * f))
                if tuple(built) != want_built:
                    problems.append("built size %s, want %s (source %s, max %d)" % (built, want_built, src_dims, mx))
                if tag and tag != "%dx%d" % src_dims:
                    problems.append("asset source dims %s != raw file %dx%d" % (tag, src_dims[0], src_dims[1]))
                if max(src_dims) > mx:
                    problems.append("source %dx%d is larger than the cap %d (downscaled on build)" % (
                        src_dims[0], src_dims[1], mx))
            if os.path.normpath(imported_from) != os.path.normpath(src):
                problems.append("imported from %s, expected %s" % (imported_from, src))
        rec["problems"] = problems
        R["textures"][path] = rec
        for p in problems:
            fail("texture %s: %s" % (path, p))
        if n % 16 == 15:
            batch = {}
            tex = None
            unreal.SystemLibrary.collect_garbage()
    batch = {}
    unreal.SystemLibrary.collect_garbage()
    # anything in the folder that ASSETS.md does not list
    listed = seen | {MAT_DIR + "/" + m for m in MATERIALS}
    on_disk = {p.split(".")[0] for p in assets.list_assets(ROOT, recursive=True, include_folder=False)}
    extra = sorted(on_disk - listed)
    missing = sorted(listed - on_disk)
    for p in extra:
        fail("asset under %s not listed in ASSETS.md: %s" % (ROOT, p))
    for p in missing:
        fail("listed in ASSETS.md but not in the asset registry: %s" % p)
    R["texture_summary"] = {"rows": len(rows), "sets": len({r["path"].split("/")[-2] for r in rows}),
                            "per_kind": per_kind, "assets_under_root": len(on_disk), "extra": extra,
                            "missing": missing,
                            "failed_rows": sum(1 for v in R["textures"].values() if v["problems"])}
    log("TEXTURES rows=%d sets=%d per_kind=%s assets_under_root=%d failed_rows=%d" % (
        len(rows), R["texture_summary"]["sets"], per_kind, len(on_disk), R["texture_summary"]["failed_rows"]))


# ======================================================================================================== 2-4. materials
def cls(ex):
    return ex.get_class().get_name()


def sampler_for(tex):
    comp = tex.get_editor_property("compression_settings")
    if comp == TC.TC_NORMALMAP:
        return ST.SAMPLERTYPE_NORMAL
    if comp == TC.TC_MASKS:
        return ST.SAMPLERTYPE_MASKS
    if comp in (TC.TC_GRAYSCALE, TC.TC_ALPHA):
        return ST.SAMPLERTYPE_GRAYSCALE if tex.get_editor_property("srgb") else ST.SAMPLERTYPE_LINEAR_GRAYSCALE
    return ST.SAMPLERTYPE_COLOR if tex.get_editor_property("srgb") else ST.SAMPLERTYPE_LINEAR_COLOR


SAMPLE_CLASSES = ("MaterialExpressionTextureSample", "MaterialExpressionTextureSampleParameter2D")
HLSL_SAMPLE = re.compile(r"\.\s*Sample\w*\s*\(|\btex2D\w*\s*\(|Texture2DSample\w*\s*\(|\.Load\s*\(")


def inputs_of(mat, ex):
    names = [str(n) for n in editing.get_material_expression_input_names(ex)]
    srcs = list(editing.get_inputs_for_material_expression(mat, ex))
    return dict(zip(names, srcs))


def check_material(name, md_params):
    path = MAT_DIR + "/" + name
    out = {"path": path}
    R["materials"][name] = out
    mat = unreal.load_asset(path)
    if not isinstance(mat, unreal.Material):
        fail("%s does not load as a Material (%s)" % (path, mat))
        return None
    # flags
    flags = {k: mat.get_editor_property(k) for k in (
        "material_domain", "blend_mode", "shading_model", "two_sided", "tangent_space_normal",
        "used_with_static_lighting", "used_with_instanced_static_meshes", "use_material_attributes")}
    out["flags"] = {k: str(v) for k, v in flags.items()}
    want = {"material_domain": unreal.MaterialDomain.MD_SURFACE, "blend_mode": unreal.BlendMode.BLEND_OPAQUE,
            "shading_model": unreal.MaterialShadingModel.MSM_DEFAULT_LIT, "two_sided": False,
            "tangent_space_normal": False, "used_with_static_lighting": False,
            "used_with_instanced_static_meshes": True, "use_material_attributes": False}   # job: "ISM-enabled"
    for k, v in want.items():
        if flags[k] != v:
            fail("%s: %s is %s, want %s" % (name, k, flags[k], v))
    # graph
    exprs = list(editing.get_material_expressions(mat))
    out["expressions"] = len(exprs)
    if len(exprs) != EXPECTED_EXPRESSIONS[name]:
        fail("%s: %d expressions, the script builds %d" % (name, len(exprs), EXPECTED_EXPRESSIONS[name]))
    graph_samples = 0
    hlsl_samples = 0
    by_param = {}
    used_as_input = set()
    for ex in exprs:
        c = cls(ex)
        ins = inputs_of(mat, ex)
        for src in ins.values():
            if src is not None:
                used_as_input.add(src.get_path_name())
        if c == "MaterialExpressionCustom":
            missing = [k for k, v in ins.items() if v is None]
            if missing:
                fail("%s: Custom '%s' has unconnected inputs %s" % (name, ex.get_editor_property("description"), missing))
            hlsl_samples += len(HLSL_SAMPLE.findall(ex.get_editor_property("code")))
        if c in SAMPLE_CLASSES:
            graph_samples += 1
            if ins.get("UVs") is None:
                fail("%s: %s sample has no UVs input (would read mesh UV0, not the world projection)" % (name, c))
            if c == "MaterialExpressionTextureSample" and ins.get("Tex") is None:
                fail("%s: TextureSample without a Tex (texture object) input" % name)
            mode = ex.get_editor_property("mip_value_mode")
            if mode == unreal.TextureMipValueMode.TMVM_DERIVATIVE and (ins.get("DDX(UVs)") is None or
                                                                      ins.get("DDY(UVs)") is None):
                fail("%s: derivative-mode sample missing DDX/DDY inputs" % name)
        if c in SAMPLE_CLASSES + ("MaterialExpressionTextureObjectParameter",):
            if ex.get_editor_property("sampler_source") != unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS:
                fail("%s: a texture node does not use the shared wrap sampler" % name)
            tex = ex.get_editor_property("texture")
            if c == "MaterialExpressionTextureSample" and ins.get("Tex") is not None:
                tex = ins["Tex"].get_editor_property("texture")
            if tex is None:
                fail("%s: a texture node has no texture" % name)
            elif ex.get_editor_property("sampler_type") != sampler_for(tex):
                fail("%s: sampler type %s does not match %s" % (name, ex.get_editor_property("sampler_type"),
                                                                 pkg(tex)))
        if c in ("MaterialExpressionScalarParameter", "MaterialExpressionVectorParameter",
                 "MaterialExpressionTextureSampleParameter2D", "MaterialExpressionTextureObjectParameter"):
            pname = str(ex.get_editor_property("parameter_name"))
            if pname in by_param:
                fail("%s: parameter %s appears on two nodes" % (name, pname))
            by_param[pname] = ex
    # every non-output node feeds something (no stray nodes); outputs connected
    outputs_fed = set()
    for prop in OUTPUTS[name]:
        node = editing.get_material_property_input_node(mat, getattr(unreal.MaterialProperty, "MP_" + prop))
        if node is None:
            fail("%s: %s output unconnected" % (name, prop))
        else:
            outputs_fed.add(node.get_path_name())
    for prop in UNUSED_OUTPUTS:
        mp = getattr(unreal.MaterialProperty, "MP_" + prop, None)
        if mp is not None and editing.get_material_property_input_node(mat, mp) is not None:
            warn("%s: %s output is connected (not documented)" % (name, prop))
    mp_front = getattr(unreal.MaterialProperty, "MP_FRONT_MATERIAL", None)
    if mp_front is not None:
        if editing.get_material_property_input_node(mat, mp_front) is not None:
            fail("%s: a Substrate node feeds Front Material (the legacy inputs would be ignored)" % name)
        out["front_material_checked"] = True
    stray = [cls(ex) for ex in exprs if ex.get_path_name() not in used_as_input | outputs_fed]
    if stray:
        fail("%s: nodes that feed nothing: %s" % (name, stray))
    out["graph_texture_samples"] = graph_samples
    out["hlsl_texture_samples"] = hlsl_samples
    # textures referenced
    used = [pkg(t) for t in editing.get_material_used_textures(mat)]
    out["used_textures"] = sorted(used)
    for t in used:
        if not (t.startswith(ROOT + "/") or t.startswith("/Engine/")):
            fail("%s references a texture outside Calder/Engine: %s" % (name, t))
    # parameters vs ASSETS.md
    have = {"scalar": {str(n) for n in editing.get_scalar_parameter_names(mat)},
            "vector": {str(n) for n in editing.get_vector_parameter_names(mat)},
            "texture": {str(n) for n in editing.get_texture_parameter_names(mat)},
            "switch": {str(n) for n in editing.get_static_switch_parameter_names(mat)}}
    all_have = set().union(*have.values())
    md_names = [p["name"] for p in md_params]
    if not md_params:
        fail("%s: ASSETS.md lists no parameters" % name)
    if len(set(md_names)) != len(md_names):
        fail("%s: ASSETS.md lists a parameter twice" % name)
    for extra in sorted(all_have - set(md_names)):
        fail("%s: parameter %s exists but ASSETS.md does not list it" % (name, extra))
    checked = []
    for p in md_params:
        pname, ptype, default = p["name"], p["type"], p["default"]
        ex = by_param.get(pname)
        problem = None
        actual = None
        if pname not in all_have or ex is None:
            problem = "missing from the material"
        elif ptype == "scalar":
            actual = editing.get_material_default_scalar_parameter_value(mat, pname)
            if pname not in have["scalar"] or cls(ex) != "MaterialExpressionScalarParameter":
                problem = "not a scalar parameter"
            elif abs(actual - float(default)) > 1e-4 * max(1.0, abs(float(default))):
                problem = "default %s, ASSETS.md says %s" % (actual, default)
        elif ptype == "vector":
            c = editing.get_material_default_vector_parameter_value(mat, pname)
            actual = (c.r, c.g, c.b, c.a)
            want_v = tuple(float(x) for x in default.strip("()").split(","))
            if pname not in have["vector"] or cls(ex) != "MaterialExpressionVectorParameter":
                problem = "not a vector parameter"
            elif any(abs(a - b) > 1e-4 for a, b in zip(actual, want_v)):
                problem = "default %s, ASSETS.md says %s" % (actual, default)
        elif ptype.startswith("texture"):
            t = editing.get_material_default_texture_parameter_value(mat, pname)
            actual = pkg(t)
            want_cls = "MaterialExpression" + re.search(r"\((\w+)\)", ptype).group(1)
            if pname not in have["texture"] or cls(ex) != want_cls:
                problem = "node is %s, ASSETS.md says %s" % (cls(ex), want_cls)
            elif actual != default:
                problem = "default texture %s, ASSETS.md says %s" % (actual, default)
        else:
            problem = "unknown type " + ptype
        if ex is not None and problem is None:
            grp = str(ex.get_editor_property("group"))
            if grp != p["group"]:
                problem = "group %s, ASSETS.md says %s" % (grp, p["group"])
        checked.append({"name": pname, "type": ptype, "default_md": default,
                        "actual": actual if not isinstance(actual, tuple) else list(actual), "ok": problem is None})
        if problem:
            fail("%s parameter %s: %s" % (name, pname, problem))
    out["parameters"] = checked
    out["parameters_ok"] = sum(1 for c in checked if c["ok"])
    log("MATERIAL %s: %d expressions, %d/%d parameters ok, graph samples %d (+%d in HLSL), textures %s" % (
        name, len(exprs), out["parameters_ok"], len(checked), graph_samples, hlsl_samples, len(used)))
    return mat


# ======================================================================================================== 5. compile
def can_render():
    return "allowcommandletrendering" in unreal.SystemLibrary.get_command_line().lower()


def flush_log():
    unreal.SystemLibrary.execute_console_command(None, "FlushLog")
    time.sleep(1.0)


def find_engine_log(token):
    folder = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_log_dir())
    if not os.path.isdir(folder):
        return None
    cands = sorted((os.path.join(folder, f) for f in os.listdir(folder) if f.endswith(".log")),
                   key=os.path.getmtime, reverse=True)
    for c in cands[:6]:
        try:
            with open(c, errors="replace") as f:
                if token in f.read():
                    return c
        except OSError:
            pass
    return None


STAT_KEYS = ("num_pixel_shader_instructions", "num_vertex_shader_instructions", "num_samplers",
             "num_pixel_texture_samples", "num_vertex_texture_samples", "num_virtual_texture_samples",
             "num_uv_scalars", "num_interpolator_scalars")


def stats_dict(mat):
    st = editing.get_statistics(mat)
    d = {}
    for k in STAT_KEYS:
        try:
            d[k] = st.get_editor_property(k)
        except Exception:  # noqa: BLE001
            pass
    return d


def transient_baselines():
    """Engine-added sample count: an empty transient material, and one with a single sample (should be +1)."""
    res = {}
    try:
        empty = unreal.Material()
        editing.recompile_material(empty)
        res["empty"] = stats_dict(empty)
        one = unreal.Material()
        tex = unreal.load_asset("/Game/SeniorSendOff/Calder/Textures/Grass004/T_Grass004_A")
        s = editing.create_material_expression(one, unreal.MaterialExpressionTextureSample, -300, 0)
        s.set_editor_properties({"texture": tex, "sampler_type": sampler_for(tex),
                                 "sampler_source": unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS})
        editing.connect_material_property(s, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        editing.recompile_material(one)
        res["one_sample"] = stats_dict(one)
    except Exception as e:  # noqa: BLE001
        res["error"] = str(e)
    return res


def compile_all(mats):
    if not can_render():
        for name in mats:
            R["materials"][name]["compile"] = "NOT COMPILED: NullRHI commandlet (add -AllowCommandletRendering)"
            fail("%s not compiled: run with -AllowCommandletRendering" % name)
        return
    unreal.SystemLibrary.execute_console_command(None, "r.ShowShaderCompilerWarnings 1")
    token = "CALDERCHECK_TOKEN_%d" % int(time.time() * 1000)
    log(token)
    flush_log()
    log_path = find_engine_log(token)
    R["engine_log"] = log_path
    base = transient_baselines()
    R["baseline_stats"] = base
    log("BASELINE %s" % json.dumps(base))
    for name, mat in mats.items():
        start = os.path.getsize(log_path) if log_path else 0
        log("COMPILE_BEGIN %s" % name)
        t0 = time.time()
        editing.recompile_material(mat)   # PostEditChange -> new StateId -> new DDC key: a real compile
        st = stats_dict(mat)              # GetStatistics: submit + FinishCompilation on GMaxRHIShaderPlatform
        dt = time.time() - t0
        log("COMPILE_END %s %.1fs %s" % (name, dt, st))
        flush_log()
        lines = []
        if log_path:
            with open(log_path, errors="replace") as f:
                f.seek(start)
                lines = f.read().splitlines()
        rec = R["materials"][name]
        rec["stats"] = st
        try:   # the shaders the current usage flags call for (shader types per vertex factory)
            shaders = list(editing.list_shaders(mat))
            vfs = {}
            for s in shaders:
                vf = str(s.get_editor_property("vertex_factory_name"))
                vfs[vf] = vfs.get(vf, 0) + 1
            rec["shader_list"] = {"total": len(shaders), "per_vertex_factory": vfs}
        except Exception as e:  # noqa: BLE001
            rec["shader_list"] = "unavailable: %s" % e
        rec["compile_seconds"] = round(dt, 1)
        path = MAT_DIR + "/" + name
        fresh = [l for l in lines if "Missing cached shadermap for " + path in l]
        platforms = sorted({m.group(1) for l in fresh for m in [re.search(r" in (\w+),", l)] if m})
        rec["fresh_compile_lines"] = len(fresh)
        rec["platforms"] = platforms
        # Only this material compiles inside the window (the transient baselines finished before it), so every
        # warning/error line in it is kept; the report separates the ones that name the material.
        msgs = []
        for l in lines:
            low = l.lower()
            if "LogPython" in l and "CALDERCHECK" in l:
                continue
            if ("error" in low or "warning" in low or "failed" in low or "(node " in low) and \
                    "missing cached shadermap" not in low:
                msgs.append(l.strip())
        rec["log_messages_naming_it"] = [m for m in msgs if name.lower() in m.lower()]
        rec["log_messages"] = msgs
        errors = [m for m in msgs if "error" in m.lower() or "failed to compile" in m.lower()]
        warnings_ = [m for m in msgs if m not in errors]
        rec["compile_errors"] = errors
        rec["compile_warnings"] = warnings_
        if not log_path:
            fail("%s: engine log not found, compile messages not scanned" % name)
        if st.get("num_pixel_shader_instructions", 0) <= 0:
            fail("%s: no pixel shader instructions after the compile (failed or default material)" % name)
        if errors:
            fail("%s: %d compile error line(s): %s" % (name, len(errors), errors[:5]))
        if warnings_:
            warn("%s: %d compile warning line(s): %s" % (name, len(warnings_), warnings_[:5]))
        if not fresh:
            warn("%s: no 'Missing cached shadermap' line after the recompile (served from DDC?)" % name)
        log("COMPILED %s platforms=%s fresh=%d errors=%d warnings=%d stats=%s" % (
            name, platforms, len(fresh), len(errors), len(warnings_), st))
    # 4b. engine-estimated samples of the ground master minus the engine-added baseline
    # The engine-added part is the DBuffer decal response (MDR_ColorNormalRoughness: 3 reads, HLSLMaterialTranslator
    # AddEstimatedTextureSample); it is booked on whichever frequency the translator ends on, so compare VS + PS totals.
    def total(st):
        return st.get("num_pixel_texture_samples", 0) + st.get("num_vertex_texture_samples", 0)

    g = R["materials"].get("M_CalderGround", {})
    if "empty" in base and "stats" in g:
        engine_added = total(base["empty"])
        own = total(g["stats"]) - engine_added
        g["engine_estimated_samples"] = total(g["stats"])
        g["engine_added_baseline"] = engine_added
        g["own_samples_by_engine"] = own
        if "one_sample" in base and total(base["one_sample"]) - engine_added != 1:
            warn("baseline check: one-sample material totals %d, empty %d (expected +1)" % (
                total(base["one_sample"]), engine_added))
        if own > SAMPLE_BUDGET:
            fail("M_CalderGround: engine estimate %d - %d engine-added = %d samples > %d" % (
                total(g["stats"]), engine_added, own, SAMPLE_BUDGET))
        for n in ("M_CalderRock", "M_CalderWater"):
            m = R["materials"].get(n, {})
            if "stats" in m:
                m["own_samples_by_engine"] = total(m["stats"]) - engine_added


# ======================================================================================================== 6. files
def baseline_time():
    txt = open(os.path.join(BASELINE, "TAKEN_AT.txt")).read().strip()   # e.g. Thu Oct 15 00:04:43 EDT 2026
    parts = txt.split()
    stamp = " ".join(parts[:4] + parts[-1:])                              # drop the zone; local time like the mtimes
    return time.mktime(time.strptime(stamp, "%a %b %d %H:%M:%S %Y")), txt


def check_files():
    t0, txt = baseline_time()
    content = os.path.join(PROJECT, "Content")
    calder = os.path.join(content, "SeniorSendOff", "Calder")
    changed, calder_files, copies = [], [], []
    for d, dirs, files in os.walk(content):
        for f in files:
            p = os.path.join(d, f)
            inside = p.startswith(calder + os.sep)
            if re.search(r" \d+(\.\w+)?$", f):
                if inside or os.path.getmtime(p) > t0:
                    copies.append(p)
            if inside:
                calder_files.append((os.path.getmtime(p), p))
            elif os.path.getmtime(p) > t0 and f != ".DS_Store":
                changed.append(p)
    dir_changes = []
    for d, dirs, files in os.walk(content):
        if d.startswith(calder):
            continue
        if os.path.getmtime(d) > t0:
            dir_changes.append(d)
    R["content"] = {"baseline": txt, "files_changed_outside_calder": changed,
                    "dirs_with_new_mtime_outside_calder": dir_changes,
                    "calder_files": len(calder_files),
                    "calder_uasset": sum(1 for _, p in calder_files if p.endswith(".uasset")),
                    "calder_non_uasset": [p for _, p in calder_files if not p.endswith(".uasset")],
                    "calder_mtime_range": [time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(min(calder_files)[0])),
                                           time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(max(calder_files)[0]))]
                    if calder_files else None,
                    "icloud_copies": copies}
    for p in changed:
        fail("file outside Calder changed since the baseline (%s): %s" % (txt, p))
    for p in copies:
        fail("iCloud-style copy: " + p)
    if R["content"]["calder_non_uasset"]:
        warn("non-.uasset files under Calder: %s" % R["content"]["calder_non_uasset"])
    for d in dir_changes:
        info("directory mtime newer than the baseline (an entry was added/removed; no file inside changed): " + d)
    # DefaultGame.ini
    a = open(os.path.join(BASELINE, "Config", "DefaultGame.ini")).read().splitlines()
    b = open(GAME_INI).read().splitlines()
    diff = [l for l in difflib.unified_diff(a, b, "baseline", "current", n=0, lineterm="")
            if not l.startswith(("---", "+++", "@@"))]
    R["ini"] = {"diff": diff}
    if diff != ["+" + COOK_LINE]:
        fail("DefaultGame.ini differs from the baseline by more than the cook line: %s" % diff)
    if b.count(COOK_LINE) != 1:
        fail("DefaultGame.ini has the cook line %d times" % b.count(COOK_LINE))
    else:
        idx = b.index(COOK_LINE)
        section = next((l for l in reversed(b[:idx]) if l.startswith("[")), None)
        R["ini"]["section"] = section
        if section != "[/Script/UnrealEd.ProjectPackagingSettings]":
            fail("cook line sits in section %s" % section)
    log("FILES changed_outside_calder=%d dirs=%d calder_files=%d ini_diff=%s" % (
        len(changed), len(dir_changes), len(calder_files), diff))


# ======================================================================================================== run
def run():
    t_start = time.time()
    log("START %s rhi_rendering=%s" % (datetime.datetime.now().isoformat(timespec="seconds"), can_render()))
    tex_rows, big, md_params, head = parse_md()
    R["md"] = {"texture_rows": len(tex_rows), "big_sets": sorted(big), "header": head,
               "param_rows": {k: len(v) for k, v in md_params.items()}}
    if head and head[0] != len(tex_rows):
        fail("ASSETS.md header says %d textures, the table has %d rows" % (head[0], len(tex_rows)))
    info("4096-cap sets parsed from ASSETS.md: %s" % sorted(big))
    for step, fn in (("files", check_files), ("textures", lambda: check_textures(tex_rows, big, read_manifest()))):
        try:
            fn()
        except Exception as e:  # noqa: BLE001
            import traceback
            fail("%s check crashed: %s\n%s" % (step, e, traceback.format_exc()))
    mats = {}
    for name in MATERIALS:
        try:
            m = check_material(name, md_params.get(name, []))
            if m:
                mats[name] = m
        except Exception as e:  # noqa: BLE001
            import traceback
            fail("%s check crashed: %s\n%s" % (name, e, traceback.format_exc()))
    g = R["materials"].get("M_CalderGround", {})
    if "graph_texture_samples" in g:
        total = g["graph_texture_samples"] + g["hlsl_texture_samples"]
        g["texture_samples"] = total
        if total > SAMPLE_BUDGET:
            fail("M_CalderGround: %d texture samples > %d" % (total, SAMPLE_BUDGET))
    try:
        compile_all(mats)
    except Exception as e:  # noqa: BLE001
        import traceback
        fail("compile check crashed: %s\n%s" % (e, traceback.format_exc()))
    R["seconds"] = round(time.time() - t_start, 1)
    R["verdict"] = "PASS" if not R["fail"] else "FAIL"
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    if os.path.exists(OUT_JSON):
        os.remove(OUT_JSON)
    with open(OUT_JSON, "w") as f:
        json.dump(R, f, indent=1, default=str)
    log("VERDICT %s fails=%d warns=%d (%s)" % (R["verdict"], len(R["fail"]), len(R["warn"]), OUT_JSON))
    for m in R["fail"]:
        log("  FAIL " + m.splitlines()[0])
    for m in R["warn"]:
        log("  WARN " + m.splitlines()[0])


run()
