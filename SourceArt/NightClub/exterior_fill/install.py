"""Install only nightclub geometry into the measured vacant ground floor."""
import pathlib,json,re,traceback,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
W=B/'exterior_fill';D='/Game/SeniorSendOff/House/NightClub/ExteriorFill'
def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 actors={a.get_actor_label():a for a in S.get_all_level_actors()};before={a.get_name():snapshot(a)for a in S.get_all_level_actors()};changed=set();added=[]
 art=json.loads((W/'geometry/manifest.json').read_text());mats={k:A.load_asset(v['reuse_asset'])for k,v in art['materials'].items()};assert all(mats.values()),[k for k,v in mats.items()if not v]
 def touch(a):changed.add(a.get_name());return a
 for row in art['assets']:
  mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
  for slot in slots:
   key=re.sub(r'[.]\d+$','',str(slot.get_editor_property('imported_material_slot_name')));assert key in mats,(row['name'],key);slot.material_interface=mats[key]
  mesh.static_materials=slots;collision(mesh,complex=row['collision']);assert A.save_loaded_asset(mesh,False)
  if row['target_actor']:
   a=touch(actors[row['target_actor']]);a.static_mesh_component.set_static_mesh(mesh)
  else:
   label='NightClub_'+row['name']
   if label in actors:
    a=touch(actors[label]);a.static_mesh_component.set_static_mesh(mesh)
   else:
    a=static(label,mesh,block=row['collision']);added.append(a.get_actor_label())
 # Every former wall-mounted object follows its actual new wall.
 moves=[('NightClub_Neon_LowerPhi',(-1770,-2180,216),0),('NightClub_Neon_LowerAfter',(-2520,-2180,216),0),('NightClub_Neon_LowerLate',(-1438,-1230,220),90),
 ('NightClub_Tube_Lower0',(-1770,-2182,260),0),('NightClub_Tube_Lower1',(-1438,-1230,292),90),
 ('NightClub_Tube_LoungeSouth',(-2520,-2182,260),0),('NightClub_Tube_LoungeWest',(-3382,-1850,260),-90),('NightClub_Tube_Return',(-1438,-1870,260),90),
 ('NightClub_Lower_DJ_console',(-2520,-2098,.88),0),('NightClub_Lower_Speaker_0',(-2740,-2098,.88),0),('NightClub_Lower_Speaker_1',(-2305,-2098,.88),0),
 ('NightClub_Lower_LoungeSofaSouth',(-3310,-1690,.88),-90),('NightClub_Lower_LoungeCoffeeTable',(-3160,-1690,.88),0),
 ('NightClub_Polish_Cup_0',(-3182,-1690,39),0),('NightClub_Polish_Bottle_1',(-3135,-1698,39),17),('NightClub_Polish_Coaster_2',(-3182,-1690,38.99),34),
 ('NightClub_Polish_DJPerch',(-1446,-1920,.88),90),('NightClub_Polish_Cup_6',(-1446,-1920,108.25),102),('NightClub_Polish_Bottle_7',(-1446,-1965,108.25),119)]
 for label,pos,yaw in moves:
  a=touch(actors[label]);a.set_actor_location(unreal.Vector(*pos),False,True);a.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
 club=touch(actors['NightClub_Ambience']);protect_keys=['lower_room_min','lower_room_max','upper_room_min','upper_room_max','upper_extra_regions','upper_light_placements','lower_door_center','upper_door_center','music_loop','music_bpm','music_volume','music_loop_seconds','moving_light_intensity','strobe_light_intensity','pole_hole_center','pole_hole_radius']
 protected={k:str(club.get_editor_property(k))for k in protect_keys}
 club.set_editor_property('lower_extra_regions',[room_region(lo,hi)for lo,hi in art['lower_regions']])
 # Same three lights downstairs: main mover, broad front-band mover, short soft strobe.
 club.set_editor_property('lower_light_placements',[
 light_placement((-1750,-1200,273.03),[-2212.51019,-1509.02675,.88],[-1490,-973.5,327.03]),
 light_placement((-2520,-1910,234.464),[-3388,-2188,.88],[-1432,-1522,288.464]),
 light_placement((-2750,-1870,240.464),[-3388,-2188,.88],[-1432,-1522,288.464])]);club.refresh_club()
 assert all(str(club.get_editor_property(k))==v for k,v in protected.items())
 after={a.get_name():snapshot(a)for a in S.get_all_level_actors()};drift=[r['label']for n,r in before.items()if n not in changed and after.get(n)!=r];assert not drift,drift
 assert all(before[n]['label'].startswith('NightClub_')or before[n]['label']in['SM_Room102_Shell','COL_Room102_Shell']for n in changed)
 assert all(after[n]==r for n,r in before.items()if r['cls']in['ExistingHouseDoor','HouseClubPole']or 'Upper' in r['label']or r['label'].startswith('BWR_'))
 assert len(club.get_editor_property('club_lights'))==6
 assert len(after)==len(before)+len(added)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=dict(status='PASS',changed=[before[n]['label']for n in sorted(changed)],added=added,deleted=[],protected_actor_count=len(before)-len(changed),upstairs_doors_pole_and_other_rooms_unchanged=True,exterior_footprint_unchanged=True,total_club_lights=6)
 (W/'evidence/install-report.json').write_text(json.dumps(report,indent=2));(W/'evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2));unreal.log('CLUB_FILL_INSTALL_PASS')
try:run()
except:unreal.log_error('CLUB_FILL_INSTALL_FAIL '+traceback.format_exc())
