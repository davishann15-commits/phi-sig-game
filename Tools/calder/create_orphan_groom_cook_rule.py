"""Exclude one proven unused, malformed legacy binding from packaged builds.

Creates only an owned editor-only PrimaryAssetLabel. Character assets and maps
are read-only. The current native character source has no reference to this
package, and the asset registry must still report zero referencers.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff')
PACKAGE='/Game/MetaHumans/Boywick/MH_Boywick/Grooms/Hair_M_Layered_Binding'
SOURCE=ROOT/'Content/MetaHumans/Boywick/MH_Boywick/Grooms/Hair_M_Layered_Binding.uasset'
FOLDER='/Game/SeniorSendOff/Calder/Build'
NAME='Calder_OrphanGroomCookRule'
inventory=json.loads((ROOT/'Saved/CalderWork/codex_cook_groom_inventory.json').read_text())
row=next(r for r in inventory if r['package']==PACKAGE)
assert row['target_skeletal_mesh']=='None' and row['target_geometry_cache']=='None'
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
assert not registry.get_referencers(PACKAGE,unreal.AssetRegistryDependencyOptions())
before=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
legacy=unreal.load_asset(PACKAGE)
assert legacy and legacy.get_editor_property('target_skeletal_mesh') is None
assert legacy.get_editor_property('target_geometry_cache') is None
assert not unreal.EditorAssetLibrary.does_asset_exist(FOLDER+'/'+NAME)
factory=unreal.DataAssetFactory()
factory.set_editor_property('data_asset_class',unreal.PrimaryAssetLabel)
label=unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME,FOLDER,unreal.PrimaryAssetLabel,factory)
assert label
rules=unreal.PrimaryAssetRules()
rules.set_editor_property('priority',100)
rules.set_editor_property('apply_recursively',False)
rules.set_editor_property('cook_rule',unreal.PrimaryAssetCookRule.NEVER_COOK)
label.set_editor_property('rules',rules)
label.set_editor_property('label_assets_in_my_directory',False)
label.set_editor_property('is_runtime_label',False)
label.set_editor_property('include_redirectors',False)
label.set_editor_property('explicit_assets',[legacy])
assert unreal.EditorAssetLibrary.save_loaded_asset(label,only_if_is_dirty=False)
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==before
receipt={'owned_label':label.get_path_name(),'never_cook_package':PACKAGE,
         'reason':'No skeletal/cache target and zero asset registry/native source references',
         'original_asset_sha256':before,'original_asset_unchanged':True,
         'recursive':False,'runtime_label':False,'character_assets_saved':0,'maps_saved':0}
(ROOT/'Saved/CalderWork/codex_orphan_groom_rule.json').write_text(json.dumps(receipt,indent=2))
unreal.log('CALDER_ORPHAN_GROOM_COOK_RULE '+json.dumps(receipt))
