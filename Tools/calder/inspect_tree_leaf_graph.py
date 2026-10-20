"""Read leaf-parent and shared foliage graphs without saving any asset/map."""
import hashlib,json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
E=unreal.MaterialEditingLibrary
plan=json.loads((ROOT/'SourceArt/CalderLandscape/phase2/tree_candidates.json').read_text())
materials=[]
for tree in plan['trees']:
    leaf=next(m for m in tree['materials'] if 'TwoSided' in m['path'])
    asset=unreal.load_asset(leaf['path'])
    while isinstance(asset,unreal.MaterialInstanceConstant):
        asset=asset.get_editor_property('parent')
    if asset not in materials: materials.append(asset)
materials.append(unreal.load_asset('/Game/SeniorSendOff/House/ExteriorNature/M_NatureFoliage'))
seen=set();rows=[]
def inspect(asset):
    if not asset or asset.get_path_name() in seen: return
    seen.add(asset.get_path_name())
    function=isinstance(asset,unreal.MaterialFunction)
    expressions=E.get_material_function_expressions(asset) if function else E.get_material_expressions(asset)
    row={'asset':asset.get_path_name(),'expressions':[]}
    for x in expressions:
        item={'name':x.get_name(),'class':x.get_class().get_name(),'inputs':[str(n) for n in E.get_material_expression_input_names(x)],'outputs':[str(n) for n in E.get_material_expression_output_names(x)]}
        for key in ['parameter_name','default_value','sampler_type','texture','collection']:
            try:
                value=x.get_editor_property(key)
                item[key]=value.get_path_name() if isinstance(value,unreal.Object) else str(value)
            except Exception: pass
        if isinstance(x,unreal.MaterialExpressionMaterialFunctionCall):
            child=x.get_editor_property('material_function')
            item['function']=child.get_path_name() if child else None
            if child: inspect(child)
        row['expressions'].append(item)
    if not function:
        row['properties']={}
        for prop in ['BASE_COLOR','OPACITY_MASK','WORLD_POSITION_OFFSET','SUBSURFACE_COLOR','ROUGHNESS','NORMAL']:
            p=getattr(unreal.MaterialProperty,'MP_'+prop)
            node=E.get_material_property_input_node(asset,p)
            row['properties'][prop]={'node':node.get_name() if node else None,'output':E.get_material_property_input_node_output_name(asset,p)}
    task=unreal.AssetExportTask();task.object=asset
    task.filename=str(ROOT/'Saved/CalderWork'/('tree_graph_'+asset.get_name()+'.copy'))
    task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.ObjectExporterT3D()
    assert unreal.Exporter.run_asset_export_task(task),asset.get_path_name()
    rows.append(row)
for asset in materials: inspect(asset)
for relative,expected in plan['source_sha256'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
out=ROOT/'Saved/CalderWork/tree_leaf_graphs.json'
out.write_text(json.dumps(rows,indent=2)+'\n')
unreal.log('CALDER_TREE_LEAF_GRAPH PASS graphs='+str(len(rows))+' sourcesUnchanged=1')
