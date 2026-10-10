import bpy,json,os,runpy
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(os.path.join(ROOT,'source/extract_interactivity.py'));n=json.load(open(os.path.join(ROOT,'context/actors.json')));r={}
for a in n['actors']:
 if not(a['label'].startswith('SM_') and '_Solid' in a['label'] and a['components']):continue
 c=a['components'][0]
 if not any(any(k in (p or '').lower() for k in ('fabric','curtain','light','glass')) for p in c['materials']):continue
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=n['meshes'][c['mesh']]['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
 ps=s['components'](o);r[a['label']]=[dict(index=i,faces=len(p['faces']),lo=list(p['lo']),hi=list(p['hi']),materials=p['materials']) for i,p in enumerate(ps)]
json.dump(r,open(os.path.join(ROOT,'inventory/structural-parts.json'),'w'),indent=2);print('STRUCTURAL_SCAN',len(r))
