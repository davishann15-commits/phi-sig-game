import unreal,json,math,shutil
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs')
(R/'placement-fix').mkdir(parents=True,exist_ok=True)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
u=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
w=u.get_editor_world()
if not w or w.get_name()!='Chapter01_House':w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
assert w.get_name()=='Chapter01_House'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
bylabel={a.get_actor_label():a for a in actors}
for name in ['GreatRoom_WestTV','GreatRoom_EastTV','GreatRoom_WestTV_Bracket','GreatRoom_EastTV_Bracket']:assert name in bylabel
assert sum(isinstance(a,unreal.HouseInteractiveProp) and 'SSOKitchenRefit' in [str(t) for t in a.tags] for a in actors)==137
backup=R/'placement-fix/before-placement.umap'
if not backup.exists():shutil.copy2(Path(unreal.Paths.project_dir())/'Content/Story/Maps/Chapter01_House.umap',backup)
# Projecting fireplace faces measured from the current Upper meshes. The source
# mesh X coordinates are metres; its owning actors apply 1.8 horizontal scale.
rows=[]
for name,shell_name,local_wall,away in [('GreatRoom_WestTV','SM_Upper_p0_m1_Solid',-6.39,1),('GreatRoom_EastTV','SM_Upper_p0_p0_Solid',6.94,-1)]:
 tv=bylabel[name];mount=bylabel[name+'_Bracket'];shell=bylabel[shell_name]
 assert abs(shell.get_actor_scale3d().x-1.8)<.001
 assert abs(shell.get_actor_rotation().yaw)<.1
 wall=local_wall*100*shell.get_actor_scale3d().x+shell.get_actor_location().x
 loc=tv.get_actor_location();oldloc=[loc.x,loc.y,loc.z];oldscale=tv.get_actor_scale3d()
 loc.x=wall+away*14.2;loc.z=251.0
 tv.set_actor_location(loc,False,True)
 tv.set_actor_scale3d(unreal.Vector(1.65,1.0,1.65));tv.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90 if away>0 else 90,roll=0),True)
 tv.refresh_prop()
 mount.set_actor_location(unreal.Vector(wall+away*.96,loc.y,loc.z),False,True)
 mount.set_actor_scale3d(unreal.Vector(1.65,1.6,1.65));mount.set_actor_rotation(tv.get_actor_rotation(),True)
 center,extent=tv.get_actor_bounds(False)
 assert abs(extent.y-151.8)<.2 and extent.x<6
 # Bracket back is at the wall plane; its front reaches the television VESA
 # bolt plane. The enclosure has 9.95 cm clearance behind it for the mount and raised fireplace molding.
 mount_back=(mount.get_actor_location().x-away*.96)
 bracket_front=mount.get_actor_location().x+away*8.64
 vesa_plane=loc.x-away*4.6
 assert abs(mount_back-wall)<.001 and abs(bracket_front-vesa_plane)<.001
 rows.append(dict(id=name,old_location=oldloc,old_scale=[oldscale.x,oldscale.y,oldscale.z],location=[loc.x,loc.y,loc.z],scale=[1.65,1,1.65],width_cm=303.6,height_cm=175.725,wall_plane_x=wall,bracket_back_x=mount_back,bracket_front_x=bracket_front,tv_mount_plane_x=vesa_plane,bracket_touches_wall=True,bracket_connects_tv=True,body_wall_clearance_cm=9.95,bounds_center=[center.x,center.y,center.z],bounds_extent=[extent.x,extent.y,extent.z]))
assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
(R/'placement-fix/placement-fixed.json').write_text(json.dumps(dict(passed=True,tvs=rows,kitchen_props_preserved=137),indent=2))
c=unreal.Vector(-990,480,190);t=unreal.Vector(-1136,190,251);d=t-c
u.set_level_viewport_camera_info(c,unreal.Rotator(pitch=math.degrees(math.atan2(d.z,math.hypot(d.x,d.y))),yaw=math.degrees(math.atan2(d.y,d.x)),roll=0))
unreal.log('GREAT_ROOM_TVS_FLUSH_AND_RESIZED')
