import json,os,re,unreal
ROOT='/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1'
helper=os.path.join(unreal.Paths.project_content_dir(),'Python/apply_bedroom.py')
exec(compile(open(helper).read().split('\ntextures={}')[0].replace("if name.startswith('SM'):","if True:"),helper,'exec'))
SOURCE=ROOT+'/import';DEST='/Game/SeniorSendOff/House/PigstyRoom';PLAN=json.load(open(SOURCE+'/import_manifest.json'))
report=json.load(open(ROOT+'/import/native-assets.json'))
for row in PLAN['meshes']:
 if row['group']!='geometry':continue
 mesh=imp(SOURCE+'/'+row['file'],row['name'])
 if row['complex_collision']:mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 for i,s in enumerate(mesh.static_materials):
  key=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0]);path=DEST+'/'+key if A.does_asset_exist(DEST+'/'+key) else '/Game/SeniorSendOff/House/Materials/'+key
  mat=A.load_asset(path) if A.does_asset_exist(path) else A.load_asset(DEST+'/PST_M_DirtyOlivePlaster') if row['complex_collision'] else None
  if not mat:raise RuntimeError('Missing material '+key)
  mesh.set_material(i,mat)
 A.save_loaded_asset(mesh);bb=mesh.get_bounds();actual=[vec(bb.origin-bb.box_extent),vec(bb.origin+bb.box_extent)];want=row['boundsCm']
 if max(abs(actual[b][i]-want['min' if b==0 else 'max'][i]) for b in range(2) for i in range(3))>.08:raise RuntimeError('Wrong native geometrybounds')
 for r in report:
  if r['name']==row['name']:r['boundsCm']=actual;r['expectedBoundsCm']=want
json.dump(report,open(ROOT+'/import/native-assets.json','w'),indent=2)
m=A.load_asset(DEST+'/PIG_M_Cheeto');m.set_editor_property('used_with_instanced_static_meshes',True);E.recompile_material(m);A.save_loaded_asset(m)
bag=imp(SOURCE+'/PIG_SM_CheetosBag.fbx','PIG_SM_CheetosBag')
for i,slot in enumerate(bag.static_materials):bag.set_material(i,A.load_asset(DEST+'/'+str(slot.material_slot_name).split('.')[0]))
A.save_loaded_asset(bag)
unreal.log('PIGSTY_FOOD_MATERIAL_FIXED instanced=1 bagLabelsReadable=1')
exec(compile(open(ROOT+'/source/apply_room.py').read(),ROOT+'/source/apply_room.py','exec'))
