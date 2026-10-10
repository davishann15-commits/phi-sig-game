import json,os,re,unreal
ROOT='/Users/Stewart/Projects/Senior sendoff/outputs/poker-room-v1'
helper=os.path.join(unreal.Paths.project_content_dir(),'Python/apply_bedroom.py')
code=open(helper).read().split('\ntextures={}')[0].replace("if name.startswith('SM'):","if True:")
exec(compile(code,helper,'exec'))
BASE=ROOT; SOURCE=ROOT+'/import'; DEST='/Game/SeniorSendOff/House/PokerRoom'
PLAN=json.load(open(SOURCE+'/import_manifest.json'))
textures={}
for file in PLAN['textures']:
 name=os.path.splitext(file)[0]
 tex=A.load_asset(DEST+'/'+name) if A.does_asset_exist(DEST+'/'+name) else imp(SOURCE+'/'+file,name)
 if 'normal' in file.lower():
  tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
  tex.set_editor_property('srgb',False)
  tex.set_editor_property('flip_green_channel',file in PLAN['flip_green_textures'])
 elif 'roughness' in file.lower():tex.set_editor_property('srgb',False)
 A.save_loaded_asset(tex);textures[file]=tex
materials={k:make_material(k,v,textures) for k,v in PLAN['materials'].items()}
# The card mesh uses its full UV square. One scalar selects a cell of the 13x4 atlas.
m=materials['POK_M_CardAtlas']; E.delete_all_material_expressions(m)
idx=node(m,unreal.MaterialExpressionScalarParameter);idx.set_editor_property('parameter_name','CardIndex');idx.set_editor_property('default_value',0)
floor=node(m,unreal.MaterialExpressionFloor);conn(idx,floor,E.get_material_expression_input_names(floor)[0])
col=node(m,unreal.MaterialExpressionFmod);conn(floor,col,'A');conn(value(m,13),col,'B')
rowdiv=node(m,unreal.MaterialExpressionDivide);conn(floor,rowdiv,'A');conn(value(m,13),rowdiv,'B')
row=node(m,unreal.MaterialExpressionFloor);conn(rowdiv,row,E.get_material_expression_input_names(row)[0])
offset=node(m,unreal.MaterialExpressionAppendVector);conn(col,offset,'A');conn(row,offset,'B')
uv=node(m,unreal.MaterialExpressionTextureCoordinate)
plus=node(m,unreal.MaterialExpressionAdd);conn(uv,plus,'A');conn(offset,plus,'B')
size=node(m,unreal.MaterialExpressionAppendVector);conn(value(m,13),size,'A');conn(value(m,4),size,'B')
div=node(m,unreal.MaterialExpressionDivide);conn(plus,div,'A');conn(size,div,'B')
tex=sample(m,'POK_T_CardAtlas.png',textures);conn(div,tex,'UVs')
prop(tex,m,unreal.MaterialProperty.MP_BASE_COLOR,'RGB');prop(value(m,.65),m,unreal.MaterialProperty.MP_ROUGHNESS)
E.recompile_material(m);A.save_loaded_asset(m)
report=[]
for row in PLAN['meshes']:
 name=row['name'];mesh=imp(SOURCE+'/'+row['file'],name)
 if row['complex_collision']:
  mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 for i,s in enumerate(mesh.static_materials):
  key=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0]);mat=materials.get(key)
  if not mat and A.does_asset_exist('/Game/SeniorSendOff/House/Materials/'+key):mat=A.load_asset('/Game/SeniorSendOff/House/Materials/'+key)
  if not mat and row['complex_collision']:mat=materials['M_POK_Walnut']
  if not mat:raise RuntimeError('Missing material '+key+' '+name)
  mesh.set_material(i,mat)
 A.save_loaded_asset(mesh)
 bounds=mesh.get_bounds();lo=bounds.origin-bounds.box_extent;hi=bounds.origin+bounds.box_extent
 actual=[vec(lo),vec(hi)];expected=row.get('boundsCm')
 if expected:
  size=[actual[1][i]-actual[0][i] for i in range(3)];want=[expected[1][i]-expected[0][i] for i in range(3)]
  if max(abs(size[i]-want[i]) for i in range(3))>.05:raise RuntimeError('Native units mismatch '+name+' '+str(size)+' wanted '+str(want))
 report.append({'name':name,'boundsCm':actual,'sourceBoundsCm':expected,'group':row['group'],'slots':[str(s.material_slot_name) for s in mesh.static_materials]})
json.dump(report,open(ROOT+'/import/native-assets.json','w'),indent=2)
unreal.log('POKER_ASSETS_IMPORT_PASSED meshes=%d'%len(report))
