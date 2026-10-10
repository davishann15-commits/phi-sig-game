"""Apply the approved polish to club-owned actors only; preserve house geometry/gameplay."""
import json,pathlib,re,traceback,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'expansion2_layout/install_expansion2.py').read_text().split('\ndef run():')[0],globals())
P=B/'connection_polish';D='/Game/SeniorSendOff/House/NightClub/ConnectionPolish'

def texture(path,name,rough=False):
 task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=D;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False;T.import_asset_tasks([task]);t=A.load_asset(D+'/'+name);assert t,name
 t.set_editor_property('srgb',not rough)
 if rough:t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
 assert A.save_loaded_asset(t,False);return t

def polished_material(name,row,textures):
 m=A.load_asset(D+'/'+name)or T.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew());E.delete_all_material_expressions(m)
 tex=row.get('texture')
 if tex:
  colour=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);colour.texture=textures[tex]['Colour'];E.connect_material_property(colour,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
  rough=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);rough.texture=textures[tex]['Roughness'];rough.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS);E.connect_material_property(rough,'R',unreal.MaterialProperty.MP_ROUGHNESS)
  if tex=='floor':
   world=E.create_material_expression(m,unreal.MaterialExpressionWorldPosition);mask=E.create_material_expression(m,unreal.MaterialExpressionComponentMask);mask.set_editor_property('r',True);mask.set_editor_property('g',True);mask.set_editor_property('b',False);mask.set_editor_property('a',False)
   scale=E.create_material_expression(m,unreal.MaterialExpressionMultiply);mask_pin=E.get_material_expression_input_names(mask)[0];assert E.connect_material_expressions(world,'',mask,mask_pin),E.get_material_expression_input_names(mask);assert E.connect_material_expressions(mask,'',scale,'A');assert E.connect_material_expressions(const(m,.01),'',scale,'B')
   uv_pin=next(k for k in E.get_material_expression_input_names(colour)if 'uv' in k.lower() or 'coord' in k.lower());assert E.connect_material_expressions(scale,'',colour,uv_pin),(uv_pin,E.get_material_expression_input_names(colour));assert E.connect_material_expressions(scale,'',rough,uv_pin)
 else:
  E.connect_material_property(const(m,row['color']),'',unreal.MaterialProperty.MP_BASE_COLOR);E.connect_material_property(const(m,row['roughness']),'',unreal.MaterialProperty.MP_ROUGHNESS)
 E.connect_material_property(const(m,row.get('metallic',0)),'',unreal.MaterialProperty.MP_METALLIC)
 power=row.get('emissive_strength',0)
 if power:
  rgb=row['color'];strength=power
  # Stable glass glow and very subtle architectural fill are geometry emission only.
  if 'Neon' in name or name in ['M_CP_WornCharcoal','M_CP_WornUpholsteredPanel']:
   if 'Neon' not in name:rgb=[.45,.37,.43];strength=.015 if 'Charcoal' in name else .014
   n=const(m,[c*strength for c in rgb]);inv=E.create_material_expression(m,unreal.MaterialExpressionEyeAdaptationInverse);pin=next(v for v in E.get_material_expression_input_names(inv)if'light'in v.lower());assert E.connect_material_expressions(n,'',inv,pin);assert E.connect_material_property(inv,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
   if 'Neon' in name:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
  else:E.connect_material_property(const(m,[v*power for v in rgb]),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 E.recompile_material(m);assert A.save_loaded_asset(m,False);return m

def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='Chapter01_House'
 actors={a.get_actor_label():a for a in S.get_all_level_actors()};before={a.get_name():snapshot(a)for a in S.get_all_level_actors()};changed=set();added=[]
 def touch(a):changed.add(a.get_name());return a
 club=actors['NightClub_Ambience'];settings={k:str(club.get_editor_property(k))for k in ['lower_room_min','lower_room_max','upper_room_min','upper_room_max','lower_extra_regions','upper_extra_regions','lower_additional_doorways','upper_additional_doorways','lower_light_placements','upper_light_placements','music_loop','music_bpm','music_loop_seconds','moving_light_intensity','strobe_light_intensity','music_volume']}
 art=json.loads((P/'art/manifest.json').read_text());textures={k:{suffix:texture(path,'T_CP_'+k+'_'+suffix,suffix=='Roughness')for suffix,path in values.items()}for k,values in art['textures'].items()};mats={name:polished_material(name,row,textures)for name,row in art['materials'].items()};meshes={}
 for row in art['assets']:
  if row['name']=='SM_CP_DJBackCableDetail':continue
  asset=imp(row['fbx'],row['name']);slots=list(asset.static_materials)
  for slot in slots:
   key=re.sub(r'[._]\d+$','',str(slot.get_editor_property('imported_material_slot_name')));assert key in mats,(row['name'],key);slot.material_interface=mats[key]
  asset.static_materials=slots
  collision(asset,box=row['kind']=='prop' or row['name']=='SM_CP_WallDrinkPerch');assert A.save_loaded_asset(asset,False);meshes[row['name']]=asset
  if row.get('target'):
   a=touch(actors[row['target']]);a.static_mesh_component.set_static_mesh(asset);a.set_actor_scale3d(unreal.Vector(1,1,1))
 # Six sign replacements, with tube pictures rather than neon lettering.
 signs=[('LowerPhi','Disco',(-1740,-1500,222),0),('LowerAfter','Headphones',(-2450,-2135,216),0),('LowerLate','Cocktail',(-1496.2,-1230,220),90),('UpperAfter','Disco',(-1851,-1501,535),0),('UpperPhi','Headphones',(-2206,-1110,537),-90),('UpperLate','Cocktail',(-1496.2,-1330,537),90)]
 for suffix,picture,pos,yaw in signs:
  a=touch(actors['NightClub_Neon_'+suffix]);a.static_mesh_component.set_static_mesh(meshes['SM_CP_NeonPicture_'+picture]);a.set_actor_location(unreal.Vector(*pos),False,True);a.set_actor_rotation(unreal.Rotator(yaw=yaw),False);a.set_actor_scale3d(unreal.Vector(.85,.85,.85));a.tags=list(a.tags)+[unreal.Name('NeonPictogram')]
 # DJ and speakers move as one functional area into the former chair room.
 moves=[('NightClub_Lower_DJ_console',(-2450,-2060,.88),0),('NightClub_Lower_Speaker_0',(-2670,-2060,.88),0),('NightClub_Lower_Speaker_1',(-2235,-2060,.88),0),
  ('NightClub_Lower_Leather_sofa',(-1555,-1200,.88),90),('NightClub_Lower_LoungeSofaSouth',(-2680,-1725,.88),-90),('NightClub_Lower_LoungeCoffeeTable',(-2560,-1740,.88),0),('NightClub_Lower_LoungeOttoman',(-1640,-1390,.88),0),('NightClub_Lower_LoungeSideTable',(-1570,-1008,.88),0)]
 for label,pos,yaw in moves:
  a=touch(actors[label]);a.set_actor_location(unreal.Vector(*pos),False,True);a.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
 # Club-only materials, never edit a material shared with house rooms.
 for label in ['NightClub_SM_NC_LowerRoomShell','NightClub_SM_NC_UpperWallLining','NightClub_SM_NCW_LoungeWallLining']:
  a=touch(actors[label]);c=a.static_mesh_component
  for i in range(c.get_num_materials()):
   m=c.get_material(i)
   if m and m.get_name()=='M_NC_Charcoal':c.set_material(i,mats['M_CP_WornCharcoal'])
 a=touch(actors['NightClub_SM_NC_UpperDarkFloorWithHole']);c=a.static_mesh_component
 for i in range(c.get_num_materials()):
  if 'Floor' in c.get_material(i).get_name():c.set_material(i,mats['M_CP_WornFloor'])
 # Worn DJ/speaker finish retained by the native prop through its appearance refresh.
 for label in ['NightClub_Lower_DJ_console','NightClub_Lower_Speaker_0','NightClub_Lower_Speaker_1']:
  a=touch(actors[label]);mesh=a.get_editor_property('intact_mesh');over=[]
  for slot in mesh.static_materials:
   old=slot.material_interface
   over.append(mats['M_CP_ChippedWalnut']if old and'Walnut'in old.get_name()else mats['M_CP_ScuffedMetal']if old and'PowderCoat'in old.get_name()else old)
  a.set_editor_property('material_overrides',over);a.refresh_prop()
 a=static('NightClub_Polish_ArchitecturalDetails',meshes['SM_CP_ClubArchitecturalDetail']);added.append(a.get_actor_label())
 # Wall ledges sit inside the room perimeter and preserve open walk lanes.
 for label,pos,yaw in [('MainPerch',(-1730,-985,.88),180),('DJPerch',(-2166,-1910,.88),90),('UpperPerch',(-1730,-985,333.1),180)]:
  a=prop('NightClub_Polish_'+label,meshes['SM_CP_WallDrinkPerch'],pos,yaw,mass=24);a.set_editor_property('wall_mounted',True);a.set_editor_property('breakable',False);a.refresh_prop();added.append(a.get_actor_label())
 # Low-mass cups, bottles and napkins are separate items, never part of the tables.
 loose=[('Cup',(-2582,-1740,39)),('Bottle',(-2535,-1748,39)),('Coaster',(-2582,-1740,38.99)),
  ('Cup',(-1730,-985,108.25)),('Bottle',(-1774,-986,108.25)),('Coaster',(-1730,-985,108.2)),
  ('Cup',(-2166,-1910,108.25)),('Bottle',(-2166,-1955,108.25)),
  ('Cup',(-1775,-1338,371.2)),('Bottle',(-1740,-1345,371.2)),
  ('Cup',(-1744,-985,440.45)),('Bottle',(-1690,-985,440.45)),
  ('Napkin',(-1517,-1360,1.17)),('Napkin',(-2620,-1920,1.17)),('Napkin',(-2160,-1618,1.17)),
  ('Napkin',(-2140,-1450,333.15)),('Coaster',(-1540,-1370,389.2))]
 for i,(kind,pos)in enumerate(loose):
  stem={'Cup':'LooseCup','Bottle':'LooseBottle','Coaster':'LooseCoaster','Napkin':'CrumpledNapkin'}[kind];mass={'Cup':.12,'Bottle':.5,'Coaster':.03,'Napkin':.02}[kind]
  a=prop('NightClub_Polish_'+kind+'_'+str(i),meshes['SM_CP_'+stem],pos,17*i%180,mass=mass);a.set_editor_property('breakable',False);a.refresh_prop();added.append(a.get_actor_label())
 assert all(str(club.get_editor_property(k))==v for k,v in settings.items()),'Club gameplay, audibility or light budget changed'
 assert len(club.get_editor_property('club_lights'))==6
 after={a.get_name():snapshot(a)for a in S.get_all_level_actors()};drift=[r['label']for n,r in before.items()if n not in changed and after.get(n)!=r];assert not drift,drift
 assert all(before[n]['label'].startswith('NightClub_')for n in changed),'Out of scope actor changed'
 assert all(after[n]==r for n,r in before.items()if r['cls'] in ['ExistingHouseDoor','HouseClubPole'])
 assert len([r for r in after.values()if r['label'].startswith('NightClub_Neon_')])==6
 assert all('SM_CP_NeonPicture_' in r['meshes'][0]['mesh']for r in after.values()if r['label'].startswith('NightClub_Neon_'))
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report=dict(status='PASS',protected_unchanged=len(before)-len(changed),changed=[before[n]['label']for n in sorted(changed)],added=added,neon_word_boards_remaining=0,new_real_lights=0,real_light_count=6,structural_collision_unchanged=True,exterior_footprint_unchanged=True,doors_pole_music_unchanged=True)
 (P/'evidence/install-report.json').write_text(json.dumps(report,indent=2));(P/'evidence/actors_after.json').write_text(json.dumps(list(after.values()),indent=2));unreal.log('CLUB_POLISH_INSTALL_PASS protected='+str(report['protected_unchanged']))
try:run()
except:unreal.log_error('CLUB_POLISH_INSTALL_FAIL '+traceback.format_exc())
