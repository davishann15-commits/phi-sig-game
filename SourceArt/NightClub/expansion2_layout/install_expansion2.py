"""Scoped club-only annex install. Uses the freshly saved pre-expansion map backup."""
import json,pathlib,re,traceback,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'install.py').read_text().split('def run():')[0],globals())
D='/Game/SeniorSendOff/House/NightClub/Expansion2'
L=B/'expansion2_layout'

def snapshot(a):
 r=a.get_actor_rotation()
 row=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),location=xyz(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=xyz(a.get_actor_scale3d()),tags=[str(t) for t in a.tags],meshes=[],lights=[])
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  if c.static_mesh:row['meshes'].append(dict(component=c.get_name(),mesh=c.static_mesh.get_path_name(),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],collision=str(c.get_collision_enabled()),visible=c.is_visible()))
 for c in a.get_components_by_class(unreal.LightComponent):row['lights'].append(dict(component=c.get_name(),intensity=c.intensity))
 if isinstance(a,unreal.ExistingHouseDoor):
  row['door']={k:str(a.get_editor_property(k)) for k in ['door_id','original_leaf','original_glass','doorway_center','closed_yaw','open_yaw','locked','panel_center_world','panel_half_extent','local_leaf_space']}
 return row

def room_region(lo,hi):
 region=unreal.HouseClubRoomRegion();region.set_editor_property('minimum',unreal.Vector(*lo));region.set_editor_property('maximum',unreal.Vector(*hi));return region

def light_placement(pos,lo,hi):
 item=unreal.HouseClubLightPlacement();item.set_editor_property('optical_origin',unreal.Vector(*pos));item.set_editor_property('sweep_region',room_region(lo,hi));item.set_editor_property('enabled',True);return item

def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='Chapter01_House',world.get_name()
 actors={a.get_actor_label():a for a in S.get_all_level_actors()}
 old={a.get_name():snapshot(a) for a in S.get_all_level_actors()}
 (L/'actors_before.json').write_text(json.dumps(list(old.values()),indent=2))
 art=json.loads((B/'expansion2_geometry/expansion2_manifest.json').read_text())
 changed=set();mats={}
 for key,value in art.get('materials',{}).items():
  path=value.get('reuse_asset','/Game/SeniorSendOff/House/NightClub/'+key)
  mats[key]=A.load_asset(path)
  if not mats[key]:mats[key]=material(key,value)
 for row in art['assets']:
  mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
  for slot in slots:
   key=re.sub(r'[._]\d+$','',str(slot.get_editor_property('imported_material_slot_name')))
   mat=mats.get(key) or A.load_asset('/Game/SeniorSendOff/House/NightClub/'+key)
   assert mat,(row['name'],key);slot.material_interface=mat
  mesh.static_materials=slots
  collision(mesh,complex=bool(row.get('collision',True)))
  assert A.save_loaded_asset(mesh,False),row['name']
  label=row['target_actor'];assert label.startswith('NightClub_'),label
  actor=actors[label];assert isinstance(actor,unreal.StaticMeshActor),label
  assert all(abs(x)<.001 for x in xyz(actor.get_actor_location())) and actor.get_actor_scale3d()==unreal.Vector(1,1,1),label
  actor.static_mesh_component.set_static_mesh(mesh);changed.add(actor.get_name())
 # East wall furnishings follow the enlarged club; the pole and doors stay fixed.
 relocations={
  'NightClub_Lower_DJ_console':(-915,-1240,.88),
  'NightClub_Lower_Speaker_0':(-905,-1400,.88),
  'NightClub_Lower_Speaker_1':(-905,-1100,.88),
  'NightClub_Neon_LowerLate':(-866.2,-1245,237),
  'NightClub_Neon_UpperLate':(-866.2,-1330,550),
  'NightClub_Tube_Lower1':(-866.2,-1270,292),
  'NightClub_Tube_Upper1':(-866.2,-1270,590),
  'NightClub_LowerDiscoBall':(-1230,-1265,294),
  'NightClub_UpperDiscoBall':(-1230,-1265,589),
 }
 for label,pos in relocations.items():
  a=actors[label];a.set_actor_location(unreal.Vector(*pos),False,True);changed.add(a.get_name())
 # Two inexpensive emissive strips fill the new north return wall. No added real lights.
 new=[]
 for floor,z in [('Lower',292),('Upper',590)]:
  label='NightClub_Tube_'+floor+'Wing';mesh=A.load_asset('/Game/SeniorSendOff/House/NightClub/SM_NC_WallStrip_Cyan');assert mesh
  a=actors.get(label) or static(label,mesh,(-1175,-1036.2,z),180,(2.7,1,1))
  if a.get_name() in old:changed.add(a.get_name())
  else:new.append(label)
 club=actors['NightClub_Ambience']
 protected_keys=['lower_room_min','lower_room_max','upper_room_min','upper_room_max','lower_door_center','upper_door_center','lower_door_inward_normal','upper_door_inward_normal','pole_hole_center','pole_hole_radius','door_fade_width','door_half_width','music_loop','music_bpm','music_loop_seconds','music_volume','hole_music_gain','moving_light_intensity','strobe_light_intensity','club_exposure_bias','neon_bloom_intensity','neon_bloom_threshold']
 protected={k:club.get_editor_property(k) for k in protected_keys}
 club.set_editor_property('lower_extra_regions',[room_region((-1490,-1509.02675,.88),(-860,-1030,327.03))])
 club.set_editor_property('upper_extra_regions',[room_region((-1490,-1509.02675,332.53),(-860,-1030,622.71))])
 for floor,low,high in [('lower',.88,327.03),('upper',332.53,622.71)]:
  # One mover covers the original pole/lounge, one covers the wing, one soft strobe per floor.
  base_lo=(-2212.51019,-1509.02675,low);base_hi=(-1490,-973.5,high)
  wing_lo=(-1490,-1509.02675,low);wing_hi=(-860,-1030,high)
  placements=[light_placement((-1800,-1265,high-54),base_lo,base_hi),light_placement((-1175,-1265,high-54),wing_lo,wing_hi),light_placement((-1260,-1240,high-48),wing_lo,wing_hi)]
  club.set_editor_property(floor+'_light_placements',placements)
 club.refresh_club();changed.add(club.get_name())
 assert all(club.get_editor_property(k)==v for k,v in protected.items()),'Protected club transport, pole, entry or lighting settings changed'
 assert len(club.get_editor_property('club_lights'))==6
 after={a.get_name():snapshot(a) for a in S.get_all_level_actors()}
 drift=[r['label'] for name,r in old.items() if name not in changed and (name not in after or r!=after[name])]
 assert not drift,('Protected actor changes',drift)
 assert all(old[name]['label'].startswith('NightClub_') for name in changed),'Nonclub actor changed'
 assert len(after)==len(old)+len(new)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 base_area=722.51019*535.52675/10000;added_area=630*479.02675/10000
 report=dict(status='PASS',actor_count=len(after),protected_actors=len(old)-len(changed),protected_unchanged=True,changed_actors=[old[k]['label'] for k in changed],new_actors=new,base_floor_area_m2=base_area,added_floor_area_m2=added_area,new_floor_area_m2=base_area+added_area,percent_added=100*added_area/base_area,kind='Club-only stepped exterior annex in empty level space',real_lights_total=6,pole_doors_and_song_unchanged=True,backup=str(B/'expansion2_context/Chapter01_House_before_expansion2.umap'))
 (L/'install-report.json').write_text(json.dumps(report,indent=2));(L/'actors_after.json').write_text(json.dumps(list(after.values()),indent=2))
 unreal.log('NIGHTCLUB_EXPANSION2_INSTALL_PASS protected='+str(report['protected_actors']))
try:run()
except:unreal.log_error('NIGHTCLUB_EXPANSION2_INSTALL_FAIL '+traceback.format_exc())
