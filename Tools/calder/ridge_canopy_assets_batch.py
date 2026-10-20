"""Owned ridge-canopy shader (three samples including the engine dither lookup); no original asset or map writes."""
import hashlib
import importlib.util
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE='/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured'
source_file=ROOT/'Content/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.uasset'
before=hashlib.sha256(source_file.read_bytes()).hexdigest()
E=unreal.MaterialEditingLibrary;A=unreal.EditorAssetLibrary
spec=importlib.util.spec_from_file_location('calder_canopy_helpers',ROOT/'Tools/calder/phase2_assets.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
path='/Game/SeniorSendOff/Calder/Materials/M_CalderRidgeCanopy'
mat=unreal.load_asset(path) if A.does_asset_exist(path) else A.duplicate_asset(SOURCE,path)
assert mat
for node in list(E.get_material_expressions(mat)):
    if str(node.get_editor_property('desc')).startswith('Calder canopy:'):
        E.delete_material_expression(mat,node)
base=next(x for x in E.get_material_expressions(mat) if isinstance(x,unreal.MaterialExpressionTextureSampleParameter2D) and str(x.get_editor_property('parameter_name'))=='BaseMap')
colour=h.node(mat,'VertexColor');colour.set_editor_property('desc','Calder canopy: palette and edge')
shade=h.custom(mat,'float light=clamp(dot(A.rgb,float3(.2126,.7152,.0722))*3.3,.18,1.6); return C.rgb*light;',{'A':base,'C':colour},'CMOT_FLOAT3')
shade.set_editor_property('desc','Calder canopy: photographic luminance')
alpha=h.custom(mat,'return saturate(C);',{'C':colour},'CMOT_FLOAT1');alpha.set_editor_property('desc','Calder canopy: edge')
assert E.connect_material_expressions(colour,'A',alpha,'C')
dither=h.node(mat,'MaterialFunctionCall',material_function=unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA'))
dither.set_editor_property('desc','Calder canopy: edge dither')
assert E.connect_material_expressions(alpha,'',dither,'Alpha Threshold')
h.output(shade,'BASE_COLOR');h.output(dither,'OPACITY_MASK')
mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_MASKED,'two_sided':False,
    'used_with_instanced_static_meshes':True,'opacity_mask_clip_value':.32})
E.recompile_material(mat);A.save_loaded_asset(mat)
stats=E.get_statistics(mat)
assert stats.num_pixel_shader_instructions>0 and 0<stats.num_pixel_texture_samples<=12
assert hashlib.sha256(source_file.read_bytes()).hexdigest()==before
out=ROOT/'SourceArt/CalderLandscape/phase2/ridge_canopy_material.json';out.unlink(missing_ok=True)
out.write_text(json.dumps({'asset':path,'pixel_instructions':stats.num_pixel_shader_instructions,
    'pixel_samples':stats.num_pixel_texture_samples,'source_parent_sha256':before,'original_parent_unchanged':True,
    'scope':'Owned scenery canopy shader; actual scene/performance acceptance pending'},indent=2)+'\n')
unreal.log('CALDER_RIDGE_CANOPY_MATERIAL PASS '+str(stats.num_pixel_shader_instructions)+' instructions '+str(stats.num_pixel_texture_samples)+' samples')
