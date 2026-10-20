import pathlib,json,re,traceback,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
W=B/'hall_conversion';D='/Game/SeniorSendOff/House/NightClub/HallConversion'
def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 actors={a.get_actor_label():a for a in S.get_all_level_actors()};before={a.get_name():snapshot(a)for a in S.get_all_level_actors()};changed=set();added=[];removed=[]
 def touch(a):changed.add(a.get_name());return a
 art=json.loads((W/'geometry/manifest.json').read_text());assert all(art['geometry_checks'].values())
 mats={k:A.load_asset(v['reuse_asset'])for k,v in art['materials'].items()};assert all(mats.values()),[k for k,v in mats.items()if not v]
 for row in art['assets']:
  mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
  for slot in slots:
   key=re.sub(r'[.]\d+$','',str(slot.get_editor_property('imported_material_slot_name')));assert key in mats,(row['name'],key);slot.material_interface=mats[key]
  mesh.static_materials=slots;collision(mesh,complex=row['collision']);mesh.set_editor_property('allow_cpu_access',True);assert A.save_loaded_asset(mesh,False)
  if row['target_actor']:
   a=touch(actors[row['target_actor']]);a.static_mesh_component.set_static_mesh(mesh);a.set_actor_scale3d(unreal.Vector(1,1,1))
  else:
   a=static('NightClub_'+row['name'],mesh,block=row['collision']);added.append(a.get_actor_label())
 for a in list(S.get_all_level_actors()):
  if a.get_actor_label().startswith('InfiniteHall_'):
   removed.append(a.get_actor_label());changed.add(a.get_name());assert S.destroy_actor(a)
 assert len(removed)==10,removed
 # Reuse the sectional, its chaise, side table and loose props, placed against real walls.
 moves=[
 ('NightClub_Upper_Leather_sofa',(-2680,-1840,333.1),-90),
 ('NightClub_Lounge_UpperChaise',(-2622.5,-2060,333.1),-90),
 ('NightClub_Upper_Lounge_table',(-2490,-1840,333.1),0),
 ('NightClub_Lounge_UpperSideTable',(-2510,-2140,333.1),0),
 ('NightClub_Lounge_UpperPillow_0',(-2665,-1730,377.1),-78),
 ('NightClub_Lounge_UpperPillow_1',(-2640,-2055,377.1),-110),
 ('NightClub_Polish_Cup_8',(-2500,-1838,371.2),10),
 ('NightClub_Polish_Bottle_9',(-2465,-1845,371.2),37),
 ('NightClub_Polish_Coaster_16',(-2510,-2140,389.2),22),
 ('NightClub_Neon_UpperAfter',(-1790,-2180,535),0),
 ('NightClub_Tube_Upper0',(-1790,-2180,590),0)]
 for label,pos,yaw in moves:
  a=touch(actors[label]);a.set_actor_location(unreal.Vector(*pos),False,True);a.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
 actors['NightClub_Tube_Upper0'].set_actor_scale3d(unreal.Vector(2.8,1,1))
 def add_static(label,path,pos,yaw=0,scale=(1,1,1)):
  mesh=A.load_asset(path);assert mesh,path;a=static(label,mesh,pos,yaw,scale);added.append(label);return a
 root='/Game/SeniorSendOff/House/NightClub/'
 for label,color,pos,yaw,sc in [
 ('LoungeWest','Cyan',(-2731,-1840,590),-90,2.8),
 ('LoungeSouth','Violet',(-2490,-2180,590),0,2.2),
 ('DanceEast','Magenta',(-1438,-1910,590),90,2.55),
 ('WideJoin','Violet',(-1835,-1529,594),0,2.8)]:
  add_static('NightClub_Hall_Tube_'+label,root+'SM_NC_WallStrip_'+color,pos,yaw,(sc,1,1))
 add_static('NightClub_Hall_NeonCocktail',root+'ConnectionPolish/SM_CP_NeonPicture_Cocktail',(-2731,-1810,536),-90,(.85,.85,.85))
 add_static('NightClub_Hall_NeonHeadphones',root+'ConnectionPolish/SM_CP_NeonPicture_Headphones',(-2400,-2180,537),0,(.85,.85,.85))
 def add_prop(label,path,pos,yaw=0,scale=(1,1,1),mass=20):
  a=prop(label,A.load_asset(path),pos,yaw,scale,mass);added.append(label);return a
 add_prop('NightClub_Hall_EastSofa',root+'SM_NC_ClubSofa',(-1510,-1930,333.1),90,(.9,.8,.9),85)
 add_prop('NightClub_Hall_EastSideTable','/Game/SeniorSendOff/House/NightClub/Lounge/SM_NCL_SideTable',(-1500,-1705,333.1),mass=18)
 add_prop('NightClub_Hall_EntrancePerch',root+'ConnectionPolish/SM_CP_WallDrinkPerch',(-2610,-1548,333.1),180,mass=26)
 # Keep room practical dim, and add no real lights.
 for c in touch(actors['HallEnd_end_Practical']).get_components_by_class(unreal.LightComponent):c.set_intensity(12);c.set_cast_shadows(False)
 club=touch(actors['NightClub_Ambience']);keys=['lower_room_min','lower_room_max','lower_extra_regions','lower_additional_doorways','lower_light_placements','upper_room_min','upper_room_max','lower_door_center','upper_door_center','music_loop','music_bpm','music_volume','music_loop_seconds','moving_light_intensity','strobe_light_intensity','pole_hole_center','pole_hole_radius']
 protected={k:str(club.get_editor_property(k))for k in keys}
 upper=[([-2739,-2188,332.53],[-2057,-1524.86675,622.71]),([-2079,-2188,332.53],[-1432,-1540.6,622.71]),([-2130,-1544,332.53],[-1540,-1495,622.71])]
 club.set_editor_property('upper_extra_regions',[room_region(lo,hi)for lo,hi in upper])
 entry=unreal.HouseClubDoorwayFade();entry.set_editor_property('center',unreal.Vector(-2394.7,-1524.86675,448.58));entry.set_editor_property('inward_normal',unreal.Vector(0,-1,0));entry.set_editor_property('room_region',room_region(*upper[0]));club.set_editor_property('upper_additional_doorways',[entry])
 club.set_editor_property('upper_light_placements',[
 light_placement((-1810,-1355,568.71),[-2212.51019,-1509.02675,332.53],[-1490,-973.5,622.71]),
 light_placement((-1870,-1840,568.71),[-2079,-2188,332.53],[-1432,-1540.6,622.71]),
 light_placement((-2370,-1770,574.71),[-2739,-2188,332.53],[-2057,-1524.86675,622.71])]);club.refresh_club()
 assert all(str(club.get_editor_property(k))==v for k,v in protected.items()),'Protected club settings changed'
 assert len(club.get_editor_property('club_lights'))==6
 after={a.get_name():snapshot(a)for a in S.get_all_level_actors()};drift=[r['label']for n,r in before.items()if n not in changed and after.get(n)!=r];assert not drift,drift
 assert all(after.get(n)==r for n,r in before.items()if r['cls']in['ExistingHouseDoor','HouseClubPole']or r['label'].startswith(('BWR_','PIG_'))),'Protected doors, pole or bedrooms changed'
 allowed=['HallEnd_SM_HallEnd_ThreeRoomsShell','HallEnd_COL_HallEnd_ThreeRoomsShell','HallEnd_SM_HallEnd_ThreeRoomsTrim','HallEnd_end_Practical']
 assert all(before[n]['label'].startswith(('NightClub_','InfiniteHall_'))or before[n]['label']in allowed for n in changed)
 assert len(after)==len(before)-len(removed)+len(added)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=dict(status='PASS',changed=[before[n]['label']for n in sorted(changed)if n in after],added=added,removed=removed,protected_actor_count=len(before)-len(changed),doors_pole_boywick_pigsty_unchanged=True,exterior_footprint_unchanged=True,total_club_lights=6,upper_extra_regions=upper)
 (W/'evidence/install.json').write_text(json.dumps(report,indent=2));(W/'evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2));unreal.log('CLUB_HALL_INSTALL_PASS')
try:run()
except:unreal.log_error('CLUB_HALL_INSTALL_FAIL '+traceback.format_exc())
