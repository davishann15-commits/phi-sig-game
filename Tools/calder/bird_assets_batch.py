"""Create only the owned shared bird atlas/material; never save source assets/maps."""
import hashlib,json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'SourceArt/CalderLandscape/phase3/sky';DEST='/Game/SeniorSendOff/Calder/Wildlife'
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools()
prior=OUT/'atlas_installed_v1.json';assert not prior.exists();(OUT/'atlas_installed.json').rename(prior)
guards={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/SeniorSendOff/Calder/Wildlife').glob('*.uasset') if p.stem not in ('T_CalderBirdAtlas','M_CalderBirdPart')}
def node(m,kind,**kwargs):
 n=E.create_material_expression(m,getattr(unreal,'MaterialExpression'+kind))
 if kwargs:n.set_editor_properties(kwargs)
 return n
def connect(a,b,pin,channel=''):
 names=E.get_material_expression_input_names(b);matches=[str(n) for n in names if str(n).replace(' ','').lower()==pin.replace(' ','').lower()]
 assert len(matches)==1,(b.get_name(),pin,list(names));assert E.connect_material_expressions(a,channel,b,matches[0]),pin
def output(n,prop,channel=''):assert E.connect_material_property(n,channel,getattr(unreal.MaterialProperty,'MP_'+prop)),prop
def custom(m,code,inputs):
 n=node(m,'Custom',code=code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3);pins=[]
 for name in inputs:
  p=unreal.CustomInput();p.set_editor_property('input_name',name);pins.append(p)
 n.set_editor_property('inputs',pins)
 for name,value in inputs.items():connect(value,n,name)
 return n
def row(m,index):
 ns=[node(m,'PerInstanceCustomData',data_index=index+i, const_default_value=0.) for i in range(4)]
 a=node(m,'AppendVector');connect(ns[0],a,'A');connect(ns[1],a,'B')
 b=node(m,'AppendVector');connect(ns[2],b,'A');connect(ns[3],b,'B')
 c=node(m,'AppendVector');connect(a,c,'A');connect(b,c,'B');return c
def matrices(m,offset):
 out={}
 for i in range(16):
  out['Q%d'%i]=row(m,offset+i*7)
  out['T%d'%i]=node(m,'PerInstanceCustomData3Vector',data_index=offset+i*7+4)
 return out
def rig_code(normal):
 code='float4 qs[16]={'+','.join('Q%d'%i for i in range(16))+'}; float3 ts[16]={'+','.join('T%d'%i for i in range(16))+'}; float2 pairs[4]={W0,W1,W2,W3}; float3 result=0; float total=0;\n'
 code+='[unroll]for(int k=0;k<4;++k){int j=clamp((int)round(pairs[k].x),0,15);float w=saturate(pairs[k].y);float4 q=qs[j];float3 v=P'+(';' if normal else '*Scale;')+'float3 rotated=v+2*cross(q.xyz,cross(q.xyz,v)+q.w*v);result+=w*(rotated'+('' if normal else '+ts[j]')+');total+=w;}\n'
 return code+('return normalize(result/max(total,.0001));' if normal else 'return result/max(total,.0001)-Current;')


tex=unreal.load_asset(DEST+'/T_CalderBirdAtlas');m=unreal.load_asset(DEST+'/M_CalderBirdPart');assert tex and m
E.delete_all_material_expressions(m);m.set_editor_property('tangent_space_normal',False)
sample=E.create_material_expression(m,unreal.MaterialExpressionTextureSampleParameter2D);sample.set_editor_properties({'parameter_name':'BirdAtlas','texture':tex,'sampler_type':unreal.MaterialSamplerType.SAMPLERTYPE_COLOR})
assert E.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
rough=E.create_material_expression(m,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.88);assert E.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
weights={'W%d'%i:node(m,'TextureCoordinate',coordinate_index=i+1) for i in range(4)};cur=matrices(m,0);prev=matrices(m,112);scale=node(m,'PerInstanceCustomData',data_index=224,const_default_value=1.)
position=node(m,'PreSkinnedPosition');world=node(m,'WorldPosition',world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
current=custom(m,rig_code(False),dict(P=position,Current=world,Scale=scale,**weights,**cur));previous=custom(m,rig_code(False),dict(P=position,Current=world,Scale=scale,**weights,**prev))
switch=node(m,'PreviousFrameSwitch');connect(current,switch,'Current Frame');connect(previous,switch,'Previous Frame');output(switch,'WORLD_POSITION_OFFSET')
normal=custom(m,rig_code(True),dict(P=node(m,'PreSkinnedNormal'),**weights,**cur));interp=node(m,'VertexInterpolator');connect(normal,interp,'VS');output(interp,'NORMAL')
E.recompile_material(m);A.save_loaded_asset(m);stats=E.get_statistics(m);assert stats.num_pixel_shader_instructions>0 and stats.num_pixel_texture_samples==1
for n,h in guards.items():assert hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h,n
report={'passed':True,'owned_assets':[tex.get_path_name(),m.get_path_name()],'pixel_instructions':stats.num_pixel_shader_instructions,'samples':stats.num_pixel_texture_samples,'existing_assets_unchanged':len(guards),'existing_sha256':guards,'no_map_saved':True,'original_code_painted':True,'rigid_controls':16,'custom_floats':225,'current_previous_pose':True,'native_quality_acceptance_pending':True}
(OUT/'atlas_installed.json').write_text(json.dumps(report,indent=2)+'\n');unreal.log('CALDER_BIRD_ASSETS PASS pixelInstructions=%d samples=1 unchanged=%d'%(stats.num_pixel_shader_instructions,len(guards)))
