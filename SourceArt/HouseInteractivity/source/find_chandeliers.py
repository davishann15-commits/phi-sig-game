import bpy,json,os,runpy
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');d=json.load(open(R+'/context/actors.json'));r={}
for a in d['actors']:
 if not(a['label'].startswith('SM_') and '_Solid' in a['label'] and a['components']):continue
 c=a['components'][0];b=d['meshes'][c['mesh']]['bounds'];t=c['transform'];sc=t['scale']
 if b[1][2]<330:continue
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=d['meshes'][c['mesh']]['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
 ps=s['components'](o);hits=[]
 for i,p in enumerate(ps):
  center=p['center'];world=Vector((center.x*sc[0],-center.y*sc[1],center.z*sc[2]))
  if 3.3<world.z<5.7 and min((world-Vector((x,1.44,4.45))).length for x in(-6.39,6.48))<1.65:
   hits.append(dict(index=i,faces=len(p['faces']),lo=list(p['lo']),hi=list(p['hi']),materials=p['materials']))
 if hits:r[a['label']]=hits
json.dump(r,open(R+'/inventory/chandelier-targets.json','w'),indent=2);print('CANDIDATES',list(r))
