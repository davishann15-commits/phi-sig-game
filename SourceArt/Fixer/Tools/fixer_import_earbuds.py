"""Import the fitted earbud rig, materials and new-face mullet binding."""
import unreal, json, traceback
from pathlib import Path
A=unreal.EditorAssetLibrary;T=unreal.AssetToolsHelpers.get_asset_tools();E=unreal.MaterialEditingLibrary
OLD='/Game/MetaHumans/Fixer/MH_Fixer'
ROOT='/Game/MetaHumans/FixerLean/MH_Fixer_Lean'
DEST=OLD+'/Details'
report={}
try:
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    materials={}
    for name,color,rough in [
        ('M_FixerEarbudWhite',(.80,.80,.77),.28),
        ('M_FixerEarbudWire',(.69,.70,.68),.51),
        ('M_FixerEarbudVent',(.035,.04,.043),.72)]:
        mat=A.load_asset(DEST+'/'+name) or T.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
        E.delete_all_material_expressions(mat)
        mat.set_editor_property('used_with_skeletal_mesh',True)
        c=E.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
        c.set_editor_property('constant',unreal.LinearColor(*color,1))
        E.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
        r=E.create_material_expression(mat,unreal.MaterialExpressionConstant);r.r=rough
        E.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
        E.recompile_material(mat);A.save_loaded_asset(mat);materials[name]=mat
    body=A.load_asset(ROOT+'/Body/SKM_MH_Fixer_Lean_BodyMesh');assert body
    opts=unreal.FbxImportUI()
    for key,val in dict(automated_import_should_detect_type=False,
        mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        original_import_type=unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        import_as_skeletal=True,import_mesh=True,import_materials=False,import_textures=False,
        import_animations=False,create_physics_asset=False,skeleton=body.get_editor_property('skeleton')).items():
        opts.set_editor_property(key,val)
    for key,val in dict(convert_scene=True,convert_scene_unit=False,force_front_x_axis=False,
        import_uniform_scale=1.,import_mesh_lods=False,update_skeleton_reference_pose=False,
        use_t0_as_ref_pose=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():
        opts.skeletal_mesh_import_data.set_editor_property(key,val)
    task=unreal.AssetImportTask();task.filename='/private/tmp/fixer_lean_details/SK_FixerWiredEarbuds.fbx'
    task.destination_path=DEST;task.destination_name='SK_FixerWiredEarbuds'
    task.automated=True;task.replace_existing=True;task.save=True;task.options=opts
    T.import_asset_tasks([task])
    mesh=A.load_asset(DEST+'/SK_FixerWiredEarbuds');assert mesh
    slots=list(mesh.get_editor_property('materials'))
    for i,slot in enumerate(slots):
        name=str(slot.material_slot_name);assert name in materials,name
        slot.material_interface=materials[name];slots[i]=slot
    mesh.set_editor_property('materials',slots)
    assert unreal.SeniorCharacterAssetTools.match_braxton_garment_bind_pose(mesh,body)
    b=mesh.get_bounds();assert 110<b.origin.z<160 and 15<b.box_extent.z<50,str(b)
    A.save_loaded_asset(mesh)
    hair=A.load_asset(DEST+'/Hair/Mullet/Hair_Fixer_ShortMulletV2');assert hair
    face=A.load_asset(ROOT+'/Face/SKM_MH_Fixer_Lean_FaceMesh');assert face
    source=A.load_asset('/Game/MetaHumans/BraxtonReview/MH_Braxton_Working/Face/SKM_MH_Braxton_Working_FaceMesh')
    path=DEST+'/Hair/Mullet/Hair_Fixer_ShortMulletV2_LeanBinding'
    if A.does_asset_exist(path):
        binding=A.load_asset(path)
        binding.set_editor_property('target_skeletal_mesh',face)
        binding.set_editor_property('source_skeletal_mesh',source)
        binding.set_editor_property('groom',hair)
    else:binding=unreal.GroomLibrary.create_new_groom_binding_asset_with_path(path,hair,face,100,source,0)
    assert binding;A.save_loaded_asset(binding)
    # The source's native eyebrow groom follows the new face, with ginger
    # shading kept independent of the other characters' materials.
    brow_count=0
    for path in A.list_assets(ROOT+'/Grooms',recursive=True):
        obj=A.load_asset(path)
        if not isinstance(obj,unreal.MaterialInstanceConstant):continue
        if 'eyebrow' not in obj.get_name().lower():continue
        names={str(n) for n in E.get_scalar_parameter_names(obj)}
        for key,val in {'hairMelanin':.47,'hairRedness':.88,'HairRoughness':.55,
            'OmbreMelanin':.52,'OmbreRedness':.88,'HighlightsMelanin':.43,
            'HighlightsRedness':.84,'RegionMelanin':.47,'RegionRedness':.88}.items():
            if key in names:E.set_material_instance_scalar_parameter_value(obj,key,val)
        E.update_material_instance(obj);A.save_loaded_asset(obj);brow_count+=1
    report={'complete':True,'earbuds':mesh.get_path_name(),'bounds':str(b),
        'hair_binding':binding.get_path_name(),'ginger_brow_materials':brow_count}
    unreal.log('FIXER_EARBUDS_AND_FACE_DETAILS_COMPLETE '+json.dumps(report))
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error']);raise
finally:Path('/private/tmp/fixer_earbuds_import_report.json').write_text(json.dumps(report,indent=2))
