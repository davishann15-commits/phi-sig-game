"""Apply only the concrete defects found in the native room review."""
import os,json,re,unreal
HELPER='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Content/Python/apply_bedroom.py'
exec(open(HELPER).read().split('textures={}')[0].replace("if name.startswith('SM'):","if True:"))
BASE='/Users/Stewart/Projects/Senior sendoff/outputs/bathroom-opposite-fridge-room'
SOURCE=os.path.join(unreal.Paths.project_dir(),'SourceArt/Room204')
DEST='/Game/SeniorSendOff/House/Room204'
PLAN=json.load(open(SOURCE+'/import_manifest.json'))
for filename in PLAN['textures']:
 if 'MottledGrayRug' not in filename:continue
 tex=imp(SOURCE+'/'+filename,os.path.splitext(filename)[0])
 if 'Normal' in filename:
  tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('srgb',False)
 elif 'Roughness' in filename:tex.set_editor_property('srgb',False)
 A.save_loaded_asset(tex)
meshes={}
for row in PLAN['meshes']:
 if 'DecoratedDoorLeaf' not in row['name']:continue
 m=imp(SOURCE+'/'+row['file'],row['name'])
 for i,s in enumerate(m.static_materials):
  key=re.sub(r'_\d{3}$','',str(s.material_slot_name).split('.')[0])
  mat=A.load_asset(DEST+'/'+key)
  if not mat:raise RuntimeError('Missing door material '+key)
  m.set_material(i,mat)
 A.save_loaded_asset(m);meshes[row['name']]=m
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
before={k:state(a) for k,a in actors.items()};changed=[]
for row in PLAN['actors']:
 a=actors[row['label']]
 a.set_actor_location(unreal.Vector(*row.get('location',[0,0,0])),False,False)
 a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=row.get('yaw',0),roll=0),False)
 a.set_actor_scale3d(unreal.Vector(*row.get('scale',[1,1,1])));changed.append(row['label'])
for row in PLAN['door_updates']:
 a=actors[row['actor']]
 for key,val in row['properties'].items():
  if key in ('doorway_center','panel_center_world','panel_half_extent'):val=unreal.Vector(*val)
  elif key in ('original_leaf','original_glass','added_handle_mesh'):val=meshes[val] if val else None
  a.set_editor_property(key,val)
 a.set_actor_location(unreal.Vector(*row['location']),False,False)
 p=a.get_actor_location();a.set_actor_location(p+unreal.Vector(.01,0,0),False,False);a.set_actor_location(p,False,False)
 changed.append(row['actor'])
registry_path=os.path.join(unreal.Paths.project_content_dir(),'HouseReview/original_house_doors.json');registry=json.load(open(registry_path))
for update in PLAN['door_updates']:
 matches=[r for r in registry if r['id']==update['id']]
 if len(matches)!=1:raise RuntimeError('Door registry mismatch')
 row=matches[0];props=update['properties']
 row.update(hinge=update['location'],closedYaw=props['closed_yaw'],openYaw=props['open_yaw'],doorwayCenter=props['doorway_center'],panelCenterWorld=props['panel_center_world'],panelHalfExtent=props['panel_half_extent'],locked=props['locked'],leafAsset=DEST+'/'+props['original_leaf'])
after={a.get_actor_label():state(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected=[k for k in before if k not in changed]
errors=[k for k in protected if before[k]!=after.get(k)]
if errors:raise RuntimeError('Protected actor changes '+str(errors))
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Level save failed')
json.dump(registry,open(registry_path,'w'),indent=2)
json.dump(dict(changed=changed,protected_actors=len(protected),all_protected_unchanged=True,door_updates=PLAN['door_updates']),open(BASE+'/refined.json','w'),indent=2)
unreal.log('ROOM204_REFINEMENT_PASSED protected=%d changed=%d'%(len(protected),len(changed)))
