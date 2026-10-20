"""Read acquired evergreen material functions and export texture inspection data."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
WORK = ROOT/'Saved/CalderWork/evergreen_screening'
inspection = json.loads((WORK/'inspection.json').read_text())
E = unreal.MaterialEditingLibrary
seen = set()
rows = []
def inspect(asset):
    if not asset or asset.get_path_name() in seen:
        return
    seen.add(asset.get_path_name())
    expressions = (E.get_material_function_expressions(asset) if isinstance(asset, unreal.MaterialFunction)
                   else E.get_material_expressions(asset))
    record = {'asset': asset.get_path_name(), 'expressions': []}
    for expression in expressions:
        item = {'class': expression.get_class().get_name(), 'name': expression.get_name()}
        for key in ['parameter_name', 'texture', 'material_function', 'r', 'g', 'b', 'a', 'code']:
            try:
                value = expression.get_editor_property(key)
                item[key] = value.get_path_name() if isinstance(value, unreal.Object) else str(value)
            except Exception:
                pass
        if isinstance(expression, unreal.MaterialExpressionMaterialFunctionCall):
            inspect(expression.get_editor_property('material_function'))
        record['expressions'].append(item)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(WORK/('graph_'+asset.get_name()+'.copy'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.ObjectExporterT3D()
    assert unreal.Exporter.run_asset_export_task(task), asset.get_path_name()
    rows.append(record)
for material in inspection['materials']:
    inspect(unreal.load_asset(material['parent']))
for suffix in ['Foliage_01_CA', 'Foliage_01_NT', 'Bark_01_C', 'Bark_01_NAH']:
    asset = unreal.load_asset('/Game/Megaplant_Library/Tree_Norway_Spruce/Textures/T_Norway_Spruce_'+suffix)
    assert isinstance(asset, unreal.Texture2D)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(WORK/(asset.get_name()+'.tga'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.TextureExporterTGA()
    assert unreal.Exporter.run_asset_export_task(task), asset.get_path_name()
for name, expected in inspection['source_sha256'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected, name
(WORK/'graphs.json').write_text(json.dumps(rows, indent=2)+'\n')
unreal.log('CALDER_EVERGREEN_GRAPH PASS graphs='+str(len(rows))+' sourceAssetsUnchanged=1')
