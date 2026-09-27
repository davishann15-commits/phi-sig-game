"""Export assembled Runner body/outfit to Blender without changing Unreal assets."""
import unreal
from pathlib import Path

ROOT = '/Game/MetaHumans/RunnerRebuild/MH_Runner_Working'
OUT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
for name, path in [
    ('RunnerNativeBody', ROOT + '/Body/SKM_MH_Runner_Working_BodyMesh'),
    ('RunnerNativeOutfit', ROOT + '/Clothing/MH_Runner_Working_Outfits'),
]:
    mesh = unreal.load_asset(path)
    if not mesh:
        raise RuntimeError('Missing assembled mesh ' + path)
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = str(OUT / (name + '.fbx'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    options = unreal.FbxExportOption()
    options.set_editor_property('level_of_detail', False)
    options.set_editor_property('export_morph_targets', False)
    task.options = options
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError('Runner mesh export failed: ' + name)
    unreal.log('RUNNER_NATIVE_EXPORTED ' + task.filename)
