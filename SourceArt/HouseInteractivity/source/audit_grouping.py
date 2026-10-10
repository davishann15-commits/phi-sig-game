import bpy,runpy,json,os
s=runpy.run_path(os.path.join(os.path.dirname(__file__),'extract_interactivity.py'))
base='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomV3/Baseline/'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=base+'SMF_Entrance_p0_p0_Solid.fbx');o=next(o for o in bpy.context.scene.objects if o.type=='MESH');ps=s['components'](o)
bpy.ops.import_scene.fbx(filepath=base+'COLF_Entrance_p0_p0_Solid.fbx');co=next(o for o in bpy.context.scene.objects if o.name.startswith('COLF'));seeds=s['components'](co)
gs,r=s['classify_groups']('SMF_Entrance_p0_p0_Solid',ps,seeds)
rows=[]
for g in gs:
 lo,hi=s['bounds'](g['parts']);rows.append({'name':g['name'],'behavior':g['behavior'],'parts':len(g['parts']),'bounds':[list(lo),list(hi)],'mats':list(set(m for p in g['parts'] for m in p['materials']))})
json.dump(rows,open(os.path.join(s['ROOT'],'inventory/grouping-preview.json'),'w'),indent=2)
print('GROUP_PREVIEW',len(gs))
