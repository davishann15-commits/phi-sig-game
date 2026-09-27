"""A shorter tapered-nape ginger mullet, independently authored from fringe."""
import unreal
A=unreal.EditorAssetLibrary
BASE='/Game/MetaHumans/Fixer/MH_Fixer/Details/Hair'
DEST=BASE+'/Mullet'
def duplicate(src,dst):
    return A.load_asset(dst) if A.does_asset_exist(dst) else A.duplicate_asset(src,dst)
hair=duplicate(BASE+'/Hair_Fixer_Fringe',DEST+'/Hair_Fixer_ShortMulletV2');assert hair
cards=list(hair.get_editor_property('hair_groups_cards'))
for card in cards:
    old=card.get_editor_property('imported_mesh');lod=card.get_editor_property('lod_index')
    mesh=duplicate(old.get_path_name().split('.')[0],DEST+'/SM_FixerMulletV2_'+str(lod));assert mesh
    card.set_editor_property('imported_mesh',mesh)
hair.set_editor_property('hair_groups_cards',cards)
if A.get_metadata_tag(hair,'FixerShortMulletVersion')!='2':
    assert unreal.SeniorCharacterAssetTools.shape_fixer_short_mullet(hair)
    A.set_metadata_tag(hair,'FixerShortMulletVersion','2')
for card in cards:A.save_loaded_asset(card.get_editor_property('imported_mesh'))
A.save_loaded_asset(hair)
unreal.log('FIXER_SHORT_TAPERED_MULLET_READY')
