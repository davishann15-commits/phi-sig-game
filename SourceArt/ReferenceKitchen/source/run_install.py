import unreal,json,traceback
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen')
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
try:exec(compile((R/'source/install_kitchen.py').read_text(),str(R/'source/install_kitchen.py'),'exec'))
except Exception:
 error=traceback.format_exc();unreal.log_error(error);(R/'install-error.json').write_text(json.dumps(dict(error=error),indent=2))
else:
 if (R/'install-error.json').exists():(R/'install-error.json').unlink()
