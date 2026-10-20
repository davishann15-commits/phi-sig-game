"""Import/reduce only owned wood meshes; no leaf competition, no map save."""
import hashlib,json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE=ROOT/'SourceArt/CalderLandscape/phase2/tree_wood'
DEST='/Game/SeniorSendOff/Calder/Trees'
A=unreal.EditorAssetLibrary;T=unreal.AssetToolsHelpers.get_asset_tools()
S=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
plan=json.loads((SOURCE/'wood_build.json').read_text())
source_plan=json.loads((ROOT/'SourceArt/CalderLandscape/phase2/tree_candidates.json').read_text())
rows=[]
for tree in plan['trees']:
    file=SOURCE/tree['file'];assert hashlib.sha256(file.read_bytes()).hexdigest()==tree['fbx_sha256']
    name='SM_CalderTreeWood_'+tree['label']
    task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=DEST;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=False
    ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False
    ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.combine_meshes=True
    ui.static_mesh_import_data.generate_lightmap_u_vs=False
    ui.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task.options=ui;T.import_asset_tasks([task])
    mesh=unreal.load_asset(DEST+'/'+name);assert mesh,name
    mesh.set_editor_property('allow_cpu_access',True)
    nanite=mesh.get_editor_property('nanite_settings');nanite.enabled=False;S.set_nanite_settings(mesh,nanite,False)
    for i,slot in enumerate(mesh.static_materials):
        label=str(slot.material_slot_name).split('.')[0]
        path=next((p for p in tree['materials'] if p.split('.')[-1]==label),None)
        assert path,(name,label,tree['materials'])
        mesh.set_material(i,unreal.load_asset(path))
    options=unreal.StaticMeshReductionOptions();reductions=[]
    for fraction,screen in [(1.,1.),(.15,.35),(.06,.12)]:
        setting=unreal.StaticMeshReductionSettings();setting.percent_triangles=fraction
        setting.screen_size=screen
        reductions.append(setting)
    options.reduction_settings=reductions;options.auto_compute_lod_screen_size=False
    assert S.set_lods(mesh,options)==3
    for i,fraction in enumerate([1.,.15,.06]):
        native=S.get_lod_reduction_settings(mesh,i)
        native.percent_vertices=fraction
        native.silhouette_importance=unreal.MeshFeatureImportance.HIGHEST
        S.set_lod_reduction_settings(mesh,i,native)
        settings=S.get_lod_build_settings(mesh,i)
        settings.recompute_normals=False;settings.recompute_tangents=True
        settings.use_full_precision_u_vs=True;settings.use_high_precision_tangent_basis=True
        S.set_lod_build_settings(mesh,i,settings)
    S.remove_collisions(mesh);mesh.set_editor_property('generate_mesh_distance_field',False)
    A.save_loaded_asset(mesh)
    bounds=mesh.get_bounds();lo=bounds.origin-bounds.box_extent;hi=bounds.origin+bounds.box_extent
    actual=[[lo.x,lo.y,lo.z],[hi.x,hi.y,hi.z]]
    error=max(abs(actual[s][a]-tree['bounds_cm'][s][a]) for s in range(2) for a in range(3))
    assert error<.12,(name,'axes/scale changed',actual,tree['bounds_cm'])
    triangles=[mesh.get_num_triangles(i) for i in range(3)]
    assert triangles[0]>triangles[1]>triangles[2]>=100,triangles
    assert triangles[1]<6000 and triangles[2]<2200,triangles
    row={'label':tree['label'],'asset':mesh.get_path_name(),'triangles':triangles,'bounds_cm':actual,'max_bounds_error_cm':error,'nanite':False,'collision':False}
    rows.append(row);unreal.log('CALDER_TREE_WOOD_REDUCED '+json.dumps(row))
for relative,expected in source_plan['source_sha256'].items():
    assert hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected,relative
out=ROOT/'SourceArt/CalderLandscape/phase2/tree_wood.json';out.unlink(missing_ok=True)
out.write_text(json.dumps({'scope':'Owned unbound wood-only derivatives; native attachment/texture/performance checks pending','source_packs_unchanged':True,'trees':rows},indent=2)+'\n')
unreal.log('CALDER_TREE_WOOD_IMPORT PASS sourcesUnchanged=1 liveForestBound=0')
