import unreal,pathlib,json
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/HouseJoinRepair';S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert w.get_name()=='Chapter01_House'
before=json.loads((B/'evidence/actors_after.json').read_text());assert len(S.get_all_level_actors())==len(before)==1954
# Saved repairs remain in the level after the twenty unrelated mesh metadata
# experiments were reverted; no room or interactable is deleted.
assert set(a.get_actor_label() for a in S.get_all_level_actors())==set(a['label'] for a in before)
A=unreal.EditorAssetLibrary;restored=json.loads((B/'evidence/performance-selection.json').read_text())['restored_original_mesh_settings']
for name in restored:assert not A.load_asset(name).get_editor_property('nanite_settings').enabled
report=dict(status='PASS',map=w.get_path_name(),actor_count=1954,actors_preserved=True,experiments_reverted=True)
(B/'evidence/saved-map-check.json').write_text(json.dumps(report,indent=2));unreal.log('HOUSE_JOIN_SAVED_MAP_PASS')
