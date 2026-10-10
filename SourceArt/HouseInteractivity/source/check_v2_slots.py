import bpy,json,os
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));d=json.load(open(R+'/extraction-manifest.json'));v=json.load(open(R+'/inventory/damage-v2-preview.json'));out=[]
for p in d['pieces']:
 if p['asset_name']not in v['assets']:continue
 if p['id']not in ['SMV_GR_ClubChair1_Solid__001','Room102_Chair_01__001','BR_SM_Bedroom_OvalCoffeeTable__001']:continue
 for kind,path in [('intact',p['intact_fbx'])]+[(k,v['assets'][p['asset_name']][k+'_fbx'])for k in ['damaged','heavily_damaged','broken']]:
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=path);o=next(o for o in bpy.context.scene.objects if o.type=='MESH'and not o.name.startswith('UCX_'));out.append(dict(id=p['id'],kind=kind,slots=[m.name for m in o.data.materials],used={i:sum(1 for po in o.data.polygons if po.material_index==i)for i in range(len(o.data.materials))}))
json.dump(out,open(R+'/inventory/damage-v2-slotcheck.json','w'),indent=2)
