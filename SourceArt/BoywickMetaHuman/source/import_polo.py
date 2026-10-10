"""Import the skinned polo outfit and author its own saved fabric materials."""
import json
import traceback
from pathlib import Path
import unreal

TASK = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BoywickMetaHuman')
ROOT = '/Game/MetaHumans/Boywick/MH_Boywick'
DEST = ROOT + '/Details'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
report = {}
try:
    body = assets.load_asset(ROOT + '/Body/SKM_MH_Boywick_BodyMesh')
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    options = unreal.FbxImportUI()
    for key,value in dict(automated_import_should_detect_type=False,
        mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        original_import_type=unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        import_as_skeletal=True,import_mesh=True,import_materials=False,
        import_textures=False,import_animations=False,create_physics_asset=False,
        skeleton=body.get_editor_property('skeleton')).items():
        options.set_editor_property(key,value)
    for key,value in dict(convert_scene=True,force_front_x_axis=False,
        convert_scene_unit=False,import_uniform_scale=1.0,import_mesh_lods=False,
        update_skeleton_reference_pose=False,use_t0_as_ref_pose=False,
        normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():
        options.skeletal_mesh_import_data.set_editor_property(key,value)
    task = unreal.AssetImportTask()
    task.filename = str(TASK/'SK_Boywick_PoloOutfit.fbx')
    task.destination_path = DEST
    task.destination_name = 'SK_Boywick_PoloOutfit'
    task.automated = True;task.replace_existing = True;task.save = True;task.options = options
    tools.import_asset_tasks([task])
    mesh = assets.load_asset(DEST+'/SK_Boywick_PoloOutfit')
    if not mesh:raise RuntimeError('Skinned polo import failed')
    if not unreal.SeniorCharacterAssetTools.match_braxton_garment_bind_pose(mesh,body):
        raise RuntimeError('Polo skeleton does not match the fitted native body')
    def import_texture(file,name,normal=False):
        task=unreal.AssetImportTask()
        task.filename=str(TASK/file);task.destination_path=DEST
        task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True
        tools.import_asset_tasks([task])
        tex=assets.load_asset(DEST+'/'+name)
        tex.set_editor_property('srgb',not normal)
        tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP if normal else unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
        if normal:tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
        assets.save_loaded_asset(tex)
        return tex
    pattern=import_texture('PoloPattern.png','T_Boywick_PoloPattern')
    weave=import_texture('FabricNormal.png','T_Boywick_FabricNormal',True)
    def fabric(name,color,roughness,patterned=False,metallic=0.0):
        mat=assets.load_asset(DEST+'/'+name) or tools.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
        lib.delete_all_material_expressions(mat)
        mat.set_editor_property('used_with_skeletal_mesh',True)
        mat.set_editor_property('two_sided',True)
        if patterned:
            sample=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
            sample.texture=pattern
            uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
            uv.u_tiling=9.0;uv.v_tiling=9.0
            lib.connect_material_expressions(uv,'',sample,'UVs')
            lib.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
        else:
            rgb=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
            rgb.constant=unreal.LinearColor(*color,1)
            lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
        if roughness>.65:
            normal=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
            normal.texture=weave;normal.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
            uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
            uv.u_tiling=70.0;uv.v_tiling=70.0
            lib.connect_material_expressions(uv,'',normal,'UVs')
            lib.connect_material_property(normal,'RGB',unreal.MaterialProperty.MP_NORMAL)
        for value,prop in [(roughness,unreal.MaterialProperty.MP_ROUGHNESS),(metallic,unreal.MaterialProperty.MP_METALLIC)]:
            node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);node.r=value
            lib.connect_material_property(node,'',prop)
        lib.recompile_material(mat);assets.save_loaded_asset(mat)
        return mat
    materials = {
        'BW_Polo':fabric('M_Boywick_Polo',(.03,.04,.08),.80,True),
        'BW_Khaki':fabric('M_Boywick_Khaki',(.55,.43,.29),.86),
        'BW_Collar':fabric('M_Boywick_Collar',(.016,.027,.052),.79),
        'BW_PinkPlacket':fabric('M_Boywick_PinkPlacket',(.73,.46,.49),.82),
        'BW_Buttons':fabric('M_Boywick_PearlButtons',(.78,.79,.72),.32),
        'BW_SneakerSole':fabric('M_Boywick_SneakerSole',(.68,.67,.58),.73),
        'BW_SneakerUpper':fabric('M_Boywick_SneakerUpper',(.055,.071,.092),.84),
        'BW_Laces':fabric('M_Boywick_Laces',(.69,.70,.66),.82)}
    slots=mesh.get_editor_property('materials')
    for i,slot in enumerate(slots):
        name=str(slot.material_slot_name)
        if name not in materials:raise RuntimeError('Unexpected outfit material '+name)
        slot.material_interface=materials[name];slots[i]=slot
    mesh.set_editor_property('materials',slots)
    assets.save_loaded_asset(mesh)
    groom=assets.load_asset(ROOT+'/Grooms/Hair_M_SideSweptFringe')
    if assets.get_metadata_tag(groom,'BoywickScoopVersion')!='1':
        if not unreal.SeniorCharacterAssetTools.shape_boywick_scoop_hair(groom):
            raise RuntimeError('Cannot shape the private scoop hairstyle')
        assets.set_metadata_tag(groom,'BoywickScoopVersion','1')
        assets.save_directory('/Game/MetaHumans/Boywick',only_if_is_dirty=True,recursive=True)
    # Save blond styling in the assembly too, so the editable BP preview agrees
    # with gameplay. Each material belongs exclusively to this character.
    for path in assets.list_assets(ROOT+'/Grooms',recursive=True):
        mat=assets.load_asset(path)
        if not isinstance(mat,unreal.MaterialInstanceConstant) or not any(x in str(path)for x in ('Hair_','Eyebrows_')):continue
        for key,value in [('hairMelanin',.24),('hairRedness',.18),('HairRoughness',.55),
            ('OmbreMelanin',.26),('OmbreRedness',.18),('HighlightsMelanin',.20),
            ('HighlightsRedness',.15),('RegionMelanin',.24),('RegionRedness',.18)]:
            lib.set_material_instance_scalar_parameter_value(mat,key,value)
        assets.save_loaded_asset(mat)
    report={'status':'PASS','outfit':mesh.get_path_name(),'slots':[str(x.material_slot_name)for x in slots], 'bounds':str(mesh.get_bounds())}
    unreal.log('BOYWICK_POLO_IMPORTED')
except Exception:
    report={'status':'FAIL','error':traceback.format_exc()};unreal.log_error(report['error'])
finally:
    (TASK/'evidence/import.json').write_text(json.dumps(report,indent=2))
    frames=[0]
    def finish(dt):
        frames[0]+=1
        if frames[0]>8:
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    handle=unreal.register_slate_post_tick_callback(finish)
