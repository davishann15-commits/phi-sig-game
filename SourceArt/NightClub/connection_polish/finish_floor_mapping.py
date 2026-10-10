import json,pathlib,unreal,traceback
P=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub/connection_polish'
try:
 exec((P/'install.py').read_text().split('\ndef run():')[0],globals())
 row=json.loads((P/'art/manifest.json').read_text())['materials']['M_CP_WornFloor']
 textures={'floor':{key:A.load_asset(D+'/T_CP_floor_'+key)for key in['Colour','Roughness']}}
 assert all(textures['floor'].values())
 polished_material('M_CP_WornFloor',row,textures)
 actors={a.get_actor_label():a for a in S.get_all_level_actors()}
 before={a.get_name():snapshot(a)for a in S.get_all_level_actors()};changed=set()
 for label,pos in [('NightClub_Polish_MainPerch',(-1730,-985,.88)),('NightClub_Polish_Cup_3',(-1730,-985,108.25)),('NightClub_Polish_Bottle_4',(-1774,-986,108.25)),('NightClub_Polish_Coaster_5',(-1730,-985,108.2)),('NightClub_Polish_UpperPerch',(-1730,-985,333.1)),('NightClub_Polish_Cup_10',(-1744,-985,440.45)),('NightClub_Polish_Bottle_11',(-1690,-985,440.45))]:
  a=actors[label];a.set_actor_location(unreal.Vector(*pos),False,True);changed.add(a.get_name())
 after={a.get_name():snapshot(a)for a in S.get_all_level_actors()};assert all(after[n]==r for n,r in before.items()if n not in changed)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 (P/'evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2))
 (P/'evidence/floor-material-report.json').write_text(json.dumps(dict(status='PASS',world_aligned_centimeter_scale=.01,reason='The old upstairs finish mesh has no UV stream; world projection shows the wear on both floors and keeps it continuous through the connection.',moved_drink_perch_actors=7,structural_geometry_changes=0),indent=2))
 unreal.log('CLUB_POLISH_FLOOR_MAPPING_PASS')
except:unreal.log_error('CLUB_POLISH_FLOOR_MAPPING_FAIL '+traceback.format_exc())
