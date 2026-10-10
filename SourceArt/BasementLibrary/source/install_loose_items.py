"""Replace only grouped library clutter; preserve the current saved house."""
import unreal,json,math,re,collections,traceback
from pathlib import Path
ROOT=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BasementLibrary')
OUT=ROOT/'loose-items';DEST='/Game/SeniorSendOff/House/BasementLibrary'
def install():
 global doc,ed,at,me
 base=json.loads((ROOT/'manifest.json').read_text());split=json.loads((OUT/'split-manifest.json').read_text())
 doc=dict(materials=base['materials'],refresh_assets=[a['id']for a in split['assets']+split['chairs']])
 ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
 code=(ROOT/'source/install_library.py').read_text();exec(code[code.index('def import_mesh('):code.index('cache={};audit=[]')],globals())
 cache={};audit=[]
 for a in split['assets']+split['chairs']:
  meshes=[import_mesh(a)]
  for field in ['damaged','heavily_damaged','broken']:meshes.append(import_mesh(a,field,a[field+'_asset_name']))
  cache[a['id']]=meshes
  audit.append(dict(asset=a['id'],hulls=[me.get_convex_collision_count(m)for m in meshes]))
  unreal.log('LOOSE_ITEMS_IMPORT '+a['id'])
 world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 if not world:raise RuntimeError('Could not load current house')
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 def signature(a):
  p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d()
  return (a.get_actor_label(),tuple(round(v,4)for v in [p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z]))
 protected={a.get_path_name():signature(a)for a in actors.get_all_level_actors()if 'SSOLibraryRebuild' not in [str(t)for t in a.tags]}
 definitions={a['id']:a for a in split['assets']};new_placements=[];removed=[];chairs=[]
 old_groups=[]
 for actor in actors.get_all_level_actors():
  if not isinstance(actor,unreal.HouseInteractiveProp) or 'SSOLibraryRebuild' not in [str(t)for t in actor.tags]:continue
  old_id=str(actor.get_editor_property('prop_id'))
  asset=next((k for k in split['replacements'] if old_id.startswith('Library_'+k+'_')),None)
  if asset:old_groups.append((actor,asset,old_id))
 expected=sum(1 for p in base['placements']if p['asset']in split['replacements'])
 if len(old_groups)!=expected:raise RuntimeError('Grouped library inventory changed: '+str((len(old_groups),expected)))
 for old,asset,old_id in old_groups:
  t=old.get_actor_transform();rot=old.get_actor_rotation();scale=old.get_actor_scale3d()
  for i,piece in enumerate(split['replacements'][asset]):
   d=definitions[piece['asset']];cx,cy,cz=piece['offset_blender_metres']
   loc=unreal.MathLibrary.transform_location(t,unreal.Vector(cx*100,-cy*100,cz*100))
   ident='Library_Loose_'+old_id.removeprefix('Library_')+'_%03d'%i
   a=actors.spawn_actor_from_class(unreal.HouseInteractiveProp,loc,rot)
   a.set_actor_label('Interactive_'+ident);a.set_editor_property('prop_id',unreal.Name(ident));a.set_editor_property('prop_name',piece['name'])
   a.set_editor_property('tags',[unreal.Name('SSOLibraryRebuild')]+([unreal.Name('SSOLibrarySmallItem')]if d['small_item']else []))
   a.set_editor_property('mass_kg',float(d['mass_kg']));a.set_editor_property('break_health',float(d['break_health']))
   a.set_editor_property('detach_health',float(d['break_health']*.35));a.set_editor_property('movable',True);a.set_editor_property('wall_mounted',False);a.set_editor_property('breakable',True)
   for key,m in zip(['intact_mesh','damaged_mesh','heavily_damaged_mesh','broken_mesh'],cache[d['id']]):a.set_editor_property(key,m)
   a.set_actor_scale3d(scale);a.refresh_prop();a.set_folder_path('Basement/ReferenceLibrary/LooseItems')
   assert not a.get_attach_parent_actor(),'Small item attached to furniture'
   new_placements.append(dict(asset=d['id'],name=piece['name'],prop_id=ident,location=[loc.x,loc.y,loc.z],yaw=rot.yaw,pitch=rot.pitch,roll=rot.roll,scale=[scale.x,scale.y,scale.z]))
  removed.append(old_id);actors.destroy_actor(old)
 small_mass={'DeskMouse':.15,'DeskOrganizer':.6,'BlueMarker':.1,'BlackMarker':.1,'DeskPen':.1,'PaperCup':.1,'CardDeck':.15,'RedCoffeeBook':.65,'GlassJar':.3,'SmallPottedPlant':1.2,'ShelfFramedPhoto':.8,'RedGameBookend':.8}
 for a in actors.get_all_level_actors():
  if not isinstance(a,unreal.HouseInteractiveProp) or 'SSOLibraryRebuild' not in [str(t)for t in a.tags]:continue
  ident=str(a.get_editor_property('prop_id'))
  chair=next((d for d in split['chairs']if ident.startswith('Library_'+d['id']+'_')),None)
  if chair:
   for key,m in zip(['intact_mesh','damaged_mesh','heavily_damaged_mesh','broken_mesh'],cache[chair['id']]):a.set_editor_property(key,m)
   a.refresh_prop();chairs.append(ident)
  mass=next((m for k,m in small_mass.items()if ident.startswith('Library_'+k+'_')),None)
  if mass is not None:
   a.set_editor_property('mass_kg',mass);a.set_editor_property('tags',list(a.tags)+([unreal.Name('SSOLibrarySmallItem')]if 'SSOLibrarySmallItem' not in [str(t)for t in a.tags]else []));a.refresh_prop()
 if len(chairs)!=11:raise RuntimeError('Rolling chair inventory mismatch: '+str(len(chairs)))
 after={a.get_path_name():signature(a)for a in actors.get_all_level_actors()if 'SSOLibraryRebuild' not in [str(t)for t in a.tags]}
 if after!=protected:
  (OUT/'protection-diff.json').write_text(json.dumps(dict(before={k:v for k,v in protected.items()if after.get(k)!=v},after={k:v for k,v in after.items()if protected.get(k)!=v}),indent=2))
  raise RuntimeError('An actor outside the library changed; see protection-diff.json')
 if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House'):raise RuntimeError('House map save failed')
 # Preserve this authored change through future reference-library rebuilds.
 base['placements']=[p for p in base['placements']if p['asset']not in split['replacements']]+new_placements
 base['assets']=[a for a in base['assets']if a['id']not in split['replacements']and a['id']not in [d['id']for d in split['chairs']]]+split['assets']+split['chairs']
 for d in base['assets']:
  if d['id']in small_mass:d.update(small_item=True,mass_kg=small_mass[d['id']])
 base['refresh_assets']=[]
 for path in [ROOT/'manifest.json',Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library/manifest.json')]:path.write_text(json.dumps(base,indent=2))
 (OUT/'installed.json').write_text(json.dumps(dict(passed=True,removed_groups=removed,individual_items=len(new_placements),narrowed_chairs=chairs,assets=audit,protected_actors=len(protected)),indent=2))
 unreal.log('LIBRARY_LOOSE_ITEMS_INSTALLED '+str(len(new_placements)))
try:install()
except Exception:
 error=traceback.format_exc();unreal.log_error(error);(OUT/'install-error.json').write_text(json.dumps(dict(error=error),indent=2))
else:
 if (OUT/'install-error.json').exists():(OUT/'install-error.json').unlink()
ticks=0
def close_ready(dt):
 global ticks
 ticks+=1
 if ticks>=5:unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(close_ready)
