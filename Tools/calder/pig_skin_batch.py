"""Run only with the ordinary editor and game closed; no map save."""
import runpy
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
for script in ["build_pig_skin.py", "import_pig_parts.py"]:
    runpy.run_path(str(root / "Tools/calder" / script),run_name="__main__")
unreal.log("CALDER_PIG_SKIN_BATCH PASS")
