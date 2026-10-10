import bpy,json,runpy
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen');S=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/extract_interactivity.py')
bpy.ops.wm.read_factory_settings(use_empty=True);rows=[]
for f in (R/'context/fbx').glob('SM_*.fbx'):
 bpy.ops.import_scene.fbx(filepath=str(f))
 for o in list(bpy.context.selected_objects):
  if o.type!='MESH':continue
  bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
  
  if not o.data.materials:o.data.materials.append(bpy.data.materials.new('Unknown'))
  ps=S['components'](o)
  for p in ps:
   rows.append(dict(source=f.stem,bounds=[list(p['lo']),list(p['hi'])],materials=p['materials'],faces=len(p['faces'])))
  bpy.data.objects.remove(o,do_unlink=True)
(R/'context/geometry-components.json').write_text(json.dumps(rows,indent=2));print('GEOMETRY_COMPONENTS',len(rows))
