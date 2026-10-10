import unreal,pathlib,json
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
actors={a.get_actor_label():a for a in S.get_all_level_actors()}
before={a.get_name():snapshot(a)for a in S.get_all_level_actors()}
changed=set()
for label,pos in [('NightClub_Polish_Cup_8',(-1775,-1338,371.2)),('NightClub_Polish_Bottle_9',(-1740,-1345,371.2))]:
 a=actors[label];a.set_actor_location(unreal.Vector(*pos),False,True);changed.add(a.get_name())
after={a.get_name():snapshot(a)for a in S.get_all_level_actors()}
assert all(r==after[n]for n,r in before.items()if n not in changed)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(B/'connection_polish/evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2))
unreal.log('CLUB_POLISH_TABLETOPS_PASS')
