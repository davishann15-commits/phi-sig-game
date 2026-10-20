"""Build a static-mesh joint skin material; original Fab assets stay read-only.

The continuous static skin follows twelve existing procedural part transforms.
UV data carries the blends; there is no runtime skeletal mesh or mesh rewrite.
"""
import unreal

E = unreal.MaterialEditingLibrary
A = unreal.EditorAssetLibrary
path = "/Game/SeniorSendOff/Calder/Pig/M_CalderPigSkin"
mat = unreal.load_asset(path) if A.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_CalderPigSkin", "/Game/SeniorSendOff/Calder/Pig", unreal.Material, unreal.MaterialFactoryNew())
if not mat:
    raise RuntimeError("Cannot create Calder pig skin")
E.delete_all_material_expressions(mat)
mat.set_editor_properties({"blend_mode": unreal.BlendMode.BLEND_OPAQUE,
                          "shading_model": unreal.MaterialShadingModel.MSM_DEFAULT_LIT,
                          "tangent_space_normal": False,
                          "used_with_instanced_static_meshes": True})


def node(kind, **values):
    n = E.create_material_expression(mat, getattr(unreal, "MaterialExpression" + kind))
    n.set_editor_properties(values)
    return n


def out(n, prop, channel=""):
    if not E.connect_material_property(n, channel, getattr(unreal.MaterialProperty, "MP_" + prop)):
        raise RuntimeError("Missing output " + prop)


def texture(name, asset, kind):
    t = unreal.load_asset("/Game/AnimalVarietyPack/Pig/Textures/" + asset)
    if not t:
        raise RuntimeError("Missing acquired pig texture: " + asset)
    if kind != "NORMAL":
        if t.compression_settings == unreal.TextureCompressionSettings.TC_MASKS:
            kind = "MASKS"
        elif t.compression_settings == unreal.TextureCompressionSettings.TC_GRAYSCALE:
            kind = "GRAYSCALE" if t.srgb else "LINEAR_GRAYSCALE"
        else:
            kind = "COLOR" if t.srgb else "LINEAR_COLOR"
    return node("TextureSampleParameter2D", parameter_name=name, texture=t,
                sampler_type=getattr(unreal.MaterialSamplerType, "SAMPLERTYPE_" + kind))


albedo = texture("SkinAlbedo", "T_Pig_BaseColor", "COLOR")
normal = texture("SkinNormal", "T_Pig_Nml", "NORMAL")
orm = texture("SkinORM", "T_Pig_OcclusionRoughnessMetallic", "MASKS")
spec = texture("SkinSpecular", "T_Pig_Specular", "COLOR")
out(albedo, "BASE_COLOR")
out(orm, "ROUGHNESS", "G")
out(orm, "AMBIENT_OCCLUSION", "R")
out(spec, "SPECULAR", "R")
local = node("PreSkinnedPosition")
world = node("WorldPosition", world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
weights = {"W%d"%i:(node("TextureCoordinate",coordinate_index=i+1),"") for i in range(4)}
transforms = {}
for joint in range(12):
    prefix = "Joint%d" % joint
    defaults = {"Origin": (0, 0, 0), "X": (1, 0, 0), "Y": (0, 1, 0), "Z": (0, 0, 1)}
    for axis, value in defaults.items():
        name = prefix + axis
        transforms[name] = (node("VectorParameter", parameter_name=name, default_value=unreal.LinearColor(*value, 0)), "")


def custom(code, inputs):
    n = node("Custom",code=code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    pins = []
    for name in inputs:
        pin=unreal.CustomInput()
        pin.set_editor_property("input_name",name)
        pins.append(pin)
    n.set_editor_property("inputs",pins)
    for name,(source,channel) in inputs.items():
        if not E.connect_material_expressions(source,channel,n,name):raise RuntimeError("Missing input "+name)
    return n


def skin_code(is_normal):
    targets=[]
    for j in range(12):
        p="Joint%d"%j
        targets.append(("" if is_normal else p+"Origin+")+"P.x*"+p+"X+P.y*"+p+"Y+P.z*"+p+"Z")
    code="float3 targets[12]={"+",".join(targets)+"};\n"
    code+="float2 pairs[4]={W0,W1,W2,W3}; float total=0; float3 result=0;\n"
    code+="[unroll] for(int i=0;i<4;++i){float w=saturate(pairs[i].y);result+=targets[clamp((int)round(pairs[i].x),0,11)]*w;total+=w;}\n"
    return code+("return normalize(result/max(total,.0001));" if is_normal else "return result/max(total,.0001)-Current;")


blend=custom(skin_code(False),{"P":(local,""),"Current":(world,""),**weights,**transforms})
out(blend,"WORLD_POSITION_OFFSET")
normal_skin=custom(skin_code(True),{"P":(node("PreSkinnedNormal"),""),**weights,**transforms})
interpolator=node("VertexInterpolator")
if not E.connect_material_expressions(normal_skin,"",interpolator,"VS"):raise RuntimeError("Missing normal interpolator")
pixel_normal=custom(r"""
float3 N=normalize(SmoothN);
float3 dx=ddx(Pw),dy=ddy(Pw);float2 ux=ddx(UV),uy=ddy(UV);
float det=ux.x*uy.y-uy.x*ux.y;
if(abs(det)<1e-8)return N;
float3 T0=(dx*uy.y-dy*ux.y)/det;
float3 T=normalize(T0-N*dot(T0,N));
float3 B0=(-dx*uy.x+dy*ux.x)/det;
float3 B=normalize(B0-N*dot(B0,N)-T*dot(B0,T));
return normalize(T*Tex.x+B*Tex.y+N*Tex.z);
""",{"SmoothN":(interpolator,""),"Pw":(node("WorldPosition"),""),
      "UV":(node("TextureCoordinate",coordinate_index=0),""),"Tex":(normal,"")})
out(pixel_normal,"NORMAL")
E.recompile_material(mat)
A.save_loaded_asset(mat)
stats = E.get_statistics(mat)
if stats.num_pixel_shader_instructions <= 0:
    raise RuntimeError("Pig skin shader did not compile")
unreal.log("CALDER_PIG_SKIN PASS staticPartTransforms=12 continuousSkin=1 pixelInstructions=%d samples=%d runtimeSkeletons=0" % (
    stats.num_pixel_shader_instructions, stats.num_pixel_texture_samples))
