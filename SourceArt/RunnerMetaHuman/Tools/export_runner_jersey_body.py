"""Export the assembled, shoulder-complete Runner body for geometry review."""
import unreal

for part in ('Body', 'Face'):
    path = ('/Game/MetaHumans/RunnerJerseyBody/MH_Runner_JerseyBody/' +
            part + '/SKM_MH_Runner_JerseyBody_' + part + 'Mesh')
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not mesh:
        raise RuntimeError('Shoulder-complete Runner ' + part + ' asset missing')
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = ('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/'
                     'SourceArt/RunnerMetaHuman/RunnerJersey' + part + '.fbx')
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    options = unreal.FbxExportOption()
    options.set_editor_property('level_of_detail', False)
    options.set_editor_property('export_morph_targets', False)
    task.options = options
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError('Runner ' + part + ' FBX export failed')
    unreal.log('RUNNER_JERSEY_' + part.upper() + '_EXPORTED ' + task.filename)
