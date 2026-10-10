import json,os,re,unreal
ROOT='/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1'
helper=os.path.join(unreal.Paths.project_content_dir(),'Python/apply_bedroom.py')
code=open(helper).read().split('\ntextures={}')[0].replace("if name.startswith('SM'):","if True:")
exec(compile(code,helper,'exec'))
BASE=ROOT;SOURCE=ROOT+'/import';DEST='/Game/SeniorSendOff/House/PigstyRoom';PLAN=json.load(open(SOURCE+'/import_manifest.json'))
textures={}
for file in PLAN['textures']:
 name=os.path.splitext(file)[0];tex=imp(SOURCE+'/'+file,name)
 if 'normal' in file.lower():
  tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('srgb',False);tex.set_editor_property('flip_green_channel',file in PLAN['flip_green_textures'])
 elif 'roughness' in file.lower():tex.set_editor_property('srgb',False)
 A.save_loaded_asset(tex);textures[file]=tex
materials={k:make_material(k,v,textures) for k,v in PLAN['materials'].items()}
materials['PIG_M_Cheeto'].set_editor_property('used_with_instanced_static_meshes',True)
E.recompile_material(materials['PIG_M_Cheeto']);A.save_loaded_asset(materials['PIG_M_Cheeto'])
# Grime mesh uses its texture alpha and stays transparent at the edges.
m=materials['PST_M_GrimeScratchDecal'];m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
n=sample(m,'PST_T_GrimeScratchDecal_RGBA.png',textures);prop(n,m,unreal.MaterialProperty.MP_OPACITY,'A');E.recompile_material(m);A.save_loaded_asset(m)
for t in PLAN['textures']:
 if any(s in t for s in ('DampCeiling','KeepShutPlaque','GrimeScratch')):
  tex=textures[t];tex.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);tex.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP);A.save_loaded_asset(tex)
report=[]
for row in PLAN['meshes']:
 name=row['name'];mesh=imp(SOURCE+'/'+row['file'],name)
 if row['complex_collision']:mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 for i,s in enumerate(mesh.static_materials):
  key=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0]);mat=materials.get(key)
  if not mat:
   path='/Game/SeniorSendOff/House/Materials/'+key
   if A.does_asset_exist(path):mat=A.load_asset(path)
  if not mat and row['complex_collision']:mat=materials['PST_M_DirtyOlivePlaster']
  if not mat:raise RuntimeError('Missing material '+key+' '+name)
  mesh.set_material(i,mat)
 A.save_loaded_asset(mesh);bounds=mesh.get_bounds();actual=[vec(bounds.origin-bounds.box_extent),vec(bounds.origin+bounds.box_extent)]
 expected=row.get('boundsCm')
 if expected:
  wanted=[expected['min'],expected['max']] if isinstance(expected,dict) else expected
  if max(abs(actual[b][i]-wanted[b][i]) for b in range(2) for i in range(3))>.08:raise RuntimeError('Native scale/pivot mismatch '+name+' '+str(actual)+' wanted '+str(wanted))
 report.append({'name':name,'boundsCm':actual,'expectedBoundsCm':expected,'group':row['group'],'slots':[str(s.material_slot_name) for s in mesh.static_materials]})
json.dump(report,open(ROOT+'/import/native-assets.json','w'),indent=2)
unreal.log('PIGSTY_ASSETS_IMPORT_PASSED meshes=%d'%len(report))
