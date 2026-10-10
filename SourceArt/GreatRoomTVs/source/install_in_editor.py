import unreal
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs')
exec(compile((R/'source/install_tvs.py').read_text(),str(R/'source/install_tvs.py'),'exec'))
unreal.SystemLibrary.execute_console_command(None,'QUIT_EDITOR')
