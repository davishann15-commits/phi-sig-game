"""Create only an owned foliage derivative; existing materials stay read-only."""
import hashlib
import importlib.util
import json
from pathlib import Path
import unreal
ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
A=unreal.EditorAssetLibrary
E=unreal.MaterialEditingLibrary
PARENT='/Game/SeniorSendOff/Calder/Materials/M_CalderHedgerow'
PATH='/Game/SeniorSendOff/Calder/Materials/M_CalderHedgerowFoliage'
REPORT=ROOT/'SourceArt/CalderLandscape/phase2/hedgerow_foliage_material.json'
files=['Content/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.uasset']
files+=['Content/SeniorSendOff/Calder/Materials/'+n+'.uasset' for n in ['M_CalderGround','M_CalderRock','M_CalderWater','M_CalderFalls','M_CalderStream','M_CalderFarmRoof','M_CalderStrataRock','M_CalderHedgerow']]
guards={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in files}
assert not A.does_asset_exist(PATH),'Do not replace an existing asset'
mat=A.duplicate_asset(PARENT,PATH)
assert mat
spec=importlib.util.spec_from_file_location('hedge_foliage_helpers',ROOT/'Tools/calder/phase2_assets.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
base=next(x for x in E.get_material_expressions(mat) if isinstance(x,unreal.MaterialExpressionTextureSampleParameter2D) and str(x.get_editor_property('parameter_name'))=='BaseMap')
colour=E.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
transmission=h.node(mat,'ScalarParameter',parameter_name='Transmission',default_value=.22)
sss=h.custom(mat,'return C.rgb*Amount;',{'C':colour,'Amount':transmission},'CMOT_FLOAT3')
h.output(sss,'SUBSURFACE_COLOR')
h.output(base,'OPACITY_MASK','A')
model=next(getattr(unreal.MaterialShadingModel,n) for n in dir(unreal.MaterialShadingModel) if 'FOLIAGE' in n)
mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_MASKED,'shading_model':model,'two_sided':True,'opacity_mask_clip_value':.32,'used_with_instanced_static_meshes':True})
E.recompile_material(mat);stats=E.get_statistics(mat)
assert stats.num_pixel_shader_instructions>0 and 0<stats.num_pixel_texture_samples<=12
for n,d in guards.items():assert hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==d,n
A.save_loaded_asset(mat)
REPORT.unlink(missing_ok=True);REPORT.write_text(json.dumps({'asset':PATH,'parent':PARENT,'pixel_instructions':stats.num_pixel_shader_instructions,'pixel_samples':stats.num_pixel_texture_samples,'masked':True,'two_sided_foliage':True,'coherent_root_pinned_wind_preserved':True,'original_material_sha256':guards,'runtime_acceptance_pending':True},indent=2)+'\n')
unreal.log('CALDER_HEDGEROW_FOLIAGE_MATERIAL PASS instructions=%d samples=%d originalsUnchanged=%d'%(stats.num_pixel_shader_instructions,stats.num_pixel_texture_samples,len(guards)))
