"""Install only the live hot-tub response materials and actor tuning."""
import unreal,json
from pathlib import Path
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/hot-tub-water')
asset=unreal.EditorAssetLibrary
edit=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
dest='/Game/SeniorSendOff/House/HotTubRoomV1'
def expression(m,cls):return edit.create_material_expression(m,cls)
def scalar(m,v):
 n=expression(m,unreal.MaterialExpressionConstant);n.r=v;return n
def rgb(m,v):
 n=expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*v,1);return n
def prop(n,m,p):
 if not edit.connect_material_property(n,'',getattr(unreal.MaterialProperty,p)):raise RuntimeError('Failed material input '+p)
def make(name,tint,rough):
 m=asset.load_asset(dest+'/'+name) if asset.does_asset_exist(dest+'/'+name) else None
 if not m:m=tools.create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
 edit.delete_all_material_expressions(m)
 m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
 m.set_editor_property('used_with_instanced_static_meshes',True)
 prop(rgb(m,tint),m,'MP_BASE_COLOR');prop(scalar(m,rough),m,'MP_ROUGHNESS')
 prop(scalar(m,.7),m,'MP_SPECULAR');prop(scalar(m,0),m,'MP_METALLIC')
 edit.recompile_material(m)
 if not asset.save_loaded_asset(m):raise RuntimeError('Could not save '+name)
 return m
splash=make('M_Spa_SplashDroplet',(.22,.56,.59),.11)
foam=make('M_Spa_SplashFoam',(.73,.86,.84),.37)
original=asset.load_asset(dest+'/M_Spa_ReactiveWater_v2')
assert isinstance(original,unreal.Material)
water=asset.load_asset(dest+'/M_Spa_ReactiveWater_v3') if asset.does_asset_exist(dest+'/M_Spa_ReactiveWater_v3') else None
if not water:water=tools.duplicate_asset('M_Spa_ReactiveWater_v3',dest,original)
changed=[]
# Match the vertex-color foam connection rather than unrelated numeric values.
original_values={n.get_name():n.r for n in edit.get_material_expressions(original) if isinstance(n,unreal.MaterialExpressionConstant)}
for n in edit.get_material_expressions(water):
 if isinstance(n,unreal.MaterialExpressionConstant) and n.get_name() in original_values:
  n.r=original_values[n.get_name()]
for n in edit.get_material_expressions(water):
 if not isinstance(n,unreal.MaterialExpressionMultiply):continue
 inputs=[v for v in edit.get_inputs_for_material_expression(water,n) if v]
 if not any(isinstance(v,unreal.MaterialExpressionVertexColor)for v in inputs):continue
 for v in inputs:
  if isinstance(v,unreal.MaterialExpressionConstant)and abs(v.r-.42)<.001:
   v.r=.65;changed.append(v.get_name())
if not changed:raise RuntimeError('Could not identify the actual localized vertex foam weight')
edit.recompile_material(water)
if not asset.save_loaded_asset(water):raise RuntimeError('Could not save reactive water')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors=unreal.EditorLevelLibrary.get_all_level_actors()
def vec(v):return[v.x,v.y,v.z]
def state(a):
 r=a.get_actor_rotation()
 d={'class':a.get_class().get_path_name(),'name':a.get_name(),'location':vec(a.get_actor_location()),'rotation':[r.pitch,r.yaw,r.roll],'scale':vec(a.get_actor_scale3d()),'tags':[str(t)for t in a.tags]}
 if isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component
  d.update(mesh=c.static_mesh.get_path_name()if c.static_mesh else None,materials=[c.get_material(i).get_path_name()if c.get_material(i)else None for i in range(c.get_num_materials())])
 return d
before=json.load(open(root/'before.json'))
waters=[a for a in actors if isinstance(a,unreal.HouseHotTubWater)]
if len(waters)!=1:raise RuntimeError('Expected exactly one existing tub')
w=waters[0]
if state(w)!=before['water']:raise RuntimeError('Tub transform or geometry changed unexpectedly')
w.set_editor_property('splash_material',splash);w.set_editor_property('foam_material',foam)
w.set_editor_property('water_material',water)
w.set_editor_property('wave_speed',62.0);w.set_editor_property('wave_damping',1.1);w.set_editor_property('maximum_wave_height',5.0)
w.refresh_water()
unchanged={a.get_actor_label():state(a)for a in actors if a!=w}
for label,s in unchanged.items():
 if s!=before['actors'][label]:raise RuntimeError('Protected actor changed: '+label)
if len(actors)!=before['count']:raise RuntimeError('Unexpected actor added')
if not unreal.EditorLoadingAndSavingUtils.save_current_level():raise RuntimeError('Map save failed')
report={'map':'Chapter01_House','actorCount':len(actors),'protectedActorsUnchanged':len(unchanged),'waterTransformUnchanged':True,'materials':{'water':water.get_path_name(),'splash':splash.get_path_name(),'foam':foam.get_path_name()},'waveSpeed':w.wave_speed,'waveDamping':w.wave_damping,'maxWaveHeightCm':w.maximum_wave_height,'foamWeightChangedNodes':changed}
json.dump(report,open(root/'applied.json','w'),indent=2)
unreal.log('SPA_WATER_RESPONSE_APPLIED '+json.dumps(report))
