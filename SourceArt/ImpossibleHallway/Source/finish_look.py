"""Give only the new hall private local lighting and seal its distant endpoint."""
import unreal,json,hashlib
from pathlib import Path

BASE=Path('/Users/Stewart/Projects/Senior sendoff/outputs/impossible-hallway')
PROJECT=Path(unreal.Paths.project_dir()).resolve()
DEST='/Game/SeniorSendOff/House/ImpossibleHallway'
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools()
def vec(v):return[v.x,v.y,v.z]
def lighting_channels(c):
 channels=c.get_editor_property('lighting_channels')
 return[bool(channels.get_editor_property('channel'+str(i)))for i in range(3)]
def color(c):return[c.r,c.g,c.b,c.a]
def stable_property(v):
 if isinstance(v,unreal.Object):return v.get_path_name()
 if isinstance(v,unreal.Vector):return vec(v)
 if isinstance(v,(bool,int,float))or v is None:return v
 return str(v)
def state(a):
 rotation=a.get_actor_rotation()
 r={'class':a.get_class().get_name(),'location':vec(a.get_actor_location()),'rotation':[rotation.pitch,rotation.yaw,rotation.roll],'scale':vec(a.get_actor_scale3d())}
 if isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component;r.update(mesh=c.static_mesh.get_path_name()if c.static_mesh else None,materials=[m.get_path_name()if m else None for m in c.get_materials()],collision=str(c.get_collision_profile_name()),visible=c.is_visible(),channels=lighting_channels(c))
 if isinstance(a,unreal.PointLight):
  c=a.get_component_by_class(unreal.PointLightComponent);r.update(light=[c.intensity,c.attenuation_radius,color(c.light_color),c.cast_shadows],channels=lighting_channels(c))
 if isinstance(a,unreal.ExistingHouseDoor):
  for name in('door_id','locked','closed_yaw','open_yaw','doorway_center','panel_center_world','panel_half_extent','original_leaf','original_glass','added_handle_mesh'):
   r[name]=stable_property(a.get_editor_property(name))
 return r

actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
before={k:state(a)for k,a in actors.items()if not k.startswith('InfiniteHall_')}
registry=PROJECT/'Content/HouseReview/original_house_doors.json';registry_sha=hashlib.sha256(registry.read_bytes()).hexdigest()
changed=[];channels={}
for label,a in actors.items():
 if not label.startswith('InfiniteHall_'):continue
 if isinstance(a,unreal.StaticMeshActor):c=a.static_mesh_component
 elif isinstance(a,unreal.PointLight):c=a.get_component_by_class(unreal.PointLightComponent)
 else:continue
 c.set_lighting_channels(False,True,False);changed.append(label);channels[label]=lighting_channels(c)

path=DEST+'/IH_M_DistantBlack'
black=A.load_asset(path)if A.does_asset_exist(path)else T.create_asset('IH_M_DistantBlack',DEST,unreal.Material,unreal.MaterialFactoryNew())
E.delete_all_material_expressions(black)
black.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
black_node=E.create_material_expression(black,unreal.MaterialExpressionConstant3Vector);black_node.constant=unreal.LinearColor(0,0,0,1)
if not E.connect_material_property(black_node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR):raise RuntimeError('Black endcap shader connection failed')
E.recompile_material(black);A.save_loaded_asset(black)
old=actors.get('InfiniteHall_DistantBlackEnd')
if old:unreal.EditorLevelLibrary.destroy_actor(old)
cap=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-2177,-4631.2425,436.2))
cap.set_actor_label('InfiniteHall_DistantBlackEnd');cap.set_folder_path('House/ImpossibleHallway');cap.set_actor_scale3d(unreal.Vector(2.28,.10,2.678))
c=cap.static_mesh_component;c.set_static_mesh(A.load_asset('/Engine/BasicShapes/Cube'));c.set_material(0,black);c.set_collision_profile_name('BlockAll');c.set_editor_property('generate_overlap_events',False);c.set_editor_property('cast_shadow',False);c.set_lighting_channels(False,True,False)
after={a.get_actor_label():state(a)for a in unreal.EditorLevelLibrary.get_all_level_actors()if not a.get_actor_label().startswith('InfiniteHall_')}
diff={k:{'before':before.get(k),'after':after.get(k)}for k in set(before)|set(after)if before.get(k)!=after.get(k)}
json.dump(diff,open(BASE/'actualdiff.json','w'),indent=2)
if diff:raise RuntimeError('An original house actor was modified; see actualdiff.json: '+str(list(diff)))
if hashlib.sha256(registry.read_bytes()).hexdigest()!=registry_sha:raise RuntimeError('The door registry was modified')
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Could not save final hallway appearance')
result={'privateLightingChannels':[False,True,False],'channelActors':channels,'originalActorsUnchanged':len(before),'doorRegistryUnchanged':True,'blackEndcapDepthCm':3240,'blackMaterial':black.get_path_name(),'blackMaterialShading':'MSM_UNLIT','endcapCollision':'BlockAll'}
json.dump(result,open(BASE/'finish-look.json','w'),indent=2)
unreal.log('IMPOSSIBLE_HALL_LOOK_FINISHED channels=%d protected=%d blackEnd=1'%(len(changed),len(before)))
