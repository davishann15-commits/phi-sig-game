"""Enable the existing twelve-triangle mattress box as physical collision."""
import unreal, json
ROOT = '/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-refine-v2'
mesh = unreal.EditorAssetLibrary.load_asset('/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_Mattress_COL')
assert mesh
body = mesh.get_editor_property('body_setup')
before = str(body.get_editor_property('collision_trace_flag'))
body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
after = str(body.get_editor_property('collision_trace_flag'))
assert body.get_editor_property('collision_trace_flag') == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
json.dump({'asset': mesh.get_path_name(), 'beforeCollisionTrace': before, 'afterCollisionTrace': after,
           'mapActorsUnchanged': True, 'method': 'existing twelve-triangle hidden mattress box as collision'},
          open(ROOT + '/mattress-collision.json', 'w'), indent=2)
unreal.log('PIG_MATTRESS_COLLISION_REPAIRED existingBox=1')
