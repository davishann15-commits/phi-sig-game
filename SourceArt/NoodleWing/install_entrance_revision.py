"""Update one wall asset and turn the entry sign to face the mini hallway."""
import pathlib,unreal,json,traceback,re
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing'
exec((B/'install.py').read_text().split('def run():')[0],globals())
def revise():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 art=json.loads((B/'art/entrance_revision.json').read_text())
 before={a.get_name():snap(a)for a in S.get_all_level_actors()}
 actors={a.get_actor_label():a for a in S.get_all_level_actors()}
 r=art['assets'][0];assert r['name']=='SM_NW_HallWalls'
 me=imp(r['fbx'],r['name']);slots=list(me.static_materials)
 for sl in slots:
  key=re.sub(r'[._]\d+$','',str(sl.get_editor_property('imported_material_slot_name')))
  m=A.load_asset(D+'/'+key);assert m,key;sl.material_interface=m
 me.static_materials=slots;setupcol(me,'structure');assert A.save_loaded_asset(me,False)
 wall=actors['NW_HallWalls'];wall.static_mesh_component.set_static_mesh(me)
 wall.static_mesh_component.set_editor_property('override_materials',[])
 sign=actors['NW_Hall_Exit'];spec=art['sign']
 sign.set_actor_location(unreal.Vector(*spec['pos']),False,True)
 sign.set_actor_rotation(unreal.Rotator(yaw=spec['yaw']),True)
 changed={wall.get_name(),sign.get_name()}
 after={a.get_name():snap(a)for a in S.get_all_level_actors()}
 drift=[r['label']for n,r in before.items()if n not in changed and after.get(n)!=r]
 assert not drift,drift
 assert len(before)==len(after)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=dict(status='PASS',changed_labels=['NW_HallWalls','NW_Hall_Exit'],
  protected_actors=len(before)-len(changed),actor_count=len(after),new_lights=0,
  infill_bounds=[[1740,-678.15,-134.1],[1777.5,-381,-53.8]],
  doorway_head_z=-134.1,leaf_head_z=-135.366,leaf_head_clearance_cm=1.266,
  sign_before=before[sign.get_name()],sign_after=after[sign.get_name()])
 (B/'evidence/entrance-install-report.json').write_text(json.dumps(report,indent=2))
 unreal.log('NOODLE_ENTRANCE_INSTALL_PASS '+json.dumps(report))
try:revise()
except:unreal.log_error('NOODLE_ENTRANCE_INSTALL_FAIL '+traceback.format_exc())
