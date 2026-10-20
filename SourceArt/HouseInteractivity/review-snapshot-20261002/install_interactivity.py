"""Install extracted original furnishings without changing protected house geometry."""
import unreal,json,os,math
from pathlib import Path
ROOT=Path('/Users/Stewart/Projects/Senior sendoff/outputs/house-interactivity')
MAN=json.load(open(ROOT/'extraction-manifest.json'))
NATIVE=json.load(open(ROOT/'context/actors.json'))
DEST='/Game/SeniorSendOff/House/Interactivity'
asset=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
meshedit=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)

def transform(d):
 return unreal.Transform(location=unreal.Vector(*d['location']),rotation=unreal.Rotator(*d['rotation']),scale=unreal.Vector(*d['scale']))
def vec(v):return[v.x,v.y,v.z]
def loadmat(path):return unreal.load_asset(path) if path else None
def piece_materials(p):
 source=next((a for a in NATIVE['actors'] if a['label']==p['source_actor_label']),None)
 if not source or not source['components']:return p['material_slots']
 comp=source['components'][0]
 defaults=NATIVE['meshes'][comp['mesh']]['materials']
 overrides={d['material']:override for d,override in zip(defaults,comp['materials']) if override and override!=d['material']}
 return [overrides.get(m,m) for m in p['material_slots']]


def import_mesh(path,name,materials,movable=False):
 key=DEST+'/'+name
 if asset.does_asset_exist(key):
  m=unreal.load_asset(key)
  bs=m.get_editor_property('body_setup')
  expected=unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if movable else unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
  same=all(m.get_material(i)==loadmat(mat) for i,mat in enumerate(materials))
  if same and bs and bs.get_editor_property('collision_trace_flag')==expected and (not movable or meshedit.get_convex_collision_count(m)+meshedit.get_simple_collision_count(m)>0):return m
 else:
  ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False;ui.import_materials=False;ui.import_textures=False
  ui.static_mesh_import_data.auto_generate_collision=False
  ui.static_mesh_import_data.combine_meshes=True
  ui.static_mesh_import_data.one_convex_hull_per_ucx=True
  task=unreal.AssetImportTask();task.filename=path;task.destination_path=DEST;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False;task.options=ui
  tools.import_asset_tasks([task]);m=unreal.load_asset(key)
 if not isinstance(m,unreal.StaticMesh):raise RuntimeError('Mesh import failed '+key)
 for i,mat in enumerate(materials):
  material=loadmat(mat)
  if material:m.set_material(i,material)
 bs=m.get_editor_property('body_setup')
 if movable:
  bs.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
  if meshedit.get_convex_collision_count(m)==0 and meshedit.get_simple_collision_count(m)==0:
   b=m.get_bounds();extent=b.box_extent
   if max(extent.x,extent.y)>170 and extent.z>28:
    meshedit.set_convex_decomposition_collisions(m,6,16,20000)
   else:meshedit.add_simple_collisions(m,unreal.ScriptCollisionShapeType.NDOP26)
 else:bs.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 # Furniture is small; it does not need permanent ray-tracing geometry memory.
 if hasattr(m,'support_ray_tracing'):m.set_editor_property('support_ray_tracing',False)
 if not asset.save_loaded_asset(m):raise RuntimeError('Asset save failed '+key)
 return m

mesh_cache={}
frame_cache={}
for p in MAN['pieces']:
 if p.get('breakaway_frame_asset_name'):
  frame_cache[p['id']]=import_mesh(p['breakaway_frame_fbx'],p['breakaway_frame_asset_name'],p['breakaway_frame_material_slots'],False)
for shared in MAN.get('shared_meshes',[]):
 import_mesh(shared['fbx'],shared['asset_name'],shared.get('material_slots',[]),False)
for p in MAN['pieces']:
 name=p['asset_name']
 if name in mesh_cache:continue
 furniture=p['behavior']=='furniture'
 intact=import_mesh(p['intact_fbx'],name,p['material_slots'],furniture)
 broken=import_mesh(p['broken_fbx'],name+'_Broken',p['material_slots'],furniture) if p.get('broken_fbx') else None
 damaged=import_mesh(p['damaged_fbx'],name+'_Damaged',p['material_slots'],furniture) if p.get('damaged_fbx') else None
 heavy=import_mesh(p['heavily_damaged_fbx'],name+'_HeavilyDamaged',p['material_slots'],furniture) if p.get('heavily_damaged_fbx') else None
 if furniture and (not damaged or not heavy):raise RuntimeError('Missing damage stages '+name)
 mesh_cache[name]=(intact,broken,damaged,heavy)
 unreal.log('HOUSE_IMPORT '+name)
for r in MAN['remainders'] + MAN.get('collision_remainders',[]):
 source=next(a for a in NATIVE['actors'] if a['label']==r['source_actor_label'])
 mats=r.get('material_slots') or [source['components'][0]['materials'][i] for i in r['material_source_indices']]
 mesh_cache[r['asset_name']]=(import_mesh(r['fbx'],r['asset_name'],mats),None)

world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
if not world:raise RuntimeError('House map load failed')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
# Reruns replace only our own pieces and leave the authored baseline intact.
for label,a in list(actors.items()):
 if label.startswith('Interactive_'):
  for child in a.get_attached_actors():
   child.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD)
  unreal.EditorLevelLibrary.destroy_actor(a);del actors[label]
converted=set();report=[];hanging=[]
for p in MAN['pieces']:
 source_labels=p.get('source_actor_labels',[p['source_actor_label']]);converted.update(source_labels)
 t=transform(p['source_transform']);location=t.transform_location(unreal.Vector(*p['local_origin_cm']))
 rotation=unreal.Rotator(*p['source_transform']['rotation']);scale=unreal.Vector(*p['source_transform']['scale'])
 intact,broken,damaged,heavy=mesh_cache[p['asset_name']]
 if p['behavior']=='furniture':
  a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.HouseInteractiveProp,location,rotation)
  a.set_actor_scale3d(scale)
  mass=min(300,p['mass_kg']*abs(scale.x*scale.y*scale.z))
  wall=p.get('wall_mounted',False)
  health=24.0 if mass<8 else 32.0 if mass<30 else 48.0 if mass<65 else 72.0 if mass<130 else 96.0
  # Mounting strength depends on actual hanging surface area as well as mass.
  bounds=p['bounds_blender_metres'];dims=sorted(abs(bounds[1][i]-bounds[0][i])*abs(p['source_transform']['scale'][i]) for i in range(3))
  area=dims[1]*dims[2]
  detach=8.0 if area<.35 else 16.0 if area<1.2 else 24.0 if area<2.5 else 40.0
  a.set_editor_property('wall_mounted',wall)
  a.set_editor_property('detach_health',detach)
  for key,value in {'intact_mesh':intact,'broken_mesh':broken,'damaged_mesh':damaged,'heavily_damaged_mesh':heavy,'prop_id':p['id'],'prop_name':p['name'],'movable':p.get('movable',True),'broken_version_movable':p.get('movable',True),'breakable':True,'break_health':health,'mass_kg':mass,'material_overrides':[loadmat(m)for m in piece_materials(p)]}.items():a.set_editor_property(key,value)
  a.refresh_prop()
 else:
  a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.HouseReactiveFixture,location,rotation)
  a.set_actor_scale3d(scale)
  a.set_editor_property('fixture_kind',getattr(unreal.HouseReactiveFixtureKind,p['behavior'].upper()))
  a.set_editor_property('fixture_id',p['id']);a.set_editor_property('original_mesh',intact)
  a.set_editor_property('material_overrides',[loadmat(m)for m in piece_materials(p)])
  a.set_editor_property('motion_pivot_local',unreal.Vector(*p.get('pivot_cm',[0,0,0])))
  if p['behavior']=='curtain':
   a.set_editor_property('curtain_axis',p['curtain_axis']);a.set_editor_property('curtain_gathered_scale',p.get('curtain_gathered_scale',.14));a.set_editor_property('initially_open',p.get('initially_open',False))
  if p['id'] in frame_cache:
   a.set_editor_property('breakaway_frame_mesh',frame_cache[p['id']])
   a.set_editor_property('breakaway_frame_materials',[loadmat(m)for m in p['breakaway_frame_material_slots']])
  a.refresh_fixture()
  if p['behavior']=='chandelier':hanging.append(a)
 a.set_actor_label('Interactive_'+p['id']);a.set_folder_path('House interactions/'+p['behavior'])
 a.tags=list(a.tags)+[unreal.Name('HouseInteractivityInstalled')]
 report.append({'label':a.get_actor_label(),'behavior':p['behavior'],'name':p['name'],'wall_mounted':p.get('wall_mounted',False),'location':vec(location),'intact':intact.get_path_name(),'broken':broken.get_path_name() if broken else None})
# Keep the existing great-room light sources attached to their actual pendants.
for label,light in actors.items():
 if label.startswith('GR_ChandelierWarmLight') and hanging:
  q=light.get_actor_location()
  fixture=min(hanging,key=lambda a:(a.get_actor_location()-q).length())
  if (fixture.get_actor_location()-q).length()<230:
   light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
   light.attach_to_component(fixture.get_editor_property('motion_pivot'),unreal.Name('None'),unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
remainders={r['source_actor_label']:r for r in MAN['remainders'] + MAN.get('collision_remainders',[])}
converted.update(remainders)
removed=[]
for label in sorted(converted):
 if label not in actors:continue
 a=actors[label]
 if label in remainders:
  r=remainders[label];a.static_mesh_component.set_static_mesh(mesh_cache[r['asset_name']][0]);a.static_mesh_component.set_editor_property('override_materials',[])
  if r.get('collision_remainder',False):
   a.static_mesh_component.set_visibility(False)
   a.static_mesh_component.set_hidden_in_game(True)
   a.static_mesh_component.set_collision_profile_name('BlockAll')
 else:unreal.EditorLevelLibrary.destroy_actor(a);removed.append(label)
for label in set(MAN['remove_colliders']):
 if label in actors and label not in removed:
  unreal.EditorLevelLibrary.destroy_actor(actors[label]);removed.append(label)
if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House'):raise RuntimeError('House map save failed')
json.dump({'installed':report,'converted_sources':sorted(converted),'removed':removed,'remainders':list(remainders),'count':len(report)},open(ROOT/'installed.json','w'),indent=2)
unreal.log('HOUSE_INTERACTIVITY_INSTALLED count='+str(len(report)))
