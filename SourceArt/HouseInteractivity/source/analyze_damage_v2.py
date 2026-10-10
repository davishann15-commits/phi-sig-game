import bpy,json,os,runpy
R=os.path.dirname(os.path.dirname(os.path.abspath(__file__)));s=runpy.run_path(R+'/source/extract_interactivity.py');d=json.load(open(R+'/extraction-manifest.json'));out=[]
for id in ['SMV_GR_ClubChair1_Solid__001','SM_GR3_NearSeating__003','BR_SM_Bedroom_OvalCoffeeTable__001','Room102_Chair_01__001','BR_BedDetail_FramedCollage_08__001','R204_GalanzFridgeAndMicrowave__001']:
 p=next(p for p in d['pieces']if p['id']==id);bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=p['intact_fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH');ps=s['components'](o);out.append(dict(id=id,parts=[dict(index=i,lo=s['raw'](q['lo']),hi=s['raw'](q['hi']),size=s['raw'](q['size']),faces=len(q['faces']),materials=q['materials'])for i,q in enumerate(ps)]))
json.dump(out,open(R+'/inventory/damage-v2-components.json','w'),indent=2);print('DONE')
