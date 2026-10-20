"""Prepare the approved woods atlases and opaque falls master, with no map save."""
import runpy
from pathlib import Path
import unreal

root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
runpy.run_path(str(root/'Tools/calder/import_woods_sprays.py'),run_name='__main__')
water=runpy.run_path(str(root/'Tools/calder/phase2_assets.py'))
water['falls_material']()
unreal.log('CALDER_WOODS_ASSET_BATCH PASS')
