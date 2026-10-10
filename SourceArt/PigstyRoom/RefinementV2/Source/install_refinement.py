"""Import only pig art and tune the two saved animals; preserve the house."""
import json, os, re, unreal

ROOT = '/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-refine-v2'
SOURCE = ROOT + '/art'
DEST = '/Game/SeniorSendOff/House/PigstyRoom'
helper = os.path.join(unreal.Paths.project_content_dir(), 'Python/apply_bedroom.py')
code = open(helper).read().split('\ntextures={}')[0].replace("if name.startswith('SM'):", "if True:")
exec(compile(code, helper, 'exec'))
SOURCE = ROOT + '/art'
DEST = '/Game/SeniorSendOff/House/PigstyRoom'
RAW = json.load(open(SOURCE + '/incremental_manifest.json'))
if isinstance(RAW['materials'], list):
    PLAN = {'textures': [r['path'] for r in RAW['textures']],
            'flip_green_textures': [r['path'] for r in RAW['textures'] if r.get('flipGreenChannel')],
            'materials': {}, 'meshes': []}
    for row in RAW['materials']:
        info = {'color': row['baseColor'], 'roughness': row['roughness'], 'metallic': row.get('metallic', 0),
                'normal_strength': row.get('normalStrength', .4)}
        for channel, key in [('BaseColor', 'base_texture'), ('Roughness', 'roughness_texture'), ('Normal', 'normal_texture')]:
            if channel in row.get('textures', {}):
                info[key] = row['textures'][channel] + '.png'
        PLAN['materials'][row['name']] = info
    for row in RAW['meshes']:
        PLAN['meshes'].append(dict(name=row['name'], file=row['fbx'], boundsCm=row['boundsCm']))
else:
    PLAN = RAW
textures = {}
for file in PLAN['textures']:
    name = os.path.splitext(os.path.basename(file))[0]
    texture = imp(os.path.join(SOURCE, file), name)
    if 'normal' in file.lower():
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property('srgb', False)
        texture.set_editor_property('flip_green_channel', file in PLAN.get('flip_green_textures', []))
    elif 'roughness' in file.lower():
        texture.set_editor_property('srgb', False)
    A.save_loaded_asset(texture)
    textures[os.path.basename(file)] = texture
materials = {name: make_material(name, info, textures) for name, info in PLAN['materials'].items()}
assets = []
for row in PLAN['meshes']:
    name = row['name']
    if name not in ('SM_PIG_BlackBody', 'SM_PIG_BlackHead', 'SM_PIG_BlackUpperLeg', 'SM_PIG_BlackLowerLeg'):
        raise RuntimeError('Refinement unexpectedly imports a room asset: ' + name)
    mesh = imp(os.path.join(SOURCE, row['file']), name)
    for i, slot in enumerate(mesh.static_materials):
        key = re.sub(r'_\d{3}$', '', str(slot.material_slot_name).split('.')[0])
        material = materials.get(key) or A.load_asset(DEST + '/' + key)
        if not material:
            raise RuntimeError('Missing pig material: ' + key)
        mesh.set_material(i, material)
    A.save_loaded_asset(mesh)
    bounds = mesh.get_bounds()
    actual = [vec(bounds.origin - bounds.box_extent), vec(bounds.origin + bounds.box_extent)]
    expected = row.get('boundsCm')
    if expected:
        wanted = [expected['min'], expected['max']] if isinstance(expected, dict) else expected
        if max(abs(actual[b][i] - wanted[b][i]) for b in range(2) for i in range(3)) > .1:
            raise RuntimeError('Pig scale/pivot mismatch: ' + name)
    assets.append({'name': name, 'boundsCm': actual, 'slots': [str(s.material_slot_name) for s in mesh.static_materials]})

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors = {a.get_actor_label(): a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
def fullstate(actor):
    result = state(actor)
    if isinstance(actor, unreal.StaticMeshActor):
        comp = actor.static_mesh_component
        result.update(visible=comp.is_visible(), hidden=comp.get_editor_property('hidden_in_game'), castShadow=comp.get_editor_property('cast_shadow'))
    return result
before = {name: fullstate(actor) for name, actor in actors.items()}
settings = {'PIG_Pepper': dict(visual_scale=.62, max_health=85., health=85., walk_speed=155., run_speed=340., bed_yaw=0.),
            'PIG_Coal': dict(visual_scale=.82, max_health=160., health=160., walk_speed=130., run_speed=280., bed_yaw=0.)}
report = []
for label, tuning in settings.items():
    pig = actors[label]
    feet_z = 302.3
    position = pig.get_actor_location()
    for key, value in tuning.items():
        pig.set_editor_property(key, value)
    bed = [-2825., -956. if label == 'PIG_Pepper' else -886., 323.3]
    pig.set_editor_property('bed_location', unreal.Vector(*bed))
    pig.set_actor_location(unreal.Vector(position.x + .01, position.y, feet_z + 48. * tuning['visual_scale']), False, False)
    pig.set_actor_location(unreal.Vector(position.x, position.y, feet_z + 48. * tuning['visual_scale']), False, False)
    report.append({'name': label, 'settings': tuning, 'feetZ': feet_z, 'bed': bed})
food = actors['PIG_EdibleCheetos']
food.set_editor_property('refill_interval', 5.)
food.set_editor_property('food_count', 24)
after = {a.get_actor_label(): fullstate(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected = [n for n in before if n not in settings and n != 'PIG_EdibleCheetos']
changed = [n for n in protected if before[n] != after.get(n)]
if changed:
    raise RuntimeError('Unrelated house actors changed: ' + str(changed))
if len(before) != len(after):
    raise RuntimeError('Actor count changed unexpectedly')
registry = os.path.join(unreal.Paths.project_content_dir(), 'HouseReview/original_house_doors.json')
if open(registry, 'rb').read() != open(ROOT + '/backup/original_house_doors.json', 'rb').read():
    raise RuntimeError('Door registry changed')
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Map save failed')
json.dump({'assets': assets, 'pigs': report, 'protectedActors': len(protected), 'allProtectedUnchanged': True,
           'doorRegistryUnchanged': True, 'foodCapacity': 24, 'foodRefillSeconds': 5}, open(ROOT + '/installed.json', 'w'), indent=2)
unreal.log('PIG_REFINEMENT_INSTALL_PASSED pigs=2 protected=%d' % len(protected))
