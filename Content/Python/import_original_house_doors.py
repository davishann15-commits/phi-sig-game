import unreal,json,os,re
root=os.path.join(unreal.Paths.project_dir(),'SourceArt/HouseDoors');dest='/Game/ExistingHouseDoors'
# Campaign-only furniture variants beside these sources use a separate import
# path. A full door rebuild must never overwrite their collision settings.
files=sorted(f for f in os.listdir(root) if f.endswith('.fbx') and
             f.startswith(('SM_Original', 'SM_HouseLeverHandle')))
tasks=[]
for f in files:
 t=unreal.AssetImportTask();t.set_editor_property('filename',root+'/'+f);t.set_editor_property('destination_path',dest);t.set_editor_property('destination_name',f[:-4]);t.set_editor_property('automated',True);t.set_editor_property('save',True);t.set_editor_property('replace_existing',True)
 ui=unreal.FbxImportUI();ui.set_editor_property('import_mesh',True);ui.set_editor_property('import_as_skeletal',False);ui.set_editor_property('import_materials',False);ui.set_editor_property('import_textures',False);t.set_editor_property('options',ui);tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
result=[]
for f in files:
 a=unreal.load_asset(dest+'/'+f[:-4])
 if not isinstance(a,unreal.StaticMesh):raise RuntimeError('Missing imported mesh '+f)
 for i,s in enumerate(a.get_editor_property('static_materials')):
  name=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0])
  mat=unreal.load_asset('/Game/SeniorSendOff/House/Materials/'+name)
  if not mat:raise RuntimeError('Missing material '+name+' for '+f)
  a.set_material(i,mat)
 unreal.EditorAssetLibrary.save_loaded_asset(a)
 result.append(f)
open('/tmp/import_original_house_doors_result.json','w').write(json.dumps(result))
