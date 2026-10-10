import unreal,json,re,hashlib
from pathlib import Path
ROOT=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-animal-v4')
DEST='/Game/SeniorSendOff/House/PigstyRoom'
A=unreal.EditorAssetLibrary;T=unreal.AssetToolsHelpers.get_asset_tools()
plan=json.loads((ROOT/'art/ear_manifest.json').read_text());report=[]
for row in plan['meshes']:
 task=unreal.AssetImportTask();task.filename=row['file'];task.destination_path=DEST;task.destination_name=row['name'];task.automated=True;task.replace_existing=True;task.save=True
 ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False;ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;task.options=ui
 T.import_asset_tasks([task]);mesh=A.load_asset(DEST+'/'+row['name']);assert mesh,row['name']
 for i,slot in enumerate(mesh.static_materials):
  key=re.sub(r'_\d{3}$','',str(slot.material_slot_name).split('.')[0]);mat=A.load_asset(DEST+'/'+key);assert mat,key;mesh.set_material(i,mat)
 b=mesh.get_bounds();lo=b.origin-b.box_extent;hi=b.origin+b.box_extent;actual=[[lo.x,lo.y,lo.z],[hi.x,hi.y,hi.z]]
 assert max(abs(actual[k][i]-row['boundsCm'][('min','max')[k]][i])for k in range(2)for i in range(3))<.12,(row['name'],actual)
 A.save_loaded_asset(mesh);report.append(dict(name=row['name'],boundsCm=actual,materialSlots=[str(s.material_slot_name)for s in mesh.static_materials]))
task=unreal.AssetImportTask();task.filename=str(ROOT/'audio/PIG_Snort.wav');task.destination_path=DEST;task.destination_name='PIG_Snort';task.automated=True;task.replace_existing=True;task.save=True;T.import_asset_tasks([task])
sound=A.load_asset(DEST+'/PIG_Snort');assert sound;assert sound.get_editor_property('duration')>.1;A.save_loaded_asset(sound)
(ROOT/'installed.json').write_text(json.dumps(dict(meshes=report,soundDuration=sound.get_editor_property('duration'),soundPath=sound.get_path_name()),indent=2))
unreal.log('PIG_ANIMAL_ASSET_INSTALL_PASSED meshes=3 snort=1')
