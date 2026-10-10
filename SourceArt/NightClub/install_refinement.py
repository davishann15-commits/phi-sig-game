import unreal,json,pathlib,traceback,re,sys
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub';sys.path.insert(0,str(B))
# Reuse import helpers without running the original whole-scene installer.
exec((B/'install.py').read_text().split('def run():')[0],globals())

def texture(path,srgb):
 n=pathlib.Path(path).stem
 t=unreal.AssetImportTask();t.filename=path;t.destination_path=D;t.destination_name=n;t.automated=True;t.replace_existing=True;t.save=False
 T.import_asset_tasks([t]);a=A.load_asset(D+'/'+n);assert a
 a.set_editor_property('srgb',srgb);assert A.save_loaded_asset(a,False)
 return a

def door_material(n,row):
 m=material(n,row)
 for key,prop,srgb in [('base_texture',unreal.MaterialProperty.MP_BASE_COLOR,True),('roughness_texture',unreal.MaterialProperty.MP_ROUGHNESS,False)]:
  if row.get(key):
   tex=texture(row[key],srgb);node=E.create_material_expression(m,unreal.MaterialExpressionTextureSample);node.texture=tex
   node.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
   assert E.connect_material_property(node,'RGB'if srgb else'R',prop)
 E.recompile_material(m);assert A.save_loaded_asset(m,False);return m

def run():
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
 actors={a.get_actor_label():a for a in S.get_all_level_actors()}
 before={a.get_name():state(a)for a in S.get_all_level_actors()}
 allowed={'HallEnd_SM_HallEnd_ThreeRoomsFloors','HallEnd_SM_HallEnd_ThreeRoomsShell','HallEnd_COL_HallEnd_ThreeRoomsShell'}
 geo=json.load(open(B/'expansion_geometry/expansion_manifest.json'));doorart=json.load(open(B/'door_refit/door_manifest.json'))
 mats={n:A.load_asset(D+'/'+n)or material(n,r)for n,r in geo['materials'].items()}
 mats.update({n:door_material(n,r)for n,r in doorart['materials'].items()})
 frame=material('M_NCR_FrameWhite',{'color':[.74,.72,.66],'roughness':.68})
 meshes={}
 for row in geo['assets']+doorart['assets']:
  # Preserve the already repaired lower hallway aperture and original assets.
  if row.get('sourceActor')and row['sourceActor']not in allowed:continue
  mesh=imp(row['fbx'],row['name']);slots=list(mesh.static_materials)
  if row.get('sourceActor'):
   original=list(actors[row['sourceActor']].static_mesh_component.static_mesh.static_materials)
   assert len(slots)==len(original)
   for i,slot in enumerate(slots):slot.material_interface=original[i].material_interface
  else:
   for slot in slots:
    n=re.sub(r'[._]\d+$','',str(slot.get_editor_property('imported_material_slot_name')))
    assert n in mats,(row['name'],n);slot.material_interface=mats[n]
  mesh.static_materials=slots
  complex_mesh=row['name'].startswith('COL_')or row['name']in ['SM_NCR_LowerRoomShell','SM_NCR_LowerCeilingWithHole','SM_NCR_UpperSupportFloorWithHole','SM_NCR_UpperExtensionCollision','SM_NCR_LowerDoorTrim']
  collision(mesh,complex=complex_mesh);assert A.save_loaded_asset(mesh,False)
  meshes[row['name']]=mesh
  if row.get('sourceActor'):actors[row['sourceActor']].static_mesh_component.set_static_mesh(mesh)
 for old,new in [('SM_NC_LowerRoomShell','SM_NCR_LowerRoomShell'),('SM_NC_LowerCeilingWithHole','SM_NCR_LowerCeilingWithHole'),('SM_NC_UpperDarkFloorWithHole','SM_NCR_UpperDarkFloorWithHole'),('SM_NC_UpperWallLining','SM_NCR_UpperWallLining'),('SM_NC_LowerDanceFloorInlay','SM_NCR_LowerDanceFloorInlay'),('SM_NC_LowerDoorTrim','SM_NCR_LowerDoorTrim')]:
  actors['NightClub_'+old].static_mesh_component.set_static_mesh(meshes[new])
 for name in ['SM_NCR_UpperSupportFloorWithHole','SM_NCR_UpperExtensionCollision']:
  label='NightClub_'+name
  if label in actors:actors[label].static_mesh_component.set_static_mesh(meshes[name])
  else:static(label,meshes[name],block=True)
 trim=actors['NightClub_SM_NC_LowerDoorTrim'].static_mesh_component
 for i in range(trim.get_num_materials()):trim.set_material(i,frame)
 door=actors['NightClub_LowerEntranceDoor'];leaf=meshes['SM_NCD101_PhotoDoor'];door.set_editor_property('original_leaf',leaf)
 for c in door.get_components_by_class(unreal.StaticMeshComponent):
  if c.get_name()=='OriginalDoorLeaf':c.set_static_mesh(leaf)
 p=door.get_actor_location();door.set_actor_location(p+unreal.Vector(.001,0,0),False,True);door.set_actor_location(p,False,True)
 pole=actors['NightClub_ConnectingFirePole'];pole.set_editor_property('interaction_radius',210.0)
 delta=146.8;center=(-2212.51019-1490.0)/2;shiftcenter=center-(-1935)
 # Absolute placement from the pre-refinement export makes repeat repairs safe.
 baseline={a['label']:a for a in json.load(open(B/'refinement_context/actors.json'))}
 def move(label,dx):
  a=actors[label];p=baseline[label]['location'];a.set_actor_location(unreal.Vector(p[0]+dx,p[1],p[2]),False,True)
 for n in ['NightClub_Neon_LowerLate','NightClub_Neon_UpperLate','NightClub_Tube_Lower1','NightClub_Tube_Upper1','NightClub_Lower_Leather_sofa','NightClub_Upper_Lounge_table','NightClub_Lower_Speaker_1']:move(n,delta)
 for n in ['NightClub_Neon_LowerPhi','NightClub_Neon_LowerAfter','NightClub_Neon_UpperAfter','NightClub_Tube_Lower0','NightClub_Tube_Lower2','NightClub_Tube_Upper0','NightClub_Tube_Upper2','NightClub_Lower_DJ_console','NightClub_Upper_Leather_sofa','NightClub_LowerDiscoBall','NightClub_UpperDiscoBall']:move(n,shiftcenter)
 club=actors['NightClub_Ambience']
 club.set_editor_property('lower_room_max',unreal.Vector(-1490,-973.5,327.03))
 club.set_editor_property('upper_room_max',unreal.Vector(-1490,-973.5,622.71));club.refresh_club()
 after={a.get_name():a for a in S.get_all_level_actors()}
 errors=[row['label']for name,row in before.items()if not row['label'].startswith('NightClub_')and row['label']not in allowed and(name not in after or state(after[name])!=row)]
 assert not errors,('Protected actors changed',errors)
 assert door.get_editor_property('door_id')=='NightClubLower'
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 report={'status':'PASS','room_width_before_cm':575.71019,'room_width_after_cm':722.51019,'floor_area_increase_percent':146.8/575.71019*100,'lower_room_bounds':[[-2212.51019,-1509.02675,.88],[-1490,-973.5,327.03]],'upper_room_bounds':[[-2212.51019,-1509.02675,332.53],[-1490,-973.5,622.71]],'protected_unchanged':True,'allowed_original_geometry':sorted(allowed),'door':'white recessed six-panel 101, black keypad, silver lever','door_id_preserved':str(door.get_editor_property('door_id')),'pole_interaction_radius':pole.get_editor_property('interaction_radius'),'added_collision':['upper support floor with true hole','upper extension walls/ceiling']}
 json.dump(report,open(B/'evidence/refinement-install.json','w'),indent=2);unreal.log('CLUB_REFINEMENT_INSTALL_PASS '+json.dumps(report))
try:run()
except:unreal.log_error('CLUB_REFINEMENT_INSTALL_FAIL '+traceback.format_exc())
