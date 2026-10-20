import unreal,json
from pathlib import Path
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/hot-tub-water')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors=unreal.EditorLevelLibrary.get_all_level_actors()
def vec(v):return [v.x,v.y,v.z]
def state(a):
 r=a.get_actor_rotation()
 d={'class':a.get_class().get_path_name(),'name':a.get_name(),'location':vec(a.get_actor_location()),'rotation':[r.pitch,r.yaw,r.roll],'scale':vec(a.get_actor_scale3d()),'tags':[str(t)for t in a.tags]}
 if isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component
  d.update(mesh=c.static_mesh.get_path_name()if c.static_mesh else None,materials=[c.get_material(i).get_path_name()if c.get_material(i)else None for i in range(c.get_num_materials())])
 return d
water=[a for a in actors if isinstance(a,unreal.HouseHotTubWater)]
assert len(water)==1
w=water[0]
props={}
for k in ['water_size','water_depth','grid_resolution','wave_speed','wave_damping','maximum_wave_height','water_material','steam_material','steam_count','steam_opacity','b_jets_running']:
 try:
  v=w.get_editor_property(k)
  props[k]=v.get_path_name()if isinstance(v,unreal.Object)else str(v)
 except Exception as e:props[k]=str(e)
report={'map':unreal.EditorLevelLibrary.get_editor_world().get_path_name(),'count':len(actors),'water':state(w),'properties':props,'actors':{a.get_actor_label():state(a)for a in actors},'dirtyMaps':[x.get_path_name()for x in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]}
json.dump(report,open(root/'before.json','w'),indent=2)
unreal.log('SPA_WATER_BEFORE_AUDIT actor=%s properties=%s dirty=%s'%(w.get_actor_label(),props,report['dirtyMaps']))
