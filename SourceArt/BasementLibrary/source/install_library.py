"""Install the reference library, preserving the rest of the saved house."""
import unreal,json,re,collections
from pathlib import Path
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library');doc=json.loads((R/'manifest.json').read_text());DEST='/Game/SeniorSendOff/House/BasementLibrary'
ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);ml=unreal.MaterialEditingLibrary
def imported_texture(file,mask=False):
 name='T_Lib_'+Path(file).stem;key=DEST+'/Textures/'+name
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
  if not path and a['asset_name'].startswith('COL_'):continue
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
 unreal.log('LIBRARY_IMPORT '+a['id'])
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
if not world:raise RuntimeError('House map load failed')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);removed=[];new=[]
# Remove only the furnishings in this room. Other basement rooms are untouched.
for a in actors.get_all_level_actors():
 if 'SSOLibraryRebuild' in [str(t)for t in a.tags]:actors.destroy_actor(a);continue
 if not isinstance(a,(unreal.HouseInteractiveProp,unreal.HouseReactiveFixture)):continue
 c,e=a.get_actor_bounds(False)
 if -3050<c.x<-2370 and -1430<c.y<776 and c.z<-100:
  removed.append(dict(label=a.get_actor_label(),center=[c.x,c.y,c.z]));actors.destroy_actor(a)
old={a.get_actor_label():a for a in actors.get_all_level_actors()}
for a in doc['assets']:
 if not a.get('source_actor'):continue
 actor=old.get(a['source_actor'])
 if not isinstance(actor,unreal.StaticMeshActor):raise RuntimeError('Original shell actor missing '+a['source_actor'])
 actor.static_mesh_component.set_static_mesh(cache[a['id']][0]);actor.set_actor_scale3d(unreal.Vector(1,1,1));actor.static_mesh_component.set_editor_property('override_materials',[])
 actor.set_editor_property('tags',list(actor.tags)+([] if 'LibraryPhotoShell' in [str(t)for t in actor.tags] else [unreal.Name('LibraryPhotoShell')]))
count=collections.Counter()
definitions={a['id']:a for a in doc['assets']}
for p in doc['placements']:
 d=definitions[p['asset']];count[p['asset']]+=1;ident=p.get('prop_id','Library_'+p['asset']+'_%03d'%count[p['asset']]);loc=unreal.Vector(*p['location']);rot=unreal.Rotator(pitch=p.get('pitch',0),yaw=p['yaw'],roll=p.get('roll',0))
 if d['damage']:
  a=actors.spawn_actor_from_class(unreal.HouseInteractiveProp,loc,rot);a.set_actor_label('Interactive_'+ident);a.set_editor_property('prop_id',unreal.Name(ident));a.set_editor_property('prop_name',p['name']);a.set_editor_property('mass_kg',float(d['mass_kg']));a.set_editor_property('break_health',float(d['break_health']));a.set_editor_property('detach_health',float(d['break_health']*.35));a.set_editor_property('movable',d['movable']);a.set_editor_property('wall_mounted',d['wall'] if p['asset']!='FramedGroupPhoto' else False);a.set_editor_property('breakable',True)
  for k,m in zip(['intact_mesh','damaged_mesh','heavily_damaged_mesh','broken_mesh'],cache[p['asset']]):a.set_editor_property(k,m)
  a.refresh_prop();new.append(dict(label=a.get_actor_label(),asset=p['asset'],location=p['location'],mass=d['mass_kg']))
 else:
  a=actors.spawn_actor_from_class(unreal.StaticMeshActor,loc,rot);a.set_actor_label(ident);a.static_mesh_component.set_static_mesh(cache[p['asset']][0])
  a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if p['asset']=='ServiceAnnexShell' else unreal.CollisionEnabled.NO_COLLISION)
 a.set_actor_scale3d(unreal.Vector(*p.get('scale',[1,1,1])))
 a.set_editor_property('tags',[unreal.Name('SSOLibraryRebuild')]+([unreal.Name('SSOLibrarySmallItem')]if d.get('small_item')else [])+([unreal.Name('SSOLibraryShelf')]if p['asset']=='SixShelfBookcase'else [])+([unreal.Name('ShelfContents_'+p['support_prop_id'])]if p.get('support_prop_id')else []));a.set_folder_path('Basement/ReferenceLibrary')
 if p['asset']=='CeilingHatchHardware':a.set_editor_property('tags',[unreal.Name('SSOLibraryRebuild'),unreal.Name('LibraryCeilingPanel')])
for d in doc['assets']:
 if d['behavior']!='door':continue
 a=actors.spawn_actor_from_class(unreal.ExistingHouseDoor,unreal.Vector(*d['hinge']));a.set_actor_label('Library_MechanicalServiceDoor');a.set_editor_property('original_leaf',cache[d['id']][0]);a.set_editor_property('door_id',unreal.Name('BasementLibrary_Mechanical'));a.set_editor_property('doorway_center',unreal.Vector(*d['doorway_center']));a.set_editor_property('closed_yaw',float(d['closed_yaw']));a.set_editor_property('open_yaw',float(d['open_yaw']));a.set_editor_property('locked',False);a.set_editor_property('panel_center_world',unreal.Vector(*d['panel_center']));a.set_editor_property('panel_half_extent',unreal.Vector(*d['panel_half_extent']));a.set_editor_property('tags',[unreal.Name('SSOLibraryRebuild')]);a.set_folder_path('Basement/ReferenceLibrary');a.set_actor_location(unreal.Vector(*d['hinge']),False,True)
 a.set_actor_location(unreal.Vector(d['hinge'][0]+.01,d['hinge'][1],d['hinge'][2]),False,False);a.set_actor_location(unreal.Vector(*d['hinge']),False,False)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-2640,560,-164),unreal.Rotator(pitch=-5,yaw=-95))
if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House'):raise RuntimeError('House map save failed')
(R/'installed.json').write_text(json.dumps(dict(passed=True,removed=removed,new_furniture=new,placements=len(doc['placements']),assets=audit,service_door=True,ceiling_access_panels=3),indent=2))
unreal.log('LIBRARY_INSTALLED '+str(len(new)))
