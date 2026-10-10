"""Import the photo-matched upstairs bedroom and preserve every unrelated actor."""
import glob,json,os,re,unreal
BASE='/Users/Stewart/Projects/Senior sendoff/outputs/bedroom-across-poster'
SOURCE=os.path.join(unreal.Paths.project_dir(),'SourceArt/AcrossPosterBedroom')
DEST='/Game/SeniorSendOff/House/AcrossPosterBedroom'
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools()
PLAN=json.load(open(SOURCE+'/import_manifest.json'))

def imp(path,name,hulls=0):
 t=unreal.AssetImportTask();t.filename=path;t.destination_path=DEST;t.destination_name=name
 t.automated=True;t.replace_existing=True;t.save=True
 if path.lower().endswith('.fbx'):
  ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False
  ui.static_mesh_import_data.auto_generate_collision=bool(hulls)
  ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.one_convex_hull_per_ucx=True
  if name.startswith('SM'):
   ui.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
  t.options=ui
 T.import_asset_tasks([t]);asset=A.load_asset(DEST+'/'+name)
 if not asset:raise RuntimeError('Import failed '+path)
 return asset

def node(m,cls):return E.create_material_expression(m,cls)
def value(m,v):
 if isinstance(v,(list,tuple)):
  n=node(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*v,1)
 else:n=node(m,unreal.MaterialExpressionConstant);n.r=v
 return n
def conn(a,b,input,output=''):
 if not E.connect_material_expressions(a,output,b,input):raise RuntimeError('Material connection '+input)
def prop(a,m,p,out=''):
 if not E.connect_material_property(a,out,p):raise RuntimeError('Material property '+str(p))
def mul(m,a,b,out=''):
 n=node(m,unreal.MaterialExpressionMultiply);conn(a,n,'A',out);conn(b,n,'B');return n
def sample(m,key,textures,scale=1,linear=False,normal=False):
 n=node(m,unreal.MaterialExpressionTextureSample);n.texture=textures[os.path.basename(key)]
 if normal:n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
 elif linear:n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
 if scale!=1:
  uv=node(m,unreal.MaterialExpressionTextureCoordinate);uv.u_tiling=scale;uv.v_tiling=scale;conn(uv,n,'UVs')
 return n
def make_material(name,info,textures):
 if info.get('reuse'):
  existing=A.load_asset('/Game/SeniorSendOff/House/Materials/'+info['reuse'])
  if not existing:raise RuntimeError('Missing reused material '+info['reuse'])
  return existing
 m=A.load_asset(DEST+'/'+name) if A.does_asset_exist(DEST+'/'+name) else T.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
 E.delete_all_material_expressions(m)
 base=value(m,info.get('color',[.7,.7,.7]))
 if info.get('base_texture'):base=sample(m,info['base_texture'],textures,info.get('uv_scale',1))
 rough=value(m,info.get('roughness',.7))
 if info.get('roughness_texture'):rough=sample(m,info['roughness_texture'],textures,info.get('uv_scale',1),linear=True)
 normal=None
 if info.get('normal_texture'):
  n=sample(m,info['normal_texture'],textures,info.get('uv_scale',1),normal=True)
  normal=node(m,unreal.MaterialExpressionMaterialFunctionCall)
  normal.set_editor_property('material_function',unreal.load_object(None,'/Engine/Functions/Engine_MaterialFunctions01/Texturing/FlattenNormal.FlattenNormal'))
  conn(n,normal,'Normal','RGB');conn(value(m,1-info.get('normal_strength',.15)),normal,'Flatness')
 if info.get('water'):
  m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
  # Substrate derives the water model from this BSDF. The Mac ray tracing
  # pass also requires the matching legacy volume output functions.
  m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
  bsdf=node(m,unreal.MaterialExpressionSubstrateSingleLayerWaterBSDF)
  names=E.get_material_expression_input_names(bsdf)
  normalize=lambda k:k.replace(' ','').replace('_','').lower()
  inputs={'BaseColor':base,'Metallic':value(m,0),'Specular':value(m,.5),'Roughness':rough,'TopMaterialOpacity':value(m,.16),'WaterAlbedo':value(m,[.095,.085,.047]),'WaterExtinction':value(m,[.006,.005,.004]),'WaterPhaseG':value(m,.1),'ColorScaleBehindWater':value(m,[.84,.83,.78])}
  for k,n in inputs.items():
   matches=[v for v in names if normalize(v)==normalize(k)]
   if len(matches)!=1:raise RuntimeError('Water input '+k+' '+str(names))
   conn(n,bsdf,matches[0])
  prop(bsdf,m,unreal.MaterialProperty.MP_FRONT_MATERIAL)
  legacy=node(m,unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
  legacy_names=E.get_material_expression_input_names(legacy)
  legacy_inputs={'ScatteringCoefficients':value(m,[.00057,.000425,.000188]),'AbsorptionCoefficients':value(m,[.00543,.004575,.003812]),'PhaseG':inputs['WaterPhaseG'],'ColorScaleBehindWater':inputs['ColorScaleBehindWater']}
  for k,n in legacy_inputs.items():
   matches=[v for v in legacy_names if normalize(v)==normalize(k)]
   if len(matches)!=1:raise RuntimeError('Water volume input '+k+' '+str(legacy_names))
   conn(n,legacy,matches[0])
 else:
  prop(base,m,unreal.MaterialProperty.MP_BASE_COLOR,'RGB' if info.get('base_texture') else '')
  prop(rough,m,unreal.MaterialProperty.MP_ROUGHNESS,'R' if info.get('roughness_texture') else '')
  prop(value(m,info.get('metallic',0)),m,unreal.MaterialProperty.MP_METALLIC)
  if 'specular' in info:prop(value(m,info['specular']),m,unreal.MaterialProperty.MP_SPECULAR)
  if normal:prop(normal,m,unreal.MaterialProperty.MP_NORMAL)
  if info.get('emission',0):prop(mul(m,base,value(m,info['emission']),'RGB' if info.get('base_texture') else ''),m,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 if info.get('two_sided'):m.set_editor_property('two_sided',True)
 if info.get('translucent'):
  m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
  m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
  m.set_editor_property('two_sided',True)
  prop(value(m,info.get('opacity',.23)),m,unreal.MaterialProperty.MP_OPACITY)
 if info.get('masked'):
  m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
  m.set_editor_property('opacity_mask_clip_value',info.get('clip',.4))
  mask=sample(m,info['mask_texture'],textures,info.get('uv_scale',1))
  prop(mask,m,unreal.MaterialProperty.MP_OPACITY_MASK,info.get('mask_channel','A'))
 E.recompile_material(m)
 if not A.save_loaded_asset(m):raise RuntimeError('Material save '+name)
 return m

def vec(v):return [v.x,v.y,v.z]
def state(a):
 r={'class':a.get_class().get_name(),'location':vec(a.get_actor_location()),'scale':vec(a.get_actor_scale3d()),'rotation':[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll]}
 if isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component;r.update(mesh=c.static_mesh.get_path_name() if c.static_mesh else None,materials=[m.get_path_name() if m else None for m in c.get_materials()],collision=str(c.get_collision_profile_name()))
 if isinstance(a,unreal.ExistingHouseDoor):
  r.update(door_id=str(a.get_editor_property('door_id')),locked=a.get_editor_property('locked'),closed_yaw=a.get_editor_property('closed_yaw'),open_yaw=a.get_editor_property('open_yaw'))
 return r

textures={}
for file in PLAN['textures']:
 name=os.path.splitext(file)[0];tex=imp(SOURCE+'/'+file,name)
 lower=file.lower()
 if 'normal' in lower:
  tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('srgb',False)
  if file in PLAN.get('flip_green_textures',[]):tex.set_editor_property('flip_green_channel',True)
 elif any(s in lower for s in ('roughness','mask')):tex.set_editor_property('srgb',False)
 A.save_loaded_asset(tex);textures[file]=tex
materials={k:make_material(k,v,textures) for k,v in PLAN['materials'].items()}
meshes={}
for row in PLAN['meshes']:
 name=row['name'];m=imp(SOURCE+'/'+row['file'],name,row.get('hulls',0))
 if row.get('complex_collision'):
  m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 if row.get('hulls'):
  count=len(m.get_editor_property('body_setup').get_editor_property('agg_geom').get_editor_property('convex_elems'))
  if count!=row['hulls']:raise RuntimeError('Wrong collision hulls '+name+' '+str(count))
 if not row.get('complex_collision'):
  for i,s in enumerate(m.static_materials):
   key=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0])
   mat=materials.get(key)
   if not mat:
    old=key if key.startswith('MI_SSO_') else 'MI_SSO_'+key
    mat=A.load_asset('/Game/SeniorSendOff/House/Materials/'+old)
   if not mat:raise RuntimeError('Missing material '+key+' on '+name)
   m.set_material(i,mat)
 A.save_loaded_asset(m);meshes[name]=m

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
before={k:state(a) for k,a in actors.items()}
changed=set();removed=[];new=[]
already_applied=any(n.startswith('BR_') for n in actors)
for label,a in actors.items():
 if label.startswith('BR_'):unreal.EditorLevelLibrary.destroy_actor(a)
for label in PLAN['remove_actors']:
 if label not in actors:
  if already_applied:continue
  raise RuntimeError('Missing intended removed actor '+label)
 unreal.EditorLevelLibrary.destroy_actor(actors[label]);removed.append(label)
for row in PLAN['replacements']:
 a=actors[row['actor']];c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_editor_property('override_materials',[])
 c.set_collision_profile_name('BlockAll' if row.get('collision') else 'NoCollision')
 if row.get('collision'):c.set_visibility(False)
 changed.add(row['actor'])
for row in PLAN.get('door_updates',[]):
 a=actors[row['actor']]
 for key,val in row['properties'].items():
  if key in ('doorway_center','panel_center_world','panel_half_extent','added_handle_world_position'):val=unreal.Vector(*val)
  elif key in ('original_leaf','original_glass','added_handle_mesh'):val=meshes[val] if val else None
  a.set_editor_property(key,val)
 if row.get('location'):a.set_actor_location(unreal.Vector(*row['location']),False,False)
 p=a.get_actor_location();a.set_actor_location(p+unreal.Vector(.01,0,0),False,False);a.set_actor_location(p,False,False)
 changed.add(row['actor'])
for row in PLAN['actors']:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*row.get('location',[0,0,0])),unreal.Rotator(pitch=0,yaw=row.get('yaw',0),roll=0))
 a.set_actor_label(row['label']);a.set_folder_path('Second floor/Photo matched bedroom')
 a.set_actor_scale3d(unreal.Vector(*row.get('scale',[1,1,1])))
 c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_collision_profile_name(row.get('collision','NoCollision'))
 c.set_editor_property('generate_overlap_events',False)
 if row.get('hidden'):c.set_visibility(False)
 if row.get('materials'):
  for i,name in enumerate(row['materials']):c.set_material(i,materials[name])
 new.append(row['label'])
for row in PLAN['lights']:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*row['location']))
 a.set_actor_label(row['label']);a.set_folder_path('Second floor/Photo matched bedroom')
 c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE)
 c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(row.get('intensity',950))
 c.set_attenuation_radius(row.get('radius',500));c.set_light_color(unreal.LinearColor(*row.get('color',[.91,.95,1]),1))
 c.set_editor_property('cast_shadows',row.get('cast_shadows',False))
 c.set_editor_property('source_radius',row.get('source_radius',0));c.set_editor_property('soft_source_radius',row.get('soft_source_radius',0));new.append(row['label'])
registry_path=os.path.join(unreal.Paths.project_content_dir(),'HouseReview/original_house_doors.json')
registry=json.load(open(registry_path));retire=set(PLAN['remove_door_ids'])
if not already_applied and not retire.issubset({r['id'] for r in registry}):raise RuntimeError('Door registry mismatch')
registry=[r for r in registry if r['id'] not in retire]
for update in PLAN.get('door_updates',[]):
 for row in registry:
  if row['id']==update['id']:
   props=update['properties']
   
   if 'closed_yaw' in props:row['closedYaw']=props['closed_yaw']
   if 'open_yaw' in props:row['openYaw']=props['open_yaw']
   if 'doorway_center' in props:row['doorwayCenter']=props['doorway_center']
   if 'locked' in props:row['locked']=props['locked']
   if update.get('location'):row['hinge']=update['location']
   if props.get('original_leaf'):row['leafAsset']=DEST+'/'+props['original_leaf']
after={a.get_actor_label():state(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected=[n for n in before if n not in changed and n not in removed and not n.startswith('BR_')]
errors=[n for n in protected if before[n]!=after.get(n)]
if errors:raise RuntimeError('Protected actors changed '+str(errors))
if any(n in after for n in removed):raise RuntimeError('Removed door remains')
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Level save failed')
json.dump(registry,open(registry_path,'w'),indent=2)
json.dump({'removed':removed,'changed':sorted(changed),'new':new,'protected_actors':len(protected),'all_protected_unchanged':True},open(BASE+'/work/applied.json','w'),indent=2)
unreal.log('ACROSS_POSTER_BEDROOM_APPLY_PASSED protected=%d removed=%d new=%d'%(len(protected),len(removed),len(new)))
