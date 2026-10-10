"""Import photo-matched folding chairs and furnish only the verified room 102."""
import glob,json,os,re,unreal
BASE='/Users/Stewart/Projects/Senior sendoff/outputs/room102-chairs'
SOURCE=os.path.join(unreal.Paths.project_dir(),'SourceArt/Room102Chairs')
DEST='/Game/SeniorSendOff/House/Room102Chairs'
A=unreal.EditorAssetLibrary
E=unreal.MaterialEditingLibrary
T=unreal.AssetToolsHelpers.get_asset_tools()

def imp(path,name):
 t=unreal.AssetImportTask();t.filename=path;t.destination_path=DEST;t.destination_name=name
 t.automated=True;t.replace_existing=True;t.save=True
 if path.lower().endswith('.fbx'):
  ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False
  # UE5.8's legacy-options converter uses auto_generate_collision as the entire collision-import switch.
  ui.static_mesh_import_data.auto_generate_collision=name.startswith('SM_R102_FoldingChair');ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.one_convex_hull_per_ucx=True
  t.options=ui
 T.import_asset_tasks([t]);a=A.load_asset(DEST+'/'+name)
 if not a:raise RuntimeError('Import failed '+path)
 return a

def constant(m,value):
 if isinstance(value,(list,tuple)):
  n=E.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*value,1)
 else:n=E.create_material_expression(m,unreal.MaterialExpressionConstant);n.r=value
 return n

def material(name,info,textures):
 path=DEST+'/M_'+name
 m=A.load_asset(path) if A.does_asset_exist(path) else T.create_asset('M_'+name,DEST,unreal.Material,unreal.MaterialFactoryNew())
 E.delete_all_material_expressions(m)
 base=constant(m,info.get('color',[.7,.7,.7]))
 if info.get('basecolor_texture'):
  base=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);base.texture=textures[info['basecolor_texture']]
 E.connect_material_property(base,'RGB' if info.get('basecolor_texture') else '',unreal.MaterialProperty.MP_BASE_COLOR)
 E.connect_material_property(constant(m,info.get('roughness',.6)),'',unreal.MaterialProperty.MP_ROUGHNESS)
 E.connect_material_property(constant(m,info.get('metallic',0)),'',unreal.MaterialProperty.MP_METALLIC)
 if info.get('normal_texture'):
  n=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);n.texture=textures[info['normal_texture']];n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
  flat=E.create_material_expression(m,unreal.MaterialExpressionMaterialFunctionCall)
  flat.set_editor_property('material_function',unreal.load_object(None,'/Engine/Functions/Engine_MaterialFunctions01/Texturing/FlattenNormal.FlattenNormal'))
  E.connect_material_expressions(n,'RGB',flat,'Normal')
  E.connect_material_expressions(constant(m,1-info.get('normalStrength',.1)),'',flat,'Flatness')
  E.connect_material_property(flat,'',unreal.MaterialProperty.MP_NORMAL)
 if info.get('roughness_texture'):
  n=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);n.texture=textures[info['roughness_texture']];n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
  E.connect_material_property(n,'R',unreal.MaterialProperty.MP_ROUGHNESS)
 if info.get('emissive'):
  E.connect_material_property(constant(m,info['emissive']),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 E.recompile_material(m)
 if not A.save_loaded_asset(m):raise RuntimeError('Cannot save material '+name)
 return m

def vec(v):return [v.x,v.y,v.z]
def state(a):
 r={'class':a.get_class().get_name(),'location':vec(a.get_actor_location()),'rotation':[a.get_actor_rotation().roll,a.get_actor_rotation().pitch,a.get_actor_rotation().yaw],'scale':vec(a.get_actor_scale3d())}
 if isinstance(a,unreal.StaticMeshActor):r['mesh']=a.static_mesh_component.static_mesh.get_path_name() if a.static_mesh_component.static_mesh else None
 if isinstance(a,unreal.ExistingHouseDoor):r.update(locked=a.get_editor_property('locked'),closedYaw=a.get_editor_property('closed_yaw'),openYaw=a.get_editor_property('open_yaw'))
 return r

# The material and room manifests are generated separately and reviewed before applying.
manifest=json.load(open(os.path.join(SOURCE,'manifest.json')))
room=json.load(open(os.path.join(BASE,'work/room_layout.json')))
textures={}
for p in sorted(glob.glob(os.path.join(SOURCE,'*.png'))):
 tex=imp(p,os.path.splitext(os.path.basename(p))[0]);name=os.path.basename(p)
 if 'normal' in name.lower():
  tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('srgb',False);A.save_loaded_asset(tex)
 elif 'rough' in name.lower():
  tex.set_editor_property('srgb',False);A.save_loaded_asset(tex)
 textures[name]=tex
materials={k:material(k,v,textures) for k,v in manifest['materials'].items()}
meshes={}
for p in sorted(glob.glob(os.path.join(SOURCE,'*.fbx'))):
 name=os.path.splitext(os.path.basename(p))[0];m=imp(p,name)
 if name.startswith('COL'):
  m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 else:
  for i,s in enumerate(m.static_materials):
   key=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0])
   mat=materials.get(key)
   if not mat:
    old=A.load_asset('/Game/SeniorSendOff/House/Materials/MI_SSO_'+key)
    if not old:raise RuntimeError('No material '+key+' for '+name)
    mat=old
   m.set_material(i,mat)
 if name.startswith('SM_R102_FoldingChair'):
  convex_count=len(m.get_editor_property('body_setup').get_editor_property('agg_geom').get_editor_property('convex_elems'))
  if convex_count!=6:raise RuntimeError('Chair collision hull count '+name+' '+str(convex_count))
 A.save_loaded_asset(m);meshes[name]=m

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
before={n:state(a) for n,a in actors.items()}
for n,a in actors.items():
 if n.startswith(('Room102_','SM_Room102_','COL_Room102_')):unreal.EditorLevelLibrary.destroy_actor(a)
changed=set()
for row in room.get('replacements',[]):
 a=actors[row['actor']];a.static_mesh_component.set_static_mesh(meshes[row['mesh']]);changed.add(row['actor'])
 if row['actor'].startswith('COL'):
  a.static_mesh_component.set_collision_profile_name('BlockAll');a.static_mesh_component.set_visibility(False)
 else:a.static_mesh_component.set_collision_profile_name('NoCollision')
door=actors[room['door']['actor']];changed.add(door.get_actor_label())
for k,v in room['door'].get('properties',{}).items():
 if k in ('doorway_center',):v=unreal.Vector(*v)
 door.set_editor_property(k,v)
door.set_editor_property('locked',False)
# Moving location refreshes construction and the leaf's closed pose.
p=door.get_actor_location();door.set_actor_location(p+unreal.Vector(.01,0,0),False,False);door.set_actor_location(p,False,False)
new=[]
for row in room.get('static',[]):
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*row.get('location',[0,0,0])),unreal.Rotator(pitch=0,yaw=row.get('yaw',0),roll=0))
 a.set_actor_label(row['label']);a.set_folder_path('First floor/Room 102 chair storage')
 c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_collision_profile_name('BlockAll' if row['mesh'].startswith('COL') else 'NoCollision')
 if row['mesh'].startswith('COL'):c.set_visibility(False)
 new.append(a.get_actor_label())
for i,row in enumerate(room['chairs']):
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*row['location']),unreal.Rotator(pitch=0,yaw=row.get('yaw',0),roll=0))
 a.set_actor_label('Room102_Chair_%02d'%(i+1));a.set_folder_path('First floor/Room 102 chair storage')
 a.set_editor_property('tags',[unreal.Name('SSOMovableFurniture'),unreal.Name('SSORoom102Chair')]+([unreal.Name('SSORoom102TestChair')] if row.get('test') else []))
 a.set_replicates(True);a.set_replicate_movement(True)
 c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(meshes[row.get('mesh','SM_R102_FoldingChair')])
 if row.get('wear'):c.set_material(0,materials['Room102_IvoryPlastic_Worn'])
 c.set_collision_profile_name('PhysicsActor');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY,next(getattr(unreal.CollisionResponseType,k) for k in dir(unreal.CollisionResponseType) if k.endswith('BLOCK')))
 c.set_editor_property('generate_overlap_events',False);c.set_use_ccd(True);c.set_editor_property('can_character_step_up_on',next(getattr(unreal.CanBeCharacterBase,k) for k in dir(unreal.CanBeCharacterBase) if k=='NO' or k.endswith('_NO')))
 body=c.get_editor_property('body_instance');body.set_editor_property('start_awake',False)
 c.set_mass_override_in_kg(unreal.Name(''),4.2,True);c.set_linear_damping(.12);c.set_angular_damping(.65)
 c.set_simulate_physics(True);c.put_rigid_body_to_sleep()
 new.append(a.get_actor_label())
for row in room.get('lights',[]):
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*row['location']))
 a.set_actor_label(row['label']);a.set_folder_path('First floor/Room 102 chair storage')
 c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE)
 c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
 c.set_intensity(row.get('intensity',850));c.set_attenuation_radius(row.get('radius',650));c.set_light_color(unreal.LinearColor(*row.get('color',[1,.9,.73]),1))
 c.set_editor_property('cast_shadows',False);new.append(a.get_actor_label())
registry_path=os.path.join(unreal.Paths.project_content_dir(),'HouseReview/original_house_doors.json')
registry=json.load(open(registry_path));found=False
for r in registry:
 if r['id']==room['door']['id']:
  r['locked']=False
  if 'closed_yaw' in room['door'].get('properties',{}):r['closedYaw']=room['door']['properties']['closed_yaw']
  if 'open_yaw' in room['door'].get('properties',{}):r['openYaw']=room['door']['properties']['open_yaw']
  found=True
if not found:raise RuntimeError('Door registry missing')
after={a.get_actor_label():state(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected=[n for n in before if n not in changed and not n.startswith(('Room102_','SM_Room102_','COL_Room102_'))]
errors=[n for n in protected if before[n]!=after.get(n)]
if errors:raise RuntimeError('Protected actors changed: '+str(errors))
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Cannot save level')
json.dump(registry,open(registry_path,'w'),indent=2)
json.dump({'door':room['door'],'chair_count':len(room['chairs']),'protected_actors':len(protected),'new_actors':new,'changed':sorted(changed),'all_protected_unchanged':True},open(os.path.join(BASE,'work/applied.json'),'w'),indent=2)
unreal.log('ROOM102_APPLY_PASSED chairs=%d protected=%d'%(len(room['chairs']),len(protected)))
