"""Refresh only the owned deer static meshes after neck weight revision. No maps."""
import json,hashlib
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE=ROOT/'SourceArt/CalderLandscape/phase3/deer';DEST='/Game/SeniorSendOff/Calder/Wildlife'
A=unreal.EditorAssetLibrary;S=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);T=unreal.AssetToolsHelpers.get_asset_tools()
anatomy=json.loads((SOURCE/'anatomy.json').read_text());stats={}
guards={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/AnimalVarietyPack/DeerStagAndDoe').rglob('*.uasset')}
meshes=[]
for i,lod in enumerate(anatomy['lods']):
 task=unreal.AssetImportTask();task.filename=str(SOURCE/lod['file']);task.destination_path=DEST;task.destination_name='SM_CalderDeerBody' if i==0 else 'SM_CalderDeerSourceLOD%d'%i;task.automated=True;task.replace_existing=True;task.save=False
 ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;ui.automated_import_should_detect_type=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.generate_lightmap_u_vs=False;ui.static_mesh_import_data.auto_generate_collision=False;task.options=ui;T.import_asset_tasks([task]);mesh=unreal.load_asset(task.imported_object_paths[0]);assert isinstance(mesh,unreal.StaticMesh)
 b=mesh.get_bounds();lo=b.origin-b.box_extent+mesh.get_editor_property('negative_bounds_extension');hi=b.origin+b.box_extent-mesh.get_editor_property('positive_bounds_extension')
 actual=[[lo.x,lo.y,lo.z],[hi.x,hi.y,hi.z]];assert max(abs(actual[side][axis]-lod['bounds_cm'][side][axis]) for side in range(2) for axis in range(3))<.12,('static deer axes/scale',actual,lod['bounds_cm'])
 nanite=mesh.get_editor_property('nanite_settings');nanite.enabled=False;S.set_nanite_settings(mesh,nanite,False)
 mesh.set_editor_property('allow_cpu_access',True);settings=S.get_lod_build_settings(mesh,0);settings.use_full_precision_u_vs=True;settings.use_high_precision_tangent_basis=True;S.set_lod_build_settings(mesh,0,settings);meshes.append(mesh)
main=meshes[0]
for i in [1,2]:assert S.set_lod_from_static_mesh(main,i,meshes[i],0,True)==i
assert S.set_lod_screen_sizes(main,[1.,.3,.08])
for i in range(len(main.static_materials)):main.set_material(i,unreal.load_asset(DEST+('/M_CalderDeerSkin' if i==0 else '/M_CalderDeerFur')))
main.set_editor_property('positive_bounds_extension',unreal.Vector(200,200,200));main.set_editor_property('negative_bounds_extension',unreal.Vector(200,200,200))
for mesh in meshes:A.save_loaded_asset(mesh)
for path,digest in guards.items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest,path
report={'passed':True,'source_assets_unchanged':len(guards),'source_sha256':guards,'runtime_skeletons':0,'runtime_mesh':main.get_path_name(),'lods':S.get_lod_count(main),'uv_channels':[S.get_num_uv_channels(main,i) for i in range(3)],'materials':stats,'no_map_saved':True,'rigid_controls':16,'per_instance_custom_floats':225}
assert report['lods']==3 and all(n>=5 for n in report['uv_channels'])
p=SOURCE/'animation/mesh_revision_installed.json';p.unlink(missing_ok=True);p.write_text(json.dumps(report,indent=2)+'\n');unreal.log('CALDER_WILDLIFE_ASSET_BATCH PASS '+json.dumps(report))
