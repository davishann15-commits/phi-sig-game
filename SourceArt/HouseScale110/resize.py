import unreal,json,os,traceback
B=unreal.Paths.project_dir()+'SourceArt/HouseScale110/'
K=1.1
MATTRESS_BOUNDS=None
def vec(p):return unreal.Vector(*p)
def mul(p):return [x*K for x in p]
def arr(p):return [p.x,p.y,p.z]
def expected(row):
 p=mul(row['location']);s=mul(row['scale']);c=row['class_name']
 if c in ['DirectionalLight','SkyLight','SkyAtmosphere']:p=row['location'];s=row['scale']
 if c in ['CameraActor','HouseHotTubWater']:s=row['scale']
 if 'capsule_half_height' in row:
  s=row['scale'];h=row['capsule_half_height'];p[2]=(row['location'][2]-h)*K+h
  if c=='HouseRoamingPig' and MATTRESS_BOUNDS:
   lo,hi=MATTRESS_BOUNDS
   if lo[0]<p[0]<hi[0] and lo[1]<p[1]<hi[1] and p[2]-h<hi[2]:
    p[2]=hi[2]+h+2.0
 return p,s
def run():
 global MATTRESS_BOUNDS
 level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
 assert level.load_level('/Game/Story/Maps/Chapter01_House')
 sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 rows=json.load(open(B+'before/actors.json'))
 mattress=next(r for r in rows if r['label']=='Interactive_PIG_Mattress__001')
 MATTRESS_BOUNDS=[mul(v) for v in mattress['bounds']]
 actors={a.get_name():a for a in sub.get_all_level_actors()}
 assert set(actors)=={r['name'] for r in rows},'Actor inventory changed since backup'
 byname={r['name']:r for r in rows}
 def depth(r):return 1+depth(byname[r['parent']]) if r['parent'] else 0
 rows.sort(key=depth)
 for r in rows:
  a=actors[r['name']];p,s=expected(r)
  rot=unreal.Rotator(pitch=r['rotation'][0],yaw=r['rotation'][1],roll=r['rotation'][2])
  a.set_actor_transform(unreal.Transform(vec(p),rot,vec(s)),False,True)
  props=r['properties'];c=r['class_name']
  if c=='ExistingHouseDoor':
   a.set_editor_property('doorway_center',vec(mul(props['doorway_center'])))
   if not props['local_leaf_space']:
    for k in ['panel_center_world','added_handle_world_position']:
     a.set_editor_property(k,vec(mul(props[k])))
    a.set_editor_property('source_geometry_origin_world',vec(mul(props['source_geometry_origin_world'] or [0,0,0])))
  if c=='HouseRoamingPig':
   for k in ['bed_location','pigsty_minimum','pigsty_maximum']:a.set_editor_property(k,vec(mul(props[k])))
   graph=list(a.get_editor_property('roam_graph'))
   assert len(graph)==len(r['roam_graph'])
   for n,original in zip(graph,r['roam_graph']):n.set_editor_property('feet',vec(mul(original['feet'])))
   a.set_editor_property('roam_graph',graph)
  if c=='HouseHotTubWater':
   a.set_editor_property('water_size',unreal.Vector2D(*mul(props['water_size'])))
   for k in ['corner_radius','water_depth','steam_rise_height']:a.set_editor_property(k,props[k]*K)
  # Light lengths are not affected by the actor transform. Inverse-square
  # intensity keeps the original brightness at proportionally moved surfaces.
  components={l.get_name():l for l in a.get_components_by_class(unreal.LocalLightComponent)}
  for original in r['lights']:
   light=components[original['name']]
   for k in ['attenuation_radius','source_radius','soft_source_radius','source_length','source_width','source_height']:
    if k in original:light.set_editor_property(k,original[k]*K)
   if 'intensity' in original:light.set_editor_property('intensity',original['intensity']*K*K)
  # Construction/property edits must not change the intended world placement.
  a.set_actor_transform(unreal.Transform(vec(p),rot,vec(s)),False,True)
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 settings=world.get_world_settings()
 tags=list(settings.get_editor_property('tags'))
 if unreal.Name('SSOHouseScale110') not in tags:tags.append(unreal.Name('SSOHouseScale110'))
 settings.set_editor_property('tags',tags)
 audit=[]
 for r in rows:
  a=actors[r['name']];p,s=expected(r)
  assert (a.get_actor_location()-vec(p)).length()<.02,r['label']+' location mismatch'
  assert (a.get_actor_scale3d()-vec(s)).length()<.002,r['label']+' scale mismatch'
  actual=a.get_actor_rotation();angles=[actual.pitch,actual.yaw,actual.roll]
  assert all(abs((got-want+180)%360-180)<.02 for got,want in zip(angles,r['rotation'])),r['label']+' rotation mismatch'
  audit.append(dict(name=r['name'],label=r['label'],class_name=r['class_name'],location=arr(a.get_actor_location()),rotation=angles,scale=arr(a.get_actor_scale3d())))
 assert level.save_current_level(),'Map save failed'
 json.dump(dict(factor=K,origin=[0,0,0],character_scale_unchanged=True,spawn_correction='Coal stands on the mattress surface rather than intersecting it',actors=audit),open(B+'evidence/resize-audit.json','w'),indent=2)
 unreal.log('HOUSE_SCALE_INSTALL_PASS actors='+str(len(rows))+' factor=1.1 character_dimensions_unchanged=1')
 p=unreal.Vector(-2250*1.1,1050*1.1,455*1.1);target=unreal.Vector(-2220*1.1,1234*1.1,440*1.1)
 key=level.get_active_viewport_config_key()
 level.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,target),key)
 level.editor_set_game_view(True,key)
try:run()
except:unreal.log_error('HOUSE_SCALE_INSTALL_FAIL '+traceback.format_exc())
