"""Finish only the new Fixer face/body revision and its fitted accessories."""
import runpy, unreal
from pathlib import Path
TOOLS=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'SourceArt'/'Fixer'/'Tools'
runpy.run_path(str(TOOLS/'fixer_mullet_v2.py'))
runpy.run_path(str(TOOLS/'fixer_import_details.py'))
runpy.run_path(str(TOOLS/'fixer_import_earbuds.py'))
runpy.run_path(str(TOOLS/'fixer_retarget.py'))
unreal.log('FIXER_REFERENCE_REVISION_FINISHED')
