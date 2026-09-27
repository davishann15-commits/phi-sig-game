"""Render an isolated photo-review turntable of the assembled Runner in Unreal."""
import time
import traceback
from pathlib import Path
import unreal

ROOT = '/Game/MetaHumans/RunnerRebuild/MH_Runner_Working'
OUT = Path('/Users/Stewart/Documents/Codex/2026-09-11/c/work/runner')
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
kind = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + '/BP_MH_Runner_Working')
if not kind:
    raise RuntimeError('Runner assembled Blueprint not found')
actor = actors.spawn_actor_from_class(kind, unreal.Vector())
body = None
wardrobe = None
for component in actor.get_components_by_class(unreal.SkeletalMeshComponent):
    mesh = component.get_skeletal_mesh_asset()
    if component.get_name() == 'Body':
        body = component
    if mesh and 'Outfits' in mesh.get_name():
        wardrobe = component
if not body or not wardrobe:
    raise RuntimeError('Runner Blueprint has no body/wardrobe')
replacement = unreal.load_asset(ROOT + '/Details/RunnerOutfit/SK_Runner_MetaOutfit')
if not replacement:
    raise RuntimeError('Runner custom wardrobe missing')
wardrobe.set_skeletal_mesh_asset(replacement)
unreal.log('RUNNER_PREVIEW_OUTFIT body=' + str(body.get_skeletal_mesh_asset().get_name())
           + ' outfit=' + replacement.get_name())
lod = actor.get_component_by_class(unreal.LODSyncComponent)
if lod:
    lod.set_editor_property('forced_lod', 0)

for loc, intensity, color, size in [
    ((70,130,225),7000,(1,.94,.87),70),
    ((-110,80,170),2800,(.84,.91,1),95),
    ((40,-100,220),6500,(1,.97,.9),55),
]:
    light = actors.spawn_actor_from_class(unreal.RectLight, unreal.Vector(*loc))
    light.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(
        light.get_actor_location(),unreal.Vector(0,0,130)),False)
    component = light.get_component_by_class(unreal.RectLightComponent)
    component.set_intensity(intensity)
    component.set_light_color(unreal.LinearColor(*color,1))
    component.set_source_width(size)
    component.set_source_height(size)
    component.set_attenuation_radius(800)

camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0,320,105))
cam = camera.get_component_by_class(unreal.CameraComponent)
cam.set_field_of_view(38)
cam.set_editor_property('aspect_ratio', .8)
pp = cam.get_editor_property('post_process_settings')
pp.set_editor_property('override_auto_exposure_method', True)
pp.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
pp.set_editor_property('override_auto_exposure_apply_physical_camera_exposure', True)
pp.set_editor_property('auto_exposure_apply_physical_camera_exposure', False)
pp.set_editor_property('override_auto_exposure_bias', True)
pp.set_editor_property('auto_exposure_bias', -9.3)
pp.set_editor_property('override_bloom_intensity', True)
pp.set_editor_property('bloom_intensity', 0)
cam.set_editor_property('post_process_settings', pp)
def position(loc, aim, fov):
    camera.set_actor_location(unreal.Vector(*loc), False, False)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(
        unreal.Vector(*loc), unreal.Vector(*aim)),False)
    cam.set_field_of_view(fov)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        camera.get_actor_location(),camera.get_actor_rotation())

position((0,320,105),(0,0,95),38)
started = time.monotonic()
state = 0
task = None
mark = 0
callback = None
def tick(_):
    global state, task, mark
    try:
        elapsed = time.monotonic() - started
        if state == 0 and elapsed > 22:
            task = unreal.AutomationLibrary.take_high_res_screenshot(
                1200,1500,str(OUT/'RunnerMeta_Full.png'),camera)
            state = 1
        elif state == 1 and task.is_task_done():
            position((0,112,165),(0,0,162),28)
            mark = elapsed
            state = 2
        elif state == 2 and elapsed > mark + 6:
            task = unreal.AutomationLibrary.take_high_res_screenshot(
                1200,1500,str(OUT/'RunnerMeta_Face.png'),camera)
            state = 3
        elif state == 3 and task.is_task_done():
            unreal.log('RUNNER_META_REVIEW_COMPLETE')
            unreal.unregister_slate_post_tick_callback(callback)
            unreal.SystemLibrary.quit_editor()
        elif elapsed > 150:
            raise RuntimeError('Runner visual review timeout')
    except Exception:
        unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(callback)
        unreal.SystemLibrary.quit_editor()

callback = unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
