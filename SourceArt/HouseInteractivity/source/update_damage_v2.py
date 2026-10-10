"""Refresh damage assets in place; preserve all saved furnishing placements."""
import unreal,json,traceback
from pathlib import Path
ROOT=Path('/Users/Stewart/Projects/Senior sendoff/outputs/house-interactivity')
source=(ROOT/'source/install_interactivity.py').read_text()
# Reuse import/material/collision code only, without rebuilding level actors.
exec(compile(source.split("world=unreal.EditorLoadingAndSavingUtils.load_map(")[0],str(ROOT/'source/install_interactivity.py'),'exec'))
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
if not world:raise RuntimeError('House map load failed')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
report=[]
material_report={}
removed=[]
for p in MAN['pieces']:
 if p['behavior']!='furniture':continue
 if p.get('superseded_by'):
  replacement=actors.get('Interactive_'+p['superseded_by'])
  if not isinstance(replacement,unreal.HouseInteractiveProp):raise RuntimeError('Approved replacement missing '+p['superseded_by'])
  obsolete=actors.get('Interactive_'+p['id'])
  if obsolete:
   for child in obsolete.get_attached_actors():child.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD)
   if not unreal.EditorLevelLibrary.destroy_actor(obsolete):raise RuntimeError('Could not remove obsolete overlap '+p['id'])
  removed.append({'id':p['id'],'preserved':p['superseded_by']})
  continue
 if p.get('damage_geometry_version')!=2:raise RuntimeError('Incomplete V2 manifest '+p['id'])
 a=actors.get('Interactive_'+p['id'])
 if not isinstance(a,unreal.HouseInteractiveProp):raise RuntimeError('Installed prop missing '+p['id'])
 before=a.get_actor_transform()
 intact,broken,damaged,heavy=mesh_cache[p['asset_name']]
 for mesh,field in [(damaged,'damaged_material_slots'),(heavy,'heavily_damaged_material_slots'),(broken,'broken_material_slots')]:
  if mesh.get_name() in material_report:continue
  slots=[]
  native_slots=mesh.get_editor_property('static_materials')
  for index,path in mesh_material_assignments(mesh,p[field],True):
   material=mesh.get_material(index)
   if material!=loadmat(path):raise RuntimeError('Wrong installed damage material '+mesh.get_name()+' '+str(index))
   slots.append({'index':index,'imported_name':str(native_slots[index].get_editor_property('imported_material_slot_name')),'material':material.get_path_name()})
  material_report[mesh.get_name()]=slots
 for key,value in {'intact_mesh':intact,'broken_mesh':broken,'damaged_mesh':damaged,'heavily_damaged_mesh':heavy}.items():a.set_editor_property(key,value)
 a.refresh_prop()
 after=a.get_actor_transform()
 if (before.translation-after.translation).length()>.001:raise RuntimeError('Unexpected prop movement '+p['id'])
 report.append({'id':p['id'],'stages':[x.get_name() for x in (intact,damaged,heavy,broken)],'hulls':[meshedit.get_convex_collision_count(x)+meshedit.get_simple_collision_count(x) for x in (intact,damaged,heavy,broken)]})
if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House'):raise RuntimeError('House save failed')
json.dump({'passed':True,'updated':len(report),'removedObsoleteOverlaps':removed,'actors':report},open(ROOT/'inventory/damage-v2-installed.json','w'),indent=2)
json.dump({'passed':True,'auditedStageMeshes':len(material_report),'meshes':material_report},open(ROOT/'inventory/damage-v2-materials-installed.json','w'),indent=2)
unreal.log('HOUSE_DAMAGE_V2_UPDATED '+str(len(report)))
