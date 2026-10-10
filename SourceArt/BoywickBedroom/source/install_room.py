"""Install the photo-reference Boy Wick bedroom, preserving the rest of the saved house."""
import unreal,json,re,collections
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BoywickBedroom');doc=json.loads((R/'manifest.json').read_text());DEST='/Game/SeniorSendOff/House/BoywickBedroom'
ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);ml=unreal.MaterialEditingLibrary
def imported_texture(file,mask=False,normal=False):
 name='T_BWR_'+Path(file).stem;key=DEST+'/Textures/'+name
 if not ed.does_asset_exist(key) or Path(file).stem in doc.get('refresh_textures',[]):
  task=unreal.AssetImportTask();task.filename=file;task.destination_path=DEST+'/Textures';task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True;at.import_asset_tasks([task])
 t=unreal.load_asset(key)
 if not isinstance(t,unreal.Texture2D):raise RuntimeError('Missing texture '+key)
 t.set_editor_property('max_texture_size',2048 if Path(file).stem.startswith(('IMG_','OakPlanks')) else 1024)
 if mask:t.set_editor_property('srgb',False);t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
 if normal:t.set_editor_property('srgb',False);t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
 ed.save_loaded_asset(t);return t
def constant(m,key,value,prop):
 node=ml.create_material_expression(m,unreal.MaterialExpressionConstant);node.set_editor_property('r',float(value));ml.connect_material_property(node,'',prop)
def texture(m,file,prop,mask=False,normal=False):
 n=ml.create_material_expression(m,unreal.MaterialExpressionTextureSample);n.texture=imported_texture(file,mask,normal)
 if mask:n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
 if normal:n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
 ml.connect_material_property(n,'R' if mask else 'RGB',prop)
for stem in doc.get('refresh_textures',[]):
 imported_texture(str(R/'textures'/(stem+'.png')),'Roughness' in stem or stem.endswith('_Mask'),stem.endswith('_Normal'))
for name,d in doc['materials'].items():
 key=DEST+'/'+name
 if ed.does_asset_exist(key):
  m=unreal.load_asset(key)
  if any(t in name for t in ['Fabric','Cotton','Shirt','Photo','Paper']):
   m.set_editor_property('two_sided',True);ml.recompile_material(m);ed.save_loaded_asset(m)
  continue
 m=unreal.load_asset(key) if ed.does_asset_exist(key) else at.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
 ml.delete_all_material_expressions(m)
 if d.get('texture'):texture(m,d['texture'],unreal.MaterialProperty.MP_BASE_COLOR)
 else:
  n=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*d['base_color'],1);ml.connect_material_property(n,'',unreal.MaterialProperty.MP_BASE_COLOR)
 if d.get('roughness_texture'):texture(m,d['roughness_texture'],unreal.MaterialProperty.MP_ROUGHNESS,True)
 else:constant(m,'rough',d['roughness'],unreal.MaterialProperty.MP_ROUGHNESS)
 if d.get('normal_texture'):texture(m,d['normal_texture'],unreal.MaterialProperty.MP_NORMAL,normal=True)
 if d.get('two_sided'):m.set_editor_property('two_sided',True)
 constant(m,'metal',d['metallic'],unreal.MaterialProperty.MP_METALLIC)
 if d.get('opacity_mask'):
  m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED);m.set_editor_property('two_sided',True);texture(m,d['opacity_mask'],unreal.MaterialProperty.MP_OPACITY_MASK,True)
 if 'opacity' in d:
  m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True);constant(m,'opacity',d['opacity'],unreal.MaterialProperty.MP_OPACITY)
 if d.get('emission'):
  n=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*[k*d['emission'] for k in d['base_color']],1);ml.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 ml.recompile_material(m)
 if not ed.save_loaded_asset(m):raise RuntimeError('Material save failed '+name)
def import_mesh(a,field='intact',name=None):
 name=name or a['asset_name'];file=a[field+'_fbx'];paths=a.get(field+'_material_slots',a['material_slots']);key=DEST+'/'+name
 if not ed.does_asset_exist(key) or a['id'] in doc.get('refresh_meshes',[]):
  ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False;ui.import_as_skeletal=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.one_convex_hull_per_ucx=True;ui.static_mesh_import_data.reorder_material_to_fbx_order=True
  task=unreal.AssetImportTask();task.factory=unreal.FbxFactory();task.filename=file;task.destination_path=DEST;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False;task.options=ui;at.import_asset_tasks([task])
 m=unreal.load_asset(key)
 if not isinstance(m,unreal.StaticMesh):raise RuntimeError('Mesh import failed '+key)
 byname={p.rsplit('/',1)[-1].split('.')[0]:p for p in paths}
 # FBX reimport retains unused original slots; keep those assigned as well.
 for material_name in doc['materials']:byname.setdefault(material_name,DEST+'/'+material_name+'.'+material_name)
 for n,p in zip(a['material_names'],a['material_slots']):byname[n]=p;byname[n.replace('.','_')]=p
 slots=list(m.get_editor_property('static_materials'))
 material_changed=False
 for i,s in enumerate(slots):
  names=[str(s.get_editor_property(k))for k in ['imported_material_slot_name','material_slot_name']];path=next((byname[n]for n in names if n in byname),None)
  if not path:path=next((byname.get(re.sub(r'[._][0-9]{3}$','',n))for n in names if byname.get(re.sub(r'[._][0-9]{3}$','',n))),None)
  if not path and a['asset_name'].startswith('COL_'):continue
  if not path:
   for n in names:
    stem=re.sub(r'[._][0-9]{3}$','',n); candidate='/Game/SeniorSendOff/House/Materials/'+stem+'.'+stem
    if stem.startswith('MI_SSO_') and ed.does_asset_exist(candidate):path=candidate;break
  if not path:raise RuntimeError('Unmapped material '+name+' '+str(names))
  ma=unreal.load_asset(path)
  if not ma:raise RuntimeError('Missing material '+path)
  if s.get_editor_property('material_interface')!=ma:s.set_editor_property('material_interface',ma);material_changed=True
 if material_changed:m.set_editor_property('static_materials',slots)
 if a['damage'] and me.get_convex_collision_count(m)<1:raise RuntimeError('Missing fitted UCX '+name)
 bs=m.get_editor_property('body_setup');bs.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if a['interactive'] else unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 m.set_editor_property('support_ray_tracing',False)
 m.set_editor_property('allow_cpu_access',a['id']=='FlavortownBanner')
 # Imported meshes can finish their asynchronous build during the first save.
 # Retry a failed write with only_if_dirty=False so the package is really saved.
 saved=ed.save_loaded_asset(m,False)
 for attempt in range(2):
  if saved:break
  unreal.log_warning('Retrying imported mesh save '+name);saved=ed.save_loaded_asset(m,False)
 if not saved:raise RuntimeError('Mesh save failed '+name)
 return m
cache={};audit=[]
for a in doc['assets']:
 meshes=[import_mesh(a)]
 if a['damage']:
  for field in ['damaged','heavily_damaged','broken']:meshes.append(import_mesh(a,field,a[field+'_asset_name']))
 cache[a['id']]=meshes;audit.append(dict(asset=a['id'],faces=a['faces'],meshes=[m.get_name()for m in meshes],hulls=[me.get_convex_collision_count(m)for m in meshes]))
 unreal.log('BWR_IMPORT '+a['id'])
# Map edits are restricted to the old BW_ room and this new room's tag.
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
if not world:raise RuntimeError('House load failed')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def state(a):
 p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d();d=dict(cls=a.get_class().get_name(),pos=[p.x,p.y,p.z],rot=[r.pitch,r.yaw,r.roll],scale=[s.x,s.y,s.z])
 if isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component;d.update(mesh=c.static_mesh.get_path_name()if c.static_mesh else None,collision_enabled=str(c.get_collision_enabled()),collision_object=str(c.get_collision_object_type()),responses=[str(c.get_collision_response_to_channel(getattr(unreal.CollisionChannel,n)))for n in ['ECC_WORLD_STATIC','ECC_WORLD_DYNAMIC','ECC_PAWN','ECC_VISIBILITY','ECC_CAMERA','ECC_PHYSICS_BODY','ECC_VEHICLE','ECC_DESTRUCTIBLE']],materials=[m.get_path_name()if m else None for m in c.get_materials()])
 if isinstance(a,unreal.ExistingHouseDoor):d.update(door=str(a.get_editor_property('door_id')),locked=a.get_editor_property('locked'),closed=a.get_editor_property('closed_yaw'),opened=a.get_editor_property('open_yaw'))
 return d
before={a.get_actor_label():state(a)for a in actors.get_all_level_actors()if not a.get_actor_label().startswith('BW_') and 'BoywickPhotoRoom'not in[str(t)for t in a.tags]}
# Spawning physics actors can normalize a preexisting Custom profile to BlockAll.
# Cache complete responses and restore them on unrelated components before saving.
channels=[getattr(unreal.CollisionChannel,n) for n in ['ECC_WORLD_STATIC','ECC_WORLD_DYNAMIC','ECC_PAWN','ECC_VISIBILITY','ECC_CAMERA','ECC_PHYSICS_BODY','ECC_VEHICLE','ECC_DESTRUCTIBLE']]
protected_collision=[]
for a in actors.get_all_level_actors():
 if a.get_actor_label()in before and isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component
  protected_collision.append((c,c.get_collision_profile_name(),c.get_collision_enabled(),c.get_collision_object_type(),[c.get_collision_response_to_channel(ch)for ch in channels]))
removed=[]
for a in actors.get_all_level_actors():
 if a.get_actor_label().startswith('BW_')or'BoywickPhotoRoom'in[str(t)for t in a.tags]:removed.append(a.get_actor_label());actors.destroy_actor(a)
count=collections.Counter();defs={a['id']:a for a in doc['assets']};placed=[]
for p in doc['placements']:
 d=defs[p['asset']];count[p['asset']]+=1;ident='BWR_'+p['asset']+'_%02d'%count[p['asset']];loc=unreal.Vector(*p['location']);rot=unreal.Rotator(pitch=p.get('pitch',0),yaw=p['yaw'],roll=p.get('roll',0))
 tags=[unreal.Name('BoywickPhotoRoom')]
 if d['interactive']:
  if d['mass_kg']<4:tags.append(unreal.Name('SSOBoywickSmallItem'))
  a=actors.spawn_actor_from_class(unreal.HouseInteractiveProp,loc,rot);a.set_editor_property('tags',tags)
  for key,val in [('prop_id',unreal.Name(ident)),('prop_name',p['name']),('mass_kg',float(d['mass_kg'])),('break_health',float(d['break_health'])),('detach_health',float(max(3,d['break_health']*.35))),('movable',True),('wall_mounted',d['wall']),('breakable',d['damage'])]:a.set_editor_property(key,val)
  for key,m in zip(['intact_mesh','damaged_mesh','heavily_damaged_mesh','broken_mesh'],cache[p['asset']]):a.set_editor_property(key,m)
  a.refresh_prop()
 else:
  a=actors.spawn_actor_from_class(unreal.StaticMeshActor,loc,rot);c=a.static_mesh_component;c.set_static_mesh(cache[p['asset']][0]);c.set_collision_profile_name('BlockAll'if d['collision']else'NoCollision');c.set_editor_property('generate_overlap_events',False);a.set_editor_property('tags',tags)
  if not d['collision']:c.set_editor_property('cast_shadow',p['asset']not in['StripedRug','WindowBlinds'])
 a.set_actor_label(ident);a.set_actor_scale3d(unreal.Vector(*p.get('scale',[1,1,1])));a.set_folder_path('Second floor/Boy Wick photo room');placed.append(dict(label=ident,name=p['name'],location=p['location'],interactive=d['interactive']))
for p in doc.get('external',[]):
 a=actors.spawn_actor_from_class(unreal.HouseInteractiveProp,unreal.Vector(*p['location']),unreal.Rotator(pitch=0,yaw=p['yaw'],roll=0));a.set_actor_label('BWR_GreenDeskChair');a.set_folder_path('Second floor/Boy Wick photo room');a.set_editor_property('tags',[unreal.Name('BoywickPhotoRoom')]);a.set_editor_property('prop_id',unreal.Name('BWR_GreenDeskChair'));a.set_editor_property('prop_name',p['name']);a.set_editor_property('mass_kg',float(p['mass']));a.set_editor_property('break_health',26.0);a.set_editor_property('movable',True);a.set_editor_property('breakable',True)
 for key,suffix in [('intact_mesh',''),('damaged_mesh','_DamageV2'),('heavily_damaged_mesh','_HeavyV2'),('broken_mesh','_BrokenV2')]:
  m=unreal.load_asset(p['asset']+suffix)
  if m:a.set_editor_property(key,m)
 a.refresh_prop();placed.append(dict(label=a.get_actor_label(),name=p['name'],location=p['location'],interactive=True))
for name,loc,intensity,radius,color in [('Ceiling',[-2683,-991,523],1700,770,[1,.955,.87]),('Window',[-2885,-1263.8,460],180,360,[.89,.94,1])]:
 a=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*loc));a.set_actor_label('BWR_'+name+'Light');a.set_folder_path('Second floor/Boy Wick photo room');a.set_editor_property('tags',[unreal.Name('BoywickPhotoRoom')]);c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(intensity);c.set_attenuation_radius(radius);c.set_light_color(unreal.LinearColor(*color,1));c.set_editor_property('cast_shadows',False);c.set_editor_property('source_radius',24)
for c,profile,enabled,object_type,responses in protected_collision:
 if str(c.get_collision_profile_name())!=str(profile):
  c.set_collision_profile_name(profile);c.set_collision_enabled(enabled);c.set_collision_object_type(object_type)
  for channel,response in zip(channels,responses):c.set_collision_response_to_channel(channel,response)
after={a.get_actor_label():state(a)for a in actors.get_all_level_actors()};bad=[n for n,d in before.items()if after.get(n)!=d]
if bad:
 (R/'evidence/protected-diff.json').write_text(json.dumps({n:{'before':before[n],'after':after.get(n)}for n in bad},indent=2))
 raise RuntimeError('Protected actors changed '+str(bad))
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-2415,-1235,456),unreal.Rotator(pitch=-3,yaw=144,roll=0))
if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House'):raise RuntimeError('House save failed')
(R/'evidence/installed.json').write_text(json.dumps(dict(passed=True,removed=removed,placed=placed,protected_unchanged=len(before),assets=audit),indent=2));unreal.log('BOYWICK_ROOM_INSTALLED removed=%d placed=%d protected=%d'%(len(removed),len(placed),len(before)))
