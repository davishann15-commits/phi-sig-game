"""Export read-only material function/collection text for parameter inspection."""
from pathlib import Path
import hashlib
import json
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
rows=[]
for name,path in [
 ('MF_HealthSeason','/Game/MSPresets/MS_Foliage_Material/MaterialFunctions/Foliage/MF_HealthSeason'),
 ('MPC_GlobalFoliageActor','/Game/MSPresets/MS_Foliage_Material/MaterialParameterCollection/MPC_GlobalFoliageActor')]:
    source=ROOT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')
    before=hashlib.sha256(source.read_bytes()).hexdigest()
    task=unreal.AssetExportTask()
    task.object=unreal.load_asset(path)
    task.filename=str(ROOT/'Saved/CalderWork'/('tree_'+name+'.copy'))
    task.automated=True
    task.prompt=False
    task.replace_identical=True
    task.exporter=unreal.ObjectExporterT3D()
    assert unreal.Exporter.run_asset_export_task(task),name
    assert hashlib.sha256(source.read_bytes()).hexdigest()==before,name
    rows.append({'source':path,'sha256':before,'text':task.filename})
    unreal.log('CALDER_TREE_GRAPH '+name+' '+task.filename)
plan=json.loads((ROOT/'SourceArt/CalderLandscape/phase2/tree_candidates.json').read_text())
for tree in plan['trees']:
    leaf=next(m for m in tree['materials'] if 'TwoSided' in m['path'])
    for parameter in ['Albedo','Mask']:
        path=leaf['textures'][parameter]
        asset=unreal.load_asset(path)
        filename=ROOT/'Saved/CalderWork'/('tree_'+tree['label']+'_'+parameter+'.png')
        task=unreal.AssetExportTask()
        task.object=asset
        task.filename=str(filename)
        task.automated=True; task.prompt=False; task.replace_identical=True
        task.exporter=unreal.TextureExporterPNG()
        assert unreal.Exporter.run_asset_export_task(task),path
        unreal.log('CALDER_TREE_TEXTURE_INSPECT '+str(filename))
for relative,expected in plan['source_sha256'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
unreal.log('CALDER_TREE_GRAPH_INSPECTION PASS sourcesUnchanged=1')
