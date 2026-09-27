"""Export locally installed MetaHuman hairstyle previews for visual review."""
from pathlib import Path
import unreal

output = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/RunnerHairReview')
output.mkdir(parents=True, exist_ok=True)
for name in ('Hair_S_Messy', 'Hair_S_Casual', 'Hair_S_CurlyFade',
             'Hair_S_SideSweptFringe', 'Hair_S_SweptUp', 'Hair_M_SideSweptFringe',
             'Hair_M_BobMessy', 'Hair_M_Layered', 'Hair_L_MessyClumps'):
    asset = unreal.load_asset('/MetaHumanCharacter/Optional/Grooms/Thumbnails/Hair/' + name + '_Thumb')
    if not asset:
        print('RUNNER_HAIR_THUMB_MISSING', name)
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(output / (name + '.png'))
    task.exporter = unreal.TextureExporterPNG()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    result = unreal.Exporter.run_asset_export_task(task)
    print('RUNNER_HAIR_THUMB_EXPORT', name, result)
