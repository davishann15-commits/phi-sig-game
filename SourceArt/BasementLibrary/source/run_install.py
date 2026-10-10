import unreal,traceback,json
from pathlib import Path
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
try:exec(compile((R/'source/install_library.py').read_text(),str(R/'source/install_library.py'),'exec'))
except Exception:
 error=traceback.format_exc();unreal.log_error(error);(R/'install-error.json').write_text(json.dumps({'error':error},indent=2))
else:
 if (R/'install-error.json').exists():(R/'install-error.json').unlink()
# ExecCmds can finish before the editor's first frame. Wait for Slate to finish
# its startup before closing, so shutdown does not tear down a half-built tab.
quit_ticks=0
def quit_when_ready(delta):
 global quit_ticks
 quit_ticks+=1
 if quit_ticks>=5:
  unreal.unregister_slate_post_tick_callback(quit_handle)
  unreal.SystemLibrary.quit_editor()
quit_handle=unreal.register_slate_post_tick_callback(quit_when_ready)
