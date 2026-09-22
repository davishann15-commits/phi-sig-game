import unreal,time,traceback
OUT='/Users/Stewart/Documents/Codex/2026-09-11/c/work'
assets=unreal.EditorAssetLibrary
paths=assets.list_assets('/Game/MetaHumans/BraxtonReview',recursive=True)
blueprints=[]
for path in paths:
    if '/BP_' in path:
        obj=assets.load_asset(path)
        if isinstance(obj,unreal.Blueprint): blueprints.append(path)
if len(blueprints)!=1: raise RuntimeError('Expected one assembled character Blueprint: '+str(blueprints))
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
character=actors.spawn_actor_from_class(assets.load_blueprint_class(blueprints[0]),unreal.Vector(0,0,0))
character.set_actor_label('Braxton - likeness study (not final)')
for loc,intensity,color in [((70,110,200),2200,(1,.97,.93)),((-90,60,180),1200,(.90,.95,1)),((20,-80,190),1800,(1,1,1))]:
    light=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*loc))
    comp=light.get_component_by_class(unreal.PointLightComponent)
    comp.set_intensity(intensity)
    comp.set_light_color(unreal.LinearColor(*color,1))
    comp.set_attenuation_radius(600)
    comp.set_editor_property('source_radius',25)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,105,163))
camera.set_actor_label('Braxton Face Review Camera')
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,8,163)),False)
camera_component=camera.get_component_by_class(unreal.CameraComponent)
camera_component.set_field_of_view(27)
camera_component.set_editor_property('aspect_ratio',1.0)
settings=camera_component.get_editor_property('post_process_settings')
settings.set_editor_property('override_auto_exposure_bias',True)
settings.set_editor_property('auto_exposure_bias',-.7)
camera_component.set_editor_property('post_process_settings',settings)
view=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
view.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/MetaHumans/BraxtonReview/Braxton_Review'):
    raise RuntimeError('Review scene did not save')
unreal.log('BRAXTON_REVIEW_SCENE_SAVED')
start=time.monotonic();stage=0;handle=None;task=None;finished_at=None
def tick(dt):
    global stage,task,finished_at
    try:
        elapsed=time.monotonic()-start
        if stage==0 and elapsed>25:
            task=unreal.AutomationLibrary.take_high_res_screenshot(1200,1200,OUT+'/Braxton_Refined_Front.png',camera)
            stage=1
        elif stage==1 and task.is_task_done():
            finished_at=elapsed
            stage=2
        elif stage==2 and elapsed>finished_at+5:
            loc=unreal.Vector(65,100,163)
            camera.set_actor_location(loc,False,False)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(loc,unreal.Vector(0,8,163)),False)
            finished_at=elapsed
            stage=3
        elif stage==3 and elapsed>finished_at+5:
            task=unreal.AutomationLibrary.take_high_res_screenshot(1200,1200,OUT+'/Braxton_Refined_Angle.png',camera)
            stage=4
        elif stage==4 and task.is_task_done():
            finished_at=elapsed
            stage=5
        elif stage==5 and elapsed>finished_at+5:
            unreal.log('BRAXTON_ASSEMBLED_REVIEW_COMPLETE')
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        elif elapsed>150:
            raise RuntimeError('Screenshot timed out')
    except Exception:
        unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
