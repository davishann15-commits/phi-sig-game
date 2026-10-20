"""Own opaque rust/galvanized roof shader; shared materials remain read-only."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
source='/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured'
path='/Game/SeniorSendOff/Calder/Materials/M_CalderFarmRoof'
report=ROOT/'SourceArt/CalderLandscape/phase2/farm_roof_material.json'
files=['Content/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.uasset']
files += ['Content/SeniorSendOff/Calder/Materials/'+name+'.uasset' for name in
          ['M_CalderGround','M_CalderRock','M_CalderWater','M_CalderFalls','M_CalderStream']]
guards={f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in files}
if A.does_asset_exist(path):
    assert report.exists() and json.loads(report.read_text())['asset']==path,'Existing asset not proven owned'
    assert A.delete_asset(path)
mat=A.duplicate_asset(source,path);assert mat
base=next(x for x in E.get_material_expressions(mat)
          if isinstance(x,unreal.MaterialExpressionTextureSampleParameter2D)
          and str(x.get_editor_property('parameter_name'))=='BaseMap')
rough=E.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate)
rough.set_editor_properties({'const_a':.93,'const_b':.49})
assert E.connect_material_expressions(base,'A',rough,'Alpha')
assert E.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
assert E.connect_material_property(base,'A',unreal.MaterialProperty.MP_METALLIC)
mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_OPAQUE,
                          'used_with_instanced_static_meshes':True})
E.recompile_material(mat);A.save_loaded_asset(mat)
stats=E.get_statistics(mat)
assert stats.num_pixel_shader_instructions>0 and 0<stats.num_pixel_texture_samples<=12
for f,sha in guards.items():assert hashlib.sha256((ROOT/f).read_bytes()).hexdigest()==sha,f
data={'asset':path,'pixel_instructions':stats.num_pixel_shader_instructions,
      'pixel_samples':stats.num_pixel_texture_samples,'opaque':True,'ISM_enabled':True,
      'albedo_alpha':'Metal fraction,0 porous rust/1 galvanized metal; linear averaged mips',
      'roughness_rust':.93,'roughness_metal':.49,'source_sha256':guards,
      'default_enabled':False,'switch':'CalderFarmTrial',
      'scope':'Owned shader compile only; native texture readback/visual/performance acceptance separate'}
report.unlink(missing_ok=True);report.write_text(json.dumps(data,indent=2)+'\n')
unreal.log(f'CALDER_FARM_ROOF_MATERIAL PASS instructions={stats.num_pixel_shader_instructions} samples={stats.num_pixel_texture_samples} originalsUnchanged=6')
