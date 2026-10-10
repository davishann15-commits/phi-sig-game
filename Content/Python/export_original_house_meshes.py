import unreal,os
root='/Game/SeniorSendOff/House/Meshes'
os.makedirs('/tmp/sso_house_original_meshes',exist_ok=True)
for path in unreal.EditorAssetLibrary.list_assets(root,recursive=False,include_folder=False):
 n=path.rsplit('/',1)[-1].split('.')[0]
 if not n.startswith(('SM_Entrance','SM_Basement','SM_Upper')) or not n.endswith(('_Solid','_Glass')):continue
 a=unreal.load_asset(path)
 if not isinstance(a,unreal.StaticMesh):continue
 t=unreal.AssetExportTask();t.set_editor_property('object',a);t.set_editor_property('filename','/tmp/sso_house_original_meshes/'+n+'.fbx');t.set_editor_property('automated',True);t.set_editor_property('prompt',False);t.set_editor_property('replace_identical',True);t.set_editor_property('exporter',unreal.StaticMeshExporterFBX())
 if unreal.Exporter.run_asset_export_task(t):print('EXPORTED',n)
