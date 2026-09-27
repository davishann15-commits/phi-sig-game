"""Import the scalp-free comparison head without replacing V2."""

import runpy
import sys

sys.argv.append('--v3')
runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman/Tools/import_runner_video_head_visual_v2.py')
