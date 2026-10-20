"""Calder landscape textures and the three masters the runtime terrain, rock and water meshes use (Calder mountains job).

1. Imports the CC0 texture sets (Poly Haven folders under ~/SeniorSendoffCC0Raw/polyhaven that hold a diffuse map, and the
   listed ambientCG sets, unzipped into /tmp/calder_unzip/<id>/) into /Game/SeniorSendOff/Calder/Textures/<id>/ as
   T_<id>_A (albedo, sRGB), _N (DirectX normal, no green flip), _ARM (Poly Haven AO/Rough/Metal), _R and _AO (ambientCG
   roughness and AO, linear grayscale), _Op (leaf opacity, linear grayscale), _S (leaf scattering, sRGB). Height maps and
   OpenGL normals are not imported (no material samples them). LOD group World; max size 2048 (4096 for BIG_SETS: the
   two 4k sets and jolcham_oak_bark_01, whose 2k download is 2048x4096), so no source is downscaled.
2. Builds three opaque default-lit masters in /Game/SeniorSendOff/Calder/Materials (legacy inputs, which Substrate
   converts; world-space normal output because every layer is projected in world space):
     M_CalderGround  terrain tiles. Vertex colour R rock, G leaf litter, B clay/mud, A cavity (1 open .. 0 deep); lawn is
                     what remains, meadow takes over the lawn with slope (and MeadowAmount). 12 texture samples.
     M_CalderRock    walls, cuts, ledges, boulders (instancing allowed). Dominant-axis projection, top-facing moss, A cavity.
     M_CalderWater   creek and pond surfaces. Vertex colour R foam/riffle, A depth (0 shallow .. 1 deep).
   The RGB of the vertex colour is decoded from sRGB in the material, because UStaticMesh::BuildFromMeshDescription stores
   FMeshDescription colours with ToFColor(true); so the C++ writes linear 0..1 weights. A is stored linearly and used as is.
3. Writes SourceArt/CalderLandscape/ASSETS.md (every asset, its raw file, source page and licence, and every material
   parameter with its default) and adds /Game/SeniorSendOff/Calder to +DirectoriesToAlwaysCook in Config/DefaultGame.ini.

Idempotent: an existing texture or material is skipped unless its set id / name is in REBUILD_TEXTURES / REBUILD.
Writes only under /Game/SeniorSendOff/Calder (plus the two text files above). Never opens or saves a map.
Run headless while the editor is closed: bash Tools/claude/ue_py.sh <this file> calder_materials. The pythonscript
commandlet runs on NullRHI, where materials are saved but never compiled; to compile and check them, run the same
UnrealEditor-Cmd command line with -AllowCommandletRendering added (the script reports which case it ran in).
"""
import csv
import datetime
import os
import zipfile

import unreal

PROJECT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
RAW = "/Users/Stewart/SeniorSendoffCC0Raw"
UNZIP = "/tmp/calder_unzip"
ROOT = "/Game/SeniorSendOff/Calder"
TEX_ROOT = ROOT + "/Textures"
MAT_DIR = ROOT + "/Materials"
ASSETS_MD = os.path.join(PROJECT, "SourceArt", "CalderLandscape", "ASSETS.md")
GAME_INI = os.path.join(PROJECT, "Config", "DefaultGame.ini")
COOK_LINE = '+DirectoriesToAlwaysCook=(Path="/Game/SeniorSendOff/Calder")'

# Material names deleted and rebuilt on the next run (put a name here when its graph or flags change). Empty: the
# current assets were built by this file on 2026-10-15 (M_CalderGround and M_CalderWater rebuilt at 03:37 by the
# verification pass, for ISM usage) and a run only loads them from disk, checks their flags, graph and parameters, and
# compiles them when the commandlet can render.
REBUILD = set()
# Texture set ids re-imported over the existing assets (jolcham_oak_bark_01 was re-imported at 03:37 for its 4096 cap).
REBUILD_TEXTURES = set()
# Max size 4096 for these; everything else is capped at 2048 (all other sources are 2048 or 2048x1024). The cap applies
# to the longer side, so jolcham_oak_bark_01's 2k download (2048x4096, a 1x2 m scan) would be halved to 1024x2048 at 2048.
BIG_SETS = {"forest_leaves_03", "mossy_rock", "jolcham_oak_bark_01"}
# Every master is ISM-enabled (the job's material requirements: "Substrate-safe, ISM-enabled, ..."), not only the rock.
MATERIAL_FLAGS = {"material_domain": unreal.MaterialDomain.MD_SURFACE,
                  "blend_mode": unreal.BlendMode.BLEND_OPAQUE,
                  "shading_model": unreal.MaterialShadingModel.MSM_DEFAULT_LIT,
                  "two_sided": False,
                  "tangent_space_normal": False,
                  "used_with_static_lighting": False,
                  "used_with_instanced_static_meshes": True}
AMBIENTCG = ["Bark014", "Ground037", "Grass004", "Rock064", "Road012B", "ScatteredLeaves001",
             "LeafSet012", "LeafSet019", "LeafSet021", "LeafSet027", "LeafSet028", "LeafSet030",
             "Foliage003", "Foliage006"]
AMBIENT_SUFFIX = {"_Color": "A", "_NormalDX": "N", "_Roughness": "R", "_AmbientOcclusion": "AO", "_Opacity": "Op",
                  "_Scattering": "S"}
KIND_TEXT = {"A": "albedo, sRGB, TC_Default", "N": "normal (DirectX), TC_Normalmap, linear, no green flip",
             "ARM": "AO/Rough/Metal, TC_Masks, linear", "R": "roughness, TC_Grayscale, linear",
             "AO": "ambient occlusion, TC_Grayscale, linear", "Op": "opacity, TC_Grayscale, linear",
             "S": "scattering colour, sRGB, TC_Default"}

assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
FAILURES = []
COUNTS = {"imported": 0, "skipped": 0, "failed": 0}
MAT_RESULTS = {}


def log(msg):
    unreal.log("CALDER " + msg)


def fail(msg):
    FAILURES.append(msg)
    unreal.log_error("CALDER_ERROR " + msg)


# ============================================================================================================ textures
def unzip_set(set_id):
    """Extracts the image members of <id>_2K-*.zip into /tmp/calder_unzip/<id>/ (only plain file names, no paths)."""
    src = os.path.join(RAW, "ambientcg", set_id)
    zips = sorted(f for f in os.listdir(src) if f.endswith(".zip"))
    if not zips:
        raise RuntimeError("no zip for " + set_id)
    zpath = os.path.join(src, zips[0])
    out = os.path.join(UNZIP, set_id)
    os.makedirs(out, exist_ok=True)
    with zipfile.ZipFile(zpath) as z:
        for info in z.infolist():
            name = info.filename
            if "/" in name or "\\" in name or ".." in name or not name.lower().endswith((".jpg", ".png")):
                continue
            dest = os.path.join(out, name)
            if os.path.isfile(dest) and os.path.getsize(dest) == info.file_size:
                continue
            with z.open(info) as fin, open(dest, "wb") as fout:
                fout.write(fin.read())
    return zips[0]


def plan_textures():
    plan = []
    ph = os.path.join(RAW, "polyhaven")
    for set_id in sorted(os.listdir(ph)):
        folder = os.path.join(ph, set_id)
        if not os.path.isdir(folder):
            continue
        files = sorted(os.listdir(folder))
        if not any(f.startswith(set_id + "_diff_") or f.startswith(set_id + "_diffuse_") for f in files):
            continue
        for f in files:
            if f.startswith(set_id + "_diff_") or f.startswith(set_id + "_diffuse_"):
                kind = "A"
            elif f.startswith(set_id + "_nor_dx_"):
                kind = "N"
            elif f.startswith(set_id + "_arm_"):
                kind = "ARM"
            else:
                continue   # _disp_ (height): no material samples it, so it is not imported
            plan.append({"site": "polyhaven", "set": set_id, "kind": kind, "file": os.path.join(folder, f),
                         "raw": "polyhaven/%s/%s" % (set_id, f), "member": ""})
    for set_id in AMBIENTCG:
        try:
            zname = unzip_set(set_id)
        except Exception as e:  # noqa: BLE001
            fail("unzip %s: %s" % (set_id, e))
            continue
        folder = os.path.join(UNZIP, set_id)
        for f in sorted(os.listdir(folder)):
            stem = os.path.splitext(f)[0]
            kind = next((k for suffix, k in AMBIENT_SUFFIX.items() if stem.endswith(suffix)), None)
            if not kind:
                continue   # NormalGL, Displacement, the preview png
            plan.append({"site": "ambientcg", "set": set_id, "kind": kind, "file": os.path.join(folder, f),
                         "raw": "ambientcg/%s/%s" % (set_id, zname), "member": f})
    for item in plan:
        item["name"] = "T_%s_%s" % (item["set"], item["kind"])
        item["dest"] = "%s/%s" % (TEX_ROOT, item["set"])
        item["path"] = "%s/%s" % (item["dest"], item["name"])
        item["max"] = 4096 if item["set"] in BIG_SETS else 2048
    return plan


def texture_settings(kind, max_size):
    tc = unreal.TextureCompressionSettings
    props = {"lod_group": unreal.TextureGroup.TEXTUREGROUP_WORLD, "max_texture_size": max_size}
    if kind in ("A", "S"):
        props.update(srgb=True, compression_settings=tc.TC_DEFAULT)
    elif kind == "N":
        props.update(srgb=False, compression_settings=tc.TC_NORMALMAP, flip_green_channel=False)
    elif kind == "ARM":
        props.update(srgb=False, compression_settings=tc.TC_MASKS)
    else:   # R, AO, Op
        props.update(srgb=False, compression_settings=tc.TC_GRAYSCALE)
    return props


def import_textures(plan):
    todo = []
    for item in plan:
        if assets.does_asset_exist(item["path"]) and item["set"] not in REBUILD_TEXTURES:
            item["status"] = "exists"
            COUNTS["skipped"] += 1
            continue
        todo.append(item)
    log("textures planned=%d to_import=%d skipped=%d" % (len(plan), len(todo), COUNTS["skipped"]))
    for start in range(0, len(todo), 12):
        chunk = todo[start:start + 12]
        tasks = []
        for item in chunk:
            assets.make_directory(item["dest"])
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", item["file"])
            task.set_editor_property("destination_path", item["dest"])
            task.set_editor_property("destination_name", item["name"])
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            # Saved once below, after the settings are applied: ~/Documents syncs to iCloud and a second write of the
            # same package mid-sync leaves "name 2" copies.
            task.set_editor_property("save", False)
            tasks.append(task)
        tools.import_asset_tasks(tasks)
        for task, item in zip(tasks, chunk):
            try:
                paths = list(task.get_editor_property("imported_object_paths"))
                if len(paths) != 1:
                    raise RuntimeError("imported %s" % paths)
                tex = unreal.load_asset(paths[0])
                if not isinstance(tex, unreal.Texture2D):
                    raise RuntimeError("not a Texture2D: %s" % tex)
                tex.set_editor_properties(texture_settings(item["kind"], item["max"]))
                if not assets.save_loaded_asset(tex, False):
                    raise RuntimeError("save failed")
                item["status"] = "imported"
                COUNTS["imported"] += 1
                log("TEXTURE %s <- %s (%dx%d)" % (item["path"], item["file"], tex.blueprint_get_size_x(),
                                                  tex.blueprint_get_size_y()))
            except Exception as e:  # noqa: BLE001
                item["status"] = "FAILED"
                COUNTS["failed"] += 1
                fail("texture %s: %s" % (item["path"], e))
        unreal.SystemLibrary.collect_garbage()


# ============================================================================================================ graph kit
ST = unreal.MaterialSamplerType
SHARED_WRAP = unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS


def sampler_for(tex):
    comp = tex.get_editor_property("compression_settings")
    tc = unreal.TextureCompressionSettings
    if comp == tc.TC_NORMALMAP:
        return ST.SAMPLERTYPE_NORMAL
    if comp == tc.TC_MASKS:
        return ST.SAMPLERTYPE_MASKS
    if comp in (tc.TC_GRAYSCALE, tc.TC_ALPHA):
        return ST.SAMPLERTYPE_GRAYSCALE if tex.get_editor_property("srgb") else ST.SAMPLERTYPE_LINEAR_GRAYSCALE
    return ST.SAMPLERTYPE_COLOR if tex.get_editor_property("srgb") else ST.SAMPLERTYPE_LINEAR_COLOR


def load_texture(path):
    tex = unreal.load_asset(path)
    if not isinstance(tex, unreal.Texture2D):
        raise RuntimeError("missing texture " + path)
    return tex


def calder_tex(set_id, kind):
    return load_texture("%s/%s/T_%s_%s" % (TEX_ROOT, set_id, set_id, kind))


class Graph:
    """Small helpers over MaterialEditingLibrary; records every parameter for ASSETS.md. With mat None (an existing
    material that is skipped) it builds nothing and only records the parameters, so ASSETS.md stays complete."""

    def __init__(self, mat, name):
        self.m = mat
        self.dry = mat is None
        self.name = name
        self.params = []   # (name, type, default text, group, meaning)

    def node(self, kind, x, y, **props):
        if self.dry:
            return None
        n = editing.create_material_expression(self.m, getattr(unreal, "MaterialExpression" + kind), x, y)
        if props:
            n.set_editor_properties(props)
        return n

    @staticmethod
    def _pin(node, want):
        names = [str(v) for v in editing.get_material_expression_input_names(node)]
        if want in names:
            return want
        key = lambda s: "".join(c for c in s.lower() if c.isalnum())  # noqa: E731
        for v in names:
            if key(v) == key(want):
                return v
        raise RuntimeError("no input %s on %s (has %s)" % (want, node.get_class().get_name(), names))

    def link(self, src, dst, pin, out=""):
        if self.dry:
            return
        pin = self._pin(dst, pin)
        if not editing.connect_material_expressions(src, out, dst, pin):
            raise RuntimeError("connect %s.%s -> %s.%s failed" % (src.get_class().get_name(), out,
                                                                   dst.get_class().get_name(), pin))

    def out(self, src, prop, out=""):
        if self.dry:
            return
        if not editing.connect_material_property(src, out, getattr(unreal.MaterialProperty, "MP_" + prop)):
            raise RuntimeError("connect to " + prop + " failed")

    def scalar(self, name, value, group, meaning, x, y):
        self.params.append((name, "scalar", "%g" % value, group, meaning))
        return self.node("ScalarParameter", x, y, parameter_name=name, default_value=value, group=group)

    def vector(self, name, rgba, group, meaning, x, y):
        self.params.append((name, "vector", "(%g, %g, %g, %g)" % tuple(rgba), group, meaning))
        return self.node("VectorParameter", x, y, parameter_name=name, group=group,
                         default_value=unreal.LinearColor(*rgba))

    def tex_param(self, name, tex, group, meaning, x, y, uv=None, ddx=None, ddy=None):
        self.params.append((name, "texture (TextureSampleParameter2D)", tex.get_path_name().split(".")[0], group,
                            meaning))
        n = self.node("TextureSampleParameter2D", x, y, parameter_name=name, group=group, texture=tex,
                      sampler_type=sampler_for(tex), sampler_source=SHARED_WRAP)
        if self.dry:
            return None
        if ddx is not None:
            n.set_editor_property("mip_value_mode", unreal.TextureMipValueMode.TMVM_DERIVATIVE)
        if uv is not None:
            self.link(uv, n, "UVs")
        if ddx is not None:
            self.link(ddx, n, "DDX(UVs)")
            self.link(ddy, n, "DDY(UVs)")
        return n

    def tex_object(self, name, tex, group, meaning, x, y):
        self.params.append((name, "texture (TextureObjectParameter)", tex.get_path_name().split(".")[0], group,
                            meaning))
        return self.node("TextureObjectParameter", x, y, parameter_name=name, group=group, texture=tex,
                         sampler_type=sampler_for(tex), sampler_source=SHARED_WRAP)

    def sample(self, obj, tex, x, y, uv, mip_bias=None):
        n = self.node("TextureSample", x, y, sampler_type=sampler_for(tex), sampler_source=SHARED_WRAP)
        if self.dry:
            return None
        if mip_bias is not None:
            n.set_editor_properties({"mip_value_mode": unreal.TextureMipValueMode.TMVM_MIP_BIAS,
                                     "const_mip_value": mip_bias})
        self.link(obj, n, "Tex")
        self.link(uv, n, "UVs")
        return n

    def custom(self, desc, code, inputs, out_type, x, y):
        n = self.node("Custom", x, y, code=code, description=desc,
                      output_type=getattr(unreal.CustomMaterialOutputType, out_type))
        if self.dry:
            return None
        pins = []
        for name, _src, _out in inputs:
            pin = unreal.CustomInput()
            pin.set_editor_property("input_name", name)
            pins.append(pin)
        n.set_editor_property("inputs", pins)
        for name, src, out in inputs:
            self.link(src, n, name, out)
        return n


def open_material(name):
    """Returns a fresh, empty material, or None when it exists and is not in REBUILD."""
    path = MAT_DIR + "/" + name
    if assets.does_asset_exist(path):
        if name not in REBUILD:
            log("MATERIAL exists, skipped " + path)
            MAT_RESULTS[name] = "skipped (exists)"
            return None
        # Delete and recreate rather than clearing in place: MaterialEditingLibrary.delete_all_material_expressions
        # left about half of the old nodes behind (orphans) when it was tried here.
        if not assets.delete_asset(path):
            raise RuntimeError("could not delete %s for the rebuild" % path)
        log("MATERIAL deleted for rebuild " + path)
    mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(mat, unreal.Material):
        raise RuntimeError("could not create " + path)
    # Drop anything the factory added (with Substrate on, a default slab on Front Material would hide the legacy inputs).
    start = list(editing.get_material_expressions(mat))
    log("MATERIAL %s created with %d expressions" % (name, len(start)))
    for ex in start:
        editing.delete_material_expression(mat, ex)
    mat.set_editor_properties(MATERIAL_FLAGS)
    return mat


def can_render():
    return "allowcommandletrendering" in unreal.SystemLibrary.get_command_line().lower()


def engine_log():
    folder = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_log_dir())
    logs = [os.path.join(folder, f) for f in os.listdir(folder) if f.endswith(".log")] if os.path.isdir(folder) else []
    return max(logs, key=os.path.getmtime) if logs else None


def compile_check(mat, name):
    """Compiles mat for this RHI's shader platform (GetStatistics waits for it) and logs any compile errors it printed."""
    if not can_render():
        msg = "NOT COMPILED: NullRHI commandlet (rerun with -AllowCommandletRendering to compile and check)"
        log("STATS %s %s" % (name, msg))
        return None, msg
    log_path = engine_log()
    start = os.path.getsize(log_path) if log_path else 0
    stats = editing.get_statistics(mat)
    keys = ("num_pixel_shader_instructions", "num_vertex_shader_instructions", "num_samplers",
            "num_pixel_texture_samples", "num_vertex_texture_samples", "num_uv_scalars", "num_interpolator_scalars")
    text = ", ".join("%s=%s" % (k, stats.get_editor_property(k)) for k in keys)
    log("STATS %s %s" % (name, text))
    errors = []
    if log_path:
        with open(log_path, errors="replace") as f:
            f.seek(start)
            for line in f.read().splitlines():
                low = line.lower()
                if "calder" in low and "logpython" in low:
                    continue
                if ("failed to compile" in low or "error" in low or "(node " in low) and (
                        "material" in low or "shader" in low or "(node " in low or name.lower() in low):
                    errors.append(line.strip())
    for e in errors[:40]:
        log("COMPILE_MSG %s %s" % (name, e))
    ok = stats.get_editor_property("num_pixel_shader_instructions") > 0 and not any(
        "failed to compile" in e.lower() for e in errors)
    if not ok:
        fail("%s: compile failed (%d messages above, prefixed COMPILE_MSG)" % (name, len(errors)))
    return ok, ("compiled OK: " if ok else "COMPILE FAILED: ") + text


def check_graph(mat, name, expected=None):
    """Every Custom node input and every used material output must be connected; no stray nodes."""
    exprs = list(editing.get_material_expressions(mat))
    problems = []
    for ex in exprs:
        if isinstance(ex, unreal.MaterialExpressionCustom):
            pins = [str(n) for n in editing.get_material_expression_input_names(ex)]
            srcs = editing.get_inputs_for_material_expression(mat, ex)
            missing = [n for n, src in zip(pins, srcs) if src is None]
            if missing:
                problems.append("%s missing %s" % (ex.get_editor_property("description"), missing))
    for prop in ("BASE_COLOR", "NORMAL", "ROUGHNESS"):
        if editing.get_material_property_input_node(mat, getattr(unreal.MaterialProperty, "MP_" + prop)) is None:
            problems.append(prop + " unconnected")
    if expected is not None and len(exprs) != expected:
        problems.append("%d expressions, expected %d" % (len(exprs), expected))
    if problems:
        fail("%s graph: %s" % (name, "; ".join(problems)))
    else:
        log("GRAPH %s ok: %d expressions, every Custom input connected" % (name, len(exprs)))
    return len(exprs)


def check_existing(g):
    """A skipped material: compare its parameter names with the ones this script would build (stale -> add to REBUILD)."""
    mat = unreal.load_asset(MAT_DIR + "/" + g.name)
    stale = {k: mat.get_editor_property(k) for k, v in MATERIAL_FLAGS.items() if mat.get_editor_property(k) != v}
    if stale:
        fail("%s exists but its flags differ from the script (%s): add it to REBUILD" % (g.name, stale))
    check_graph(mat, g.name, EXPECTED_EXPRESSIONS.get(g.name))
    MAT_RESULTS[g.name] = "exists (built earlier by this script, loaded from disk); " + compile_check(mat, g.name)[1]
    have = set()
    for fn in (editing.get_scalar_parameter_names, editing.get_vector_parameter_names,
               editing.get_texture_parameter_names):
        have |= {str(n) for n in fn(mat)}
    want = {p[0] for p in g.params}
    if have != want:
        fail("%s exists but its parameters differ from the script (missing %s, extra %s): add it to REBUILD" % (
            g.name, sorted(want - have), sorted(have - want)))
    else:
        log("MATERIAL %s exists; its %d parameters match the script" % (g.name, len(have)))


def finish(g):
    if g.dry:
        check_existing(g)
        return g
    mat, name = g.m, g.name
    check_graph(mat, name, EXPECTED_EXPRESSIONS.get(name))
    editing.layout_material_expressions(mat)
    editing.recompile_material(mat)
    _ok, text = compile_check(mat, name)
    if not assets.save_loaded_asset(mat, False):
        fail("%s: save failed" % name)
        text += "; SAVE FAILED"
    MAT_RESULTS[name] = "built and saved; " + text
    log("MATERIAL saved %s/%s" % (MAT_DIR, name))
    return g


# Node counts of the graphs below (a check against stray nodes; update when a graph changes).
EXPECTED_EXPRESSIONS = {"M_CalderGround": 54, "M_CalderRock": 26, "M_CalderWater": 33}


# ------------------------------------------------------------------------------------------------------------ HLSL
SRGB_DECODE = r"""
// Vertex colour RGB arrives sRGB-encoded (BuildFromMeshDescription uses ToFColor(true)); decode it to the linear value
// the C++ wrote. A is stored linearly.
float3 Enc = saturate(VC.rgb);
float3 Lin = lerp(Enc / 12.92, pow((Enc + 0.055) / 1.055, 2.4), step(0.04045, Enc));
"""

PLANE_UV = r"""
// Dominant-axis projection ("biplanar-lite"): flat faces read the texture in XY, steep faces in YZ or XZ with v down.
float3 a = abs(normalize(N));
float2 uv;
if (a.z >= max(a.x, a.y)) uv = P.xy;
else if (a.x >= a.y) uv = float2(P.y, -P.z);
else uv = float2(P.x, -P.z);
return uv / max(T, 1.0);
"""

PLANE_GRAD = r"""
// The same projection applied to the screen-space derivative of the world position: the mip stays right on both sides
// of an axis switch (no seam line where neighbouring pixels picked different planes).
float3 a = abs(normalize(N));
float3 d = %s(P);
float2 g;
if (a.z >= max(a.x, a.y)) g = d.xy;
else if (a.x >= a.y) g = float2(d.y, -d.z);
else g = float2(d.x, -d.z);
return g / max(T, 1.0);
"""

GROUND_WEIGHTS = SRGB_DECODE + r"""
// Layer weights (meadow, litter, clay, rock); lawn = 1 - sum. VC.rgb arrives sRGB-encoded (BuildFromMeshDescription).
float3 n = normalize(N);
float slope = saturate(1.0 - n.z);
float rock = saturate(max(Lin.r, smoothstep(RockSlopeStart, RockSlopeEnd, slope)));
float meadow = saturate(smoothstep(MeadowSlopeStart, MeadowSlopeEnd, slope) + MeadowAmount);
float litter = saturate(Lin.g);
float clay = saturate(Lin.b);
float under = litter + clay;
if (under > 1.0) { litter /= under; clay /= under; }
float grass = saturate(1.0 - litter - clay);
float keep = 1.0 - rock;
float w0 = grass * (1.0 - meadow) * keep;
float w1 = grass * meadow * keep;
float w2 = litter * keep;
float w3 = clay * keep;
float w4 = rock;
// Height-aware sharpening: each layer's texture "height" (albedo luminance or AO, scaled so the default sets average
// about 0.5) leans the weights, then BlendSharpness steepens the transition. HeightBlend 0 + BlendSharpness 1 = linear.
const float3 L = float3(0.2126, 0.7152, 0.0722);
float k = saturate(HeightBlend);
float p = max(BlendSharpness, 1.0);
float b0 = pow(max(w0 * lerp(1.0, 2.0 * saturate(dot(LawnA, L) / 0.276), k), 0.0), p);
float b1 = pow(max(w1 * lerp(1.0, 2.0 * saturate(dot(MeadowA, L) / 0.62), k), 0.0), p);
float b2 = pow(max(w2 * lerp(1.0, 2.0 * saturate(LitterARM.r / 1.36), k), 0.0), p);
float b3 = pow(max(w3 * lerp(1.0, 2.0 * saturate(dot(ClayA, L) / 0.148), k), 0.0), p);
float b4 = pow(max(w4 * lerp(1.0, 2.0 * saturate(RockARM.r / 1.68), k), 0.0), p);
float s = b0 + b1 + b2 + b3 + b4;
if (s < 1e-6) return float4(0, 0, 0, 0);
return float4(b1, b2, b3, b4) / s;
"""

GROUND_BASE = r"""
float wl = saturate(1.0 - dot(W, float4(1, 1, 1, 1)));
const float3 L = float3(0.2126, 0.7152, 0.0722);
// Macro variation: the lawn albedo read 23x larger (and a few mips blurrier), its luminance around MacroPivot remapped
// to 1 +- MacroStrength, multiplies the lawn and the meadow.
float m = 1.0 + MacroStrength * clamp((dot(MacroA, L) / max(MacroPivot, 1e-3) - 1.0) * 2.5, -1.0, 1.0);
float3 c = (LawnA * LawnTint * wl + MeadowA * MeadowTint * W.x) * m
         + LitterA * LitterTint * W.y + ClayA * ClayTint * W.z + RockA * RockTint * W.w;
return max(c * Darken, 0.0);
"""

GROUND_NORMAL = r"""
float3 n = normalize(N);
float wl = saturate(1.0 - dot(W, float4(1, 1, 1, 1)));
float3 a = abs(n);
float3 rt;
if (a.z >= max(a.x, a.y)) rt = float3(RockN.x, RockN.y, 0);
else if (a.x >= a.y) rt = float3(0, RockN.x, -RockN.y);
else rt = float3(RockN.x, 0, -RockN.y);
// Lawn, meadow (reuses the lawn normal), litter and clay are read in world XY, so their tilt is (x, y, 0).
float2 xy = LawnN.xy * (wl + W.x) + LitterN.xy * W.y + ClayN.xy * W.z;
return normalize(n + NormalStrength * (float3(xy, 0) + rt * W.w));
"""

GROUND_ROUGH = r"""
float wl = saturate(1.0 - dot(W, float4(1, 1, 1, 1)));
return saturate(LawnRoughness * wl + MeadowRoughness * W.x + LitterARM.g * W.y + ClayRoughness * W.z + RockARM.g * W.w);
"""

GROUND_AO = r"""
float wl = saturate(1.0 - dot(W, float4(1, 1, 1, 1)));
float ao = wl + W.x + W.z + LitterARM.r * W.y + RockARM.r * W.w;
return saturate(ao * lerp(1.0, saturate(VA), CavityStrength));
"""

ROCK_MOSS = r"""
// Moss on the faces that look up: the threshold drops as MossAmount rises (0 = none, 1 = every face up to vertical);
// the detail normal and the cavities (low AO) let it creep along joints.
float3 n = normalize(N);
float3 a = abs(n);
float3 rt;
if (a.z >= max(a.x, a.y)) rt = float3(RockN.x, RockN.y, 0);
else if (a.x >= a.y) rt = float3(0, RockN.x, -RockN.y);
else rt = float3(RockN.x, 0, -RockN.y);
float3 nd = normalize(n + rt * NormalStrength);
float t = 1.0 - saturate(MossAmount);
float w = saturate((nd.z - t + (0.85 - ARM.r) * 0.6) * max(MossSharpness, 0.01));
return w * saturate(MossAmount * 20.0);
"""

ROCK_BASE = r"""
return lerp(Albedo * Tint, MossAlbedo * MossTint, saturate(Moss));
"""

ROCK_NORMAL = r"""
float3 n = normalize(N);
float3 a = abs(n);
float3 rt;
if (a.z >= max(a.x, a.y)) rt = float3(RockN.x, RockN.y, 0);
else if (a.x >= a.y) rt = float3(0, RockN.x, -RockN.y);
else rt = float3(RockN.x, 0, -RockN.y);
float3 mt = float3(MossN.x, MossN.y, 0);   // moss is read in world XY
return normalize(n + NormalStrength * lerp(rt, mt, saturate(Moss)));
"""

ROCK_ROUGH = r"""
return saturate(lerp(ARM.g, MossRoughness, saturate(Moss)));
"""

ROCK_AO = r"""
return saturate(ARM.r * lerp(1.0, saturate(VA), CavityStrength));
"""

WATER_RIPPLE_UV1 = r"""
float2 d = normalize(Dir.xy + float2(1e-5, 0));
float Ts = max(T, 1.0);
return P.xy / Ts - frac(d * Speed * Clock / Ts);
"""

WATER_RIPPLE_UV2 = r"""
// Second layer: 0.61x the size, flowing 25 degrees off FlowDir at 0.73x the speed, offset so the two never line up.
float2 d = normalize(Dir.xy + float2(1e-5, 0));
float s = 0.4226, c = 0.9063;
d = float2(c * d.x - s * d.y, s * d.x + c * d.y);
float Ts = max(T * 0.61, 1.0);
return P.xy / Ts + float2(0.37, 0.71) - frac(d * Speed * 0.73 * Clock / Ts);
"""

WATER_FOAM_UV = r"""
// Flow-aligned and stretched 2.5x along the flow, so the foam reads as streaks drifting downstream.
float2 d = normalize(Dir.xy + float2(1e-5, 0));
float2 q = float2(dot(P.xy, d), dot(P.xy, float2(-d.y, d.x)));
float Ts = max(T * 2.5, 1.0);
return float2(q.x / Ts - frac(Speed * Clock / Ts), q.y / max(T, 1.0));
"""

WATER_BED_UV = r"""
// The bed seen through the surface: shifted a little by the ripples, less where it is deep.
float2 o = (R1.xy + R2.xy) * 0.5 * Refraction * (1.0 - 0.5 * saturate(VA));
return P.xy / max(T, 1.0) + o;
"""

WATER_FOAM = SRGB_DECODE + r"""
float l = dot(Foam, float3(0.2126, 0.7152, 0.0722)) * FoamLumScale;
return saturate(smoothstep(0.3, 0.7, l) * saturate(Lin.r) * FoamStrength);
"""

WATER_BASE = r"""
float3 shallow = Bed * ShallowTint;
float3 c = lerp(shallow, DeepColor, saturate(VA));
return lerp(c, FoamColor, saturate(F));
"""

WATER_NORMAL = SRGB_DECODE + r"""
float3 n = normalize(N);
float s = Strength * (1.0 + saturate(Lin.r) * RiffleBoost);
float2 t = (R1.xy + R2.xy) * 0.5 * s;
return normalize(n + float3(t, 0));
"""

WATER_ROUGH = r"""
return saturate(lerp(ClearRough, FoamRoughness, saturate(F)));
"""


def grad(fn):
    return PLANE_GRAD % fn


# ------------------------------------------------------------------------------------------------------------ M_CalderGround
def build_ground():
    name = "M_CalderGround"
    mat = open_material(name)
    g = Graph(mat, name)
    pos = g.node("WorldPosition", -2600, 0)
    nrm = g.node("VertexNormalWS", -2600, 120)
    vc = g.node("VertexColor", -2600, 240)

    def uv(tile, x, y, scale=""):
        code = "return P.xy / max(T%s, 1.0);" % scale
        return g.custom("WorldXY", code, [("P", pos, ""), ("T", tile, "")], "CMOT_FLOAT2", x, y)

    lawn_tile = g.scalar("LawnTileSize", 220.0, "Lawn", "cm per repeat of the lawn textures", -2400, 0)
    meadow_tile = g.scalar("MeadowTileSize", 250.0, "Meadow", "cm per repeat of the meadow albedo", -2400, 400)
    litter_tile = g.scalar("LitterTileSize", 230.0, "Litter", "cm per repeat of the leaf litter", -2400, 700)
    clay_tile = g.scalar("ClayTileSize", 200.0, "Clay", "cm per repeat of the clay/mud", -2400, 1100)
    rock_tile = g.scalar("RockTileSize", 320.0, "Rock", "cm per repeat of the rock (projected per dominant axis)",
                         -2400, 1400)

    lawn_obj = g.tex_object("LawnAlbedo", calder_tex("Grass004", "A"), "Lawn",
                            "lawn albedo; also read 23x larger for the macro variation", -2000, 0)
    lawn_uv = uv(lawn_tile, -2200, 0)
    macro_uv = uv(lawn_tile, -2200, 120, " * 23.0")
    lawn_a = g.sample(lawn_obj, calder_tex("Grass004", "A"), -1700, 0, lawn_uv)
    macro_a = g.sample(lawn_obj, calder_tex("Grass004", "A"), -1700, 160, macro_uv, mip_bias=3.0)
    lawn_n = g.tex_param("LawnNormal", calder_tex("Grass004", "N"), "Lawn", "lawn normal (also the meadow's)",
                         -1700, 300, uv=lawn_uv)
    meadow_a = g.tex_param("MeadowAlbedo", calder_tex("withered_grass", "A"), "Meadow", "meadow (dry grass) albedo",
                           -1700, 450, uv=uv(meadow_tile, -2200, 450))
    litter_uv = uv(litter_tile, -2200, 700)
    litter_a = g.tex_param("LitterAlbedo", calder_tex("forest_leaves_03", "A"), "Litter", "leaf litter albedo",
                           -1700, 650, uv=litter_uv)
    litter_n = g.tex_param("LitterNormal", calder_tex("forest_leaves_03", "N"), "Litter", "leaf litter normal",
                           -1700, 800, uv=litter_uv)
    litter_arm = g.tex_param("LitterARM", calder_tex("forest_leaves_03", "ARM"), "Litter",
                             "leaf litter AO/Rough/Metal", -1700, 950, uv=litter_uv)
    clay_uv = uv(clay_tile, -2200, 1100)
    clay_a = g.tex_param("ClayAlbedo", calder_tex("red_mud_stones", "A"), "Clay", "clay/mud albedo", -1700, 1100,
                         uv=clay_uv)
    clay_n = g.tex_param("ClayNormal", calder_tex("red_mud_stones", "N"), "Clay", "clay/mud normal", -1700, 1250,
                         uv=clay_uv)
    rock_in = [("P", pos, ""), ("N", nrm, ""), ("T", rock_tile, "")]
    rock_uv = g.custom("PlaneUV", PLANE_UV, rock_in, "CMOT_FLOAT2", -2200, 1400)
    rock_dx = g.custom("PlaneDDX", grad("ddx"), rock_in, "CMOT_FLOAT2", -2200, 1500)
    rock_dy = g.custom("PlaneDDY", grad("ddy"), rock_in, "CMOT_FLOAT2", -2200, 1600)
    rock_a = g.tex_param("RockAlbedo", calder_tex("mossy_rock", "A"), "Rock", "rock albedo", -1700, 1400,
                         uv=rock_uv, ddx=rock_dx, ddy=rock_dy)
    rock_n = g.tex_param("RockNormal", calder_tex("mossy_rock", "N"), "Rock", "rock normal", -1700, 1550,
                         uv=rock_uv, ddx=rock_dx, ddy=rock_dy)
    rock_arm = g.tex_param("RockARM", calder_tex("mossy_rock", "ARM"), "Rock", "rock AO/Rough/Metal", -1700, 1700,
                           uv=rock_uv, ddx=rock_dx, ddy=rock_dy)

    s = lambda n, v, grp, meaning, y: g.scalar(n, v, grp, meaning, -1300, y)  # noqa: E731
    meadow_start = s("MeadowSlopeStart", 0.28, "Meadow", "slope (1 - N.z) where the meadow starts replacing the lawn",
                     -400)
    meadow_end = s("MeadowSlopeEnd", 0.40, "Meadow", "slope (1 - N.z) where the lawn is all meadow", -320)
    meadow_amount = s("MeadowAmount", 0.0, "Meadow", "0..1 added to the slope term: whole-tile meadow", -240)
    rock_start = s("RockSlopeStart", 0.55, "Rock", "slope (1 - N.z) where rock starts showing", -160)
    rock_end = s("RockSlopeEnd", 0.75, "Rock", "slope (1 - N.z) where it is all rock", -80)
    height_blend = s("HeightBlend", 0.5, "Blend", "0..1: how much each layer's texture height leans the blend",
                     1850)
    sharp = s("BlendSharpness", 2.0, "Blend", ">=1: power on the weights before normalising (1 = linear)", 1930)
    weights = g.custom("GroundWeights", GROUND_WEIGHTS, [
        ("N", nrm, ""), ("VC", vc, ""), ("MeadowSlopeStart", meadow_start, ""), ("MeadowSlopeEnd", meadow_end, ""),
        ("MeadowAmount", meadow_amount, ""), ("RockSlopeStart", rock_start, ""), ("RockSlopeEnd", rock_end, ""),
        ("HeightBlend", height_blend, ""), ("BlendSharpness", sharp, ""), ("LawnA", lawn_a, ""),
        ("MeadowA", meadow_a, ""), ("LitterARM", litter_arm, ""), ("ClayA", clay_a, ""), ("RockARM", rock_arm, "")],
        "CMOT_FLOAT4", -1000, -200)

    v = lambda n, grp, meaning, y: g.vector(n, (1.0, 1.0, 1.0, 1.0), grp, meaning, -1300, y)  # noqa: E731
    base = g.custom("GroundBaseColor", GROUND_BASE, [
        ("W", weights, ""), ("LawnA", lawn_a, ""), ("MacroA", macro_a, ""), ("MeadowA", meadow_a, ""),
        ("LitterA", litter_a, ""), ("ClayA", clay_a, ""), ("RockA", rock_a, ""),
        ("LawnTint", v("LawnTint", "Lawn", "multiplies the lawn albedo", 0), ""),
        ("MeadowTint", v("MeadowTint", "Meadow", "multiplies the meadow albedo", 100), ""),
        ("LitterTint", v("LitterTint", "Litter", "multiplies the litter albedo", 200), ""),
        ("ClayTint", v("ClayTint", "Clay", "multiplies the clay albedo", 300), ""),
        ("RockTint", v("RockTint", "Rock", "multiplies the rock albedo", 400), ""),
        ("MacroStrength", s("MacroStrength", 0.35, "Macro", "0..1 depth of the large-scale lawn/meadow variation", 500),
         ""),
        ("MacroPivot", s("MacroPivot", 0.14, "Macro",
                         "linear luminance of the macro read that maps to x1 (Grass004 averages 0.14)", 580), ""),
        ("Darken", s("Darken", 1.0, "Global", "multiplies the final base colour", 660), "")],
        "CMOT_FLOAT3", -700, 0)
    g.out(base, "BASE_COLOR")
    normal = g.custom("GroundNormal", GROUND_NORMAL, [
        ("N", nrm, ""), ("W", weights, ""), ("LawnN", lawn_n, ""), ("LitterN", litter_n, ""), ("ClayN", clay_n, ""),
        ("RockN", rock_n, ""),
        ("NormalStrength", s("NormalStrength", 1.0, "Global", "scales every layer's normal detail", 760), "")],
        "CMOT_FLOAT3", -700, 300)
    g.out(normal, "NORMAL")
    rough = g.custom("GroundRoughness", GROUND_ROUGH, [
        ("W", weights, ""),
        ("LawnRoughness", s("LawnRoughness", 0.9, "Lawn", "lawn roughness (constant)", 860), ""),
        ("MeadowRoughness", s("MeadowRoughness", 0.92, "Meadow", "meadow roughness (constant)", 940), ""),
        ("LitterARM", litter_arm, ""),
        ("ClayRoughness", s("ClayRoughness", 0.92, "Clay",
                            "clay roughness (constant; the 12-sample budget leaves no clay ARM read)", 1020), ""),
        ("RockARM", rock_arm, "")], "CMOT_FLOAT1", -700, 500)
    g.out(rough, "ROUGHNESS")
    ao = g.custom("GroundAO", GROUND_AO, [
        ("W", weights, ""), ("LitterARM", litter_arm, ""), ("RockARM", rock_arm, ""), ("VA", vc, "A"),
        ("CavityStrength", s("CavityStrength", 0.85, "Global", "AO *= lerp(1, vertex A, CavityStrength)", 1100), "")],
        "CMOT_FLOAT1", -700, 650)
    g.out(ao, "AMBIENT_OCCLUSION")
    return finish(g)


# ------------------------------------------------------------------------------------------------------------ M_CalderRock
def build_rock():
    name = "M_CalderRock"
    mat = open_material(name)
    g = Graph(mat, name)
    pos = g.node("WorldPosition", -2200, 0)
    nrm = g.node("VertexNormalWS", -2200, 120)
    vc = g.node("VertexColor", -2200, 240)
    tile = g.scalar("TileSize", 180.0, "Rock", "cm per repeat (projected per dominant axis)", -2000, 0)
    plane_in = [("P", pos, ""), ("N", nrm, ""), ("T", tile, "")]
    puv = g.custom("PlaneUV", PLANE_UV, plane_in, "CMOT_FLOAT2", -1800, 0)
    pdx = g.custom("PlaneDDX", grad("ddx"), plane_in, "CMOT_FLOAT2", -1800, 100)
    pdy = g.custom("PlaneDDY", grad("ddy"), plane_in, "CMOT_FLOAT2", -1800, 200)
    albedo = g.tex_param("Albedo", calder_tex("stacked_stone_wall", "A"), "Rock", "rock albedo", -1500, 0,
                         uv=puv, ddx=pdx, ddy=pdy)
    rnorm = g.tex_param("Normal", calder_tex("stacked_stone_wall", "N"), "Rock", "rock normal", -1500, 150,
                        uv=puv, ddx=pdx, ddy=pdy)
    arm = g.tex_param("ARM", calder_tex("stacked_stone_wall", "ARM"), "Rock", "rock AO/Rough/Metal", -1500, 300,
                      uv=puv, ddx=pdx, ddy=pdy)
    moss_tile = g.scalar("MossTileSize", 150.0, "Moss", "cm per repeat of the moss (read in world XY)", -2000, 400)
    muv = g.custom("WorldXY", "return P.xy / max(T, 1.0);", [("P", pos, ""), ("T", moss_tile, "")], "CMOT_FLOAT2",
                   -1800, 400)
    malb = g.tex_param("MossAlbedo", calder_tex("mossy_rock", "A"), "Moss", "moss albedo", -1500, 450, uv=muv)
    mnorm = g.tex_param("MossNormal", calder_tex("mossy_rock", "N"), "Moss", "moss normal", -1500, 600, uv=muv)
    strength = g.scalar("NormalStrength", 1.0, "Rock", "scales the rock and moss normal detail", -1200, 700)
    moss = g.custom("MossMask", ROCK_MOSS, [
        ("N", nrm, ""), ("RockN", rnorm, ""), ("ARM", arm, ""),
        ("MossAmount", g.scalar("MossAmount", 0.35, "Moss", "0..1: 0 none, 1 every face that looks up", -1200, 800),
         ""),
        ("MossSharpness", g.scalar("MossSharpness", 6.0, "Moss", "edge sharpness of the moss line", -1200, 880), ""),
        ("NormalStrength", strength, "")], "CMOT_FLOAT1", -900, 600)
    base = g.custom("RockBaseColor", ROCK_BASE, [
        ("Albedo", albedo, ""), ("MossAlbedo", malb, ""), ("Moss", moss, ""),
        ("Tint", g.vector("Tint", (1.0, 1.0, 1.0, 1.0), "Rock", "multiplies the rock albedo", -1200, -200), ""),
        ("MossTint", g.vector("MossTint", (1.0, 1.0, 1.0, 1.0), "Moss", "multiplies the moss albedo", -1200, -100),
         "")], "CMOT_FLOAT3", -600, 0)
    g.out(base, "BASE_COLOR")
    normal = g.custom("RockNormal", ROCK_NORMAL, [
        ("N", nrm, ""), ("RockN", rnorm, ""), ("MossN", mnorm, ""), ("Moss", moss, ""), ("NormalStrength", strength, "")],
        "CMOT_FLOAT3", -600, 200)
    g.out(normal, "NORMAL")
    rough = g.custom("RockRoughness", ROCK_ROUGH, [
        ("ARM", arm, ""), ("Moss", moss, ""),
        ("MossRoughness", g.scalar("MossRoughness", 0.85, "Moss", "moss roughness (constant)", -1200, 960), "")],
        "CMOT_FLOAT1", -600, 400)
    g.out(rough, "ROUGHNESS")
    ao = g.custom("RockAO", ROCK_AO, [
        ("ARM", arm, ""), ("VA", vc, "A"),
        ("CavityStrength", g.scalar("CavityStrength", 1.0, "Rock", "AO *= lerp(1, vertex A, CavityStrength)", -1200,
                                    1040), "")], "CMOT_FLOAT1", -600, 550)
    g.out(ao, "AMBIENT_OCCLUSION")
    return finish(g)


# ------------------------------------------------------------------------------------------------------------ M_CalderWater
def ripple_default():
    for p in ("/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n",
              "/Engine/EngineMaterials/DefaultNormal"):
        tex = unreal.load_asset(p)
        if isinstance(tex, unreal.Texture2D):
            return tex
    raise RuntimeError("no engine normal texture for RippleNormal")


def build_water():
    name = "M_CalderWater"
    mat = open_material(name)
    g = Graph(mat, name)
    pos = g.node("WorldPosition", -2200, 0)
    nrm = g.node("VertexNormalWS", -2200, 120)
    vc = g.node("VertexColor", -2200, 240)
    clock = g.node("Time", -2200, 360)
    flow = g.vector("FlowDir", (1.0, 0.0, 0.0, 0.0), "Flow", "world XY direction the water flows (normalised in the shader)",
                    -2000, 360)
    speed = g.scalar("FlowSpeed", 35.0, "Flow", "cm/s the ripples and the foam drift along FlowDir", -2000, 460)
    rtile = g.scalar("RippleTileSize", 180.0, "Ripple", "cm per repeat of the first ripple layer (second: x0.61)",
                     -2000, 560)
    rin = [("P", pos, ""), ("T", rtile, ""), ("Dir", flow, ""), ("Speed", speed, ""), ("Clock", clock, "")]
    uv1 = g.custom("RippleUV1", WATER_RIPPLE_UV1, rin, "CMOT_FLOAT2", -1800, 400)
    uv2 = g.custom("RippleUV2", WATER_RIPPLE_UV2, rin, "CMOT_FLOAT2", -1800, 500)
    rtex = ripple_default()
    robj = g.tex_object("RippleNormal", rtex, "Ripple", "ripple normal map, read twice (two scales, two drifts)",
                        -1800, 650)
    r1 = g.sample(robj, rtex, -1500, 400, uv1)
    r2 = g.sample(robj, rtex, -1500, 550, uv2)
    btile = g.scalar("BedTileSize", 150.0, "Bed", "cm per repeat of the bed albedo", -2000, -200)
    bed_uv = g.custom("BedUV", WATER_BED_UV, [
        ("P", pos, ""), ("T", btile, ""), ("R1", r1, ""), ("R2", r2, ""),
        ("Refraction", g.scalar("BedRefraction", 0.03, "Bed", "UV shift of the bed by the ripples (shallow water)",
                                -2000, -100), ""),
        ("VA", vc, "A")], "CMOT_FLOAT2", -1300, -200)
    bed = g.tex_param("BedAlbedo", calder_tex("river_small_rocks", "A"), "Bed", "creek bed albedo (seen through the water)",
                      -1000, -200, uv=bed_uv)
    ftile = g.scalar("FoamTileSize", 90.0, "Foam", "cm per repeat of the foam texture across the flow (x2.5 along it)",
                     -2000, 800)
    fuv = g.custom("FoamUV", WATER_FOAM_UV, [("P", pos, ""), ("T", ftile, ""), ("Dir", flow, ""), ("Speed", speed, ""),
                                             ("Clock", clock, "")], "CMOT_FLOAT2", -1800, 800)
    ftex = g.tex_param("FoamTex", calder_tex("ScatteredLeaves001", "A"), "Foam",
                       "foam pattern (its luminance is used)", -1500, 800, uv=fuv)
    foam = g.custom("FoamMask", WATER_FOAM, [
        ("VC", vc, ""), ("Foam", ftex, ""),
        ("FoamLumScale", g.scalar("FoamLumScale", 4.5, "Foam",
                                  "FoamTex luminance x this, then smoothstep(0.3, 0.7): 4.5 suits ScatteredLeaves001, ~1 a 0..1 noise",
                                  -1200, 900), ""),
        ("FoamStrength", g.scalar("FoamStrength", 1.0, "Foam", "scales the foam (vertex R x pattern)", -1200, 980), "")],
        "CMOT_FLOAT1", -900, 800)
    base = g.custom("WaterBaseColor", WATER_BASE, [
        ("Bed", bed, ""),
        ("ShallowTint", g.vector("ShallowTint", (0.55, 0.36, 0.18, 1.0), "Colour",
                                 "tea-brown tint on the bed albedo = ShallowColor", -1000, -400), ""),
        ("DeepColor", g.vector("DeepColor", (0.010, 0.013, 0.008, 1.0), "Colour", "near-black green-brown at A = 1",
                               -1000, -300), ""),
        ("VA", vc, "A"), ("F", foam, ""),
        ("FoamColor", g.vector("FoamColor", (0.75, 0.74, 0.70, 1.0), "Foam", "colour the foam whitens toward", -1000,
                               -100), "")], "CMOT_FLOAT3", -600, -200)
    g.out(base, "BASE_COLOR")
    normal = g.custom("WaterNormal", WATER_NORMAL, [
        ("N", nrm, ""), ("R1", r1, ""), ("R2", r2, ""), ("VC", vc, ""),
        ("Strength", g.scalar("RippleStrength", 0.5, "Ripple", "strength of the blended ripple normal", -1200, 300), ""),
        ("RiffleBoost", g.scalar("RiffleBoost", 1.5, "Ripple", "extra ripple strength where vertex R (riffle) is 1",
                                 -1200, 380), "")], "CMOT_FLOAT3", -600, 300)
    g.out(normal, "NORMAL")
    rough = g.custom("WaterRoughness", WATER_ROUGH, [
        ("ClearRough", g.scalar("Roughness", 0.04, "Surface", "roughness of clear water", -1200, 500), ""),
        ("FoamRoughness", g.scalar("FoamRoughness", 0.55, "Foam", "roughness where the foam is full", -1200, 580), ""),
        ("F", foam, "")], "CMOT_FLOAT1", -600, 500)
    g.out(rough, "ROUGHNESS")
    g.out(g.scalar("Specular", 0.5, "Surface", "specular: 0.5 = F0 0.04 (engine default); water's physical F0 0.02 would be 0.25",
                   -600, 650), "SPECULAR")
    return finish(g)


# ============================================================================================================ docs, config
def read_manifest():
    rows = {}
    with open(os.path.join(RAW, "CC0_SOURCES.csv"), newline="") as f:
        for row in csv.DictReader(f):
            rows[row["file"]] = row
    return rows


GROUND_VC = """| Channel | Meaning |
|---|---|
| R | rock weight (0..1). Final rock = max(R, smoothstep(RockSlopeStart, RockSlopeEnd, 1 - N.z)); rock covers every other layer. |
| G | leaf-litter weight (0..1) |
| B | clay/mud weight (0..1). If G + B > 1 they are scaled to sum 1. |
| A | cavity/valley occlusion: 1 open, 0 deep. AO *= lerp(1, A, CavityStrength). |
| (rest) | lawn = 1 - G - B (under the rock); the meadow takes the lawn's share by slope: smoothstep(MeadowSlopeStart, MeadowSlopeEnd, 1 - N.z) + MeadowAmount. |"""

ROCK_VC = """| Channel | Meaning |
|---|---|
| A | cavity AO multiplier: AO = ARM.r * lerp(1, A, CavityStrength) (default CavityStrength 1, so AO = ARM.r * A) |
| RGB | unused |"""

WATER_VC = """| Channel | Meaning |
|---|---|
| R | foam/riffle mask (0..1): foam = smoothstep(0.3, 0.7, lum(FoamTex) * FoamLumScale) * R * FoamStrength; the ripple normal is also x(1 + R * RiffleBoost) |
| A | depth: 0 shallow .. 1 deep. BaseColor = lerp(BedAlbedo * ShallowTint, DeepColor, A), then foam whitens toward FoamColor |
| G, B | unused |"""

VC_NOTE = """**How the C++ should write vertex colour.** Write linear 0..1 values into the FMeshDescription vertex-instance colours
(`FVector4f`). `UStaticMesh::BuildFromMeshDescription` stores them as `FColor` with `ToFColor(true)`
(Engine/Source/Runtime/Engine/Private/StaticMesh.cpp:8606), i.e. RGB sRGB-encoded and A linear; the materials decode RGB
back to linear (exact sRGB curve) and use A as stored. If a mesh is ever filled with raw `FColor`s some other way (no sRGB
encoding), RGB weights would read too low (0.5 -> 0.21): encode them with `FLinearColor(...).ToFColor(true)` first. A mesh
without a colour buffer reads white (R = G = B = A = 1): all rock on the ground master, so always give terrain tiles colours."""


def write_assets_md(plan, graphs):
    manifest = read_manifest()
    lines = ["# Calder landscape assets", "",
             "Generated by `Tools/claude/calder_materials.py` (%s). Edit the script, not this file." %
             datetime.datetime.now().strftime("%Y-%m-%d %H:%M"), "",
             "Everything lives under `/Game/SeniorSendOff/Calder/` (cooked: `+DirectoriesToAlwaysCook` in "
             "Config/DefaultGame.ini). Raw files: `~/SeniorSendoffCC0Raw/` (manifest `CC0_SOURCES.csv`); ambientCG zips "
             "were unzipped to `/tmp/calder_unzip/<id>/` for the import.", ""]
    present = [i for i in plan if assets.does_asset_exist(i["path"])]
    sets = sorted({i["set"] for i in present})
    lines += ["## Textures (%d assets, %d sets)" % (len(present), len(sets)), "",
              "LOD group World. Max size 2048, except %s (4096; jolcham_oak_bark_01's 2K download is 2048x4096), "
              "so no source is downscaled. Kinds: " % " and ".join([", ".join(sorted(BIG_SETS)[:-1]), sorted(BIG_SETS)[-1]]) +
              "; ".join("`%s` %s" % (k, v) for k, v in KIND_TEXT.items()) + ". Height (`_disp_`/`_Displacement`), "
              "OpenGL normals and the ambientCG preview PNGs were not imported (no material reads them).", "",
              "| Asset | Kind | Raw file | Source page | Licence |", "|---|---|---|---|---|"]
    for i in sorted(present, key=lambda x: x["path"].lower()):
        row = manifest.get(i["raw"], {})
        raw = i["raw"] + (" : " + i["member"] if i["member"] else "")
        lines.append("| `%s` | %s | `%s` | %s | %s |" % (i["path"], i["kind"], raw, row.get("page", "(not in manifest)"),
                                                       row.get("licence", "(not in manifest)")))
    missing = [i for i in plan if not assets.does_asset_exist(i["path"])]
    if missing:
        lines += ["", "**Not imported (failed):** " + ", ".join("`%s`" % i["path"] for i in missing)]
    lines += ["", "## Materials", "",
              "All three: opaque, default lit, legacy inputs (Substrate converts them), **world-space normal output** "
              "(`Tangent Space Normal` off; every layer is projected in world space, so mesh tangents do not matter), "
              "`Used with Static Lighting` off, `Used with Instanced Static Meshes` on (ISM-enabled), every texture on "
              "the shared wrap sampler (World group). Parameter names "
              "are exact; the C++ sets them by name on dynamic instances.", "", VC_NOTE, ""]
    usage = {"M_CalderGround": ("runtime terrain tile static meshes (FMeshDescription with vertex colour)", GROUND_VC,
                                "12 texture samples: lawn A + N, macro (lawn A again, 23x, mip bias 3), meadow A, "
                                "litter A + N + ARM, clay A + N, rock A + N + ARM (dominant-axis projection, "
                                "derivative-corrected mips). Weights are normalised; see HeightBlend/BlendSharpness."),
             "M_CalderRock": ("code-built retaining walls, rock cuts, ledges, boulders", ROCK_VC,
                              "Rock read per dominant world axis (XY for flat, YZ/XZ for steep faces, derivative-"
                              "corrected mips); moss read in world XY on faces that look up."),
             "M_CalderWater": ("creek and pond surface static meshes", WATER_VC,
                               "RippleNormal is read twice (RippleTileSize and x0.61, drifting along FlowDir at "
                               "FlowSpeed and 25 degrees off it at x0.73) and the two tilts are averaged.")}
    for name in ("M_CalderGround", "M_CalderRock", "M_CalderWater"):
        g = graphs.get(name)
        what, vc_table, note = usage[name]
        lines += ["### %s/%s" % (MAT_DIR, name), "", "For " + what + ". " + note, "",
                  "Result this run: " + MAT_RESULTS.get(name, "not built"), "", "Vertex colour:", "", vc_table, ""]
        if g is None:
            lines += ["(Parameters not listed: the build failed this run.)", ""]
            continue
        lines += ["| Parameter | Type | Default | Group | Meaning |", "|---|---|---|---|---|"]
        for p in g.params:
            lines.append("| `%s` | %s | `%s` | %s | %s |" % p)
        lines.append("")
    text = "\n".join(lines) + "\n"
    os.makedirs(os.path.dirname(ASSETS_MD), exist_ok=True)
    if os.path.exists(ASSETS_MD):
        os.remove(ASSETS_MD)   # iCloud: delete before rewriting, or a "name 2" copy appears
    with open(ASSETS_MD, "w") as f:
        f.write(text)
    log("ASSETS.md written: %s (%d texture rows)" % (ASSETS_MD, len(present)))


def add_cook_dir():
    with open(GAME_INI) as f:
        text = f.read()
    if COOK_LINE in text.splitlines():
        log("DefaultGame.ini already cooks /Game/SeniorSendOff/Calder")
        return
    if not text.endswith("\n"):
        text += "\n"
    text += COOK_LINE + "\n"
    os.remove(GAME_INI)   # iCloud: delete before rewriting
    with open(GAME_INI, "w") as f:
        f.write(text)
    log("DefaultGame.ini: appended " + COOK_LINE)


# ============================================================================================================ run
def run():
    assets.make_directory(TEX_ROOT)
    assets.make_directory(MAT_DIR)
    plan = plan_textures()
    import_textures(plan)
    graphs = {}
    for builder, name in ((build_ground, "M_CalderGround"), (build_rock, "M_CalderRock"), (build_water, "M_CalderWater")):
        try:
            graphs[name] = builder()
        except Exception as e:  # noqa: BLE001
            import traceback
            MAT_RESULTS[name] = "FAILED: %s" % e
            fail("%s: %s\n%s" % (name, e, traceback.format_exc()))
    try:
        write_assets_md(plan, graphs)
    except Exception as e:  # noqa: BLE001
        fail("ASSETS.md: %s" % e)
    try:
        add_cook_dir()
    except Exception as e:  # noqa: BLE001
        fail("DefaultGame.ini: %s" % e)
    for name, res in MAT_RESULTS.items():
        log("RESULT %s: %s" % (name, res))
    log("DONE textures planned=%d imported=%d skipped=%d failed=%d; failures=%d%s" % (
        len(plan), COUNTS["imported"], COUNTS["skipped"], COUNTS["failed"], len(FAILURES),
        "".join("\n  - " + f.splitlines()[0] for f in FAILURES)))


run()
