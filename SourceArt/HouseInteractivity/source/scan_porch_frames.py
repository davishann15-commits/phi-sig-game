import bpy,json,os,runpy
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');n=json.load(open(R+'/context/actors.json'));out=[]
for a in n['actors']:
 if not a['label'].startswith('OriginalPorchDoor'):continue
 c=next(c for c in a['components'] if c['name']=='OriginalDoorLeaf');info=n['meshes'][c['mesh']];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'));ps=s['components'](o)
 out.append(dict(label=a['label'],parts=[dict(lo=s['raw'](p['lo']),hi=s['raw'](p['hi']),size=s['raw'](p['size']),faces=len(p['faces']),materials=p['materials'])for p in ps]))
json.dump(out,open(R+'/inventory/porch-frame-parts.json','w'),indent=2)
print('PORCH_SCAN',len(out))
