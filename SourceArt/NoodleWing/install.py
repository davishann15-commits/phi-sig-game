"""Install only surveyed new east wing and its five scoped access cuts."""
import unreal,json,pathlib,re,traceback
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing';D='/Game/SeniorSendOff/House/NoodleWing';A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools();S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);M=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
def xyz(v):return[v.x,v.y,v.z]
def snap(a):
 r=a.get_actor_rotation();o=dict(label=a.get_actor_label(),cls=a.get_class().get_name(),p=xyz(a.get_actor_location()),r=[r.pitch,r.yaw,r.roll],s=xyz(a.get_actor_scale3d()),tags=[str(t)for t in a.tags],meshes=[],lights=[])
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  o['meshes'].append(dict(name=c.get_name(),mesh=c.static_mesh.get_path_name()if c.static_mesh else None,materials=[c.get_material(i).get_path_name()if c.get_material(i)else None for i in range(c.get_num_materials())],collision=str(c.get_collision_enabled()),visible=c.is_visible()))
 for c in a.get_components_by_class(unreal.LightComponent):o['lights'].append([c.get_name(),c.intensity])
 if isinstance(a,unreal.ExistingHouseDoor):o['door']={k:str(a.get_editor_property(k))for k in['door_id','doorway_center','closed_yaw','open_yaw','locked','panel_center_world','panel_half_extent','local_leaf_space']}
 return o

def imp(path,name):
 task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=D;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False
 if str(path).endswith('.fbx'):
  task.factory=unreal.FbxFactory();o=unreal.FbxImportUI();o.import_mesh=True;o.import_as_skeletal=False;o.import_materials=False;o.import_textures=False;o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False;o.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;task.options=o
 T.import_asset_tasks([task]);asset=A.load_asset(D+'/'+name);assert asset,name;return asset

def con(m,v):
 if isinstance(v,list):n=E.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*v,1)
 else:n=E.create_material_expression(m,unreal.MaterialExpressionConstant);n.r=float(v)
 return n

def material(n,r,tex):
 m=A.load_asset(D+'/'+n)or T.create_asset(n,D,unreal.Material,unreal.MaterialFactoryNew());E.delete_all_material_expressions(m);m.set_editor_property('two_sided',True)
 base=con(m,r['color']);rough=con(m,r['roughness'])
 if r.get('photo'):
  t=imp(r['photo'],'T_NW_'+n+'_Photo');t.set_editor_property('max_texture_size',1024);A.save_loaded_asset(t,False);base=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);base.texture=t
 elif r.get('texture'):
  tx=tex[r['texture']];sample=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);sample.texture=tx['Detail'];sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
  mul=E.create_material_expression(m,unreal.MaterialExpressionMultiply);E.connect_material_expressions(sample,'RGB',mul,'A');E.connect_material_expressions(con(m,[v*2 for v in r['color']]),'',mul,'B');base=mul
  rough=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);rough.texture=tx['Roughness'];rough.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
  normal=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);normal.texture=tx['Normal'];normal.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL);E.connect_material_property(normal,'RGB',unreal.MaterialProperty.MP_NORMAL)
 E.connect_material_property(base,'RGB'if isinstance(base,unreal.MaterialExpressionTextureSample)else'',unreal.MaterialProperty.MP_BASE_COLOR)
 E.connect_material_property(rough,'R'if isinstance(rough,unreal.MaterialExpressionTextureSample)else'',unreal.MaterialProperty.MP_ROUGHNESS);E.connect_material_property(con(m,r['metallic']),'',unreal.MaterialProperty.MP_METALLIC)
 if r.get('emissive'):E.connect_material_property(con(m,[v*r['emissive']for v in r['color']]),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 E.recompile_material(m);assert A.save_loaded_asset(m,False);return m

def setupcol(mesh,kind):
 M.remove_collisions(mesh);body=mesh.get_editor_property('body_setup');body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if kind=='structure'else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
 if kind in['prop','wall','door']:M.add_simple_collisions(mesh,unreal.ScriptingCollisionShapeType.BOX)
 mesh.set_editor_property('support_ray_tracing',False)

def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 art=json.loads((B/'art/manifest.json').read_text());cuts=json.loads((B/'art/cuts.json').read_text())['assets'];tex={};mats={};meshes={};meta={r['name']:r for r in art['assets']}
 for r in art['assets']:
  me=A.load_asset(D+'/'+r['name']);assert me,r['name'];meshes[r['name']]=me
 for r in cuts:
  me=imp(r['fbx'],r['name']);slots=list(me.static_materials);old=A.load_asset(r['old_asset']);assert old
  lookup={p.split('.')[-1]:p for p in r['materials']}
  for sl in slots:
   key=re.sub(r'[._]\d+$','',str(sl.get_editor_property('imported_material_slot_name')));assert key in lookup,(r['name'],key,list(lookup));sl.material_interface=A.load_asset(lookup[key])
  me.static_materials=slots;setupcol(me,'structure');me.set_editor_property('allow_cpu_access',True);assert A.save_loaded_asset(me,False);meshes[r['name']]=me
 before={a.get_name():snap(a)for a in S.get_all_level_actors()if not a.get_actor_label().startswith('NW_')};changed=set()
 for a in list(S.get_all_level_actors()):
  if a.get_actor_label().startswith('NW_'):S.destroy_actor(a)
 actors={a.get_actor_label():a for a in S.get_all_level_actors()}
 for r in cuts:
  a=actors[r['sourceActor']];changed.add(a.get_name());a.static_mesh_component.set_static_mesh(meshes[r['name']]);a.static_mesh_component.set_editor_property('override_materials',[])
 for r in art['placements']:
  md=meta[r['asset']];kind=r.get('kind')or md['kind'];mesh=meshes[r['asset']];p=unreal.Vector(*r['pos']);rot=unreal.Rotator(yaw=r['yaw']);sc=unreal.Vector(*r['scale'])
  if kind in['prop','wall']:
   a=S.spawn_actor_from_class(unreal.HouseInteractiveProp,p,rot);a.set_actor_scale3d(sc);a.set_editor_property('intact_mesh',mesh);a.set_editor_property('prop_id',r['label']);a.set_editor_property('prop_name',r['label'].replace('NW_','').replace('_',' '));a.set_editor_property('mass_kg',float(r.get('mass')or md['mass']));a.set_editor_property('movable',True);a.set_editor_property('breakable',False);a.set_editor_property('wall_mounted',kind=='wall');a.refresh_prop()
  else:
   a=S.spawn_actor_from_class(unreal.StaticMeshActor,p,rot);a.set_actor_scale3d(sc);a.static_mesh_component.set_static_mesh(mesh);a.static_mesh_component.set_collision_profile_name('BlockAll'if kind=='structure'else'NoCollision')
   if kind=='detail':a.static_mesh_component.set_cast_shadow(False)
  a.set_actor_label(r['label']);a.tags=[unreal.Name('NoodleWingStairwell')]if r['label']=='NW_StairStructure'else[unreal.Name('NoodleWing')];a.set_folder_path('Noodle Wing/'+('Architecture'if kind in['structure','detail']else'Furniture'))
 for i,r in enumerate(art['lights']):
  a=S.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*r['pos']));a.set_actor_label(r['label']);a.set_folder_path('Noodle Wing/Lighting');c=a.point_light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_intensity(r['power']);c.set_attenuation_radius(r['radius']);c.set_temperature(r['temperature']);c.set_use_temperature(True);c.set_source_radius(12);c.set_soft_source_radius(15);c.set_cast_shadows(i in[0,1,2,5,8,11]);c.set_editor_property('max_draw_distance',1800);c.set_editor_property('max_distance_fade_range',250)
 for r in art['doors']:
  a=S.spawn_actor_from_class(unreal.ExistingHouseDoor,unreal.Vector(*r['pos']));a.set_actor_label(r['label']);a.set_folder_path('Noodle Wing/Doors');a.set_actor_scale3d(unreal.Vector(*r.get('scale',[1,1,1])));a.set_editor_property('original_leaf',meshes['SM_NW_FireDoor']);a.set_editor_property('door_id',r['label']);a.set_editor_property('local_leaf_space',True);a.set_editor_property('doorway_center',unreal.Vector(*r['center']));a.set_editor_property('panel_center_world',unreal.Vector(82.5,0,118));a.set_editor_property('panel_half_extent',unreal.Vector(82.5,3,118));a.set_editor_property('closed_yaw',float(r['yaw']));a.set_editor_property('open_yaw',float(r['yaw']+r['swing']));a.set_editor_property('locked',False)
  p=a.get_actor_location();a.set_actor_location(p+unreal.Vector(.001,0,0),False,True);a.set_actor_location(p,False,True)
 for label,angle in[('OriginalDoor_Basement_m1_p1_Solid_01',90),('OriginalDoor_Upper_m1_p1_Solid_05',90)]:
  a=actors[label];changed.add(a.get_name());a.set_editor_property('locked',False);a.set_editor_property('open_yaw',float(angle))
  center,half=([1747.55,-513.81,-248.666],[5,83.16,113.3])if 'Basement'in label else([2070.82,-328.95,457.03],[7,84.15,116.49]);sc=a.get_actor_scale3d()
  a.set_editor_property('panel_center_world',unreal.Vector(*center));a.set_editor_property('panel_half_extent',unreal.Vector(half[0]/sc.x,half[1]/sc.y,half[2]/sc.z))
  p=a.get_actor_location();a.set_actor_location(p+unreal.Vector(.001,0,0),False,True);a.set_actor_location(p,False,True)
 after={a.get_name():snap(a)for a in S.get_all_level_actors()};drift=[r['label']for n,r in before.items()if n not in changed and after.get(n)!=r];assert not drift,drift
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=dict(status='PASS',new_actors=len(after)-len(before),protected_actors=len(before)-len(changed),changed_existing=[before[n]['label']for n in changed],new_lights=len(art['lights']),assets=len(art['assets']),doors=art['doors']);(B/'evidence/install-report.json').write_text(json.dumps(report,indent=2));unreal.log('NOODLE_INSTALL_PASS '+json.dumps(report))
try:run()
except:unreal.log_error('NOODLE_INSTALL_FAIL '+traceback.format_exc())
