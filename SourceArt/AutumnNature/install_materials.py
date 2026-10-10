"""Installs nature textures and two material masters; never opens/saves the map."""
import unreal
from pathlib import Path

D='/Game/SeniorSendOff/House/ExteriorNature'
A=unreal.EditorAssetLibrary
E=unreal.MaterialEditingLibrary
T=unreal.AssetToolsHelpers.get_asset_tools()

def node(m,kind,**props):
    n=E.create_material_expression(m,getattr(unreal,'MaterialExpression'+kind))
    for k,v in props.items():n.set_editor_property(k,v)
    return n

def link(a,b,slot,output=''):
    names=list(E.get_material_expression_input_names(b))
    if slot not in names:
        key=lambda s: ''.join(c for c in s.lower() if c.isalnum())
        slot=next((v for v in names if key(v)==key(slot)),slot)
        if slot=='Coordinates':slot=next((v for v in names if 'uv' in v.lower()),slot)
    assert E.connect_material_expressions(a,output,b,slot),(b,slot,output)

def out(n,prop,output=''):
    assert E.connect_material_property(n,output,getattr(unreal.MaterialProperty,'MP_'+prop))

def scalar(m,name,v):
    return node(m,'ScalarParameter',parameter_name=name,default_value=v)

def texture(m,name,normal=False):
    p='/Game/SeniorSendOff/House/Room204/T_R204_R2_FridgeEnamel_Normal' if normal else '/Engine/EngineResources/WhiteSquareTexture'
    t=A.load_asset(p)
    assert t,p
    return node(m,'TextureSampleParameter2D',parameter_name=name,texture=t,
        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

def op(m,kind,a,b):
    n=node(m,kind);link(a,n,'A');link(b,n,'B');return n

def custom(m,code,inputs,typ='CMOT_FLOAT3'):
    n=node(m,'Custom',code=code,output_type=getattr(unreal.CustomMaterialOutputType,typ))
    data=[]
    for name,_ in inputs:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);data.append(i)
    n.set_editor_property('inputs',data)
    for name,source in inputs:link(source,n,name)
    return n

def material(name):
    m=A.load_asset(D+'/'+name) if A.does_asset_exist(D+'/'+name) else T.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew())
    E.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes',True)
    return m

def save(m):
    E.recompile_material(m)
    assert A.save_loaded_asset(m,False),m.get_name()
    unreal.log('AUTUMN_MATERIAL_SAVED '+m.get_path_name())

def import_surfaces():
    files=[
        ('grass_ground','GrassGreen'),('withered_grass','GrassDry'),('tree_bark_03','Bark')]
    tasks=[]
    for source,label in files:
        for suffix,kind,ext in [('Diffuse','Base','jpg'),('nor_dx','Normal','png')]:
            task=unreal.AssetImportTask()
            task.set_editor_property('filename',str(Path(__file__).resolve().parent/'assets'/f'{source}_{suffix}.{ext}'))
            task.set_editor_property('destination_path',D)
            task.set_editor_property('destination_name',f'T_Nature_{label}_{kind}')
            task.set_editor_property('automated',True)
            task.set_editor_property('replace_existing',True)
            task.set_editor_property('save',False)
            tasks.append((task,kind))
    T.import_asset_tasks([task for task,_ in tasks])
    for task,kind in tasks:
        paths=task.get_editor_property('imported_object_paths')
        assert len(paths)==1,(task,paths)
        tex=A.load_asset(paths[0])
        tex.set_editor_property('srgb',kind!='Normal')
        tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if kind=='Normal' else unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP)
        tex.set_editor_property('address_y',unreal.TextureAddress.TA_WRAP)
        tex.set_editor_property('max_texture_size',1024)
        if kind=='Normal':tex.set_editor_property('flip_green_channel',False)
        assert A.save_loaded_asset(tex,False)
        unreal.log('AUTUMN_TEXTURE_SAVED '+tex.get_path_name())

def run():
    import_surfaces()
    m=material('M_NatureGround')
    p=node(m,'WorldPosition')
    macro_uv=custom(m,'return (P.xy + float2(8200, 10000)) / float2(16400, 18000);',[('P',p)],'CMOT_FLOAT2')
    macro=texture(m,'MacroMap');link(macro_uv,macro,'Coordinates')
    uv=node(m,'TextureCoordinate')
    green=texture(m,'BaseMap');link(op(m,'Multiply',uv,scalar(m,'GreenTileScale',.72)),green,'Coordinates')
    dry=texture(m,'DryMap')
    shade=custom(m,'''float3 grass=lerp(Dry*float3(.65,.63,.56), Green*float3(.82,1.05,.76), Macro.r);
return lerp(grass,Green*float3(.38,.28,.19),Macro.g*.75)*(0.84+Macro.b*.26);''',
        [('Macro',macro),('Green',green),('Dry',dry)])
    out(shade,'BASE_COLOR')
    gn=texture(m,'NormalMap',True);link(op(m,'Multiply',uv,scalar(m,'GreenNormalTileScale',.72)),gn,'Coordinates')
    dn=texture(m,'DryNormalMap',True)
    normal=custom(m,'float3 N=lerp(Dry,Green,Macro.r);return normalize(float3(N.xy*.7,N.z));',[('Macro',macro),('Green',gn),('Dry',dn)])
    out(normal,'NORMAL')
    out(scalar(m,'Roughness',.96),'ROUGHNESS')
    save(m)

    m=material('M_NatureFoliage')
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('opacity_mask_clip_value',.32)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    base=texture(m,'BaseMap')
    tint=node(m,'VectorParameter',parameter_name='Tint',default_value=unreal.LinearColor(1,1,1,1))
    col=op(m,'Multiply',base,tint);out(col,'BASE_COLOR')
    sss=op(m,'Multiply',col,scalar(m,'Transmission',.25));out(sss,'SUBSURFACE_COLOR')
    fade=node(m,'PerInstanceFadeAmount')
    alpha=node(m,'Multiply');link(base,alpha,'A','A');link(fade,alpha,'B');out(alpha,'OPACITY_MASK')
    out(scalar(m,'Roughness',.82),'ROUGHNESS')
    out(scalar(m,'Specular',.2),'SPECULAR')
    normal=texture(m,'NormalMap',True)
    # Keep the authored volume normal on both sides of the thin leaf cards.
    out(op(m,'Multiply',normal,node(m,'TwoSidedSign')),'NORMAL')
    wind=custom(m,'''float tip=saturate(1-UV.y); tip*=tip;
float phase=Clock*.85 + P.x*.006 + P.y*.004;
float gust=sin(phase)+sin(Clock*1.55+P.y*.011)*.23;
return float3(gust, sin(phase*.8+1.3)*.55, sin(phase*1.2)*.08)*Strength*tip;''',
        [('P',node(m,'WorldPosition')),('Clock',node(m,'Time')),('UV',node(m,'TextureCoordinate')),('Strength',scalar(m,'WindStrength',1.3))])
    out(wind,'WORLD_POSITION_OFFSET')
    save(m)
    (Path(__file__).resolve().parent/'evidence/materials.txt').write_text('PASS: Six nature textures and ground/masked two-sided foliage masters saved. No map edits.\n')

run()
