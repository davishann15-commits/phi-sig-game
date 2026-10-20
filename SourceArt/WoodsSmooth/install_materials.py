"""Save two woodland-only surface masters; leave the house lawn and map untouched."""
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

def link(a,b,slot):
    names=list(E.get_material_expression_input_names(b))
    if slot not in names:
        key=lambda s: ''.join(c for c in s.lower() if c.isalnum())
        slot=next((v for v in names if key(v)==key(slot)),slot)
        if slot=='Coordinates':slot=next((v for v in names if 'uv' in v.lower()),slot)
    assert E.connect_material_expressions(a,'',b,slot),(b,slot)

def custom(m,code,inputs,typ='CMOT_FLOAT3'):
    n=node(m,'Custom',code=code,output_type=getattr(unreal.CustomMaterialOutputType,typ))
    data=[]
    for name,_ in inputs:
        item=unreal.CustomInput();item.set_editor_property('input_name',name);data.append(item)
    n.set_editor_property('inputs',data)
    for name,source in inputs:link(source,n,name)
    return n

def texture(m,name,normal=False):
    path='/Game/SeniorSendOff/House/ExteriorNature/T_Nature_GrassDry_Normal' if normal else '/Engine/EngineResources/WhiteSquareTexture'
    return node(m,'TextureSampleParameter2D',parameter_name=name,texture=A.load_asset(path),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

def output(n,p):assert E.connect_material_property(n,'',getattr(unreal.MaterialProperty,'MP_'+p))

def make(name,trail):
    m=A.load_asset(D+'/'+name) if A.does_asset_exist(D+'/'+name) else T.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew())
    E.delete_all_material_expressions(m)
    p=node(m,'WorldPosition');uv=node(m,'TextureCoordinate')
    grounduv=custom(m,'return P.xy/180.0;',[('P',p)],'CMOT_FLOAT2')
    greenuv=custom(m,'return UV*0.72;',[('UV',grounduv)],'CMOT_FLOAT2')
    macrouv=custom(m,'return (P.xy+float2(8200,10000))/float2(16400,18000);',[('P',p)],'CMOT_FLOAT2')
    macro=texture(m,'MacroMap');coverage=texture(m,'CoverageMap');link(macrouv,macro,'Coordinates');link(macrouv,coverage,'Coordinates')
    mixed=custom(m,'''float mask=smoothstep(4600,5350,P.y)*(1-smoothstep(18550,19200,P.y))*smoothstep(-8200,-7600,P.x)*(1-smoothstep(7600,8200,P.x));
return lerp(Macro,Coverage,mask);''',[('P',p),('Macro',macro),('Coverage',coverage)])
    green=texture(m,'BaseMap');dry=texture(m,'DryMap');link(greenuv,green,'Coordinates');link(grounduv,dry,'Coordinates')
    ground=custom(m,'''float3 grass=lerp(Dry*float3(.65,.63,.56),Green*float3(.82,1.05,.76),Macro.r);
return lerp(grass,Green*float3(.38,.28,.19),Macro.g*.75)*(0.84+Macro.b*.26);''',[('Macro',mixed),('Green',green),('Dry',dry)])
    gn=texture(m,'NormalMap',True);dn=texture(m,'DryNormalMap',True);link(greenuv,gn,'Coordinates');link(grounduv,dn,'Coordinates')
    normal=custom(m,'float3 N=lerp(Dry,Green,Macro.r);return normalize(float3(N.xy*.7,N.z));',[('Macro',mixed),('Green',gn),('Dry',dn)])
    if trail:
        dirt=texture(m,'DirtMap');dirt_n=texture(m,'DirtNormalMap',True);link(uv,dirt,'Coordinates');link(uv,dirt_n,'Coordinates')
        blend=custom(m,'''float rim=min(UV.y,1-UV.y);
float irregular=(Dirt.r-.32)*.12+sin(P.x*.061+P.y*.045)*.008;
return smoothstep(.015,.17,rim+irregular);''',[('P',p),('UV',uv),('Dirt',dirt)],'CMOT_FLOAT1')
        ground=custom(m,'return lerp(Ground,Dirt,Blend);',[('Ground',ground),('Dirt',dirt),('Blend',blend)])
        normal=custom(m,'float3 N=normalize(float3(Dirt.xy*.65,Dirt.z));return normalize(lerp(Ground,N,Blend));',[('Ground',normal),('Dirt',dirt_n),('Blend',blend)])
    output(ground,'BASE_COLOR');output(normal,'NORMAL');output(node(m,'ScalarParameter',parameter_name='Roughness',default_value=.99 if trail else .96),'ROUGHNESS')
    E.recompile_material(m);assert A.save_loaded_asset(m,False),name
    unreal.log('WOODS_SURFACE_SAVED '+m.get_path_name())

try:
    make('M_WoodsGround',False);make('M_WoodsTrail',True)
    (Path(__file__).resolve().parent/'evidence/materials.txt').write_text('PASS: Separate woodland coverage and feathered dirt masters saved. House lawn and map unchanged.\n')
except Exception:
    import traceback
    unreal.log_error('WOODS_SURFACE_FAILED '+traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
