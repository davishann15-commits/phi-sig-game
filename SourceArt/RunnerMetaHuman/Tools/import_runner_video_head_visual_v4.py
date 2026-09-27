"""Import the contoured-hairline comparison head without replacing V3."""

import runpy
import sys

sys.argv.append('--v4')
runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman/Tools/import_runner_video_head_visual_v2.py')
