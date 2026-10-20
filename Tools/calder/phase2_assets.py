"""Stage Calder's asset batch with Unreal closed; never load or save a map.

Creates only M_CalderFalls in the Calder folder, and exports already-acquired
animal meshes for offline rigid-part preparation. Existing Fab assets and the
three Phase 1 masters are read-only. Run with -AllowCommandletRendering.
"""
import json
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "SourceArt/CalderLandscape/phase2"
MAT_DIR = "/Game/SeniorSendOff/Calder/Materials"
EDIT = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def node(mat, kind, **properties):
    value = EDIT.create_material_expression(mat, getattr(unreal, "MaterialExpression" + kind))
    if properties:
        value.set_editor_properties(properties)
    return value


def custom(mat, code, inputs, output_type):
    value = node(mat, "Custom", code=code, output_type=getattr(unreal.CustomMaterialOutputType, output_type))
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    value.set_editor_property("inputs", pins)
    for name, source in inputs.items():
        if not EDIT.connect_material_expressions(source, "", value, name):
            raise RuntimeError("Could not connect " + name)
    return value


def output(value, property_name, channel=""):
    if not EDIT.connect_material_property(value, channel, getattr(unreal.MaterialProperty, "MP_" + property_name)):
        raise RuntimeError("Could not connect " + property_name)


def falls_material():
    name = "M_CalderFalls"
    path = MAT_DIR + "/" + name
    mat = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else TOOLS.create_asset(
        name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("Cannot create " + path)
    for expression in list(EDIT.get_material_expressions(mat)):
        EDIT.delete_material_expression(mat, expression)
    mat.set_editor_properties({
        "blend_mode": unreal.BlendMode.BLEND_OPAQUE,
        "shading_model": unreal.MaterialShadingModel.MSM_DEFAULT_LIT,
        "two_sided": False,  # curtain already has separately wound front/back faces
        "tangent_space_normal": False,
        "used_with_instanced_static_meshes": True,
    })
    position = node(mat, "WorldPosition")
    clock = node(mat, "Time")
    normal = node(mat, "VertexNormalWS")
    top = node(mat, "ScalarParameter", parameter_name="FallTopZ", default_value=600.0)
    bottom = node(mat, "ScalarParameter", parameter_name="FallBottomZ", default_value=-80.0)
    speed = node(mat, "ScalarParameter", parameter_name="FlowSpeed", default_value=380.0)
    # World X/Z projection: XY projection is effectively stationary on a
    # vertical sheet. Advect down Z, stretch noise vertically and break it
    # into overlapping jets instead of painting a solid brown surface.
    pattern = custom(mat, r"""
struct WaterNoise {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float sample(float2 p) {
  float2 i=floor(p), f=frac(p); f=f*f*(3-2*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),
          lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
 }
};
WaterNoise n;
float across=P.x+P.y*0.11;
float2 uv=float2(across/24,(P.z+T*Speed)/180);
float a=n.sample(uv), b=n.sample(uv*float2(2.71,1.93)+float2(17,3));
float fine=n.sample(float2(across/3.7,(P.z+T*Speed*1.17)/19)+float2(8,23));
float grains=n.sample(float2(across/1.9,(P.z+T*Speed*.87)/6.3));
float jets=smoothstep(0.19,0.76,a*0.55+b*0.28+fine*0.17);
float height=saturate((Top-P.z)/max(Top-Bottom,1));
float aeration=smoothstep(0.12,0.9,height);
float bubbles=smoothstep(.28,.73,fine*.65+grains*.35);
float foam=saturate(jets*(.22+.68*aeration)+bubbles*(.14+.37*aeration));
float ripple=fine*.7+grains*.3;
return float4(foam,jets,ripple,height);
""", {"P": position, "T": clock, "Top": top, "Bottom": bottom, "Speed": speed}, "CMOT_FLOAT4")
    colour = custom(mat, "return lerp(float3(.012,.025,.031),float3(.67,.72,.73),Water.r);", {"Water": pattern}, "CMOT_FLOAT3")
    perturb = custom(mat, r"""
float3 n=normalize(N), dx=ddx(P), dy=ddy(P);
float3 rx=cross(dy,n), ry=cross(n,dx);
float det=dot(dx,rx);
float h=Water.b*.40;
return normalize(abs(det)*n-sign(det)*(ddx(h)*rx+ddy(h)*ry));
""", {"N": normal, "P": position, "Water": pattern}, "CMOT_FLOAT3")
    emission = custom(mat, "return float3(.32,.45,.56)*Water.r;", {"Water": pattern}, "CMOT_FLOAT3")
    rough = node(mat, "Constant", r=0.06)
    specular = node(mat, "Constant", r=0.25)
    output(colour, "BASE_COLOR")
    output(perturb, "NORMAL")
    output(emission, "EMISSIVE_COLOR")
    output(rough, "ROUGHNESS")
    output(specular, "SPECULAR")
    EDIT.recompile_material(mat)
    ASSETS.save_loaded_asset(mat)
    stats = EDIT.get_statistics(mat)
    result = {k: stats.get_editor_property(k) for k in ["num_pixel_shader_instructions", "num_pixel_texture_samples"]}
    if result["num_pixel_shader_instructions"] <= 0:
        raise RuntimeError("Falls material did not compile; use -AllowCommandletRendering")
    result.update({"material": path, "blend_mode": "opaque", "roughness": 0.06,
                   "projection": "world XZ", "normal": "world derivative bump"})
    OUT.mkdir(parents=True, exist_ok=True)
    report_path = OUT / 'falls_material.json'
    report_path.unlink(missing_ok=True)
    report_path.write_text(json.dumps(result, indent=2))
    unreal.log("CALDER_PHASE2_MATERIAL " + json.dumps(result))
    return result


def stream_material():
    """Keep the original water master read-only. The new look replaces the
    leaf-photo foam mask with continuous advected foam and glossy whitewater.
    """
    path = MAT_DIR + '/M_CalderStream'
    if ASSETS.does_asset_exist(path):
        if not ASSETS.delete_asset(path):
            raise RuntimeError('Cannot rebuild ' + path)
    mat = ASSETS.duplicate_asset(MAT_DIR + '/M_CalderWater', path)
    if not mat:
        raise RuntimeError('Cannot duplicate original water master')
    expressions = list(EDIT.get_material_expressions(mat))
    nodes = {str(x.get_editor_property('description')): x for x in expressions
             if x.get_class().get_name() == 'MaterialExpressionCustom'}
    foam, flow_uv = nodes['FoamMask'], nodes['FoamUV']
    flow_uv.set_editor_property('code', r"""
float2 d=normalize(Dir.xy+float2(1e-5,0));
float2 q=float2(dot(P.xy,d),dot(P.xy,float2(-d.y,d.x)));
float size=max(T*2.5,1.0);
return float2((q.x-Speed*Clock)/size,q.y/max(T,1.0));
""")
    prefix = str(foam.get_editor_property('code')).split('float l =')[0]
    foam.set_editor_property('code', prefix + r"""
struct FoamNoise {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float sample(float2 p) {
  float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),
              lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
 }
};
FoamNoise n;
float pattern=n.sample(FlowUV*float2(5,8))*.65+n.sample(FlowUV*float2(17,19)+float2(3,17))*.35;
return saturate(smoothstep(.29,.73,pattern)*saturate(Lin.r)*FoamStrength);
""")
    inputs = list(foam.get_editor_property('inputs'))
    pin = unreal.CustomInput()
    pin.set_editor_property('input_name', 'FlowUV')
    inputs.append(pin)
    foam.set_editor_property('inputs', inputs)
    if not EDIT.connect_material_expressions(flow_uv, '', foam, 'FlowUV'):
        raise RuntimeError('Cannot bind continuous foam coordinates')
    glow = custom(mat, 'return F*float3(.045,.065,.085);', {'F': foam}, 'CMOT_FLOAT3')
    if not EDIT.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Cannot bind whitewater scatter')
    EDIT.recompile_material(mat)
    ASSETS.save_loaded_asset(mat)
    stats = EDIT.get_statistics(mat)
    result = {k: stats.get_editor_property(k) for k in ['num_pixel_shader_instructions', 'num_pixel_texture_samples']}
    if result['num_pixel_shader_instructions'] <= 0 or result['num_pixel_texture_samples'] > 12:
        raise RuntimeError('Stream material compile/budget failed')
    report_path = OUT / 'stream_material.json'
    report_path.unlink(missing_ok=True)
    report_path.write_text(json.dumps(result, indent=2))
    unreal.log('CALDER_STREAM_MATERIAL ' + json.dumps(result))
    return result


def export_donors():
    records = []
    for species, path in {
        "pig": "/Game/AnimalVarietyPack/Pig/Meshes/SK_Pig",
        "doe": "/Game/AnimalVarietyPack/DeerStagAndDoe/Meshes/SK_DeerDoe",
        "fox": "/Game/AnimalVarietyPack/Fox/Meshes/SK_Fox",
        "crow": "/Game/AnimalVarietyPack/Crow/Meshes/SK_Crow",
    }.items():
        asset = unreal.load_asset(path)
        if not asset:
            raise RuntimeError("Missing acquired donor: " + path)
        destination = OUT / (species + ".fbx")
        if not destination.exists():
            task = unreal.AssetExportTask()
            task.object = asset
            task.filename = str(destination)
            task.automated = True
            task.prompt = False
            task.replace_identical = False
            task.exporter = unreal.SkeletalMeshExporterFBX()
            task.options = unreal.FbxExportOption()
            if not unreal.Exporter.run_asset_export_task(task):
                raise RuntimeError("Failed export: " + species)
        records.append({"species": species, "source": path, "export": str(destination), "bytes": destination.stat().st_size})
    return records


def run():
    if "allowcommandletrendering" not in unreal.SystemLibrary.get_command_line().lower():
        raise RuntimeError("Run with -AllowCommandletRendering")
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"falls_material": falls_material(), "donors": export_donors()}
    report["vegetation_assets"] = [p for base in ["/Game/EuropeanBeech", "/Game/NorwayMaple", "/Game/Environment/Megascans"]
                                    for p in ASSETS.list_assets(base, recursive=True, include_folder=False)]
    target = OUT / "asset_batch.json"
    target.unlink(missing_ok=True)
    target.write_text(json.dumps(report, indent=2))
    unreal.log("CALDER_PHASE2_ASSETS PASS " + str(target))


if __name__ == "__main__":
    run()
