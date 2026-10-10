"""Reimport just the modeled sectional/ottoman without touching the house map."""
import os
import unreal

root = unreal.Paths.project_dir()
source = os.path.join(root, 'SourceArt/GreatRoom/SM_GR_PhotoSectionalOttoman.fbx')
dest = '/Game/SeniorSendOff/House/GreatRoom'
name = 'SM_GR_PhotoSectionalOttoman'
task = unreal.AssetImportTask()
task.filename = source
task.destination_path = dest
task.destination_name = name
task.automated = True
task.replace_existing = True
task.save = True
ui = unreal.FbxImportUI()
ui.import_mesh = True
ui.import_as_skeletal = False
ui.import_materials = False
ui.import_textures = False
task.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(dest + '/' + name)
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError('Furniture reimport failed')
materials = ['M_GR_ChocolateLeather', 'M_GR_WalnutWood',
             'M_GR_IvoryPillow', 'M_GR_OliveLeather']
if len(mesh.static_materials) != len(materials):
    raise RuntimeError('Furniture material slots changed')
for index, material_name in enumerate(materials):
    material = unreal.load_asset(dest + '/' + material_name)
    if not material:
        raise RuntimeError('Missing ' + material_name)
    mesh.set_material(index, material)
if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
    raise RuntimeError('Could not save updated sectional')
unreal.log('GREAT_ROOM_FURNITURE_UPDATED ' + str(mesh.get_bounds()))
