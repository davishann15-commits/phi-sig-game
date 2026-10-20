"""Isolated masked-jet look trial; existing falls/three original masters read-only.

Still an opaque GBuffer/Lumen surface wherever water exists; temporal cutouts
break the cloth-like sheet without a translucent/refraction pass. Runtime trial extends only noncolliding water strands to their existing pool.
No water-query, collision, bridge, mist or map changes.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
folder='/Game/SeniorSendOff/Calder/Materials'
source=folder+'/M_CalderFalls';path=folder+'/M_CalderFallsJetsTrial'
guards={}
for name in ['M_CalderFalls','M_CalderGround','M_CalderRock','M_CalderWater','M_CalderStream']:
    file=ROOT/'Content/SeniorSendOff/Calder/Materials'/(name+'.uasset')
    guards[str(file.relative_to(ROOT))]=hashlib.sha256(file.read_bytes()).hexdigest()
if A.does_asset_exist(path):assert A.delete_asset(path),path
mat=A.duplicate_asset(source,path);assert mat,path
spec=importlib.util.spec_from_file_location('calder_falls_jet_helpers',ROOT/'Tools/calder/phase2_assets.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
colour=E.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
emission=E.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert isinstance(colour,unreal.MaterialExpressionCustom) and isinstance(emission,unreal.MaterialExpressionCustom)
pattern=next(x for x in E.get_material_expressions(mat) if isinstance(x,unreal.MaterialExpressionCustom)
             and 'struct WaterNoise' in str(x.get_editor_property('code')))
# Advect by ballistic flight time, rather than constant world-Z speed.
# A feature born at the lip accelerates downward without moving the mesh.
code=str(pattern.get_editor_property('code'))
code=code.replace('float2 uv=float2(across/24,(P.z+T*Speed)/180);',
                  'float drop=max(Top-P.z,0); float flight=(sqrt(90.0*90.0+1960.0*drop)-90.0)/980.0;\nfloat phase=(T-flight)*Speed;\nfloat2 uv=float2(across/24,phase/180);')
code=code.replace('(P.z+T*Speed*1.17)/19','phase*1.17/19')
code=code.replace('(P.z+T*Speed*.87)/6.3','phase*.87/6.3')
assert 'T*Speed' not in code and 'float flight=' in code
pattern.set_editor_property('code',code)
# Bright foam remains in the wet jets; lowering the old self-light prevents a
# uniformly glowing curtain. Original normal/flow/top/bottom parameters stay.
colour.set_editor_property('code','return lerp(float3(.009,.021,.027),float3(.62,.68,.70),saturate(Water.r*1.05));')
emission.set_editor_property('code','return float3(.075,.095,.11)*Water.r;')
coverage=h.custom(mat,'float core=smoothstep(.16,.74,W.g); float spray=smoothstep(.44,.83,W.r); return saturate(.06+.86*core+.25*spray);',{'W':pattern},'CMOT_FLOAT1')
dither=h.node(mat,'MaterialFunctionCall',material_function=unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA'))
assert E.connect_material_expressions(coverage,'',dither,'Alpha Threshold')
h.output(dither,'OPACITY_MASK')
mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_MASKED,'opacity_mask_clip_value':.32,
                          'two_sided':False,'used_with_instanced_static_meshes':True,'has_pixel_animation':True})
assert mat.get_editor_property('has_pixel_animation')
E.recompile_material(mat);A.save_loaded_asset(mat)
stats=E.get_statistics(mat)
assert stats.num_pixel_shader_instructions>0 and 0<stats.num_pixel_texture_samples<=12
for name,expected in guards.items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==expected,name
out=ROOT/'SourceArt/CalderLandscape/phase2/falls_jets_trial.json';out.unlink(missing_ok=True)
out.write_text(json.dumps({'asset':path,'pixel_instructions':stats.num_pixel_shader_instructions,
 'pixel_samples':stats.num_pixel_texture_samples,'surface':'Masked opaque jets; no translucent/refraction pass',
 'animation':'Ballistic time-of-flight advection, v0=90 cm/s, g=980 cm/s2','roughness':.06,'has_pixel_animation':True,'default_enabled':False,'trial_switch':'CalderFallsJetsTrial','source_sha256':guards,
 'collision_water_queries_mist_unchanged':True,'static_geometry_tails':False,'scope':'Appearance trial; native/visual acceptance pending'},indent=2)+'\n')
unreal.log('CALDER_FALLS_JETS_MATERIAL PASS instructions='+str(stats.num_pixel_shader_instructions)+' samples='+str(stats.num_pixel_texture_samples)+' originalsUnchanged=1')
