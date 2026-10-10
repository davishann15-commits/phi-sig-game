import unreal,json
plan=json.load(open(unreal.Paths.project_dir() + 'Content/HouseReview/original_house_doors.json'))
porch=json.load(open(unreal.Paths.project_dir() + 'Content/HouseReview/original_porch_doors.json'))
dst='/Game/Story/Maps/Chapter01_House'
unreal.EditorLoadingAndSavingUtils.load_map(dst)
actors=unreal.EditorLevelLibrary.get_all_level_actors()
for a in actors:
 if isinstance(a,unreal.ExistingHouseDoor):unreal.EditorLevelLibrary.destroy_actor(a)
actors=unreal.EditorLevelLibrary.get_all_level_actors()
for stem in sorted(set(a['sourceMesh'] for a in plan)):
 room=next((a for a in actors if a.get_actor_label()==stem),None)
 if not room:raise RuntimeError('Missing source actor '+stem)
 path='/Game/ExistingHouseDoors/SM_OriginalRoom_'+stem[3:]
 asset=unreal.load_asset(path)
 if not asset:raise RuntimeError('Missing replacement '+path)
 room.modify()
 room.static_mesh_component.modify()
 room.static_mesh_component.set_static_mesh(asset)
 # The original map has separate COL_ mesh actors. Imported visual chunks must
 # not generate broad convex collision that fills room openings.
 room.static_mesh_component.set_collision_profile_name('NoCollision')
# The ground-floor porch has four original pairs of framed glass doors. Their
# panes and leaf trim were split from these same imported chunks; static jambs
# and transoms remain in the room assets.
for chunk in ('p0_m1','p0_p0'):
 for suffix in ('Solid','Glass'):
  label='SM_Entrance_'+chunk+'_'+suffix
  room=next((a for a in actors if a.get_actor_label()==label),None)
  if not room:raise RuntimeError('Missing porch source actor '+label)
  path='/Game/ExistingHouseDoors/SM_OriginalRoom_Entrance_'+chunk+'_'+suffix
  asset=unreal.load_asset(path)
  if not asset:raise RuntimeError('Missing porch room asset '+path)
  room.modify();room.static_mesh_component.modify()
  room.static_mesh_component.set_static_mesh(asset)
  room.static_mesh_component.set_collision_profile_name('NoCollision')
# The side door beyond the South A room has a solid wall behind its frame.
# Preserve the original wall collision and mark that leaf locked in the plan.
collider=next((a for a in actors if a.get_actor_label()=='COL_Upper_p0_m2_Solid'),None)
if not collider:raise RuntimeError('Missing upper-floor collision chunk')
collider.modify()
collider.static_mesh_component.modify()
collider.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/SeniorSendOff/House/Meshes/COL_Upper_p0_m2_Solid'))
collider.static_mesh_component.set_collision_profile_name('BlockAll')
for item in plan:
 if not item['locked'] and abs((float(item.get('openYaw',0.0))-float(item['closedYaw'])+180.0)%360.0-180.0)<5.0:
  raise RuntimeError('Usable door has no opening swing: '+item['id'])
 h=item['hinge'];c=item['doorwayCenter'];a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.ExistingHouseDoor,unreal.Vector(*h),unreal.Rotator(pitch=0,yaw=0,roll=0))
 a.set_actor_label('OriginalDoor_'+item['id'][16:])
 a.set_editor_property('original_leaf',unreal.load_asset('/Game/ExistingHouseDoors/'+item['id']))
 a.set_editor_property('doorway_center',unreal.Vector(*c))
 a.set_editor_property('door_id',unreal.Name(item['id']))
 a.set_editor_property('closed_yaw',float(item['closedYaw']))
 a.set_editor_property('open_yaw',float(item.get('openYaw',0.0)))
 a.set_editor_property('locked',bool(item['locked']))
 if item['id'] in {
  'SM_OriginalLeaf_Entrance_m1_m1_Solid_01',
  'SM_OriginalLeaf_Entrance_p0_p0_Solid_01',
  'SM_OriginalLeaf_Upper_m1_m1_Solid_01',
  'SM_OriginalLeaf_Upper_m1_p0_Solid_02',
  'SM_OriginalLeaf_Upper_m1_p0_Solid_03',
 }:
  bounds=item['fbxBounds']
  lo=[bounds[0][0]*180,-bounds[1][1]*180]
  hi=[bounds[1][0]*180,-bounds[0][1]*180]
  axis=0 if hi[0]-lo[0]>hi[1]-lo[1] else 1
  latch=max((lo[axis],hi[axis]),key=lambda v:abs(v-h[axis]))
  sign=1 if latch>h[axis] else -1
  position=[(lo[0]+hi[0])/2,(lo[1]+hi[1])/2,bounds[0][2]*100+103]
  position[axis]=latch-sign*10
  yaw=(0 if sign>0 else 180) if axis==0 else (90 if sign>0 else -90)
  a.set_editor_property('added_handle_mesh',unreal.load_asset('/Game/ExistingHouseDoors/SM_HouseLeverHandle'))
  a.set_editor_property('added_handle_world_position',unreal.Vector(*position))
  a.set_editor_property('added_handle_rotation',unreal.Rotator(pitch=0,yaw=yaw,roll=0))
 a.set_actor_location(unreal.Vector(h[0]+.01,h[1],h[2]),False,False)
 a.set_actor_location(unreal.Vector(*h),False,False)
for index,item in enumerate(porch):
 b=item['bounds'];side=index%2
 pair=porch[index-side:index-side+2]
 left,right=pair
 group_x=(left['bounds'][0][0]+right['bounds'][1][0])/2*180
 y=-(b[0][1]+b[1][1])/2*180
 hinge_x=(b[0][0]-.067 if side==0 else b[1][0]+.067)*180
 hinge=(hinge_x,y,0)
 center=(group_x,y,110)
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.ExistingHouseDoor,unreal.Vector(*hinge),unreal.Rotator(pitch=0,yaw=0,roll=0))
 a.set_actor_label('OriginalPorchDoor_'+item['id'])
 a.set_editor_property('original_leaf',unreal.load_asset('/Game/ExistingHouseDoors/'+item['frame']))
 a.set_editor_property('original_glass',unreal.load_asset('/Game/ExistingHouseDoors/'+item['glass']))
 a.set_editor_property('doorway_center',unreal.Vector(*center))
 a.set_editor_property('door_id',unreal.Name(item['id']))
 a.set_editor_property('door_group',unreal.Name(item['group']))
 xlo,xhi=b[0][0]*180,b[1][0]*180
 zlo,zhi=b[0][2]*100,b[1][2]*100
 a.set_editor_property('panel_center_world',unreal.Vector((xlo+xhi)/2,y,(zlo+zhi)/2))
 a.set_editor_property('panel_half_extent',unreal.Vector((xhi-xlo)/2+8,8,(zhi-zlo)/2+8))
 a.set_editor_property('closed_yaw',0.0)
 a.set_editor_property('open_yaw',90.0 if side==0 else -90.0)
 position=(group_x-(10 if side==0 else -10),y,104)
 a.set_editor_property('added_handle_mesh',unreal.load_asset('/Game/ExistingHouseDoors/SM_HouseLeverHandle'))
 a.set_editor_property('added_handle_world_position',unreal.Vector(*position))
 a.set_editor_property('added_handle_rotation',unreal.Rotator(pitch=0,yaw=0 if side==0 else 180,roll=0))
 a.set_actor_location(unreal.Vector(hinge[0]+.01,hinge[1],hinge[2]),False,False)
 a.set_actor_location(unreal.Vector(*hinge),False,False)
unreal.EditorLevelLibrary.save_current_level()
unreal.log('ORIGINAL_HOUSE_DOORS_PLACED: %d room leaves, %d porch leaves; %d locked' % (len(plan),len(porch),sum(a['locked'] for a in plan)))
