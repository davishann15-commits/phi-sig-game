"""Short ginger mullet on an isolated groom; keep the approved face binding."""
import unreal, json, traceback
from pathlib import Path
A=unreal.EditorAssetLibrary
ROOT='/Game/MetaHumans/Fixer/MH_Fixer'
BASE=ROOT+'/Details/Hair'
DEST=BASE+'/Mullet'
report={}
def duplicate(source,dest):
    result=A.load_asset(dest) if A.does_asset_exist(dest) else A.duplicate_asset(source,dest)
    assert result,source
    return result
try:
    hair=duplicate(BASE+'/Hair_Fixer_Fringe',DEST+'/Hair_Fixer_ShortMullet')
    cards=list(hair.get_editor_property('hair_groups_cards'))
    for card in cards:
        lod=card.get_editor_property('lod_index')
        old=card.get_editor_property('imported_mesh')
        mesh=duplicate(old.get_path_name().split('.')[0],DEST+'/SM_FixerMullet_'+str(lod))
        card.set_editor_property('imported_mesh',mesh)
    hair.set_editor_property('hair_groups_cards',cards)
    if A.get_metadata_tag(hair,'FixerShortMulletVersion')!='1':
        assert unreal.SeniorCharacterAssetTools.shape_fixer_short_mullet(hair)
        A.set_metadata_tag(hair,'FixerShortMulletVersion','1')
    for card in hair.get_editor_property('hair_groups_cards'):
        A.save_loaded_asset(card.get_editor_property('imported_mesh'))
    A.save_loaded_asset(hair)
    path=DEST+'/Hair_Fixer_ShortMullet_Binding'
    binding=A.load_asset(path) if A.does_asset_exist(path) else unreal.GroomLibrary.create_new_groom_binding_asset_with_path(
        path,hair,A.load_asset(ROOT+'/Face/SKM_MH_Fixer_FaceMesh'),100,
        A.load_asset('/Game/MetaHumans/BraxtonReview/MH_Braxton_Working/Face/SKM_MH_Braxton_Working_FaceMesh'),0)
    assert binding,'Mullet binding failed'
    A.save_loaded_asset(binding)
    report={'complete':True,'groom':hair.get_path_name(),'binding':binding.get_path_name()}
    unreal.log('FIXER_SHORT_MULLET_COMPLETE')
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error']);raise
finally:Path('/private/tmp/fixer_mullet_report.json').write_text(json.dumps(report,indent=2))
