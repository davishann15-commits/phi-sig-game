"""Import only derived CC0 branch atlases into the new Calder folder."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
source=root/'SourceArt/CalderLandscape/phase2/woods_sprays'
dest='/Game/SeniorSendOff/Calder/Woods'
assets=unreal.EditorAssetLibrary
assets.make_directory(dest)
report=[]
for species in ['Oak','Maple']:
    for kind in ['A','N']:
        name='T_CalderWoods'+species+'_'+kind
        task=unreal.AssetImportTask()
        task.filename=str(source/(name+'.png'))
        task.destination_path=dest
        task.destination_name=name
        task.automated=True
        task.replace_existing=True
        task.save=False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture=unreal.load_asset(dest+'/'+name)
        if not texture:raise RuntimeError('Missing texture '+name)
        texture.set_editor_properties({'srgb':kind=='A','max_texture_size':1024,
            'compression_settings':unreal.TextureCompressionSettings.TC_DEFAULT if kind=='A' else unreal.TextureCompressionSettings.TC_NORMALMAP,
            'do_scale_mips_for_alpha_coverage':kind=='A','alpha_coverage_thresholds':unreal.Vector4(0,0,0,.32)})
        assets.save_loaded_asset(texture)
        report.append(texture.get_path_name())
path=source/'installed.json'
path.unlink(missing_ok=True)
path.write_text(json.dumps(report,indent=2))
unreal.log('CALDER_WOODS_SPRAYS_IMPORT PASS textures=4 nativeWindShaderUnchanged=1')
