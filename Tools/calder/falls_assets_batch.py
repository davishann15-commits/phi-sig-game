"""Rebuild only the owned falls master; no level load or save."""
import runpy
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
water = runpy.run_path(str(root / 'Tools/calder/phase2_assets.py'))
water['falls_material']()
water['stream_material']()
unreal.log('CALDER_FALLS_ASSET_BATCH PASS')
