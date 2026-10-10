from pathlib import Path
p=Path('/Users/Stewart/Projects/Senior sendoff/outputs/house-interactivity/source/install_interactivity.py')
s=p.read_text().split("unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')")[0]
exec(compile(s,str(p),'exec'))
unreal.log('HOUSE_ASSET_IMPORT_COMPLETE')
