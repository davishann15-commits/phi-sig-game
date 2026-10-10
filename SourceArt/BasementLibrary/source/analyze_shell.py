import bpy,json,runpy
from pathlib import Path
from mathutils import Vector
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
S=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/extract_interactivity.py')
report=[]
for path in sorted((R/'context/fbx').glob('*.fbx')):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(path))
 for o in bpy.context.scene.objects:
  if o.type!='MESH':continue
  if not o.data.materials:o.data.materials.append(bpy.data.materials.new('Collider'))
  o.scale.x*=1.8;o.scale.y*=1.8;bpy.context.view_layer.update()
  for p in S['components'](o):
   lo=p['lo'];hi=p['hi'];s=p['size']
   if (lo.x<-23.7 and hi.x>-30.5) and (s.x*s.y>.4 or s.y*s.z>.25 or s.x*s.z>.25):
    report.append(dict(source=path.stem,lo=list(lo),hi=list(hi),size=list(s),materials=p['materials'],faces=len(p['faces'])))
(R/'context/shell-parts.json').write_text(json.dumps(report,indent=2))
print('SHELL_PARTS',len(report))
