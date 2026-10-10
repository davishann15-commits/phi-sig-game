"""Import the leaf loop only; no map edits or other sound changes."""
from pathlib import Path
import unreal

base = Path(__file__).resolve().parent
task = unreal.AssetImportTask()
task.filename = str(base/'assets/LeavesRustlingLoop.wav')
task.destination_path = '/Game/SeniorSendOff/House/ExteriorNature'
task.destination_name = 'S_LeavesRustlingLoop'
task.automated = True
task.replace_existing = True
task.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = task.get_editor_property('imported_object_paths')
assert len(paths) == 1, paths
sound = unreal.EditorAssetLibrary.load_asset(paths[0])
assert isinstance(sound, unreal.SoundWave)
sound.set_editor_property('looping', True)
sound.set_editor_property('compression_quality', 80)
sound.set_editor_property('virtualization_mode', unreal.VirtualizationMode.PLAY_WHEN_SILENT)
assert unreal.EditorAssetLibrary.save_loaded_asset(sound, False)
unreal.log('LEAF_WIND_AUDIO_SAVED '+sound.get_path_name())
(base/'evidence/audio-import.txt').write_text('PASS: Leaf rustle SoundWave imported, looping, saved. No map edits.\n')
