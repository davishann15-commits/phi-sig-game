"""New owned scanned furniture textures only; never overwrite an existing asset or save a map."""
import unreal,json,hashlib
from pathlib import Path
R=Path(unreal.Paths.project_dir()).resolve();W=R/'Saved/StadiumDetailWork';grant=json.loads((W/'integration_grant.json').read_text());assert grant['granted'] and grant['scope']=='full' and grant.get('release_evidence')
destination='/Game/SeniorSendOff/Calder/Stadium/Textures';tools=unreal.AssetToolsHelpers.get_asset_tools();result=[]
for folder,prefix,max_size in [('wood_table_worn','FurnitureWood',2048),('brown_leather','FurnitureLeather',1024)]:
 src=R/'SourceArt/StadiumDetail/Assets/PolyHaven'/folder;manifest=json.loads((src/'SOURCE.json').read_text())
 for entry in manifest['files']:
  path=src/entry['file'];assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256'];normal=entry['kind']=='nor_dx';name='T_Stadium_'+prefix+('_Normal' if normal else '_Color');asset_path=destination+'/'+name
  assert not unreal.EditorAssetLibrary.does_asset_exist(asset_path),'Refuse to overwrite '+asset_path
  task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=destination;task.destination_name=name;task.automated=True;task.save=False;task.replace_existing=False;tools.import_asset_tasks([task]);asset=unreal.load_asset(asset_path);assert isinstance(asset,unreal.Texture2D)
  asset.set_editor_property('srgb',not normal);asset.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_DEFAULT);asset.set_editor_property('max_texture_size',max_size);asset.set_editor_property('never_stream',False);assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
  result.append({'asset':asset_path,'normal_dx':normal,'source_sha256':entry['sha256']})
(W/'furniture_wear_import_receipt.json').write_text(json.dumps({'passed':len(result)==4,'new_textures':result,'map_saved':False,'materials_created':0},indent=2)+'\n');unreal.log('STADIUM_FURNITURE_WEAR_IMPORT PASS newTextures=4 mapSaved=0')
