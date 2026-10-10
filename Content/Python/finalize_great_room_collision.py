"""Ensure the refitted visual chunks use their dedicated furniture colliders."""
import unreal

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
for label in ('SMF_Entrance_m1_m1_Solid','SMF_Entrance_m1_p0_Solid',
              'SMF_Entrance_p0_m1_Solid','SMF_Entrance_p0_p0_Solid'):
    actor=actors.get(label)
    if not isinstance(actor,unreal.StaticMeshActor):
        raise RuntimeError('Missing refitted visual chunk '+label)
    actor.static_mesh_component.set_collision_profile_name('NoCollision')
proxy=actors.get('COL_GR_PhotoSectionalOttoman')
if not isinstance(proxy,unreal.StaticMeshActor):
    raise RuntimeError('Missing new furniture collider')
proxy.static_mesh_component.set_collision_profile_name('BlockAll')
proxy.static_mesh_component.set_visibility(False)
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Cannot save great-room collision settings')
