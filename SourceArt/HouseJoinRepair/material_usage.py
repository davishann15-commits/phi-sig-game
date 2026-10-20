import unreal,pathlib,json,shutil,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseJoinRepair';A=unreal.EditorAssetLibrary;L=unreal.MaterialEditingLibrary
try:
 report=json.loads((B/'evidence/performance-assets.json').read_text());usage=unreal.MaterialUsage.MATUSAGE_NANITE;done=[];seen=set()
 for name in ['M_House_Normal','M_House_Plain','M_House_Textured','M_House_Glass']:
  m=A.load_asset('/Game/SeniorSendOff/House/Materials/'+name);assert m
  L.set_base_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,True);assert L.has_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES);assert A.save_loaded_asset(m,False)

 # Nanite is an additional shader usage. Retain every authored expression,
 # parameter, and material assignment, and save the necessary permutations.
 for path in report['enabled_nanite']+[ '/Game/SeniorSendOff/House/HouseJoinRepairs/'+n+'.'+n for n in ['SM_HJ_EntranceStairAligned','SM_HJ_UpperStairAligned','SM_HJ_StairDoorThresholds','SM_HJ_GalleryBannerBrackets'] ]:
  mesh=A.load_asset(path);assert mesh,path
  for slot in mesh.static_materials:
   m=slot.material_interface
   if not m:continue
   while isinstance(m,unreal.MaterialInstance):m=m.get_editor_property('parent')
   assert isinstance(m,unreal.Material),m
   if m.get_path_name() in seen:continue
   seen.add(m.get_path_name());assert m.get_path_name().startswith('/Game/'),m.get_path_name()
   src=pathlib.Path(unreal.Paths.project_content_dir())/(m.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset');dest=B/'before/materials'/src.relative_to(unreal.Paths.project_content_dir());dest.parent.mkdir(parents=True,exist_ok=True)
   if not dest.exists():shutil.copy2(src,dest)
   L.set_base_material_usage(m,usage,True);assert L.has_material_usage(m,usage);assert A.save_loaded_asset(m,False);done.append(m.get_path_name())
 (B/'evidence/material-usage.json').write_text(json.dumps(dict(status='PASS',materials=done,appearance_and_assignments_unchanged=True),indent=2));unreal.log('HOUSE_JOIN_MATERIAL_USAGE_PASS '+str(len(done)))
except:unreal.log_error('HOUSE_JOIN_MATERIAL_USAGE_FAIL '+traceback.format_exc())
unreal.SystemLibrary.quit_editor()
