"""Prepare an owned, twelve-sample landscape material; never save a map.

Run only after all game/editor processes close, with AllowCommandletRendering.
This creates M_CalderLandscapeGround, leaving all Phase 1 masters and acquired
texture assets read-only. Runtime binding and visual acceptance are separate.
"""
import hashlib
import importlib.util
import json
from pathlib import Path

import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
spec=importlib.util.spec_from_file_location('calder_asset_helpers',ROOT/'Tools/calder/phase2_assets.py')
helpers=importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
node,custom,output=helpers.node,helpers.custom,helpers.output
EDIT,ASSETS,TOOLS=helpers.EDIT,helpers.ASSETS,helpers.TOOLS
NAME='M_CalderLandscapeGround'
PATH=helpers.MAT_DIR+'/'+NAME


def scalar(mat,name,value):
    return node(mat,'ScalarParameter',parameter_name=name,default_value=value)


def connect(source,target,pin,channel=''):
    names=[str(v) for v in EDIT.get_material_expression_input_names(target)]
    key=lambda value:''.join(c for c in value.lower() if c.isalnum())
    actual=next((name for name in names if key(name)==key(pin)),None)
    if actual is None or not EDIT.connect_material_expressions(source,channel,target,actual):
        raise RuntimeError('Cannot connect '+pin+'; available '+str(names))


def texture(mat,name,set_name,kind,coordinates,gradients=None):
    path='/Game/SeniorSendOff/Calder/Textures/'+set_name+'/T_'+set_name+'_'+kind
    source=unreal.load_asset(path)
    if not source:
        raise RuntimeError('Missing existing approved texture '+path)
    sample=node(mat,'TextureSampleParameter2D',parameter_name=name,texture=source,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='N' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    connect(coordinates,sample,'UVs')
    if gradients:
        sample.set_editor_property('mip_value_mode',unreal.TextureMipValueMode.TMVM_DERIVATIVE)
        connect(gradients[0],sample,'DDX(UVs)')
        connect(gradients[1],sample,'DDY(UVs)')
    return sample


def run():
    if 'allowcommandletrendering' not in unreal.SystemLibrary.get_command_line().lower():
        raise RuntimeError('Requires -AllowCommandletRendering with the editor closed.')
    originals=[ROOT/'Content/SeniorSendOff/Calder/Materials'/(name+'.uasset')
               for name in ('M_CalderGround','M_CalderRock','M_CalderWater')]
    original_sha={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
    original=unreal.load_asset(helpers.MAT_DIR+'/M_CalderGround')
    if not original:
        raise RuntimeError('Original ground material is missing')
    old_stats=EDIT.get_statistics(original)
    original_stats={key:old_stats.get_editor_property(key) for key in ('num_pixel_shader_instructions','num_pixel_texture_samples')}
    if original_stats['num_pixel_shader_instructions']<=0:
        raise RuntimeError('Could not verify the original ground shader on this RHI')
    material=unreal.load_asset(PATH) if ASSETS.does_asset_exist(PATH) else TOOLS.create_asset(
        NAME,helpers.MAT_DIR,unreal.Material,unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError('Cannot create owned material '+PATH)
    for expression in list(EDIT.get_material_expressions(material)):
        EDIT.delete_material_expression(material,expression)
    material.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_OPAQUE,
        'shading_model':unreal.MaterialShadingModel.MSM_DEFAULT_LIT,'two_sided':False,
        'tangent_space_normal':False,'used_with_instanced_static_meshes':True,'used_with_static_lighting':False})
    position=node(material,'WorldPosition')
    normal=node(material,'VertexNormalWS')
    colour=node(material,'VertexColor')
    def xy(name,size):
        return custom(material,'return P.xy/max(T,1.);',{'P':position,'T':scalar(material,name,size)},'CMOT_FLOAT2')
    lawn_uv=xy('LawnTileSize',220.)
    meadow_uv=xy('MeadowTileSize',250.)
    litter_uv=xy('LitterTileSize',230.)
    clay_uv=xy('ClayTileSize',200.)
    rock_size=scalar(material,'RockTileSize',320.)
    projections=[custom(material,'return P.'+axes+'/max(T,1.);',{'P':position,'T':rock_size},'CMOT_FLOAT2')
                 for axes in ('yz','xz','xy')]
    # Nine explicit reads leave room for the three engine/base-pass reads
    # observed even on the texture-free falls shader. Actual statistics are
    # the acceptance gate. Ground layers share one normal; rock colour blends
    # three projections and its detailed normal uses a derivative-safe axis.
    samples={
        'LawnA':texture(material,'LawnAlbedo','Grass004','A',lawn_uv),
        'MeadowA':texture(material,'MeadowAlbedo','withered_grass','A',meadow_uv),
        'LitterA':texture(material,'LitterAlbedo','forest_leaves_03','A',litter_uv),
        'ClayA':texture(material,'ClayAlbedo','red_mud_stones','A',clay_uv),
        'LawnN':texture(material,'LawnNormal','Grass004','N',lawn_uv)}
    for axis,uv in zip(('X','Y','Z'),projections):
        samples['RockA'+axis]=texture(material,'RockAlbedo','mossy_rock','A',uv)
    plane_inputs={'P':position,'N':normal,'T':rock_size}
    choose='float3 a=abs(N); if(a.x>=a.y&&a.x>=a.z) return D.yz/max(T,1.); if(a.y>=a.z) return D.xz/max(T,1.); return D.xy/max(T,1.);'
    normal_uv=custom(material,'float3 D=P; '+choose,plane_inputs,'CMOT_FLOAT2')
    normal_dx=custom(material,'float3 D=ddx(P); '+choose,plane_inputs,'CMOT_FLOAT2')
    normal_dy=custom(material,'float3 D=ddy(P); '+choose,plane_inputs,'CMOT_FLOAT2')
    samples['RockN']=texture(material,'RockNormal','mossy_rock','N',normal_uv,(normal_dx,normal_dy))
    weights=custom(material,r'''
float3 c=lerp(VC.rgb/12.92,pow(max((VC.rgb+.055)/1.055,0.),2.4),step(.04045,VC.rgb));
float litter=saturate(c.g),clay=saturate(c.b);
float sumLC=max(1.,litter+clay); litter/=sumLC; clay/=sumLC;
float field=max(max(smoothstep(19000.,22000.,P.x),smoothstep(19000.,21500.,P.y)),1.-smoothstep(-23000.,-20500.,P.x));
float meadow=max(field,smoothstep(.0513,.1056,1.-normalize(N).z));
float rest=max(0.,1.-litter-clay);
return float4(rest*(1.-meadow),rest*meadow,litter,clay);
''',{'VC':colour,'N':normal,'P':position},'CMOT_FLOAT4')
    rock=custom(material,r'''
float r=VC.r<=.04045?VC.r/12.92:pow((VC.r+.055)/1.055,2.4);
return max(saturate(r),smoothstep(.180848,.292893,1.-normalize(N).z));
''',{'VC':colour,'N':normal},'CMOT_FLOAT1')
    common={'P':position,'N':normal,'W':weights,'Rock':rock}
    base=custom(material,r'''
struct MacroNoise {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float value(float2 p) { float2 i=floor(p),f=frac(p); f=f*f*(3.-2.*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1.),f.x),f.y); }
};
MacroNoise noise;
float variation=lerp(.82,1.12,noise.value(P.xy/1800.));
float stripes=1.-.045*(.5+.5*sin(P.z*.055));
float3 ground=LawnA.rgb*W.x*stripes+MeadowA.rgb*W.y+LitterA.rgb*W.z+ClayA.rgb*W.w;
float3 tri=pow(max(abs(normalize(N)),.0001),8.);tri/=dot(tri,1.);
float bands=lerp(.88,1.,smoothstep(.12,.7,frac((P.z+.12*P.x-.05*P.y)/90.)));
float3 stone=(RockAX.rgb*tri.x+RockAY.rgb*tri.y+RockAZ.rgb*tri.z)*bands;
return lerp(ground,stone,saturate(Rock))*variation;
''',{**common,**{k:v for k,v in samples.items() if 'A' in k}},'CMOT_FLOAT3')
    output(base,'BASE_COLOR')
    detail=custom(material,r'''
float3 n=normalize(N),t=normalize(float3(n.z,0.,-n.x)+float3(.0001,0.,0.));
float3 b=normalize(cross(n,t));
float3 map=normalize(LawnN.xyz);
float3 soil=normalize(map.x*t+map.y*b+map.z*n);
float3 a=abs(n),stone;
if(a.x>=a.y&&a.x>=a.z) stone=float3(RockN.z*sign(n.x),RockN.x,RockN.y);
else if(a.y>=a.z) stone=float3(RockN.x,RockN.z*sign(n.y),RockN.y);
else stone=float3(RockN.x,RockN.y,RockN.z*sign(n.z));
return normalize(lerp(soil,stone,saturate(Rock)));
''',{**common,**{k:v for k,v in samples.items() if 'N' in k}},'CMOT_FLOAT3')
    output(detail,'NORMAL')
    rough=custom(material,'return lerp(dot(W,float4(.90,.94,.96,.92)),.87,saturate(Rock));',
                 {'W':weights,'Rock':rock},'CMOT_FLOAT1')
    output(rough,'ROUGHNESS')
    # The default VertexColor pin is RGB. Select its named alpha output for
    # cavity rather than masking an alpha channel out of that float3.
    ao=custom(material,'return lerp(.35,1.,saturate(A));',{'A':colour},'CMOT_FLOAT1')
    if not EDIT.connect_material_expressions(colour,'A',ao,'A'):
        raise RuntimeError('Cannot connect cavity alpha')
    output(ao,'AMBIENT_OCCLUSION')
    EDIT.layout_material_expressions(material)
    logs=list(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_log_dir())).glob('*.log'))
    log=max(logs,key=lambda p:p.stat().st_mtime) if logs else None
    log_offset=log.stat().st_size if log else 0
    EDIT.recompile_material(material)
    stats=EDIT.get_statistics(material)
    messages=[]
    if log:
        with log.open('rb') as stream:
            stream.seek(log_offset)
            for line in stream.read().decode('utf-8',errors='replace').splitlines():
                lower=line.lower()
                if ('error' in lower or 'failed to compile' in lower) and ('shader' in lower or 'material' in lower or '(node ' in lower):
                    messages.append(line)
    report={'asset':PATH,'num_pixel_shader_instructions':stats.get_editor_property('num_pixel_shader_instructions'),
            'num_pixel_texture_samples':stats.get_editor_property('num_pixel_texture_samples'),
            'compile_error_messages':messages,
            'original_ground_stats':original_stats,'saved':False,
            'scope':'Owned unbound appearance material; runtime/visual/performance acceptance separate',
            'originals_sha256':original_sha}
    out=ROOT/'SourceArt/CalderLandscape/phase2/ground_material.json'
    out.unlink(missing_ok=True);out.write_text(json.dumps(report,indent=2))
    if report['num_pixel_shader_instructions']<=0 or report['num_pixel_texture_samples']>12 or messages:
        raise RuntimeError('Ground material compile/sample budget failed: '+json.dumps(report))
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==value for p,value in original_sha.items())
    if not ASSETS.save_loaded_asset(material):
        raise RuntimeError('Owned ground material did not save')
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==value for p,value in original_sha.items())
    report['saved']=True
    owned=ROOT/'Content/SeniorSendOff/Calder/Materials'/(NAME+'.uasset')
    report['asset_sha256']=hashlib.sha256(owned.read_bytes()).hexdigest()
    out.unlink(missing_ok=True);out.write_text(json.dumps(report,indent=2))
    unreal.log('CALDER_GROUND_ASSET PASS '+json.dumps(report))


if __name__=='__main__':
    run()
