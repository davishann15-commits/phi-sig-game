import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub';D='/Game/SeniorSendOff/House/NightClub';E=unreal.MaterialEditingLibrary;A=unreal.EditorAssetLibrary
try:
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 j=json.load(open(B/'assets/club_manifest.json'));done=[]
 for n,row in j['materials'].items():
  if 'Neon'not in n:continue
  m=A.load_asset(D+'/'+n);assert m,n;E.delete_all_material_expressions(m);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
  light=E.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);light.constant=unreal.LinearColor(*[v*2.0 for v in row['color']],1)
  inverse=E.create_material_expression(m,unreal.MaterialExpressionEyeAdaptationInverse);inputs=E.get_material_expression_input_names(inverse);iname=next(x for x in inputs if'light'in x.lower());assert E.connect_material_expressions(light,'',inverse,iname);assert E.connect_material_property(inverse,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  E.recompile_material(m);assert A.save_loaded_asset(m,False);done.append(n)
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();pp=[]
 for a in actors:
  if isinstance(a,unreal.PostProcessVolume):
   settings=a.get_editor_property('settings');keys=['auto_exposure_method','auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_bias','auto_exposure_bias_curve','override_auto_exposure_bias','override_auto_exposure_method']
   pp.append(dict(label=a.get_actor_label(),settings={k:str(settings.get_editor_property(k))for k in keys}))
  if a.get_actor_label().startswith('NightClub_Neon_')or a.get_actor_label().startswith('NightClub_Tube_'):
   for c in a.get_components_by_class(unreal.StaticMeshComponent):
    c.set_editor_property('affect_dynamic_indirect_lighting',False);c.set_editor_property('affect_distance_field_lighting',False)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 json.dump(dict(status='PASS',materials=done,existingExposure=pp),open(B/'evidence/glow.json','w'),indent=2);unreal.log('NIGHTCLUB_GLOW_PASS')
except:unreal.log_error('NIGHTCLUB_GLOW_FAIL '+traceback.format_exc())
