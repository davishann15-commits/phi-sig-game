"""Independent softer fringe bound to the Fixer's own face geometry."""
import unreal,json,traceback
from pathlib import Path
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
ROOT='/Game/MetaHumans/Fixer/MH_Fixer'
SRC='/Game/MetaHumans/BraxtonReview/MH_Braxton_Working'
DEST=ROOT+'/Details/Hair'
report={}
try:
    path=DEST+'/Hair_Fixer_Fringe'
    hair=A.load_asset(path) if A.does_asset_exist(path) else A.duplicate_asset(SRC+'/Grooms/Hair_S_SideSweptFringe',path)
    assert hair
    slots=list(hair.get_editor_property('hair_groups_materials'))
    for i,slot in enumerate(slots):
        original=slot.get_editor_property('material');dst=DEST+'/MI_Fixer_Fringe_'+str(i)
        mat=A.load_asset(dst) if A.does_asset_exist(dst) else A.duplicate_asset(original.get_path_name().split('.')[0],dst)
        params={str(x) for x in E.get_scalar_parameter_names(mat)}
        for k,v in {'hairMelanin':.52,'hairRedness':.90,'RegionMelanin':.52,'RegionRedness':.90,
                    'OmbreMelanin':.58,'OmbreRedness':.85,'HighlightsMelanin':.45,'HighlightsRedness':.85,
                    'RedVariation':.08,'HairRoughness':.58,'LightAmount':0.,'WhiteAmount':0.}.items():
            if k in params:E.set_material_instance_scalar_parameter_value(mat,k,v)
        E.update_material_instance(mat);A.save_loaded_asset(mat)
        slot.set_editor_property('material',mat)
    hair.set_editor_property('hair_groups_materials',slots)
    # The source's optimized-low assembly retains only LOD3 cards. Restore
    # authored card levels so Mac/mobile can render a complete close-up.
    raw=A.load_asset('/MetaHumanCharacter/Optional/Grooms/GroomAssets/Hair/Hair_S_SideSweptFringe/Hair_S_SideSweptFringe')
    cards=list(raw.get_editor_property('hair_groups_cards'))
    for card in cards:
        lod=card.get_editor_property('lod_index')
        conformed=SRC+'/Grooms/Hair_S_SideSweptFringe_CardsMesh_Group0_LOD'+str(lod)
        original=A.load_asset(conformed) if A.does_asset_exist(conformed) else card.get_editor_property('imported_mesh')
        dest=DEST+'/Cards/SM_FixerFringe_'+str(lod)
        mesh=A.load_asset(dest) if A.does_asset_exist(dest) else A.duplicate_asset(original.get_path_name().split('.')[0],dest)
        card.set_editor_property('imported_mesh',mesh);A.save_loaded_asset(mesh)
        textures=card.get_editor_property('textures');local=[]
        for i,tex in enumerate(textures.get_editor_property('textures')):
            if tex:
                tp=DEST+'/Cards/T_FixerFringe_'+str(lod)+'_'+str(i)
                tex=A.load_asset(tp) if A.does_asset_exist(tp) else A.duplicate_asset(tex.get_path_name().split('.')[0],tp)
                A.save_loaded_asset(tex)
            local.append(tex)
        textures.set_editor_property('textures',local);card.set_editor_property('textures',textures)
    hair.set_editor_property('hair_groups_cards',cards)
    A.save_loaded_asset(hair)
    bp=DEST+'/Hair_Fixer_Fringe_BindingV2'
    binding=A.load_asset(bp) if A.does_asset_exist(bp) else unreal.GroomLibrary.create_new_groom_binding_asset_with_path(
        bp,hair,A.load_asset(ROOT+'/Face/SKM_MH_Fixer_FaceMesh'),100,A.load_asset(SRC+'/Face/SKM_MH_Braxton_Working_FaceMesh'),0)
    assert binding,'Fixer groom binding failed'
    A.save_loaded_asset(binding)
    report={'complete':True,'groom':hair.get_path_name(),'binding':binding.get_path_name()}
    unreal.log('FIXER_FRINGE_COMPLETE')
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error']);raise
finally:Path('/private/tmp/fixer_fringe_report.json').write_text(json.dumps(report,indent=2))
