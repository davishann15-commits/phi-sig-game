"""Install the reference kitchen, preserving the rest of the saved house."""
import unreal,json,re,collections
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen');doc=json.loads((R/'manifest.json').read_text());DEST='/Game/SeniorSendOff/House/ReferenceKitchen'
ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);ml=unreal.MaterialEditingLibrary
def imported_texture(file,mask=False):
 name='T_Kitchen_'+Path(file).stem;key=DEST+'/Textures/'+name
 if not ed.does_asset_exist(key) or Path(file).stem in doc.get('refresh_textures',[]):
  task=unreal.AssetImportTask();task.filename=file;task.destination_path=DEST+'/Textures';task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True;at.import_asset_tasks([task])
 t=unreal.load_asset(key)
 if not isinstance(t,unreal.Texture2D):raise RuntimeError('Missing texture '+key)
 t.set_editor_property('max_texture_size',2048 if Path(file).stem.startswith('IMG_') else 1024)
 if mask:t.set_editor_property('srgb',False);t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
 ed.save_loaded_asset(t);return t
def constant(m,key,value,prop):
 node=ml.create_material_expression(m,unreal.MaterialExpressionConstant);node.set_editor_property('r',float(value));ml.connect_material_property(node,'',prop)
def texture(m,file,prop,mask=False):
 n=ml.create_material_expression(m,unreal.MaterialExpressionTextureSample);n.texture=imported_texture(file,mask)
 if mask:n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
 ml.connect_material_property(n,'R' if mask else 'RGB',prop)
for stem in doc.get('refresh_textures',[]):
 imported_texture(str(R/'textures'/(stem+'.png')),'Roughness' in stem)
for name,d in doc['materials'].items():
 key=DEST+'/'+name
 if ed.does_asset_exist(key) and name not in doc.get('refresh_materials',[]):continue
 m=unreal.load_asset(key) if ed.does_asset_exist(key) else at.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
 ml.delete_all_material_expressions(m)
 if d.get('texture'):texture(m,d['texture'],unreal.MaterialProperty.MP_BASE_COLOR)
 else:
  n=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*d['base_color'],1);ml.connect_material_property(n,'',unreal.MaterialProperty.MP_BASE_COLOR)
 if d.get('roughness_texture'):texture(m,d['roughness_texture'],unreal.MaterialProperty.MP_ROUGHNESS,True)
 else:constant(m,'rough',d['roughness'],unreal.MaterialProperty.MP_ROUGHNESS)
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
 if not ed.does_asset_exist(key) or a['id'] in doc.get('refresh_assets',[]):
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
 bs=m.get_editor_property('body_setup');bs.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if a['damage'] else unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 m.set_editor_property('support_ray_tracing',False)
 if not ed.save_loaded_asset(m):raise RuntimeError('Mesh save failed '+name)
 return m
cache={};audit=[]
for a in doc['assets']:
 meshes=[import_mesh(a)]
 if a['damage']:
  for field in ['damaged','heavily_damaged','broken']:meshes.append(import_mesh(a,field,a[field+'_asset_name']))
 cache[a['id']]=meshes;audit.append(dict(asset=a['id'],faces=a['faces'],meshes=[m.get_name()for m in meshes],hulls=[me.get_convex_collision_count(m)for m in meshes]))
 unreal.log('KITCHEN_IMPORT '+a['id'])
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
if not world:raise RuntimeError('House map load failed')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);removed=[];new=[]
# Remove exact old kitchen components. Stored closet props, other rooms, ceiling lights and windows remain.
ids=[21,22,23,24,25,26,27,28,29,30,46,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,106,107,123,124,125,128,129,130,131,133,134,136,140,155,156,157,158,159,160]
labels={'Interactive_SMF_Entrance_ALL__%03d'%i for i in ids}
labels.add('Interactive_SM_Entrance_p0_p1_Glass__001') # Original television screen, not a window.
for a in actors.get_all_level_actors():
 if 'SSOKitchenRefit' in [str(t)for t in a.tags] or a.get_actor_label() in labels:
  removed.append(dict(label=a.get_actor_label()));actors.destroy_actor(a)
old={a.get_actor_label():a for a in actors.get_all_level_actors()}
for a in doc['assets']:
 if not a.get('source_actor'):continue
 actor=old.get(a['source_actor'])
 if not isinstance(actor,unreal.StaticMeshActor):raise RuntimeError('Original shell actor missing '+a['source_actor'])
 actor.static_mesh_component.set_static_mesh(cache[a['id']][0]);actor.set_actor_scale3d(unreal.Vector(1,1,1));actor.static_mesh_component.set_editor_property('override_materials',[])
 if a['source_actor'].startswith('COL_'):
  actor.static_mesh_component.set_visibility(False);actor.static_mesh_component.set_hidden_in_game(True);actor.static_mesh_component.set_collision_profile_name('BlockAll')
 actor.set_editor_property('tags',list(actor.tags)+([] if 'KitchenPhotoShell' in [str(t)for t in actor.tags] else [unreal.Name('KitchenPhotoShell')]))
count=collections.Counter()
definitions={a['id']:a for a in doc['assets']}
for p in doc['placements']:
 d=definitions[p['asset']];count[p['asset']]+=1;ident='Kitchen_'+p['asset']+'_%03d'%count[p['asset']];loc=unreal.Vector(*p['location']);rot=unreal.Rotator(pitch=p.get('pitch',0),yaw=p['yaw'],roll=p.get('roll',0))
 if p['asset']=='WindowBlinds':
  a=actors.spawn_actor_from_class(unreal.HouseReactiveFixture,loc,rot);a.set_editor_property('fixture_id',unreal.Name(ident));a.set_editor_property('fixture_kind',unreal.HouseReactiveFixtureKind.CURTAIN);a.set_editor_property('original_mesh',cache[p['asset']][0]);a.set_editor_property('curtain_axis',2);a.set_editor_property('curtain_gathered_scale',.07);a.set_editor_property('motion_pivot_local',unreal.Vector(0,0,153));a.refresh_fixture()
 elif d['damage']:
  a=actors.spawn_actor_from_class(unreal.HouseInteractiveProp,loc,rot);a.set_actor_label('Interactive_'+ident);a.set_editor_property('prop_id',unreal.Name(ident));a.set_editor_property('prop_name',p['name']);a.set_editor_property('mass_kg',float(d['mass_kg']));a.set_editor_property('break_health',float(d['break_health']));a.set_editor_property('detach_health',float(d['break_health']*.35));a.set_editor_property('movable',d['movable']);a.set_editor_property('wall_mounted',d['wall']);a.set_editor_property('breakable',True)
  for k,m in zip(['intact_mesh','damaged_mesh','heavily_damaged_mesh','broken_mesh'],cache[p['asset']]):a.set_editor_property(k,m)
  a.refresh_prop();new.append(dict(label=a.get_actor_label(),asset=p['asset'],location=p['location'],mass=d['mass_kg']))
 else:
  a=actors.spawn_actor_from_class(unreal.StaticMeshActor,loc,rot);a.set_actor_label(ident);a.static_mesh_component.set_static_mesh(cache[p['asset']][0])
  a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if p['asset']=='ServiceAnnexShell' else unreal.CollisionEnabled.NO_COLLISION)
 a.set_actor_scale3d(unreal.Vector(*p.get('scale',[1,1,1])))
 a.set_editor_property('tags',[unreal.Name('SSOKitchenRefit')]);a.set_folder_path('First floor/Reference kitchen')
 if p['asset']=='CeilingHatchHardware':a.set_editor_property('tags',[unreal.Name('SSOKitchenRefit'),unreal.Name('LibraryCeilingPanel')])
for d in doc['assets']:
 if d['behavior']!='door':continue
 a=actors.spawn_actor_from_class(unreal.ExistingHouseDoor,unreal.Vector(*d['hinge']));a.set_actor_label('Kitchen_ClosedClosetDoor');a.set_editor_property('original_leaf',cache[d['id']][0]);a.set_editor_property('door_id',unreal.Name('ReferenceKitchen_Closet'));a.set_editor_property('doorway_center',unreal.Vector(*d['doorway_center']));a.set_editor_property('closed_yaw',float(d['closed_yaw']));a.set_editor_property('open_yaw',float(d['open_yaw']));a.set_editor_property('locked',True);a.set_editor_property('panel_center_world',unreal.Vector(*d['panel_center']));a.set_editor_property('panel_half_extent',unreal.Vector(*d['panel_half_extent']));a.set_editor_property('tags',[unreal.Name('SSOKitchenRefit')]);a.set_folder_path('First floor/Reference kitchen');a.set_actor_location(unreal.Vector(d['hinge'][0]+.01,d['hinge'][1],d['hinge'][2]),False,False);a.set_actor_location(unreal.Vector(*d['hinge']),False,False)
# Explicitly serialize native component state after configuring the new leaf.
# Python property assignment alone does not rerun a native actor's construction.
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  if c.get_name()=='OriginalDoorLeaf':
   c.set_static_mesh(cache[d['id']][0]);c.set_relative_scale3d(unreal.Vector(1.8,1.8,1));c.set_relative_location(unreal.Vector(*[-k for k in d['hinge']]),False,False);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 for c in a.get_components_by_class(unreal.BoxComponent):
  if c.get_name()=='MovingPanelCollision':
   c.set_box_extent(unreal.Vector(*d['panel_half_extent']),False);c.set_relative_location(unreal.Vector(*[v-h for v,h in zip(d['panel_center'],d['hinge'])]),False,False);c.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY);c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN,unreal.CollisionResponse.ECR_BLOCK)
 for c in a.get_components_by_class(unreal.SceneComponent):
  if c.get_name()=='LeafPivot':c.set_relative_rotation(unreal.Rotator(pitch=0,yaw=d['closed_yaw'],roll=0),False,False)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(2330,-272,163),unreal.Rotator(pitch=-1,yaw=78))
if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House'):raise RuntimeError('House map save failed')
(R/'installed.json').write_text(json.dumps(dict(passed=True,removed=removed,new_furniture=new,placements=len(doc['placements']),assets=audit,closed_closet=True,interactive_blinds=5),indent=2))
unreal.log('KITCHEN_INSTALLED '+str(len(new)))
