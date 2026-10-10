import unreal,json,traceback
B='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ExitSigns/NeonV2/'
D='/Game/SeniorSendOff/House/ExitSigns';ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
try:
 colors=json.load(open(B+'materials.json'));mats={}
 for n,c in colors.items():
  name='NEX_'+n;m=unreal.load_asset(D+'/'+name) or at.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew());ml.delete_all_material_expressions(m)
  node=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);node.constant=unreal.LinearColor(*c,1)
  if n in ['RedLens','Status']:
   m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
   node.constant=unreal.LinearColor(1.8,0,0,1) if n=='RedLens' else unreal.LinearColor(0,.35,0,1)
   exposure=ml.create_material_expression(m,unreal.MaterialExpressionEyeAdaptationInverse)
   light_input=next(x for x in ml.get_material_expression_input_names(exposure) if 'light' in x.lower())
   assert ml.connect_material_expressions(node,'',exposure,light_input)
   assert ml.connect_material_property(exposure,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  else:
   assert ml.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
   for val,prop in [(.3 if n in ['Screw','Housing','Face'] else .58,unreal.MaterialProperty.MP_ROUGHNESS),(.55 if n in ['Screw','Housing'] else .15,unreal.MaterialProperty.MP_METALLIC)]:
    x=ml.create_material_expression(m,unreal.MaterialExpressionConstant);x.r=val;ml.connect_material_property(x,'',prop)
  m.set_editor_property('two_sided',True);ml.recompile_material(m);ed.save_loaded_asset(m);mats[name]=m
 w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 vol=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()=='Exit lens bloom')
 settings=vol.get_editor_property('settings');settings.set_editor_property('bloom_intensity',1.2);settings.set_editor_property('bloom_threshold',.05);vol.set_editor_property('settings',settings)
 assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Story/Maps/Chapter01_House')
 unreal.log('NEON_MATERIAL_PASS')
except:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
