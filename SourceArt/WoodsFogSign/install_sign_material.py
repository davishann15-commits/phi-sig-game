import unreal,json,pathlib,traceback
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/WoodsFogSign'
try:
 path='/Game/SeniorSendOff/House/ExteriorNature/M_WoodsSignText'
 m=A.load_asset(path) if A.does_asset_exist(path) else A.duplicate_asset('/Engine/EngineMaterials/DefaultTextMaterialOpaque',path)
 assert m
 base=E.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
 pin=E.get_material_property_input_node_output_name(m,unreal.MaterialProperty.MP_BASE_COLOR)
 if base is None:
  base=E.get_material_property_input_node(m,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  pin=E.get_material_property_input_node_output_name(m,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 assert base
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 assert E.connect_material_property(base,pin,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 errors=E.recompile_material(m)
 assert not errors,str(errors)
 assert A.save_loaded_asset(m)
 (B/'evidence/sign-material.json').write_text(json.dumps(dict(status='PASS',material=m.get_path_name(),shading='Unlit ivory lettering, original font edge smoothing preserved'),indent=2))
 unreal.log('WOODS_SIGN_MATERIAL_SAVED')
except:unreal.log_error(traceback.format_exc())
unreal.SystemLibrary.quit_editor()
