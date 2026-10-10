"""Only the club and the user-approved former chair room are changed."""
import json,pathlib,re,traceback,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
W=B/'wrap_refit';D='/Game/SeniorSendOff/House/NightClub/WrapRefit'
def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='Chapter01_House'
 actors={a.get_actor_label():a for a in S.get_all_level_actors()};before={a.get_name():snapshot(a) for a in S.get_all_level_actors()}
 baseline={r['label']:r for r in json.loads((B/'expansion2_layout/actors_before.json').read_text())}
 changed=set();deleted=[];new=[]
 def touch(a):changed.add(a.get_name());return a
 # Restore every pre-annex club structure asset, then apply only the lower corner refit.
 for label,r in baseline.items():
  if not label.startswith('NightClub_SM_'):continue
  a=touch(actors[label]);mesh=A.load_asset(r['meshes'][0]['mesh']);assert mesh,label;a.static_mesh_component.set_static_mesh(mesh)
 for label in ['NightClub_Lower_DJ_console','NightClub_Lower_Speaker_0','NightClub_Lower_Speaker_1','NightClub_Neon_LowerLate','NightClub_Neon_UpperLate','NightClub_Tube_Lower1','NightClub_Tube_Upper1','NightClub_LowerDiscoBall','NightClub_UpperDiscoBall']:
  a=touch(actors[label]);a.set_actor_location(unreal.Vector(*baseline[label]['location']),False,True)
 for label in ['NightClub_Tube_LowerWing','NightClub_Tube_UpperWing']:
  if label in actors:touch(actors[label]);deleted.append(label);S.destroy_actor(actors[label])
 # Remove the52 storage chairs and fluorescent fixture mesh. Keep102's working door and sign.
 for label,a in actors.items():
  if 'Room102_Chair_' in label or label=='SM_Room102_Fixtures':
   touch(a);deleted.append(label);S.destroy_actor(a)
 for label in ['Room102_CeilingLight_1','Room102_CeilingLight_2']:
  a=touch(actors[label]);deleted.append(label);S.destroy_actor(a)
 art=json.loads((W/'geometry/manifest.json').read_text());mats={}
 for key,value in art['materials'].items():
  mats[key]=A.load_asset(value['reuse_asset']);assert mats[key],(key,value)
 for row in art['assets']:
  mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
  for slot in slots:
   key=re.sub(r'[.]\d+$','',str(slot.get_editor_property('imported_material_slot_name')))
   mat=mats.get(key)
   if not mat:key=re.sub(r'[_]\d{3}$','',key);mat=mats.get(key)
   assert mat,(row['name'],key);slot.material_interface=mat
  mesh.static_materials=slots;collision(mesh,complex=row['collision']);assert A.save_loaded_asset(mesh,False)
  label=row.get('target_actor')
  if label:
   a=touch(actors[label]);a.static_mesh_component.set_static_mesh(mesh);a.set_actor_scale3d(unreal.Vector(1,1,1))
  else:
   label='NightClub_'+row['name'];a=static(label,mesh,block=row['collision'],hidden=row['hidden']);new.append(label)
 # Move the old lower sofa into the lounge, away from the main dance floor and corner passage.
 a=touch(actors['NightClub_Lower_Leather_sofa']);a.set_actor_location(unreal.Vector(-2680,-1915,.88),False,True);a.set_actor_rotation(unreal.Rotator(yaw=-90),False)
 a=touch(actors['NightClub_Neon_LowerPhi']);a.set_actor_location(unreal.Vector(-1740,-1500,242),False,True)
 a=touch(actors['NightClub_Tube_Lower0']);a.set_actor_location(unreal.Vector(-1730,-1500,292),False,True);a.set_actor_scale3d(unreal.Vector(2.1,1,1))
 a=touch(actors['NightClub_Neon_LowerAfter']);a.set_actor_location(unreal.Vector(-2744,-1855,237),False,True);a.set_actor_rotation(unreal.Rotator(yaw=-90),False)
 for label,mesh,pos,yaw,scale,mass in [
  ('NightClub_Lower_LoungeSofaSouth','/Game/SeniorSendOff/House/NightClub/SM_NC_ClubSofa',(-2390,-2070,.88),0,(.90,.8,.90),85),
  ('NightClub_Lower_LoungeCoffeeTable','/Game/SeniorSendOff/House/NightClub/Lounge/SM_NCL_CoffeeTable',(-2450,-1930,.88),0,(1,1,1),30),
  ('NightClub_Lower_LoungeOttoman','/Game/SeniorSendOff/House/NightClub/Lounge/SM_NCL_Ottoman',(-2245,-1940,.88),0,(1,1,1),22),
  ('NightClub_Lower_LoungeSideTable','/Game/SeniorSendOff/House/NightClub/Lounge/SM_NCL_SideTable',(-2670,-1675,.88),0,(1,1,1),18)]:
  assert label not in actors;asset=A.load_asset(mesh);assert asset,mesh;prop(label,asset,pos,yaw,scale,mass);new.append(label)
 for label,pos,yaw,scale in [('NightClub_Tube_LoungeSouth',(-2450,-2140,260),0,(2.55,1,1)),('NightClub_Tube_LoungeWest',(-2744,-1850,260),-90,(2.55,1,1)),('NightClub_Tube_Return',(-1976.2,-1625,261),90,(.9,1,1))]:
  static(label,A.load_asset('/Game/SeniorSendOff/House/NightClub/SM_NC_WallStrip_Cyan'),pos,yaw,scale);new.append(label)
 club=touch(actors['NightClub_Ambience'])
 main_lo=art['main'][0];main_hi=art['main'][1];chair_lo,chair_hi=art['chair'];neck_lo,neck_hi=art['connector']
 club.set_editor_property('lower_extra_regions',[room_region(chair_lo,chair_hi),room_region(neck_lo,neck_hi)])
 club.set_editor_property('upper_extra_regions',[]);club.set_editor_property('upper_light_placements',[])
 club.set_editor_property('lower_light_placements',[
  light_placement((-1750,-1200,273.03),main_lo,main_hi),
  light_placement((-2450,-1850,234.464),chair_lo,chair_hi),
  light_placement((-1970,-1300,273.03),main_lo,main_hi)])
 entry=unreal.HouseClubDoorwayFade();entry.set_editor_property('center',unreal.Vector(-2451.24,-1482.58,129.8));entry.set_editor_property('inward_normal',unreal.Vector(0,-1,0));entry.set_editor_property('room_region',room_region(chair_lo,chair_hi))
 club.set_editor_property('lower_additional_doorways',[entry]);club.set_editor_property('upper_additional_doorways',[]);club.refresh_club()
 assert len(club.get_editor_property('club_lights'))==6
 after={a.get_name():snapshot(a) for a in S.get_all_level_actors()}
 drift=[r['label'] for name,r in before.items() if name not in changed and (name not in after or r!=after[name])]
 assert not drift,('Protected actors changed',drift)
 assert all(before[n]['label'].startswith('NightClub_') or 'Room102' in before[n]['label'] for n in changed)
 # Existing door actors and pole actors must be byte-for-byte unchanged in the snapshot.
 for n,r in before.items():
  if r['cls'] in ['ExistingHouseDoor','HouseClubPole']:assert after.get(n)==r,r['label']
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 area=lambda lo,hi:(hi[0]-lo[0])*(hi[1]-lo[1])/10000
 area_main=area(main_lo,main_hi);area_chair=area(chair_lo,chair_hi);overlap=(main_lo[0]-chair_hi[0])*-1*max(0,chair_hi[1]-main_lo[1])/10000
 report=dict(status='PASS',actor_count=len(after),protected_actors=len(before)-len(changed),protected_unchanged=True,
  changed=[before[n]['label'] for n in sorted(changed)],deleted=deleted,added=new,upper_pre_annex_area_m2=area_main,
  lower_connected_area_m2=area_main+area_chair+area(neck_lo,neck_hi)-overlap,
  east_annex_removed_both_floors=True,doors_and_pole_unchanged=True,real_lights_total=6,
  music_loop_unchanged=str(club.music_loop),lower_regions=[art['main'],art['chair'],art['connector']])
 (W/'evidence/install-report.json').write_text(json.dumps(report,indent=2));(W/'evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2))
 unreal.log('CLUB_WRAP_INSTALL_PASS protected='+str(report['protected_actors']))
try:run()
except:unreal.log_error('CLUB_WRAP_INSTALL_FAIL '+traceback.format_exc())
