"""Import authored periodic hallway and preserve the house outside its end room.

Root runs this only after compiling HouseImpossibleHallway and backing up the
saved map. This script does not modify the original door or its registry.
"""
import unreal,json,os,re,hashlib
from pathlib import Path

PROJECT=Path(unreal.Paths.project_dir()).resolve()
BASE=Path('/Users/Stewart/Projects/Senior sendoff/outputs/impossible-hallway')
SOURCE=PROJECT/'SourceArt/ImpossibleHallway'
DEST='/Game/SeniorSendOff/House/ImpossibleHallway'
PLAN=json.load(open(SOURCE/'import_manifest.json'))
runtime_cls=unreal.load_class(None,'/Script/SeniorSendoff.HouseImpossibleHallway')
if not runtime_cls:raise RuntimeError('Compile HouseImpossibleHallway before applying this map change')

# Existing vetted material authoring helpers, definitions only. Its apply body
# is never evaluated; our source/destination and manifest are supplied below.
exec(open(PROJECT/'Content/Python/apply_bedroom.py').read().split('textures={}')[0])
BASE=Path('/Users/Stewart/Projects/Senior sendoff/outputs/impossible-hallway')
SOURCE=PROJECT/'SourceArt/ImpossibleHallway'
DEST='/Game/SeniorSendOff/House/ImpossibleHallway'
PLAN=json.load(open(SOURCE/'import_manifest.json'))

textures={}
for filename in PLAN['textures']:
 tex=imp(str(SOURCE/'Art'/filename),Path(filename).stem)
 if 'Normal' in filename:
  tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('srgb',False)
 elif 'Roughness' in filename:tex.set_editor_property('srgb',False)
 A.save_loaded_asset(tex);textures[filename]=tex
materials={name:make_material(name,spec,textures) for name,spec in PLAN['materials'].items()}
black=make_material('IH_M_DistantBlack',{'color':[0,0,0],'roughness':1,'specular':0,'emission':1},textures)
black.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);E.recompile_material(black);A.save_loaded_asset(black)
meshes={};bounds_audit={}
for row in PLAN['meshes']:
 mesh=imp(str(SOURCE/'Art'/row['file']),row['name'])
 if row.get('complex_collision'):
  mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 for i,slot in enumerate(mesh.static_materials):
  name=re.sub(r'_\d{3}$','',str(slot.material_slot_name).split('.')[0])
  if row.get('complex_collision'):
   m=unreal.load_asset('/Engine/EngineMaterials/WorldGridMaterial')
  else:
   m=materials.get(name) or unreal.load_asset('/Game/SeniorSendOff/House/Materials/'+name)
   if not m:raise RuntimeError('Unknown preserved material '+name+' on '+row['name'])
  mesh.set_material(i,m)
 b=mesh.get_bounds();lo=b.origin-b.box_extent;hi=b.origin+b.box_extent
 actual=[[lo.x,lo.y,lo.z],[hi.x,hi.y,hi.z]];expected=[row['boundsCm']['min'],row['boundsCm']['max']]
 err=max(abs(actual[i][j]-expected[i][j]) for i in range(2) for j in range(3))
 if err>.20:raise RuntimeError('Mesh transform mismatch '+row['name']+str(err))
 bounds_audit[row['name']]={'boundsCm':actual,'maxErrorCm':err}
 A.save_loaded_asset(mesh);meshes[row['name']]=mesh

audio_path=BASE/'audio/IH_PipeDrone.wav'
if not audio_path.exists():raise RuntimeError('Root pipe drone is not yet ready')
sound=imp(str(audio_path),'IH_PipeDrone');sound.set_editor_property('looping',True);A.save_loaded_asset(sound)

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
allactors=unreal.EditorLevelLibrary.get_all_level_actors()
actors={a.get_actor_label():a for a in allactors}
def full_state(a):
 r=state(a)
 if isinstance(a,unreal.PointLight):
  c=a.get_component_by_class(unreal.PointLightComponent)
  color=c.light_color;r['light']=[c.intensity,c.attenuation_radius,[color.r,color.g,color.b,color.a],c.cast_shadows]
 if isinstance(a,unreal.ExistingHouseDoor):
  for key in('original_leaf','original_glass','added_handle_mesh','doorway_center','panel_center_world','panel_half_extent'):
   v=a.get_editor_property(key)
   if isinstance(v,unreal.Object):r[key]=v.get_path_name()
   elif isinstance(v,unreal.Vector):r[key]=vec(v)
   else:r[key]=v if isinstance(v,(bool,int,float))or v is None else str(v)
 return r
before={k:full_state(a) for k,a in actors.items()}
registry=PROJECT/'Content/HouseReview/original_house_doors.json'
registry_sha=hashlib.sha256(registry.read_bytes()).hexdigest()
door=actors.get('HallEnd_End')
if not door or str(door.get_editor_property('door_id'))!='HallEnd_End':raise RuntimeError('Expected unchanged End door was not found')
before_door=full_state(door)
changed=set(r['actor'] for r in PLAN['replacements'])|{'HallEnd_end_Practical'}
removed=[];new=[]
for label,a in actors.items():
 if label.startswith('InfiniteHall_'):
  unreal.EditorLevelLibrary.destroy_actor(a);removed.append(label)
for row in PLAN['replacements']:
 a=actors[row['actor']];c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_editor_property('override_materials',[])
 c.set_collision_profile_name(row['collision']);c.set_visibility(not row.get('hidden',False));c.set_editor_property('cast_shadow',not row.get('hidden',False))
light=actors.get('HallEnd_end_Practical')
if light:light.get_component_by_class(unreal.PointLightComponent).set_intensity(0.)
for row in PLAN['actors']:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*row['location']))
 a.set_actor_label(row['label']);a.set_folder_path('House/ImpossibleHallway');a.set_actor_scale3d(unreal.Vector(*row['scale']))
 c=a.static_mesh_component;c.set_static_mesh(meshes[row['mesh']]);c.set_collision_profile_name(row['collision']);c.set_visibility(not row['hidden']);c.set_editor_property('generate_overlap_events',False);c.set_editor_property('cast_shadow',False);c.set_lighting_channels(False,True,False)
 new.append(row['label'])
for row in PLAN['lights']:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*row['location']));a.set_actor_label(row['label']);a.set_folder_path('House/ImpossibleHallway')
 c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
 c.set_intensity(row['intensity']);c.set_attenuation_radius(row['radius']);c.set_light_color(unreal.LinearColor(*row['color'],1));c.set_editor_property('cast_shadows',False);c.set_editor_property('source_radius',8.);c.set_lighting_channels(False,True,False)
 new.append(row['label'])
cap=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-2177,-4631.2425,436.2))
cap.set_actor_label('InfiniteHall_DistantBlackEnd');cap.set_folder_path('House/ImpossibleHallway');cap.set_actor_scale3d(unreal.Vector(2.28,.10,2.678))
c=cap.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));c.set_material(0,black);c.set_collision_profile_name('BlockAll');c.set_editor_property('generate_overlap_events',False);c.set_editor_property('cast_shadow',False);c.set_lighting_channels(False,True,False)
new.append('InfiniteHall_DistantBlackEnd')
r=PLAN['runtime'];a=unreal.EditorLevelLibrary.spawn_actor_from_class(runtime_cls,unreal.Vector(*r['location']),unreal.Rotator(pitch=0,yaw=r['yaw'],roll=0))
a.set_actor_label(r['label']);a.set_folder_path('House/ImpossibleHallway')
for name,value in r['properties'].items():a.set_editor_property(name,value)
new.append(r['label'])
after={a.get_actor_label():full_state(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected=[k for k in before if k not in changed and not k.startswith('InfiniteHall_')]
errors=[k for k in protected if before[k]!=after.get(k)]
json.dump({k:{'before':before[k],'after':after.get(k)}for k in errors},open(BASE/'apply-actualdiff.json','w'),indent=2)
if errors:raise RuntimeError('Unrelated actor changed; see apply-actualdiff.json '+str(errors))
if full_state(door)!=before_door:raise RuntimeError('End door was changed')
if hashlib.sha256(registry.read_bytes()).hexdigest()!=registry_sha:raise RuntimeError('Door registry was changed')
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Save map failed')
result={'protectedActors':len(protected),'allProtectedUnchanged':True,'doorUnchanged':True,'doorRegistrySha256':registry_sha,'doorRegistryUnchanged':True,'changed':sorted(changed),'removedPreviousHallActors':removed,'new':new,'meshes':bounds_audit,'actorCount':len(after),'runtime':r,'audio':sound.get_path_name(),'newCorridorLightingChannels':[False,True,False],'distantBlackCapDepthCm':3240}
json.dump(result,open(BASE/'applied.json','w'),indent=2)
unreal.log('IMPOSSIBLE_HALL_APPLY_PASSED protected=%d added=%d doorUnchanged=1 registryUnchanged=1'%(len(protected),len(new)))
