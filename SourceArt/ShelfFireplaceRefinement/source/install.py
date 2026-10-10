import unreal,json,re,traceback,collections
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ShelfFireplaceRefinement');DEST='/Game/SeniorSendOff/House/FireplaceRefinement';LIB='/Game/SeniorSendOff/House/BasementLibrary';doc=json.loads((R/'models-manifest.json').read_text())
ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);ml=unreal.MaterialEditingLibrary

def import_mesh(a,field='intact',destination=DEST):
 name=a.get(field+'_asset_name',a['asset_name']);paths=a.get(field+'_material_slots',a['material_slots']);names=a.get(field+'_material_names',a['material_names'])
 ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False;ui.import_as_skeletal=False;ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.one_convex_hull_per_ucx=True;ui.static_mesh_import_data.reorder_material_to_fbx_order=True
 t=unreal.AssetImportTask();t.factory=unreal.FbxFactory();t.filename=a[field+'_fbx'];t.destination_path=destination;t.destination_name=name;t.automated=True;t.replace_existing=True;t.options=ui;t.save=False;at.import_asset_tasks([t]);m=unreal.load_asset(destination+'/'+name);assert isinstance(m,unreal.StaticMesh)
 def clean(n):return re.sub(r'[._][0-9]{3}$','',n).replace('.','_')
 mapping={clean(n):p for n,p in zip(names,paths)};slots=list(m.static_materials)
 for slot in slots:
  n=clean(str(slot.get_editor_property('imported_material_slot_name')))
  if n not in mapping:raise RuntimeError('Missing material '+name+' '+n)
  material=unreal.load_asset(mapping[n]);assert material,mapping[n];slot.set_editor_property('material_interface',material)
 m.set_editor_property('static_materials',slots);m.set_editor_property('support_ray_tracing',False)
 m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if name in ['SM_FPR_ClassicFireplace',doc['shelf']['broken_asset_name']]else unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 if name in ['SM_FPR_ClassicFireplace',doc['shelf']['broken_asset_name']]:assert me.get_convex_collision_count(m)>=3,name
 assert ed.save_loaded_asset(m,False);return m

def install():
 for name,d in doc['materials'].items():
  key=DEST+'/'+name;m=unreal.load_asset(key)if ed.does_asset_exist(key)else at.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew());ml.delete_all_material_expressions(m)
  color=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);color.constant=unreal.LinearColor(*d['base_color'],1);ml.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
  for prop,key0 in [(unreal.MaterialProperty.MP_ROUGHNESS,'roughness'),(unreal.MaterialProperty.MP_METALLIC,'metallic')]:
   n=ml.create_material_expression(m,unreal.MaterialExpressionConstant);n.r=d[key0];ml.connect_material_property(n,'',prop)
  if d['emission']:
   n=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*[c*d['emission']for c in d['base_color']],1);ml.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  ml.recompile_material(m);assert ed.save_loaded_asset(m,False)
 wreck=import_mesh(doc['shelf'],'broken',LIB);fireplace=import_mesh(doc['fireplace']);shells={a['label']:import_mesh(a)for a in doc['retained']}
 world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');assert world
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);all0=actors.get_all_level_actors();bylabel={a.get_actor_label():a for a in all0}
 def sig(a):
  p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d();return (a.get_actor_label(),[round(v,3)for v in [p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z]])
 protected={a.get_path_name():sig(a)for a in all0}
 for label,mesh in shells.items():
  a=bylabel[label];a.static_mesh_component.set_static_mesh(mesh);a.static_mesh_component.set_editor_property('override_materials',[])
 cases=[]
 for a in all0:
  if isinstance(a,unreal.HouseInteractiveProp)and str(a.prop_id).startswith('Library_SixShelfBookcase_'):
   a.set_editor_property('broken_mesh',wreck);a.set_editor_property('tags',list(a.tags)+([]if' SSOLibraryShelf'.strip()in[str(t)for t in a.tags]else[unreal.Name('SSOLibraryShelf')]));a.refresh_prop();cases.append(a)
 assert len(cases)==6
 associations={str(c.prop_id):[]for c in cases}
 for a in all0:
  if not isinstance(a,unreal.HouseInteractiveProp)or'SSOLibrarySmallItem'not in[str(t)for t in a.tags]:continue
  p=a.get_actor_location();candidates=[]
  for c in cases:
   b,e=c.get_actor_bounds(False)
   if abs(p.x-b.x)<e.x+16 and abs(p.y-b.y)<e.y+16 and b.z-e.z-1<=p.z<=b.z+e.z+16:candidates.append(c)
  if not candidates:continue
  c=min(candidates,key=lambda c:(a.get_actor_location()-c.get_actor_location()).length());ident=str(c.prop_id);a.set_editor_property('tags',list(a.tags)+[unreal.Name('ShelfContents_'+ident)]);associations[ident].append(str(a.prop_id))
 assert min(map(len,associations.values()))>=40,associations
 for p in doc['fireplace']['placements']:
  a=bylabel.get(p['label'])or actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p['location']),unreal.Rotator(yaw=p['yaw']))
  a.set_actor_label(p['label']);a.static_mesh_component.set_static_mesh(fireplace);a.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC);a.static_mesh_component.set_collision_profile_name('BlockAll');a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS);a.set_editor_property('tags',[unreal.Name('GreatRoomSolidFireplace')]);a.set_folder_path('GreatRoom/Fireplaces')
 after={a.get_path_name():sig(a)for a in actors.get_all_level_actors()if a.get_path_name()in protected};assert after==protected,'Existing placement changed'
 assert sum(isinstance(a,unreal.HouseInteractiveProp)and'HouseWallTelevision'in[str(t)for t in a.tags]for a in all0)==2
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House')
 # Keep future library rebuilds on this damage asset and independent-content linkage.
 for path in [Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BasementLibrary/manifest.json'),Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library/manifest.json')]:
  lib=json.loads(path.read_text())
  for a in lib['assets']:
   if a['id']=='SixShelfBookcase':a.update({k:v for k,v in doc['shelf'].items()if k.startswith('broken_')})
  inverse={item:case for case,items in associations.items()for item in items}
  counts=collections.Counter()
  for p in lib['placements']:
   counts[p['asset']]+=1
   ident=p.get('prop_id','Library_'+p['asset']+'_%03d'%counts[p['asset']])
   if ident in inverse:p['support_prop_id']=inverse[ident]
  path.write_text(json.dumps(lib,indent=2))
 (R/'installed.json').write_text(json.dumps(dict(passed=True,shelves=len(cases),shelf_contents=associations,fireplaces=doc['fireplace'],unchanged_actor_transforms=len(protected),shells=doc['retained']),indent=2))
try:install()
except Exception:(R/'install-error.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc())
else:
 if(R/'install-error.txt').exists():(R/'install-error.txt').unlink()
ticks=0
def close(dt):
 global ticks
 ticks+=1
 if ticks>5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(close)
