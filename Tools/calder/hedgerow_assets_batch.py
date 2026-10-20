"""Owned opaque shrub material; existing parents/materials stay read-only."""
import hashlib
import importlib.util
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
A = unreal.EditorAssetLibrary
E = unreal.MaterialEditingLibrary
PARENT = '/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured'
PATH = '/Game/SeniorSendOff/Calder/Materials/M_CalderHedgerow'
REPORT = ROOT / 'SourceArt/CalderLandscape/phase2/hedgerow_material.json'
files = ['Content/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.uasset']
files += ['Content/SeniorSendOff/Calder/Materials/' + name + '.uasset' for name in
          ['M_CalderGround', 'M_CalderRock', 'M_CalderWater', 'M_CalderFalls',
           'M_CalderStream', 'M_CalderFarmRoof', 'M_CalderStrataRock']]
guards = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in files}
if A.does_asset_exist(PATH):
    assert REPORT.exists() and json.loads(REPORT.read_text())['asset'] == PATH, 'Unproven existing asset'
    assert A.delete_asset(PATH)
mat = A.duplicate_asset(PARENT, PATH)
assert mat
REPORT.write_text(json.dumps({'asset': PATH, 'stage': 'Owned duplicate; compile pending'}, indent=2) + '\n')
assert E.get_material_property_input_node(mat, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET) is None
spec = importlib.util.spec_from_file_location('calder_hedge_helpers', ROOT / 'Tools/calder/phase2_assets.py')
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
position = h.node(mat, 'WorldPosition')
clock = h.node(mat, 'Time')
strength = h.node(mat, 'ScalarParameter', parameter_name='WindStrength', default_value=0.)
root = h.node(mat, 'PerInstanceCustomData', data_index=0, const_default_value=0.)
height = h.node(mat, 'PerInstanceCustomData', data_index=1, const_default_value=220.)
offset = h.custom(mat, r'''
float heightMask=saturate((P.z-RootZ)/max(Height,1.));
heightMask*=heightMask;
float phase=T*1.35+P.x*.0027+P.y*.0031;
float bend=sin(phase)+.22*sin(phase*2.7+P.z*.006);
return float3(.82,-.42,.10)*bend*Strength*heightMask;
''', {'P': position, 'T': clock, 'Strength': strength,
      'RootZ': root, 'Height': height}, 'CMOT_FLOAT3')
h.output(offset, 'WORLD_POSITION_OFFSET')
mat.set_editor_properties({'blend_mode': unreal.BlendMode.BLEND_OPAQUE,
                          'used_with_instanced_static_meshes': True})
E.recompile_material(mat)
stats = E.get_statistics(mat)
assert stats.num_pixel_shader_instructions > 0 and 0 < stats.num_pixel_texture_samples <= 12
for name, digest in guards.items():
    assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
A.save_loaded_asset(mat)
report = {'asset': PATH, 'parent': PARENT, 'pixel_instructions': stats.num_pixel_shader_instructions,
          'pixel_samples': stats.num_pixel_texture_samples, 'opaque': True, 'ISM_enabled': True,
          'instance_custom_data': ['rootZ', 'height'],
          'wind': 'Same root-pinned world displacement on woody canes, leaves and hips; existing gust actor drives WindStrength',
          'original_material_sha256': guards, 'default_enabled': False,
          'scope': 'Asset compile only; native placement/wind/visual/performance acceptance separate'}
REPORT.unlink()
REPORT.write_text(json.dumps(report, indent=2) + '\n')
unreal.log('CALDER_HEDGEROW_MATERIAL PASS instructions=%d samples=%d originalsUnchanged=%d' %
           (stats.num_pixel_shader_instructions, stats.num_pixel_texture_samples, len(guards)))
