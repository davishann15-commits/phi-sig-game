import bpy,pathlib,json
B=pathlib.Path(__file__).resolve().parent
for r in json.load(open(B/'art/cuts.json'))['assets']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=r['fbx']);o=next(o for o in bpy.context.scene.objects if o.type=='MESH');print('NOODLE_SLOTS',r['name'],[m.name for m in o.data.materials],r['materials'],flush=True)
