import unreal,json,pathlib,re,traceback,sys,math
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub';sys.path.insert(0,str(B));from install_ambience import install_ambience
D='/Game/SeniorSendOff/House/NightClub';A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools();S=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);M=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
def xyz(v):return[v.x,v.y,v.z]
def state(a):
 r=a.get_actor_rotation();return dict(label=a.get_actor_label(),location=xyz(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=xyz(a.get_actor_scale3d()),meshes=[c.static_mesh.get_path_name()if c.static_mesh else None for c in a.get_components_by_class(unreal.StaticMeshComponent)],lights=[c.intensity for c in a.get_components_by_class(unreal.LightComponent)])
def imp(path,name):
 t=unreal.AssetImportTask();t.filename=str(path);t.destination_path=D;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=False;t.factory=unreal.FbxFactory();o=unreal.FbxImportUI();o.import_mesh=True;o.import_as_skeletal=False;o.import_materials=False;o.import_textures=False;o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False;o.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;t.options=o;T.import_asset_tasks([t]);mesh=A.load_asset(D+'/'+name);assert mesh,name;return mesh

def const(m,v):
 if isinstance(v,list):n=E.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*v,1)
 else:n=E.create_material_expression(m,unreal.MaterialExpressionConstant);n.r=v
 return n

def material(n,row):
 m=A.load_asset(D+'/'+n)or T.create_asset(n,D,unreal.Material,unreal.MaterialFactoryNew());E.delete_all_material_expressions(m)
 for prop,key,default in [(unreal.MaterialProperty.MP_BASE_COLOR,'color',[.03,.03,.05]),(unreal.MaterialProperty.MP_ROUGHNESS,'roughness',.5),(unreal.MaterialProperty.MP_METALLIC,'metallic',0)]:E.connect_material_property(const(m,row.get(key,default)),'',prop)
 if row.get('emissive_strength',0)>0:
  v=row.get('emissive',row['color'])[:3]
  if 'Neon' in n:
   m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);light=const(m,[x*2.0 for x in v]);inverse=E.create_material_expression(m,unreal.MaterialExpressionEyeAdaptationInverse);iname=next(x for x in E.get_material_expression_input_names(inverse)if'light'in x.lower());assert E.connect_material_expressions(light,'',inverse,iname);assert E.connect_material_property(inverse,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  else:E.connect_material_property(const(m,[x*row['emissive_strength']for x in v]),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 E.recompile_material(m);assert A.save_loaded_asset(m,False),n;return m

def collision(mesh,complex=False,box=False):
 M.remove_collisions(mesh)
 if complex:
  setup=mesh.get_editor_property('body_setup');setup.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 if box:M.add_simple_collisions(mesh,unreal.ScriptingCollisionShapeType.BOX)
 mesh.set_editor_property('support_ray_tracing',False)

def static(label,mesh,pos=(0,0,0),yaw=0,scale=(1,1,1),block=False,hidden=False):
 a=S.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),unreal.Rotator(yaw=yaw));a.set_actor_label(label);a.set_folder_path('Nightclub/'+('Structure'if'COL_'in label or'Shell'in label else'Decor'));a.set_actor_scale3d(unreal.Vector(*scale));c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('BlockAll'if block else'NoCollision');c.set_editor_property('visible',not hidden);a.set_actor_hidden_in_game(hidden)
 if not block:c.set_cast_shadow(False)
 if label.startswith('NightClub_Neon_')or label.startswith('NightClub_Tube_'):
  c.set_editor_property('affect_dynamic_indirect_lighting',False);c.set_editor_property('affect_distance_field_lighting',False)
 return a

def prop(label,mesh,pos,yaw=0,scale=(1,1,1),mass=45):
 a=S.spawn_actor_from_class(unreal.HouseInteractiveProp,unreal.Vector(*pos),unreal.Rotator(yaw=yaw));a.set_actor_label(label);a.set_folder_path('Nightclub/Furniture');a.set_actor_scale3d(unreal.Vector(*scale));a.set_editor_property('intact_mesh',mesh);a.set_editor_property('prop_id',label);a.set_editor_property('prop_name',label.replace('NightClub_','').replace('_',' '));a.set_editor_property('mass_kg',float(mass));a.set_editor_property('break_health',float(mass*.45+15));a.set_editor_property('movable',True);a.set_editor_property('breakable',True);a.refresh_prop();return a

def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0');unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House');old={a.get_name():state(a)for a in S.get_all_level_actors()};actors={a.get_actor_label():a for a in S.get_all_level_actors()};changed=set()
 # Idempotent rebuild of only this request's new actors.
 for a in list(S.get_all_level_actors()):
  if a.get_actor_label().startswith('NightClub_'):S.destroy_actor(a)
 art=json.load(open(B/'assets/club_manifest.json'));extra=json.load(open(B/'assets/addendummanifest.json'));art['assets']+=extra['assets'];art['materials'].update(extra['materials']);geo=json.load(open(B/'geometry/geometry_manifest.json'));mats={n:material(n,r)for n,r in {**art['materials'],**geo['materials']}.items()};meshes={}
 for row in art['assets']+geo['assets']:
  mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
  if row.get('sourceActor'):
   a=actors[row['sourceActor']];source=a.static_mesh_component.static_mesh;orig=list(source.static_materials);assert len(slots)==len(orig),(row['name'],len(slots),len(orig))
   for i,slot in enumerate(slots):slot.material_interface=orig[i].material_interface
  else:
   for slot in slots:
    key=re.sub(r'[._]\d+$','',str(slot.get_editor_property('imported_material_slot_name')));assert key in mats,(row['name'],key);slot.material_interface=mats[key]
  mesh.static_materials=slots
  complex=row['name'].startswith('COL_')or row['name']in['SM_NC_LowerRoomShell','SM_NC_LowerCeilingWithHole','SM_NC_PoleGuard','SM_NC_FirePole','SM_NC_LowerDoorTrim']
  box=any(k in row['name']for k in['DJBooth','PASpeaker','ClubSofa'])
  collision(mesh,complex,box);assert A.save_loaded_asset(mesh,False),row['name'];meshes[row['name']]=mesh
  if row.get('sourceActor'):
   a=actors[row['sourceActor']];changed.add(a.get_name());a.static_mesh_component.set_static_mesh(mesh)
 for name in ['SM_NC_LowerRoomShell','SM_NC_LowerCeilingWithHole','SM_NC_UpperDarkFloorWithHole','SM_NC_UpperWallLining','SM_NC_LowerDanceFloorInlay','SM_NC_LowerDoorTrim']:
  static('NightClub_'+name,meshes[name],block=name in['SM_NC_LowerRoomShell','SM_NC_LowerCeilingWithHole','SM_NC_LowerDoorTrim'])
 # Only the existing east-room lamp is dimmed; no other house lighting is changed.
 lamp=actors['HallEnd_east_Practical'];changed.add(lamp.get_name())
 for c in lamp.get_components_by_class(unreal.LightComponent):c.set_intensity(20);c.set_cast_shadows(False)
 # The existing luminous lens belongs to a mesh shared with other rooms.
 # Cover only this room's lens so its bright material cannot wash out the club.
 lens=static('NightClub_UpperPracticalLensCover',A.load_asset('/Engine/BasicShapes/Cylinder'),pos=(-1924.67,-1224.3,614.65),scale=(.65,.65,.012))
 lens.static_mesh_component.set_material(0,mats['M_NC_PowderCoat'])
 # Lower room entrance faces inward (+X) when opened.
 door=S.spawn_actor_from_class(unreal.ExistingHouseDoor,unreal.Vector(-2228.35,-1296,.88));door.set_actor_label('NightClub_LowerEntranceDoor');door.set_folder_path('Nightclub/Entrance')
 leaf=meshes['SM_NC_ClubEntranceDoor'];door.set_editor_property('original_leaf',leaf);door.set_editor_property('door_id','NightClubLower');door.set_editor_property('local_leaf_space',True);door.set_editor_property('doorway_center',unreal.Vector(-2228.35,-1375,111));door.set_editor_property('panel_center_world',unreal.Vector(79,0,109.5));door.set_editor_property('panel_half_extent',unreal.Vector(79,3,109.5));door.set_editor_property('closed_yaw',-90.0);door.set_editor_property('open_yaw',0.0);door.set_editor_property('locked',False)
 p=door.get_actor_location();door.set_actor_location(p+unreal.Vector(.001,0,0),False,True);door.set_actor_location(p,False,True)
 # Shared pole actor plus a genuine annular floor opening and guard, never an invisible disk.
 pole=S.spawn_actor_from_class(unreal.HouseClubPole,unreal.Vector(-1935,-1175,.88));pole.set_actor_label('NightClub_ConnectingFirePole');pole.set_folder_path('Nightclub/Pole')
 for key,v in [('lower_landing',(0,150,0)),('upper_landing',(0,150,331.65)),('lower_shaft',(0,45,0)),('upper_shaft',(0,45,331.65)),('pole_base',(0,0,0)),('pole_top',(0,0,621.83))]:pole.set_editor_property(key,unreal.Vector(*v))
 pole.set_editor_property('auto_fit_pole_mesh',False);pc=pole.get_editor_property('pole_mesh');pc.set_static_mesh(meshes['SM_NC_FirePole']);pc.set_relative_location(unreal.Vector(),False,True);pc.set_relative_scale3d(unreal.Vector(1,1,621.83/301.5));pc.set_collision_profile_name('BlockAll')
 pivot=pole.get_editor_property('gate_pivot');pivot.set_relative_location(unreal.Vector(59.125,102.4075,331.65),False,True);pivot.set_relative_rotation(unreal.Rotator(),False,True)
 gv=pole.get_editor_property('gate_visual');gv.set_static_mesh(meshes['SM_NC_PoleAccessGate']);gv.set_relative_location(unreal.Vector(),False,True);gv.set_relative_scale3d(unreal.Vector(1.1,1.1,1.1));gv.set_collision_profile_name('NoCollision')
 gc=pole.get_editor_property('gate_collision');gc.set_relative_location(unreal.Vector(0,105,384.15),False,True);gc.set_box_extent(unreal.Vector(62,10,52.5),False)
 static('NightClub_UpperPoleGuard',meshes['SM_NC_PoleGuard'],(-1935,-1175,333.1),scale=(1.1,1.1,1.1),block=True)
 for label,z in [('Upper',332.7),('Lower',327.1)]:static('NightClub_'+label+'HoleCollar',meshes['SM_NC_PoleHoleCollar'],(-1935,-1175,z),scale=(100/86.36,100/86.36,1.1))
 for label,z in [('Lower',.88),('Upper',333.1)]:static('NightClub_'+label+'PoleLanding',meshes['SM_NC_PoleLandingPad'],(-1935,-1025,z),scale=(.6,.27,.25))
 # Six neon signs, all mesh emission, plus small tube accents.
 signs=[('LowerPhi','PhiSig',(-1935,-1500,242),0,1.05),('LowerLate','OpenLate',(-1643,-1245,237),90,.85),('LowerAfter','AfterHours',(-1900,-980,265),180,.7),('UpperAfter','AfterHours',(-1935,-1501,552),0,1.05),('UpperPhi','PhiSig',(-2206,-1110,540),-90,.8),('UpperLate','OpenLate',(-1643,-1330,550),90,.8)]
 for label,n,pos,yaw,sc in signs:static('NightClub_Neon_'+label,meshes['SM_NC_Sign_'+n],pos,yaw,(sc,sc,sc))
 for floor,z in [('Lower',292),('Upper',590)]:
  for i,(color,pos,yaw,sc)in enumerate([('Cyan',(-1935,-1500,z),0,2.3),('Magenta',(-1643,-1230,z),90,2.1),('Violet',(-1935,-980,z),180,2.3)]):static('NightClub_Tube_'+floor+str(i),meshes['SM_NC_WallStrip_'+color],pos,yaw,(sc,1,1))
 prop('NightClub_Lower_DJ_console',meshes['SM_NC_DJBooth'],(-1935,-1445,.88),scale=(.85,.85,.9),mass=125)
 for i,x in enumerate([-2160,-1710]):prop('NightClub_Lower_Speaker_'+str(i),meshes['SM_NC_PASpeaker'],(x,-1444,.88),mass=38)
 # Seating leaves the middle open, including the north approach to the pole.
 prop('NightClub_Upper_Leather_sofa',meshes['SM_NC_ClubSofa'],(-1935,-1440,333.1),mass=85)
 prop('NightClub_Lower_Leather_sofa',meshes['SM_NC_ClubSofa'],(-1685,-1210,.88),yaw=90,scale=(.9,.8,.9),mass=85)
 for label,z,xy in [('Lower',294,(-1990,-1275)),('Upper',589,(-1920,-1360))]:static('NightClub_'+label+'DiscoBall',meshes['SM_NC_DiscoBall'],(*xy,z),scale=(.43,.43,.43))
 # A slim lounge table uses the existing house asset, independently moveable.
 table=A.load_asset('/Game/SeniorSendOff/House/BoywickBedroom/SM_BWR_CoffeeTable');assert table
 prop('NightClub_Upper_Lounge_table',table,(-1720,-1310,333.1),scale=(.55,.45,.8),mass=22)
 ambiance=install_ambience(unreal,meshes)
 club=next(a for a in S.get_all_level_actors()if a.get_actor_label()=='NightClub_Ambience');club.set_editor_property('moving_light_intensity',320.0)
 for a in S.get_all_level_actors():
  if isinstance(a,unreal.PostProcessVolume):
   pp=a.get_editor_property('settings');unreal.log('CLUB_EXISTING_PP '+a.get_actor_label()+' priority='+str(a.priority)+' bias='+str(pp.get_editor_property('auto_exposure_bias'))+' bloom='+str(pp.get_editor_property('bloom_intensity'))+' threshold='+str(pp.get_editor_property('bloom_threshold')))
 after={a.get_name():a for a in S.get_all_level_actors()};errs=[r['label']for n,r in old.items()if n not in changed and not r['label'].startswith('NightClub_') and (n not in after or state(after[n])!=r)]
 assert not errs,('Protectedactorschanged',errs)
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level(),'Mapsavefailed'
 report=dict(status='PASS',protected_actors=len(old)-len(changed),protected_unchanged=True,changed_original_actors=[old[n]['label']for n in changed],new_actors=[a.get_actor_label()for a in after.values()if a.get_actor_label().startswith('NightClub_')],upper_room='HallEnd_East across from Boy Wick',lower_room='new room directly beneath HallEnd_East',room101_and_boywick_preserved=True,pole_current_cm=[-1935,-1175,.88],hole_radius_cm=100,ambience=ambiance)
 json.dump(report,open(B/'evidence/install.json','w'),indent=2,default=str);unreal.log('NIGHTCLUB_INSTALL_PASS '+str(report['protected_actors']))
try:run()
except:unreal.log_error('NIGHTCLUB_INSTALL_FAIL '+traceback.format_exc())
