import bpy,json,os,runpy,collections
from mathutils import Vector
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));d=json.load(open(R+'/extraction-manifest.json'));aa={a['label']:a for a in n['actors']};out=[]
for label in ['COL_Entrance_m1_p2_Solid']:
 c=aa[label]['components'][0];info=n['meshes'][c['mesh']];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'));ps=s['components'](o)
 out.append(dict(actor=label,transform=c['transform'],parts=[dict(lo=s['raw'](q['lo']),hi=s['raw'](q['hi']),size=s['raw'](q['size']),faces=len(q['faces']))for q in ps]))
json.dump(out,open(R+'/inventory/collision-window-volumes.json','w'),indent=2);print('SCAN_DONE')
