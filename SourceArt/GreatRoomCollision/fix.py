"""Replace the upper structural tile's enclosing hull with fitted triangle collision."""
import unreal, json
from pathlib import Path

base = Path(__file__).resolve().parent
mesh = unreal.load_asset('/Game/SeniorSendOff/House/Meshes/SM_Upper_p0_m1_Solid')
if not mesh:
    raise RuntimeError('Great-room structural mesh not found')
body = mesh.get_editor_property('body_setup')
before = str(body.get_editor_property('collision_trace_flag'))
body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, False):
    raise RuntimeError('Could not save fitted structural collision')
(base/'fix-result.json').write_text(json.dumps({'mesh': mesh.get_path_name(), 'before': before,
    'after': str(body.get_editor_property('collision_trace_flag'))}, indent=2))
unreal.log('GREAT_ROOM_FITTED_COLLISION_SAVED')
