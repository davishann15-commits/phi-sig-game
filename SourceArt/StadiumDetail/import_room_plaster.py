"""Unreal Editor Python, only after complete shared-project release; imports owned new textures, never a map."""
import unreal,json,hashlib
from pathlib import Path
R=Path(unreal.Paths.project_dir()).resolve();W=R/'Saved/StadiumDetailWork'
grant=json.loads((W/'integration_grant.json').read_text())
assert grant['granted'] and grant['scope']=='full' and grant.get('release_evidence'),'No full stadium asset/source reservation'
src=R/'SourceArt/StadiumDetail/Assets/PolyHaven/painted_plaster_wall';manifest=json.loads((src/'SOURCE.json').read_text())
destination='/Game/SeniorSendOff/Calder/Stadium/Textures';tools=unreal.AssetToolsHelpers.get_asset_tools();result=[]
for entry in manifest['files']:
 path=src/entry['file'];assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
 normal=entry['kind']=='nor_dx';name='T_Stadium_RoomPlaster_Normal' if normal else 'T_Stadium_RoomPlaster_Color';asset_path=destination+'/'+name
 assert not unreal.EditorAssetLibrary.does_asset_exist(asset_path),'Unexpected existing texture; inspect instead of overwriting '+asset_path
 task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=destination;task.destination_name=name;task.automated=True;task.save=False;task.replace_existing=False;tools.import_asset_tasks([task]);asset=unreal.load_asset(asset_path);assert isinstance(asset,unreal.Texture2D)
 asset.set_editor_property('srgb',not normal);asset.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_DEFAULT);asset.set_editor_property('max_texture_size',2048);asset.set_editor_property('never_stream',False)
 assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
 result.append({'asset':asset_path,'normal_dx':normal,'source_sha256':entry['sha256']})
(W/'room_plaster_import_receipt.json').write_text(json.dumps({'passed':len(result)==2,'new_textures':result,'map_saved':False,'materials_created':0},indent=2)+'\n')
unreal.log('STADIUM_ROOM_PLASTER_IMPORT PASS newTextures=2 newMaterials=0 mapSaved=0')
