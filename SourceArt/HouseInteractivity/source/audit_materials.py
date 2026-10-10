import bpy,json,os,sys
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));d=json.load(open(R+'/context/actors.json'));labels=['SMF_Entrance_m1_m2_Solid','SM_GR3_NearSeating','SM_Entrance_p0_m1_Solid']
for label in labels:
 a=next(a for a in d['actors'] if a['label']==label);c=a['components'][0];info=d['meshes'][c['mesh']]
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=info['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
 print('ASSET',label)
 print('FBX',[(i,m.name) for i,m in enumerate(o.data.materials)])
 print('NATIVE',[(i,m['slot'],m['material'].split('/')[-1] if m['material'] else None) for i,m in enumerate(info['materials'])])
 print('COMP',c['materials'])
