import json,pathlib,unreal,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
O=B/'hall_conversion/context'
try:
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 rows=[]
 for a in S.get_all_level_actors():
  row=snapshot(a);o,e=a.get_actor_bounds(False);row['bounds']=[xyz(o-e),xyz(o+e)]
  if row['label'].startswith(('HallEnd_','InfiniteHall_','NightClub_')) or row['label'] in ['SM_Upper_m1_m2_Solid','COL_Upper_m1_m2_Solid']:
   for c,m in zip([c for c in a.get_components_by_class(unreal.StaticMeshComponent)if c.static_mesh],row['meshes']):
    p=O/'fbx'/(a.get_name()+'_'+c.get_name()+'.fbx');t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(p);t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t);m['fbx']=str(p)
  rows.append(row)
  if row['label']=='NightClub_Ambience':
   keys=['lower_room_min','lower_room_max','upper_room_min','upper_room_max','lower_extra_regions','upper_extra_regions','lower_door_center','upper_door_center','upper_door_inward_normal','lower_light_placements','upper_light_placements','music_loop','music_volume','pole_hole_center','pole_hole_radius','upper_additional_doorways']
   pr={k:xyz(a.get_editor_property(k))if isinstance(a.get_editor_property(k),unreal.Vector)else str(a.get_editor_property(k)) for k in keys};(O/'club.json').write_text(json.dumps(pr,indent=2))
 (O/'actors.json').write_text(json.dumps(rows,indent=2));unreal.log('HALL_CONTEXT_PASS '+str(len(rows)))
except:unreal.log_error('HALL_CONTEXT_FAIL '+traceback.format_exc())
