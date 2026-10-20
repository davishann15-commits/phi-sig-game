"""Create one stadium-only grass-detail shader; existing field art and assets stay read-only.

Run with Unreal closed, -run=pythonscript and -AllowCommandletRendering.
Grass004 A/N are the existing approved ambientCG CC0 assets (ASSETS.md64..67).
Only the new owned material is saved. No map is loaded or saved.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
WORK = ROOT / 'Saved/StadiumDetailWork'
spec = importlib.util.spec_from_file_location('stadium_material_helpers', ROOT / 'Tools/calder/phase2_assets.py')
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
node, custom, output = helpers.node, helpers.custom, helpers.output
EDIT, ASSETS, TOOLS = helpers.EDIT, helpers.ASSETS, helpers.TOOLS
DIRECTORY = '/Game/SeniorSendOff/Calder/Stadium/Materials'
NAME = 'M_Stadium_FieldDetail'
PATH = DIRECTORY + '/' + NAME

def run():
    grant = json.loads((WORK / 'integration_grant.json').read_text())
    assert grant['granted'] and grant['scope'] == 'full' and grant.get('release_evidence')
    assert 'allowcommandletrendering' in unreal.SystemLibrary.get_command_line().lower()
    assert not ASSETS.does_asset_exist(PATH), 'Inspect existing owned material before an explicit revision.'
    originals = [ROOT / 'Content/SeniorSendOff/Calder/Textures/Grass004' / ('T_Grass004_' + kind + '.uasset') for kind in ('A', 'N')]
    hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
    mat = TOOLS.create_asset(NAME, DIRECTORY, unreal.Material, unreal.MaterialFactoryNew())
    assert mat
    mat.set_editor_properties({'blend_mode': unreal.BlendMode.BLEND_OPAQUE,
        'shading_model': unreal.MaterialShadingModel.MSM_DEFAULT_LIT,
        'two_sided': False, 'tangent_space_normal': False,
        'used_with_instanced_static_meshes': True, 'used_with_static_lighting': False})
    position = node(mat, 'WorldPosition')
    uv = node(mat, 'TextureCoordinate')
    size = node(mat, 'ScalarParameter', parameter_name='DetailTileCm', default_value=220.)
    detail_uv = custom(mat, 'return P.xy/max(T,1.);', {'P': position, 'T': size}, 'CMOT_FLOAT2')
    def sample(name, path, coords, normal=False):
        asset = unreal.load_asset(path)
        assert asset
        result = node(mat, 'TextureSampleParameter2D', parameter_name=name, texture=asset,
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        assert EDIT.connect_material_expressions(coords, '', result, 'UVs')
        return result
    # BaseMap is replaced at runtime with the original painted field texture.
    grass_path = '/Game/SeniorSendOff/Calder/Textures/Grass004/T_Grass004_'
    painted = sample('BaseMap', grass_path + 'A', uv)
    detail = sample('DetailAlbedo', grass_path + 'A', detail_uv)
    normal = sample('DetailNormal', grass_path + 'N', detail_uv, True)
    base = custom(mat, '''
float lum=dot(D.rgb,float3(.2126,.7152,.0722));
float detail=clamp(lum/.14,.52,1.65);
float paint=smoothstep(.22,.5,min(min(B.r,B.g),B.b));
return B.rgb*lerp(lerp(.76,1.24,saturate(detail*.5)),lerp(.94,1.04,saturate(detail*.5)),paint);
''', {'B': painted, 'D': detail}, 'CMOT_FLOAT3')
    output(base, 'BASE_COLOR')
    # World-space top projection: positive Y in the map is world Y, independent of the old field UV winding.
    strength = node(mat, 'ScalarParameter', parameter_name='NormalStrength', default_value=.55)
    output(custom(mat, 'return normalize(float3(N.xy*S,max(.25,N.z)));', {'N': normal, 'S': strength}, 'CMOT_FLOAT3'), 'NORMAL')
    output(node(mat, 'Constant', r=.95), 'ROUGHNESS')
    output(node(mat, 'Constant', r=.22), 'SPECULAR')
    output(node(mat, 'Constant3Vector', constant=unreal.LinearColor(.006,.009,.006,1)), 'EMISSIVE_COLOR')
    EDIT.layout_material_expressions(mat)
    EDIT.recompile_material(mat)
    stats = EDIT.get_statistics(mat)
    report = {'asset': PATH, 'saved': False, 'map_saved': False,
        'num_pixel_shader_instructions': stats.get_editor_property('num_pixel_shader_instructions'),
        'num_pixel_texture_samples': stats.get_editor_property('num_pixel_texture_samples'),
        'existing_assets_sha256': hashes,
        'license': 'ambientCG Grass004 CC0; existing acquired assets reused read-only',
        'runtime_visual_and_performance_pending': True}
    assert report['num_pixel_shader_instructions'] > 0 and report['num_pixel_texture_samples'] <= 6, report
    assert all(hashlib.sha256((ROOT / p).read_bytes()).hexdigest() == sha for p, sha in hashes.items())
    assert ASSETS.save_loaded_asset(mat, False)
    report['saved'] = True
    report['asset_sha256'] = hashlib.sha256((ROOT / 'Content/SeniorSendOff/Calder/Stadium/Materials' / (NAME + '.uasset')).read_bytes()).hexdigest()
    (WORK / 'field_material_receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    unreal.log('STADIUM_FIELD_MATERIAL PASS ' + json.dumps(report))

if __name__ == '__main__':
    run()
