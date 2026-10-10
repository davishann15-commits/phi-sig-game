import unreal,json
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs')
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rows=[]
def vec(v):return [v.x,v.y,v.z]
for a in actors.get_all_level_actors():
 if a.get_actor_label() not in ['GreatRoom_WestTV','GreatRoom_EastTV','GreatRoom_WestTV_Bracket','GreatRoom_EastTV_Bracket']:continue
 before=a.get_actor_rotation();yaw=-90 if 'West' in a.get_actor_label() else 90
 a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
 if isinstance(a,unreal.HouseInteractiveProp):a.refresh_prop()
 c,e=a.get_actor_bounds(False)
 rows.append(dict(actor=a.get_actor_label(),before=[before.pitch,before.yaw,before.roll],after=[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll],world_center=vec(c),world_extent=vec(e)))
for label in ['SM_Entrance_p0_m1_Solid','SM_Entrance_p0_p0_Solid']:
 a=next(a for a in actors.get_all_level_actors() if a.get_actor_label()==label)
 t=unreal.AssetExportTask();t.object=a.static_mesh_component.static_mesh;t.filename=str(R/'context/fbx'/('Actual_'+label+'.fbx'));t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(t)
assert len(rows)==4
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House')
(R/'orientation-fixed.json').write_text(json.dumps(dict(passed=True,actors=rows),indent=2))
unreal.log('GREAT_ROOM_TV_ORIENTATION_FIXED')
