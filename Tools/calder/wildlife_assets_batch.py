"""Owned deer static/material batch; editor and game closed, no map saves.
Fab assets are read-only. Static UV data controls rigid procedural poses.
"""
import json,hashlib
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE=ROOT/'SourceArt/CalderLandscape/phase3/deer';DEST='/Game/SeniorSendOff/Calder/Wildlife'
E=unreal.MaterialEditingLibrary;A=unreal.EditorAssetLibrary;S=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
T=unreal.AssetToolsHelpers.get_asset_tools();A.make_directory(DEST)
guards={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/AnimalVarietyPack/DeerStagAndDoe').rglob('*.uasset')}
assert guards,'Missing read-only deer source pack; stop before authoring materials'
receipt=SOURCE/'installed.json'
if receipt.exists():
 import time
 previous=SOURCE/('installed_prior_'+str(time.time_ns())+'.json');receipt.rename(previous)
anatomy=json.loads((SOURCE/'anatomy.json').read_text());stats={}
def material(name,masked=False):
 path=DEST+'/'+name;m=unreal.load_asset(path) if A.does_asset_exist(path) else T.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
 assert m;E.delete_all_material_expressions(m);m.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_MASKED if masked else unreal.BlendMode.BLEND_OPAQUE,'shading_model':unreal.MaterialShadingModel.MSM_DEFAULT_LIT,'two_sided':masked,'tangent_space_normal':False,'used_with_instanced_static_meshes':True});return m
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

def finish(m):
 E.recompile_material(m);A.save_loaded_asset(m);s=E.get_statistics(m);assert s.num_pixel_shader_instructions>0 and s.num_pixel_texture_samples<=12,m.get_name()
 stats[m.get_name()]={'pixel_instructions':s.num_pixel_shader_instructions,'samples':s.num_pixel_texture_samples}
 unreal.log('CALDER_WILDLIFE_MATERIAL '+m.get_name()+' '+json.dumps(stats[m.get_name()]))
for name,masked in [('M_CalderDeerSkin',False),('M_CalderDeerFur',True)]:
 m=material(name,masked);texs={}
 for param,tex,kind in [('Coat','T_DeeDoe_BaseColor','COLOR'),('DetailNormal','T_DeeDoe_Nml','NORMAL'),('ORM','T_DeerDoe_OcclusionRoughnessMetallic','MASKS')]:
  asset=unreal.load_asset('/Game/AnimalVarietyPack/DeerStagAndDoe/Textures/'+tex);assert asset
  if kind!='NORMAL':kind='COLOR' if asset.srgb else 'MASKS' if asset.compression_settings==unreal.TextureCompressionSettings.TC_MASKS else 'LINEAR_COLOR'
  texs[param]=node(m,'TextureSampleParameter2D',parameter_name=param,texture=asset,sampler_type=getattr(unreal.MaterialSamplerType,'SAMPLERTYPE_'+kind))
 coat=custom(m,'float g=dot(C.rgb,float3(.27,.59,.14));return lerp(C.rgb,g*float3(.94,.92,.87),.72);',{'C':texs['Coat']});output(coat,'BASE_COLOR');output(texs['ORM'],'ROUGHNESS','G');output(texs['ORM'],'AMBIENT_OCCLUSION','R')
 if masked:output(texs['Coat'],'OPACITY_MASK','A')
 weights={'W%d'%i:node(m,'TextureCoordinate',coordinate_index=i+1) for i in range(4)};cur=matrices(m,0);prev=matrices(m,112);scale=node(m,'PerInstanceCustomData',data_index=224,const_default_value=1.)
 position=node(m,'PreSkinnedPosition');world=node(m,'WorldPosition',world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
 current=custom(m,rig_code(False),dict(P=position,Current=world,Scale=scale,**weights,**cur));previous=custom(m,rig_code(False),dict(P=position,Current=world,Scale=scale,**weights,**prev))
 switch=node(m,'PreviousFrameSwitch');connect(current,switch,'Current Frame');connect(previous,switch,'Previous Frame');output(switch,'WORLD_POSITION_OFFSET')
 normal=custom(m,rig_code(True),dict(P=node(m,'PreSkinnedNormal'),**weights,**cur));interp=node(m,'VertexInterpolator');connect(normal,interp,'VS')
 normalmap=custom(m,'float3 n=normalize(N);float3 dx=ddx(P),dy=ddy(P);float2 ux=ddx(UV),uy=ddy(UV);float d=ux.x*uy.y-uy.x*ux.y;if(abs(d)<1e-8)return n;float3 t0=(dx*uy.y-dy*ux.y)/d;float3 t=normalize(t0-n*dot(t0,n));float3 b0=(-dx*uy.x+dy*ux.x)/d;float3 b=normalize(b0-n*dot(b0,n)-t*dot(b0,t));return normalize(t*Tex.x+b*Tex.y+n*Tex.z);',{'N':interp,'P':node(m,'WorldPosition'),'UV':node(m,'TextureCoordinate'),'Tex':texs['DetailNormal']});output(normalmap,'NORMAL');finish(m)
m=material('M_CalderWildlifePart');colour=node(m,'VertexColor');grain=custom(m,'float v=frac(sin(dot(P,float3(12.9,78.2,33.7)))*43758.5);return C.rgb*(.94+.12*v);',{'C':colour,'P':node(m,'WorldPosition')});output(grain,'BASE_COLOR');output(node(m,'Constant',r=.88),'ROUGHNESS');finish(m)
m=material('M_CalderWildlifeContact',True);output(node(m,'Constant3Vector',constant=unreal.LinearColor(.014,.011,.008,1)),'BASE_COLOR');mask=custom(m,'float2 q=(UV-.5)*2;float a=saturate(1-dot(q,q));return a*a*.36;',{'UV':node(m,'TextureCoordinate')});mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1);dither=node(m,'MaterialFunctionCall',material_function=unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA'));connect(mask,dither,'Alpha Threshold');output(dither,'OPACITY_MASK');output(node(m,'Constant',r=1.),'ROUGHNESS');finish(m)
meshes=[]
for i,lod in enumerate(anatomy['lods']):
 task=unreal.AssetImportTask();task.filename=str(SOURCE/lod['file']);task.destination_path=DEST;task.destination_name='SM_CalderDeerBody' if i==0 else 'SM_CalderDeerSourceLOD%d'%i;task.automated=True;task.replace_existing=True;task.save=False
 ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;ui.automated_import_should_detect_type=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.generate_lightmap_u_vs=False;ui.static_mesh_import_data.auto_generate_collision=False;task.options=ui;T.import_asset_tasks([task]);mesh=unreal.load_asset(task.imported_object_paths[0]);assert isinstance(mesh,unreal.StaticMesh)
 b=mesh.get_bounds();lo=b.origin-b.box_extent+mesh.get_editor_property('negative_bounds_extension');hi=b.origin+b.box_extent-mesh.get_editor_property('positive_bounds_extension')
 actual=[[lo.x,lo.y,lo.z],[hi.x,hi.y,hi.z]];assert max(abs(actual[side][axis]-lod['bounds_cm'][side][axis]) for side in range(2) for axis in range(3))<.12,('static deer axes/scale',actual,lod['bounds_cm'])
 nanite=mesh.get_editor_property('nanite_settings');nanite.enabled=False;S.set_nanite_settings(mesh,nanite,False)
 mesh.set_editor_property('allow_cpu_access',True);settings=S.get_lod_build_settings(mesh,0);settings.use_full_precision_u_vs=True;settings.use_high_precision_tangent_basis=True;S.set_lod_build_settings(mesh,0,settings);meshes.append(mesh)
main=meshes[0]
for i in [1,2]:assert S.set_lod_from_static_mesh(main,i,meshes[i],0,True)==i
assert S.set_lod_screen_sizes(main,[1.,.3,.08])
for i in range(len(main.static_materials)):main.set_material(i,unreal.load_asset(DEST+('/M_CalderDeerSkin' if i==0 else '/M_CalderDeerFur')))
main.set_editor_property('positive_bounds_extension',unreal.Vector(200,200,200));main.set_editor_property('negative_bounds_extension',unreal.Vector(200,200,200))
for mesh in meshes:A.save_loaded_asset(mesh)
for path,digest in guards.items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest,path
report={'passed':True,'source_assets_unchanged':len(guards),'source_sha256':guards,'runtime_skeletons':0,'runtime_mesh':main.get_path_name(),'lods':S.get_lod_count(main),'uv_channels':[S.get_num_uv_channels(main,i) for i in range(3)],'materials':stats,'no_map_saved':True,'rigid_controls':16,'per_instance_custom_floats':225}
assert report['lods']==3 and all(n>=5 for n in report['uv_channels'])
p=SOURCE/'installed.json';p.unlink(missing_ok=True);p.write_text(json.dumps(report,indent=2)+'\n');unreal.log('CALDER_WILDLIFE_ASSET_BATCH PASS '+json.dumps(report))
