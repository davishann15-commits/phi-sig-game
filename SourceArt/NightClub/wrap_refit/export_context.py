"""Read the saved house before the scoped club/chair-room refit."""
import json,pathlib,traceback,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
O=B/'wrap_refit/context';(O/'fbx').mkdir(parents=True,exist_ok=True)
try:
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 assert world.get_name()=='Chapter01_House',world.get_name()
 rows=[];all_rows=[];properties={}
 for a in S.get_all_level_actors():
  row=snapshot(a);all_rows.append(row)
  o,e=a.get_actor_bounds(False);lo=o-e;hi=o+e
  if a.get_actor_label()=='NightClub_Ambience':
   for k in ['lower_room_min','lower_room_max','upper_room_min','upper_room_max','lower_door_center','upper_door_center','lower_door_inward_normal','upper_door_inward_normal','lower_extra_regions','upper_extra_regions','lower_light_placements','upper_light_placements']:
    v=a.get_editor_property(k);properties[k]=xyz(v) if isinstance(v,unreal.Vector) else str(v)
  if hi.x < -3000 or lo.x > -1400 or hi.y < -2240 or lo.y > -930 or hi.z < -25 or lo.z > 650:continue
  r=dict(row,bounds=[xyz(lo),xyz(hi)])
  for c,m in zip([c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh],r['meshes']):
   if a.get_class().get_name()=='StaticMeshActor':
    p=O/'fbx'/(a.get_name()+'_'+c.get_name()+'.fbx')
    t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(p);t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX()
    assert unreal.Exporter.run_asset_export_task(t),r['label'];m['fbx']=str(p)
  rows.append(r)
 (O/'actors.json').write_text(json.dumps(rows,indent=2))
 (O/'all_actors.json').write_text(json.dumps(all_rows,indent=2))
 (O/'club_properties.json').write_text(json.dumps(properties,indent=2))
 unreal.log('CLUB_WRAP_CONTEXT_PASS actors='+str(len(all_rows)))
except:unreal.log_error('CLUB_WRAP_CONTEXT_FAIL '+traceback.format_exc())
