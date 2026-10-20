"""Import two owned needle-atlas textures; source kit and materials read-only."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE=ROOT/'SourceArt/CalderLandscape/phase2/woods_sprays'
plan=json.loads((SOURCE/'evergreen_manifest.json').read_text())
inspection=json.loads((ROOT/'Saved/CalderWork/evergreen_screening/inspection.json').read_text())
for name,expected in inspection['source_sha256'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==expected,name
rows=[]
for kind in ['A','N']:
    name='T_CalderWoodsEvergreen_'+kind
    file=SOURCE/(name+'.png')
    expected=next(r['sha256'] for r in plan['files'] if r['file']==file.name)
    assert hashlib.sha256(file.read_bytes()).hexdigest()==expected,name
    task=unreal.AssetImportTask()
    task.filename=str(file);task.destination_path='/Game/SeniorSendOff/Calder/Woods';task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=unreal.load_asset(task.destination_path+'/'+name)
    assert texture,name
    texture.set_editor_properties({'srgb':kind=='A','max_texture_size':1024,'virtual_texture_streaming':False,
        'compression_settings':unreal.TextureCompressionSettings.TC_DEFAULT if kind=='A' else unreal.TextureCompressionSettings.TC_NORMALMAP,
        'do_scale_mips_for_alpha_coverage':kind=='A','alpha_coverage_thresholds':unreal.Vector4(0,0,0,.32)})
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    rows.append({'asset':texture.get_path_name(),'srgb':texture.get_editor_property('srgb'),
                 'alpha_coverage_mips':texture.get_editor_property('do_scale_mips_for_alpha_coverage'),
                 'virtual_texture_streaming':texture.get_editor_property('virtual_texture_streaming')})
for name,expected in inspection['source_sha256'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==expected,name
out=SOURCE/'evergreen_installed.json';out.unlink(missing_ok=True)
out.write_text(json.dumps({'textures':rows,'source_kit_sha_unchanged':True,
    'scope':'Owned atlas trial; default material/wind/mesh/collision/placement unchanged'},indent=2)+'\n')
unreal.log('CALDER_EVERGREEN_ATLAS_IMPORT PASS textures=2 sourceAssetsUnchanged=1')
