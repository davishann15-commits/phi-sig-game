"""Read-only editor/PIE census of great-room blockers."""
import unreal, json
from pathlib import Path

BASE = Path(__file__).resolve().parent
BASE.mkdir(parents=True, exist_ok=True)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
worlds = []
for method in ('get_editor_world', 'get_game_world'):
    try:
        world = getattr(sub, method)()
        if world and world not in worlds:
            worlds.append(world)
    except Exception as error:
        unreal.log('GREAT_ROOM_AUDIT_WORLD ' + str(error))

def xyz(v):
    return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]

def overlap(o, e):
    return (o.x+e.x > -1450 and o.x-e.x < 1450 and
            o.y+e.y > -700 and o.y-e.y < 800 and
            o.z+e.z > 15 and o.z-e.z < 500)

result = []
for world in worlds:
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    rows = []
    for actor in actors:
        components = []
        for c in actor.get_components_by_class(unreal.PrimitiveComponent):
            ao, ae = actor.get_actor_bounds(False)
            if not overlap(ao, ae):
                continue
            if c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
                continue
            row = {'name': c.get_name(), 'class': c.get_class().get_name(),
                   'profile': str(c.get_collision_profile_name()),
                   'pawn_response': str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)),
                   'visible': c.is_visible(), 'hidden_in_game': c.get_editor_property('hidden_in_game')}
            if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
                mesh = c.static_mesh
                b = mesh.get_bounds()
                row.update(mesh=mesh.get_path_name(), mesh_origin=xyz(b.origin), mesh_extent=xyz(b.box_extent))
                body = mesh.get_editor_property('body_setup')
                if body:
                    row['trace_flag'] = str(body.get_editor_property('collision_trace_flag'))
                row['location'] = xyz(c.get_world_location())
                row['scale'] = xyz(c.get_world_scale())
            components.append(row)
        if components:
            o, e = actor.get_actor_bounds(False)
            rows.append({'label': actor.get_actor_label(), 'name': actor.get_name(),
                         'class': actor.get_class().get_name(), 'origin': xyz(o), 'extent': xyz(e),
                         'folder': str(actor.get_folder_path()), 'components': components})
    result.append({'world': world.get_name(), 'actors': rows})

(BASE/'audit.json').write_text(json.dumps(result, indent=2))
(BASE/'trace-api.txt').write_text(str(unreal.SystemLibrary.capsule_trace_multi_for_objects.__doc__))
unreal.log('GREAT_ROOM_AUDIT_SAVED worlds=' + str(len(result)))
