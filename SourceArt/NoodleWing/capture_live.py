import unreal,json,pathlib,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing/context'
def v(p):return [p.x,p.y,p.z]
def rot(p):return [p.pitch,p.yaw,p.roll]
def snap(a):
 o,e=a.get_actor_bounds(False)
 d=dict(name=a.get_name(),label=a.get_actor_label(),className=a.get_class().get_name(),location=v(a.get_actor_location()),rotation=rot(a.get_actor_rotation()),scale=v(a.get_actor_scale3d()),bounds=[v(o-e),v(o+e)],meshes=[])
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  if c.static_mesh:
   t=c.get_world_transform();d['meshes'].append(dict(name=c.get_name(),asset=c.static_mesh.get_path_name(),location=v(t.translation),rotation=rot(t.rotation.rotator()),scale=v(t.scale3d),materials=[m.get_path_name() if m else None for m in c.get_materials()]))
 return d
try:
 worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
 for w in worlds:
  if 'House' not in w.get_name():continue
  rows=[snap(a) for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)]
  pc=unreal.GameplayStatics.get_player_controller(w,0);cam=pc.player_camera_manager
  (B/'live.json').write_text(json.dumps(dict(world=w.get_path_name(),actors=rows,cameraLocation=v(cam.get_camera_location()),cameraRotation=rot(cam.get_camera_rotation())),indent=2))
 unreal.log('NOODLE_LIVE_CAPTURE_PASS')
except:unreal.log_error(traceback.format_exc())
