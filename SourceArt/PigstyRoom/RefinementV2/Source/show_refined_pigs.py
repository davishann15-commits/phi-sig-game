import json, unreal
ROOT = '/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-refine-v2'
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
camera = unreal.Vector(-2420, -1280, 451)
aim = unreal.Vector(-2760, -990, 367)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, aim))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
actors = unreal.EditorLevelLibrary.get_all_level_actors()
pigs = [a for a in actors if isinstance(a, unreal.HouseRoamingPig)]
dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
info = {'map': 'Chapter01_House', 'pigs': [], 'dirtyMaps': [p.get_path_name() for p in dirty],
        'camera': [camera.x, camera.y, camera.z]}
for pig in pigs:
    info['pigs'].append({key: pig.get_editor_property(key) for key in
                         ['pig_name', 'visual_scale', 'max_health', 'walk_speed', 'run_speed']})
json.dump(info, open(ROOT + '/editor-ready.json', 'w'), indent=2)
unreal.log('PIG_REFINEMENT_EDITOR_READY saved=%d pigs=%d' % (not dirty, len(pigs)))
