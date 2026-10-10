"""Install only the central front entrance; assert all unrelated actors match."""
import unreal,json,re,traceback
from pathlib import Path
B=Path(__file__).resolve().parent;D='/Game/SeniorSendOff/House/FrontDoubleDoors'
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools();S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def xyz(v):return [v.x,v.y,v.z]
def snap(a):
 tr=a.get_actor_transform();r=tr.rotation
 row=dict(name=a.get_name(),label=a.get_actor_label(),class_name=a.get_class().get_name(),pos=xyz(tr.translation),scale=xyz(tr.scale3d),q=[r.x,r.y,r.z,r.w],tags=[str(t) for t in a.tags],meshes=[])
 for c in a.get_components_by_class(unreal.StaticMeshComponent):row['meshes'].append(dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,materials=[m.get_path_name() if m else None for m in c.get_materials()],collision=str(c.get_collision_enabled())))
 return row
def imp(path,name):
 task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=D;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False
 task.factory=unreal.FbxFactory();o=unreal.FbxImportUI();o.import_mesh=True;o.import_as_skeletal=False;o.import_materials=False;o.import_textures=False;o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False;o.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;o.static_mesh_import_data.normal_generation_method=unreal.FBXNormalGenerationMethod.BUILT_IN;task.options=o
 T.import_asset_tasks([task]);m=A.load_asset(D+'/'+name);assert m,name;return m
def const(m,v):
 if isinstance(v,list):n=E.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*v,1)
 else:n=E.create_material_expression(m,unreal.MaterialExpressionConstant);n.r=float(v)
 return n
def sample(m,p,typ):
 n=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);n.texture=A.load_asset(p);assert n.texture,p;n.sampler_type=typ;return n
def op(m,cls,a,ao,b,bo):
 n=E.create_material_expression(m,cls);E.connect_material_expressions(a,ao,n,'A');E.connect_material_expressions(b,bo,n,'B');return n
def material(name,color,rough,metal,wood=False):
 m=A.load_asset(D+'/'+name) or T.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew());E.delete_all_material_expressions(m)
 base=const(m,color);rn=const(m,rough)
 if wood:
  detail=sample(m,'/Game/SeniorSendOff/House/NoodleWing/T_NW_wood_Detail',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
  tone=op(m,unreal.MaterialExpressionMultiply,detail,'RGB',const(m,.8),'')
  tone=op(m,unreal.MaterialExpressionAdd,tone,'',const(m,.6),'')
  base=op(m,unreal.MaterialExpressionMultiply,tone,'',base,'')
  rs=sample(m,'/Game/SeniorSendOff/House/NoodleWing/T_NW_wood_Roughness',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
  rn=op(m,unreal.MaterialExpressionMultiply,rs,'R',const(m,.22),'')
  rn=op(m,unreal.MaterialExpressionAdd,rn,'',const(m,rough),'')
  ns=sample(m,'/Game/SeniorSendOff/House/NoodleWing/T_NW_wood_Normal',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
  n=E.create_material_expression(m,unreal.MaterialExpressionLinearInterpolate);E.connect_material_expressions(const(m,[0,0,1]),'',n,'A');E.connect_material_expressions(ns,'RGB',n,'B');E.connect_material_expressions(const(m,.27),'',n,'Alpha');E.connect_material_property(n,'',unreal.MaterialProperty.MP_NORMAL)
 E.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR);E.connect_material_property(rn,'',unreal.MaterialProperty.MP_ROUGHNESS);E.connect_material_property(const(m,metal),'',unreal.MaterialProperty.MP_METALLIC);E.recompile_material(m);assert A.save_loaded_asset(m,False);return m
def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');assert world
 manifest=json.loads((B/'manifest.json').read_text())
 before={a.get_name():snap(a) for a in S.get_all_level_actors()};actors={a.get_actor_label():a for a in S.get_all_level_actors()};changed=set();removed=[]
 mats={}
 for name,color,r,metal,wood in [('M_FD_Walnut',[.19,.076,.029],.28,0,True),('M_FD_WalnutRail',[.19,.076,.029],.28,0,True),('M_FD_WalnutPanel',[.225,.098,.04],.29,0,True),('M_FD_WalnutDark',[.075,.025,.009],.32,0,True),('M_FD_Brass',[.52,.32,.12],.23,1,False),('M_FD_BrassDark',[.32,.18,.055],.34,1,False),('M_FD_Black',[.005,.005,.005],.52,0,False)]:mats[name]=material(name,color,r,metal,wood)
 meshes={}
 for key,name in [('leaf','SM_FD_WalnutLeaf'),('frame','SM_FD_WalnutFrame')]:
  m=imp(manifest[key],name);slots=list(m.static_materials)
  for sl in slots:
   n=re.sub(r'[._]\d+$','',str(sl.get_editor_property('imported_material_slot_name')));assert n in mats,(name,n);sl.material_interface=mats[n]
  m.static_materials=slots;m.set_editor_property('allow_cpu_access',True);m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX);assert A.save_loaded_asset(m,False);meshes[key]=m
 for cut in manifest['cuts']:
  m=imp(cut['fbx'],cut['name']);lookup={p.split('.')[-1]:p for p in cut['materials'] if p};slots=list(m.static_materials)
  for sl in slots:
   n=re.sub(r'[._]\d+$','',str(sl.get_editor_property('imported_material_slot_name')));assert n in lookup,(cut['name'],n);sl.material_interface=A.load_asset(lookup[n])
  m.static_materials=slots;m.set_editor_property('allow_cpu_access',True);m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);assert A.save_loaded_asset(m,False)
  a=actors[cut['source_actor']];changed.add(a.get_name());a.static_mesh_component.set_static_mesh(m);a.static_mesh_component.set_editor_property('override_materials',[])
 for label in ['Interactive_SM_Entrance_m1_m1_Glass__001','Interactive_SM_Entrance_m1_m1_Glass__002']:
  a=actors.get(label);assert a,label;changed.add(a.get_name());removed.append(label);S.destroy_actor(a)
 for a in list(S.get_all_level_actors()):
  if a.get_actor_label().startswith('FrontWalnut_'):changed.add(a.get_name());S.destroy_actor(a)
 for i,p in enumerate(manifest['hinges']):
  a=S.spawn_actor_from_class(unreal.ExistingHouseDoor,unreal.Vector(*p),unreal.Rotator(yaw=0 if i==0 else 180));a.set_actor_label('FrontWalnut_'+('LeftDoor' if i==0 else 'RightDoor'));a.set_folder_path('Front Entrance/Walnut Doors');a.tags=[unreal.Name('FrontWalnutEntrance')]
  a.set_editor_property('original_leaf',meshes['leaf']);a.set_editor_property('original_glass',None);a.set_editor_property('door_id','FrontWalnut_'+('Left' if i==0 else 'Right'));a.set_editor_property('door_group','FrontWalnutDoubleDoors');a.set_editor_property('local_leaf_space',True);a.set_editor_property('doorway_center',unreal.Vector(*manifest['center']));a.set_editor_property('panel_center_world',unreal.Vector(manifest['width']/2,0,manifest['height']/2));a.set_editor_property('panel_half_extent',unreal.Vector(0,0,0));a.set_editor_property('locked',False);a.set_editor_property('closed_yaw',0.0);a.set_editor_property('open_yaw',90.0 if i==0 else -90.0)
  q=a.get_actor_location();a.set_actor_location(q+unreal.Vector(.001,0,0),False,True);a.set_actor_location(q,False,True)
 a=S.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-36.65,-989,0));a.set_actor_label('FrontWalnut_FittedFrame');a.set_folder_path('Front Entrance/Walnut Doors');a.tags=[unreal.Name('FrontWalnutEntrance')];a.static_mesh_component.set_static_mesh(meshes['frame']);a.static_mesh_component.set_collision_profile_name('BlockAll')
 after={a.get_name():snap(a) for a in S.get_all_level_actors()};drift=[r['label'] for n,r in before.items() if n not in changed and after.get(n)!=r];assert not drift,drift
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House')
 report=dict(status='PASS',protected_actors=len(before)-len(changed),removed_glass=removed,retained_shell_cuts=[c['source_actor'] for c in manifest['cuts']],new_actors=['FrontWalnut_LeftDoor','FrontWalnut_RightDoor','FrontWalnut_FittedFrame'],new_lights=0)
 (B/'evidence/install-report.json').write_text(json.dumps(report,indent=2));unreal.log('FRONT_DOUBLE_DOORS_INSTALLED '+json.dumps(report))
try:run()
except:unreal.log_error('FRONT_DOUBLE_DOORS_INSTALL_FAILED '+traceback.format_exc());raise
