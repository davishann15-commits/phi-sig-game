"""Create one owned water-effect material. Run only with all game/editor jobs closed.

Preparation only until invoked through the existing isolated Unreal batch flow.
Never edits existing materials,textures,source assets or maps.
"""
import hashlib,json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/SeniorSendOff/Calder/Wildlife'
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools()
NAME='M_CalderBirdWater';REPORT=ROOT/'SourceArt/CalderLandscape/phase3/sky/bird_water_material_v1.json'
assert not REPORT.exists() and not A.does_asset_exist(DEST+'/'+NAME)
guards={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/SeniorSendOff/Calder').rglob('*.uasset')}
def node(m,kind,**props):
 n=E.create_material_expression(m,getattr(unreal,'MaterialExpression'+kind))
 if props:n.set_editor_properties(props)
 return n
def connect(a,b,pin,channel=''):
 names=E.get_material_expression_input_names(b);matches=[str(n) for n in names if str(n).replace(' ','').lower()==pin.replace(' ','').lower()]
 assert len(matches)==1,(b.get_name(),pin,list(names))
 assert E.connect_material_expressions(a,channel,b,matches[0])
def output(n,prop,channel=''):assert E.connect_material_property(n,channel,getattr(unreal.MaterialProperty,'MP_'+prop))
m=T.create_asset(NAME,DEST,unreal.Material,unreal.MaterialFactoryNew());assert m
m.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_TRANSLUCENT,'two_sided':True,'translucency_lighting_mode':unreal.TranslucencyLightingMode.TLM_SURFACE})
E.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
assert E.has_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
alpha=node(m,'PerInstanceCustomData',data_index=0,const_default_value=0.)
colour=node(m,'PerInstanceCustomData3Vector',data_index=1,const_default_value=unreal.LinearColor(.04,.05,.055,1.))
a=node(m,'VertexInterpolator');connect(alpha,a,'VS');c=node(m,'VertexInterpolator');connect(colour,c,'VS');output(c,'BASE_COLOR')
vertex=node(m,'VertexColor');multiply=node(m,'Multiply');connect(a,multiply,'A');connect(vertex,multiply,'B','A')
depth=node(m,'DepthFade',opacity_default=1.,fade_distance_default=12.);connect(multiply,depth,'Opacity');output(depth,'OPACITY')
output(node(m,'Constant',r=.42),'ROUGHNESS');output(node(m,'Constant',r=.22),'SPECULAR')
E.recompile_material(m);assert A.save_loaded_asset(m);stats=E.get_statistics(m);assert stats.num_pixel_shader_instructions>0
for n,h in guards.items():assert hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h,n
report={'asset_created':True,'asset':m.get_path_name(),'existing_assets_unchanged':len(guards),'existing_sha256':guards,'authored_textures':0,'uses_scene_depth_fade':True,'lit_translucency_no_emission':True,'custom_floats':4,'pixel_instructions':stats.num_pixel_shader_instructions,'pixel_samples':stats.num_pixel_texture_samples,'no_map_saved':True,'native_water_effect_acceptance_pending':True}
REPORT.write_text(json.dumps(report,indent=2)+'\n');unreal.log('CALDER_BIRD_WATER_MATERIAL_CREATED owned=1 existingAssetsUnchanged=%d noMapSaved=1 nativePending=1'%len(guards))
